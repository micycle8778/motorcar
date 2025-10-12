#pragma once
// TODO: handle failure for the lua functions better. they currently crash in an unclear way

#include <any>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <optional>
#include <queue>
#include <sol/types.hpp>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <ranges>

#include <sol/sol.hpp>
#include <utility>
#include "spdlog/spdlog.h"

#include "types.h"

#define MOTORCAR_EAT_EXCEPTION(code, msg) try { code; } catch (const std::exception& e) { SPDLOG_ERROR(msg, " what(): {}", e.what()); } catch (...) { SPDLOG_ERROR(msg); }
namespace motorcar {
    using Command = std::function<void()>;

    template <typename ...Components>
    class Query;

    template <typename T>
    struct ComponentTypeTrait {
        constexpr static bool value = false;
        constexpr static std::string_view component_name = "";
    };

    class ComponentStorage {
        template <typename ...T>
        friend class Query;
        friend class ECSWorld;

        const std::string_view component_name = "";
        const std::type_info* type;

        void* blob = nullptr;
        size_t capacity = 0;
        size_t len = 0;
        size_t stride = 0;

        std::unordered_map<Entity, size_t> indices;
        std::vector<Entity> entities;

        void (*dtor)(void*) = nullptr;
        void (*move_from_ptr)(void* dest, void* src) = nullptr;
        void (*ctor_from_sol_object)(void* dest, sol::object src) = nullptr;
        sol::object (*get_sol_object)(void*, sol::state&) = nullptr;

        void* compute_pointer(size_t index) const { return (void*)((size_t)blob + (index * stride)); }
        ComponentStorage(
                const std::string_view component_name,
                const std::type_info* type
        ) : component_name(component_name), type(type)
        {}
        public:
            template <typename T>
            static ComponentStorage create(size_t initial_capacity) {
                static_assert(ComponentTypeTrait<T>::value);
                ComponentStorage result = ComponentStorage(
                    ComponentTypeTrait<T>::component_name,
                    &typeid(T)
                );

                result.stride = sizeof(T);
                result.capacity = initial_capacity;

                if (result.capacity > 0) {
                    result.blob = malloc(result.stride * result.capacity);
                }

                result.dtor = [](void* t) { ((T*)t)->~T(); };
                result.move_from_ptr = [](void* dest, void* src) { new ((T*)dest) T(std::move(*(T*)src)); };
                result.ctor_from_sol_object = [](void* dest, sol::object src) { new ((T*)dest) T(src); };
                result.get_sol_object = [](void* ptr, sol::state& lua) { return sol::make_object(lua, std::ref(*(T*)ptr)); };

                return result;
            }

            template <typename T, typename ...Args>
            void emplace_component(Entity e, Args&& ...args) {
                assert(type == &typeid(T));

                if (indices.contains(e)) {
                    MOTORCAR_EAT_EXCEPTION(new (compute_pointer(indices[e])) T(std::forward<Args&&>(args)...), "caught exception when constructing component");
                } else {
                    if (len == capacity) {
                        expand();
                    }

                    MOTORCAR_EAT_EXCEPTION(new (compute_pointer(len)) T(std::forward<Args&&>(args)...), "caught exception when constructing component");
                    len++;
                    indices.emplace(e, len - 1);
                    entities.push_back(e);
                }
            }

            template <typename T>
            std::optional<T*> get_component(Entity e) {
                if (!indices.contains(e)) {
                    return {};
                }

                return (T*)compute_pointer(indices[e]);
            }

            void expand();
            void insert_sol_object(Entity e, sol::object object);
            bool has_component(Entity e);
            sol::object get_component_as_lua_object(Entity e, sol::state& lua);
            void remove_component(Entity e);

            // maybe we'll need them, maybe we won't ¯\_(a)_/¯
            ComponentStorage(ComponentStorage&) = delete;
            ComponentStorage& operator=(ComponentStorage&) = delete;

