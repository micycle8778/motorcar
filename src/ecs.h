#include <any>
#include <cstddef>
#include <cstdlib>
#include <optional>
#include <tuple>
#include <typeindex>
#include <ranges>

#include <sol/sol.hpp>
#include <utility>
#include "spdlog/spdlog.h"

#include "types.h"

#define MOTORCAR_EAT_EXCEPTION(code, msg) try { code; } catch (const std::exception& e) { SPDLOG_ERROR(msg, " what(): {}", e.what()); } catch (...) { SPDLOG_ERROR(msg); }
namespace motorcar {
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

            // maybe we'll need them, maybe we won't ¯\_(ツ)_/¯
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

    class ECSWorld {
        static std::vector<std::any> static_storage;

        template <typename ...T>
        friend class Query;

        std::unordered_map<std::type_index, ComponentStorage> storage;
        std::unordered_map<std::string, std::type_index> component_type_indices;

        Entity next_entity = 0;

        // TODO: systems

        public:
            Entity new_entity() {
                return next_entity++;
            }

            template <typename T>
            void register_component() {
                static_assert(ComponentTypeTrait<T>::value);
                std::type_index type_idx = typeid(T);

                if (!storage.contains(type_idx)) {
                    const std::string_view sv = ComponentTypeTrait<T>::component_name;
                    std::string key = { sv.begin(), sv.end() };
                    if (component_type_indices.contains(key)) {
                        SPDLOG_ERROR("multiple components sharing names! aborting!");
                        std::abort();
                    }
                    storage.emplace(type_idx, ComponentStorage::create<T>(100));
                    component_type_indices.emplace(key, type_idx);
                }
            }

            template <typename T, typename ...Args>
            void emplace_component(Entity e, Args ...args) {
                if (!storage.contains(typeid(T))) {
                    register_component<T>();
                }

                storage.at(typeid(T)).emplace_component<T>(e, args...);
            }

            // template <typename Component>
            // void insert_component(Entity e, Component&& c) {
            //     using RealType = std::remove_reference_t<Component>;
            //
            //     if (!storage.contains(typeid(RealType))) {
            //         register_component<RealType>();
            //     }
            //
            //     SPDLOG_TRACE("insert_component");
            //     storage.at(typeid(RealType)).emplace_component<RealType>(e, c);
            // }

            void insert_lua_component(Entity e, std::string_view component_name, sol::object object) {
                std::string key = { component_name.begin(), component_name.end() };
                if (!component_type_indices.contains(key)) {
                    SPDLOG_ERROR("attempt to insert non-existent lua component");
                    std::abort();
                }

                storage.at(component_type_indices.at(key)).insert_sol_object(e, object);
            }

            template <typename T>
            bool has_component(Entity e) {
                if (!storage.contains(typeid(T))) {
                    return false;
                }

                return storage.at(typeid(T)).has_component(e);
            }

            template <typename T>
            std::optional<T*> get_component(Entity e) {
                if (!storage.contains(typeid(T))) {
                    return {};
                }

                return storage.at(typeid(T)).get_component<T>(e);
            }

            sol::object get_component_as_lua_object(Entity e, std::string_view component_name, sol::state& lua) {
                std::string key = { component_name.begin(), component_name.end() };
                if (!component_type_indices.contains(key)) {
                    return {};
                }

                return storage.at(component_type_indices.at(key)).get_component_as_lua_object(e, lua);
            }

            template <typename T>
            void delete_component(Entity e) {
                if (storage.contains(typeid(T))) {
                    storage.at(typeid(T)).remove_component(e);
                }
            }

            void delete_component(Entity e, std::string_view component_name) {
                std::string key = { component_name.begin(), component_name.end() };
                if (component_type_indices.contains(key)) {
                    storage.at(component_type_indices.at(key)).remove_component(e);
                }
            }

            void delete_entity(Entity e) {
                for (auto& [_, s] : storage) {
                    s.remove_component(e);
                }
            }
    };


    template <typename ...Components>
    class Query {
        template <typename First, typename ...Other>
        static auto internal(ECSWorld& world) {
            if constexpr (sizeof...(Other) > 0) {
                if constexpr (std::is_same_v<First, Entity>) {
                    return internal<Other...>(world) |
                        std::views::transform([](auto& p) {
                            return std::make_pair(
                                std::get<0>(p),
                                std::tuple_cat(std::make_tuple(std::get<0>(p)), std::get<1>(p))
                            );
                        });
                } else {
                    ComponentStorage& cs = world.storage.at(typeid(First));

                    return internal<Other...>(world) |
                        std::views::filter([&](auto& p) {
                            return cs.has_component(std::get<0>(p));
                        }) |
                        std::views::transform([&](auto& p) {
                            return std::make_pair(
                                std::get<0>(p),
                                std::tuple_cat(std::make_tuple(cs.get_component<First>(std::get<0>(p)).value()), std::get<1>(p))
                            );
                        });
                }
            } else {
                using V = std::vector<std::pair<Entity, std::tuple<First*>>>;
                ComponentStorage& cs = world.storage.at(typeid(First));
                V v;
                v.reserve(cs.len);
                for (int idx = 0; idx < cs.len; idx++) {
                    v.push_back(std::make_pair(cs.entities[idx], std::make_tuple((First*)cs.compute_pointer(idx))));
                }

                ECSWorld::static_storage.emplace_back(std::move(v));
                return std::any_cast<V&>(ECSWorld::static_storage.back());
            }
        }

        public:
            static auto it(ECSWorld& world) {
                return std::views::transform(internal<Components...>(world), [](auto p) { return std::get<1>(p); });
            }
    };
}
#undef MOTORCAR_EAT_EXCEPTION