            ComponentStorage(ComponentStorage&& other) noexcept :
                component_name(other.component_name), type(other.type)
            {
                if (this == &other) return;

                blob = other.blob;
                capacity = other.capacity;
                len = other.len;
                stride = other.stride;

                indices = std::move(other.indices);
                entities = std::move(other.entities);

                dtor = other.dtor;
                move_from_ptr = other.move_from_ptr;
                ctor_from_sol_object = other.ctor_from_sol_object;
                get_sol_object = other.get_sol_object;

                other.blob = nullptr;
            };
            ComponentStorage& operator=(ComponentStorage&&) = delete;

            ~ComponentStorage();
    };

    // an aggregate bump allocator
    class Ocean {
        struct Pool {
            size_t capacity;
            void* ptr;
            void* ending_pointer;

            Pool(size_t capacity) : 
                capacity(capacity), 
                ptr(malloc(capacity)),
                ending_pointer((void*)((size_t)ptr + capacity))
            {}
        };

        // one MiB
        static const size_t MIB = 1 << 20;

        std::vector<Pool> pools;
        size_t current_pool = 0;

        public:
            template <typename T>
            struct Allocator {
                Ocean& ocean;

                using value_type = T;

                Allocator(Ocean& ocean) : ocean(ocean) {}

                // TODO: alignment
                T* allocate(std::size_t n) {
                    size_t bytes_needed = sizeof(T) * n;

                    // lets find bytes_needed
                    while (ocean.current_pool < ocean.pools.size()) {
                        Pool& pool = ocean.pools[ocean.current_pool];
                        size_t bytes_available = (size_t)pool.ending_pointer - (size_t)pool.ptr;

                        // align = 4
                        // ptr = 0 => (4 - (0 % 4)) % 4 = (4 - 0) % 4 = 4 % 4 = 0
                        // ptr = 1 => (4 - (1 % 4)) % 4 = (4 - 1) % 4 = 3 % 4 = 3
                        // ptr = 2 => (4 - (2 % 4)) % 4 = (4 - 2) % 4 = 2 % 4 = 2
                        // ptr = 3 => (4 - (3 % 4)) % 4 = (4 - 3) % 4 = 1 % 4 = 1
                        size_t padding_needed = (alignof(T) - ((size_t)pool.ptr % alignof(T))) % alignof(T);

                        // not enough memory! check the next pool!
                        if (bytes_available < bytes_needed + padding_needed) {
                            ocean.current_pool++;
                            continue;
                        } else {
                            void* ptr = (void*)((size_t)pool.ptr + padding_needed);
                            pool.ptr = (void*)((size_t)ptr + bytes_needed); 
                            return (T*)ptr;
                        }
                    }
                    
                    // we're out of memory! get more!
                    // allocate as much memory as needed, to the nearest MiB
                    ocean.pools.emplace_back(((bytes_needed / MIB) + 1) * MIB);

                    void* ptr = ocean.pools.back().ptr;
                    ocean.pools.back().ptr = (void*)((size_t)ptr + bytes_needed);

                    return (T*)ptr;
                }

                void deallocate(T* p, std::size_t n) noexcept {
                    /* no-op ;) */
                }
            };

            void reset() {
                current_pool = 0;

                // combine the pools together if we have many
                if (pools.size() > 1) {
                    size_t bytes = 0;
                    for (Pool& pool : pools) {
                        bytes += pool.capacity;
                        free(pool.ptr);
                    }

                    pools.clear();
                    pools.emplace_back(bytes);
                } else if (pools.size() == 1) { // otherwise just reset the first one
                    Pool& pool = pools.front();
                    pool.ptr = (void*)((size_t)pool.ending_pointer - pool.capacity);
                }

            }

            ~Ocean() {
                for (Pool& pool : pools) {
                    pool.ptr = (void*)((size_t)pool.ending_pointer - pool.capacity);
                    free(pool.ptr);
                }
            }
    };

    class ECSWorld {
        template <typename ...T>
        friend class Query;

        // usage: lua_storage[component_name][entity] = component
        std::unordered_map<std::type_index, ComponentStorage> native_storage;
        std::unordered_map<std::string, std::type_index> component_type_indices;

        Entity next_entity = 0;

        class CommandQueue {
            std::mutex mutex;
            std::deque<Command> queue;

            public:
                void push_command(Command command) {
                    std::lock_guard guard { mutex };
                    queue.push_back(command);
                }

                Command pop_command() {
                    std::lock_guard guard { mutex };
                    Command ret = queue.front();
                    queue.pop_front();
                    return ret;
                }

                bool has_command() {
                    std::lock_guard guard { mutex };
                    return !queue.empty();
                }
        };


        public:
            CommandQueue command_queue;
            sol::table lua_storage;
            static Ocean ocean;

            Entity new_entity() {
                return next_entity++;
            }

            template <typename T>
            void register_component() {
                static_assert(ComponentTypeTrait<T>::value);
                std::type_index type_idx = typeid(T);

                if (!native_storage.contains(type_idx)) {
                    const std::string_view sv = ComponentTypeTrait<T>::component_name;
                    std::string key = { sv.begin(), sv.end() };
                    if (component_type_indices.contains(key)) {
                        SPDLOG_ERROR("multiple components sharing names! aborting!");
                        std::abort();
                    }
                    native_storage.emplace(type_idx, ComponentStorage::create<T>(100));
                    component_type_indices.emplace(key, type_idx);
                }
            }

            template <typename T, typename ...Args>
            void emplace_native_component(Entity e, Args ...args) {
                ECSWorld* self = this;
                command_queue.push_command([=]() {
                    if (!self->native_storage.contains(typeid(T))) {
                        self->register_component<T>();
                    }

                    self->native_storage.at(typeid(T)).emplace_component<T>(e, args...);
                });
            }

            void insert_native_component_from_lua(Entity e, std::string_view component_name, sol::object object) {
                std::string key = { component_name.begin(), component_name.end() };
                if (!component_type_indices.contains(key)) {
                    SPDLOG_ERROR("attempt to insert non-existent component from lua");
                    throw std::runtime_error("attempt to insert non-existent component from lua");
                }

                ECSWorld* self = this;
                command_queue.push_command([=]() {
                    self->native_storage.at(self->component_type_indices.at(key)).insert_sol_object(e, object);
                });
            }

            bool native_component_exists(std::string component_name) {
                if (!component_type_indices.contains(component_name)) {
                    return false;
                }

                return true;
            }

            template <typename T>
            bool entity_has_native_component(Entity e) {
                if (!native_storage.contains(typeid(T))) {
                    return false;
                }

                return native_storage.at(typeid(T)).has_component(e);
            }

            bool entity_has_native_component(Entity e, std::string component_name) {
                if (!component_type_indices.contains(component_name)) {
                    return false;
                }

                return native_storage.at(component_type_indices.at(component_name)).has_component(e);
            }

            template <typename T>
            std::optional<T*> get_native_component(Entity e) {
                if (!native_storage.contains(typeid(T))) {
                    return {};
                }

                return native_storage.at(typeid(T)).get_component<T>(e);
            }

            sol::object get_native_component_as_lua_object(Entity e, std::string_view component_name, sol::state& lua) {
                std::string key = { component_name.begin(), component_name.end() };
                if (!component_type_indices.contains(key)) {
                    return lua_storage[component_name];
                }

                return native_storage.at(component_type_indices.at(key)).get_component_as_lua_object(e, lua);
            }

            template <typename ...Components>
            auto query() {
                return Query<Components...>::it(*this);
            }

            const std::vector<Entity>& get_entities_from_native_component_name(std::string component_name) {
                return 
                    native_storage.at(
                        component_type_indices.at(component_name)
                    ).entities;
            }

            template <typename T>
            void remove_native_component_from_entity(Entity e) {
                ECSWorld* self = this;
                command_queue.push_command([=]() {
                    if (self->native_storage.contains(typeid(T))) {
                        self->native_storage.at(typeid(T)).remove_component(e);
                    }
                });
            }

            void remove_native_component_from_entity(Entity e, std::string_view component_name) {
                std::string key = { component_name.begin(), component_name.end() };
                ECSWorld* self = this;
                command_queue.push_command([=]() {
                    if (self->component_type_indices.contains(key)) {
                        self->native_storage.at(self->component_type_indices.at(key)).remove_component(e);
                    }
                });
            }

            void delete_entity(Entity e) {
                ECSWorld* self = this;
                command_queue.push_command([=]() {
                    for (auto& [_, s] : self->native_storage) {
                        s.remove_component(e);
                    }

                    self->lua_storage.for_each([&](sol::object, sol::object components) {
                        if (components.is<sol::table>()) {
                            components.as<sol::table>()[e] = sol::nil;
                        } else {
                            SPDLOG_WARN("non-table found in lua_storage. this is an engine bug.");
                        }
                    });
                });
            }

            void flush_command_queue() {
                while (command_queue.has_command()) {
                    Command command = command_queue.pop_command();
                    if (!command) {
                        SPDLOG_ERROR("command_queue has a null!");
                    }
                    command();
                }
            }

            void fire_event(std::string event_name, sol::object event_payload);
    };


    template <typename ...Components>
    class Query {
        template <typename First, typename ...Other>
        static auto internal(ECSWorld& world) {
            if constexpr (sizeof...(Other) > 0) {
                if constexpr (std::is_same_v<First, Entity>) {
                    return internal<Other...>(world) |
                        std::views::transform([](auto p) {
                            return std::make_pair(
                                std::get<0>(p),
                                std::tuple_cat(std::make_tuple(std::get<0>(p)), std::get<1>(p))
                            );
                        });
                } else {
                    ComponentStorage& cs = world.native_storage.at(typeid(First));

                    return internal<Other...>(world) |
                        std::views::filter([&](auto& p) {
                            return cs.has_component(std::get<0>(p));
                        }) |
                        std::views::transform([&](auto p) {
                            return std::make_pair(
                                std::get<0>(p),
                                std::tuple_cat(std::make_tuple(cs.get_component<First>(std::get<0>(p)).value()), std::get<1>(p))
                            );
                        });
                }
            } else {
                // for right now, lets not deal with this annoyance.
                // the convention is to have Entity *first*.
                static_assert(!std::is_same_v<Entity, First>);

                using Pair = std::pair<Entity, std::tuple<First*>>;
                // using V = std::vector<Pair, Ocean::Allocator<Pair>>;

                ComponentStorage& cs = world.native_storage.at(typeid(First));
                // V v(Ocean::Allocator(world.ocean));
                Pair* pairs = Ocean::Allocator<Pair>(world.ocean).allocate(cs.len);

                // v.reserve(cs.len);
                for (size_t idx = 0; idx < cs.len; idx++) {
                    pairs[idx] = std::make_pair(cs.entities[idx], std::make_tuple((First*)cs.compute_pointer(idx)));
                    // v.push_back(std::make_pair(cs.entities[idx], std::make_tuple((First*)cs.compute_pointer(idx))));
                }

                return std::ranges::subrange(pairs, pairs + cs.len);
                // ECSWorld::static_storage.emplace_back(std::move(v));
                // return std::any_cast<V&>(ECSWorld::static_storage.back());
            }
        }

        public:
            static auto it(ECSWorld& world) {
                return std::views::transform(internal<Components...>(world), [](auto p) { return std::get<1>(p); });
            }
    };
}
#undef MOTORCAR_EAT_EXCEPTION

