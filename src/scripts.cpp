#include "components.h"
#include <functional>
#include <ranges>
#include <sol/as_args.hpp>
#include <sol/forward.hpp>
#include <sol/protected_function_result.hpp>
#include <sol/raii.hpp>
#include <stdexcept>
#include <unordered_set>
#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include <spdlog/spdlog.h>

#include "scripts.h"
#include "resources.h"
#include "engine.h"
#include "input.h"
#include "sound.h"
#include "ecs.h"

using namespace motorcar;
using Tables = std::vector<sol::table>;

#define TOKCAT2(t1, t2) t1 ## t2
#define TOKCAT(t1, t2) TOKCAT2(t1, t2)
#define PCALL(call) \
    sol::protected_function_result TOKCAT(result, __LINE__) = call; \
    if (!TOKCAT(result, __LINE__).valid()) { \
        sol::error error = TOKCAT(result, __LINE__); \
        spdlog::error("Caught lua error: {}", error.what()); \
    } \
    

namespace {
    struct Event {
        std::string name;
        Event(std::string name) : name(name) {}
    };

    struct CallInfo {
        const char* filename;
        int lineno;
    }; 

    std::optional<CallInfo> get_debug_info(sol::state& lua) {
        lua_Debug ld;
        if (!lua_getstack(lua.lua_state(), 1, &ld)) return {};
        if (!lua_getinfo(lua.lua_state(), "Sl", &ld)) return {};

        return CallInfo { .filename = ld.source, .lineno = ld.currentline };
    }

    class ScriptLoader : public ILoadResources {
        sol::state& lua;

        public:
            ScriptLoader(sol::state& lua) : lua(lua) {}

            virtual std::optional<Resource> load_resource(const std::filesystem::path&, std::ifstream& file_stream, std::string_view resource_path) {
                if (!resource_path.ends_with(".lua")) {
                    SPDLOG_TRACE("Skipping loading script {}. Doesn't end with '.lua'", resource_path);
                }
                
                file_stream.seekg(0);

                std::string lua_code{ std::istreambuf_iterator<char>(file_stream), std::istreambuf_iterator<char>() };
                // im not sure why sol::state::load requires a reference to a const string for the chunk name
                // it also requires a reference to a string view for the code for whatever reason
                std::string owned_resource_path{ resource_path.data(), resource_path.size() };
                
                sol::load_result result = lua.load(lua_code, owned_resource_path);

                if (result.valid()) {
                    return sol::protected_function(result);
                } else {
                    sol::error error = result;
                    SPDLOG_TRACE("Failed to load script {}. what(): ", resource_path, error.what());
                    return {};
                }
            };
    };

    Tables query(sol::state& lua, ECSWorld& ecs, sol::table query) {
        Tables ret;

        if (!query.valid()) {
            throw std::runtime_error("query not specified.");
        }

        if (query.size() == 0) {
            ret.push_back(sol::table(lua, sol::create));
            return ret;
        }

#define STRCMP(object, s) (object.is<std::string>() && object.get<std::string>() == s)
        // are entity only queries desirable?
        if (query.size() == 1 && STRCMP(query[1],std::string("entity"))) {
            // TODO: is throwing bad here?
            throw std::runtime_error("component list of just 'entity' not supported.");
        }
#undef STRCMP

        std::vector<std::string> component_names;
        for (size_t idx = 1; idx <= query.size(); idx++) {
            component_names.push_back(query[idx].get<std::string>());
        }

        auto view = component_names | std::views::filter([](auto s) { return s != "entity"; });
        auto first_component_name = *view.begin();
        std::vector<Entity> starting_entites;

        if (ecs.native_component_exists(first_component_name)) {
            starting_entites = ecs.get_entities_from_native_component_name(first_component_name);
        } else if (ecs.lua_storage[first_component_name].valid()) {
            ecs.lua_storage[first_component_name].get<sol::table>().for_each([&](sol::object e, sol::object) {
                starting_entites.push_back(e.as<Entity>());
            });
        } else {
            SPDLOG_WARN("requested component {} doesn't exist in ECS.", first_component_name);
            return ret;
        }
        
        std::unordered_set<Entity> entities(starting_entites.begin(), starting_entites.end());

        std::vector<Entity> to_remove;
        for (auto component_name : component_names | std::views::drop(1)) {
            if (component_name == "entity") continue;
            for (Entity e : entities) {
                bool is_native_component = ecs.native_component_exists(component_name);
                bool is_lua_component = ecs.lua_storage[component_name].valid();

                // if the component doesn't exist in the ecs, give up
                if (!is_native_component && !is_lua_component) {
                    to_remove.push_back(e);
                // if the component does exist in the native storage, but this entity doesn't have it, filter
                } else if (is_native_component && !ecs.entity_has_native_component(e, component_name)) {
                    to_remove.push_back(e);
                // ditto for lua
                } else if (is_lua_component && !ecs.lua_storage[component_name][e].valid()) {
                    to_remove.push_back(e);
                }
            }

            for (Entity e : to_remove) {
                entities.erase(e);
            }

            to_remove.clear();
        }

        for (Entity e : entities) {
            sol::table argument = sol::table(lua, sol::new_table());
            for (auto component : component_names) {
                bool is_native_component = ecs.native_component_exists(component);

                if (component == "entity") {
                    argument[component] = e;
                } else {
                    if (is_native_component) {
                        argument[component] = ecs.get_native_component_as_lua_object(e, component, lua);
                    } else {
                        argument[component] = ecs.lua_storage[component][e];
                    }
                }
            }
            ret.push_back(argument);
        }
        return ret;
    }

    using I = std::vector<Tables>::iterator;
    template <typename Func>
    void f(
        I begin, I end, 
        Tables args, 
        Func callback
    ) {
        if (begin == end) {
            callback(args);
            return;
        }

        for (sol::table arg : *begin) {
            // NOTE: this is quite slow, O(n*m), where n is the number
            // of queries the user is doing and m is the number of
            // entities we've queried. This is a good spot to
            // use the engine's ocean allocator, and/or a cons list.
            Tables new_args;
            new_args.reserve(args.size() + 1);
            new_args.assign(args.begin(), args.end());
            new_args.push_back(arg);

            f(begin + 1, end, new_args, callback);
        }
    }
}

ScriptManager::ScriptManager(Engine& engine) : engine(engine) {
    lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::table);
    engine.ecs->lua_storage = sol::table(lua, sol::new_table());

    sol::table input_namespace = lua["Input"].force();
    #define INPUT_METHOD(name) input_namespace.set_function(#name, [&](std::string key_name) { return engine.input->name(Key::key_from_string(key_name)); })
    INPUT_METHOD(is_key_held_down);
    INPUT_METHOD(is_key_pressed_this_frame);
    INPUT_METHOD(is_key_repeated_this_frame);
    INPUT_METHOD(is_key_released_this_frame);
    #undef INPUT_METHOD


    sol::table log_namespace = lua["Log"].force();
    #define LOG_FN(log_level) \
    log_namespace.set_function(#log_level, [&](std::string message) {\
        auto call_info = get_debug_info(lua);\
        spdlog::log_level("[{}:{}] {}", call_info->filename, call_info->lineno, message);\
    })
    LOG_FN(trace);
    LOG_FN(debug);
    LOG_FN(warn);
    LOG_FN(error);
    #undef LOG_FN


    sol::table sound_namespace = lua["Sound"].force();
    sound_namespace.set_function("play_sound", [&](std::string sound_name) {
        engine.sound->play_sound(sound_name);
    });


    sol::table resources_namespace = lua["Resources"].force();
    resources_namespace.set_function("load_resource", [&](std::string resource_path) {
        engine.resources->load_resource(resource_path);
    });


    sol::table engine_namespace = lua["Engine"].force();
    engine_namespace.set_function("quit", [&]() {
        engine.keep_running = false;
    });


    sol::table ecs_namespace = lua["ECS"].force();
    ecs_namespace.set_function("new_entity", [&]() {
        return engine.ecs->new_entity();
    });
    ecs_namespace.set_function("delete_entity", [&](Entity e) {
        engine.ecs->delete_entity(e);
    });
    ecs_namespace.set_function("get_component", [&](Entity e, std::string component) {
        return engine.ecs->get_native_component_as_lua_object(e, component, lua);
    });
    ecs_namespace.set_function("remove_component_from_entity", [&](Entity e, std::string component) {
        bool is_native_component = engine.ecs->native_component_exists(component);
        bool is_lua_component = engine.ecs->lua_storage[component].valid();

        if (is_native_component) {
            engine.ecs->remove_native_component_from_entity(e, component);
        } else if (is_lua_component) {
            engine.ecs->command_queue.push_back([=](ECSWorld* ecs) {
                ecs->lua_storage[component][e] = sol::nil;
            });
        }
    });
    ecs_namespace.set_function("register_component", [&](Entity e, std::string component) {
        if (engine.ecs->native_component_exists(component)) {
            throw std::runtime_error(std::format("trying to register native component {}", component));
        }

        if (!engine.ecs->lua_storage[component].valid()) {
            engine.ecs->lua_storage[component] = sol::table(lua, sol::new_table());
        }
    });
    ecs_namespace.set_function("insert_component", [&](Entity e, std::string component_name, sol::object component) {
        if (engine.ecs->native_component_exists(component_name)) {
            engine.ecs->insert_native_component_from_lua(e, component_name, component);
            return;
        }

        if (!engine.ecs->lua_storage[component_name].valid()) {
            SPDLOG_DEBUG("Registering new lua component {}.", component_name);
            engine.ecs->lua_storage[component_name] = sol::table(lua, sol::new_table());
        }

        if (component == sol::nil) {
            SPDLOG_TRACE("component == sol::nil");
        }

        engine.ecs->lua_storage[component_name][e] = component;
    });
    ecs_namespace.set_function("for_each", [&](sol::table components, sol::protected_function callback) {
        if (!callback.valid()) {
            throw std::runtime_error("callback not specified.");
        }

        for (sol::table argument : query(lua, *engine.ecs, components)) {
            PCALL(callback(argument));
        }
    });
    ecs_namespace.set_function("register_system", [&](sol::table queries, sol::protected_function callback, sol::object lifecycle) {
        if (!callback.valid()) {
            throw std::runtime_error("callback not specified.");
        }
        if (!queries.valid()) {
            throw std::runtime_error("queries not specified.");
        }

        Entity e = engine.ecs->new_entity();

#define STRCMP(object, s) (object.is<std::string>() && object.as<std::string>() == s)
        if (!lifecycle.valid() || STRCMP(lifecycle, "render")) {
            engine.ecs->emplace_native_component<RenderSystem>(e);
        } else if (STRCMP(lifecycle, "physics")) {
            engine.ecs->emplace_native_component<PhysicsSystem>(e);
        } else if (lifecycle.is<Event>()) {
            // handle this a little later
        } else {
            throw std::runtime_error("lifecycle is not valid. needs to be 'render', 'physics', Event.new('event-name'), or unspecified.");
        }
#undef STRCMP

        int queries_length = queries.size();
        if (queries.empty()) {
            if (lifecycle.is<Event>()) {
                engine.ecs->emplace_native_component<EventHandler>(e, [=](sol::object event_payload) {
                    callback(event_payload);
                }, lifecycle.as<Event>().name);
            } else {
                engine.ecs->emplace_native_component<System>(e, [=]() {
                    callback();
                }, 0); // TODO: priority
            }
        }

        bool is_strings = true;
        bool is_tables = true;
        for (int idx = 1; idx <= queries_length; idx++) {
            if (!queries[idx].is<std::string>()) {
                is_strings = false;
                break;
            }
        }
        for (int idx = 1; idx <= queries_length; idx++) {
            if (!queries[idx].is<sol::table>()) {
                is_tables = false;
                break;
            }
        }

        if (!is_strings && !is_tables) {
            throw std::runtime_error("queries should be an array of strings or a 2d array of strings.");
        } else if (is_strings) {
            sol::state* state = &lua;
            ECSWorld* ecs = &*engine.ecs;
            if (lifecycle.is<Event>()) {
                engine.ecs->emplace_native_component<EventHandler>(e, [=](sol::object event_payload) {
                    for (sol::table argument : query(*state, *ecs, queries)) {
                        callback(argument, event_payload);
                    }
                }, lifecycle.as<Event>().name);
            } else {
                engine.ecs->emplace_native_component<System>(e, [=]() {
                    for (sol::table argument : query(*state, *ecs, queries)) {
                        callback(argument);
                    }
                }, 0); // TODO: priority
            }
        } else { // is_tables
            sol::state* state = &lua;
            ECSWorld* ecs = &*engine.ecs;
            if (lifecycle.is<Event>()) {
                engine.ecs->emplace_native_component<EventHandler>(e, [=](sol::object event_payload) {
                    // arguments squared?
                    std::vector<Tables> arguments2;
                    for (int idx = 1; idx <= queries_length; idx++) {
                        arguments2.push_back(query(*state, *ecs, queries[idx]));
                    }
                    f(arguments2.begin(), arguments2.end(), Tables(), [&](Tables& args) {
                        callback(sol::as_args(args), event_payload);
                    });
                }, lifecycle.as<Event>().name);
            } else {
                engine.ecs->emplace_native_component<System>(e, [=]() {
                    // arguments squared?
                    std::vector<Tables> arguments2;
                    for (int idx = 1; idx <= queries_length; idx++) {
                        arguments2.push_back(query(*state, *ecs, queries[idx]));
                    }
                    f(arguments2.begin(), arguments2.end(), Tables(), [&](Tables& args) {
                        callback(sol::as_args(args));
                    }); 
                }, 0); // TODO: priority
            }
        }
    });
    ecs_namespace.set_function("fire_event", [&](std::string event_name, sol::object event_payload) {
        engine.ecs->fire_event(event_name, event_payload);
    });


    lua.new_usertype<Event>("Event", sol::constructors<Event(std::string)>());

    lua.new_usertype<vec2>("vec2",
        sol::constructors<vec2(), vec2(float), vec2(float, float)>(),
        "x", &vec2::x,
        "y", &vec2::y,
        // optional and fancy: operator overloading. see: https://github.com/ThePhD/sol2/issues/547
        sol::meta_function::addition, sol::overload( [](const vec2& v1, const vec2& v2) -> vec2 { return v1+v2; } ),
        sol::meta_function::subtraction, sol::overload( [](const vec2& v1, const vec2& v2) -> vec2 { return v1-v2; } ),
        sol::meta_function::multiplication, sol::overload(
            [](const vec2& v1, const vec2& v2) -> vec2 { return v1*v2; },
            [](const vec2& v1, float f) -> vec2 { return v1*f; },
            [](float f, const vec2& v1) -> vec2 { return f*v1; }
        )
    );

    lua.new_usertype<vec3>("vec3",
        sol::constructors<vec3(), vec3(float), vec3(float, float, float)>(),
        "x", &vec3::x,
        "y", &vec3::y,
        "z", &vec3::z,
        // optional and fancy: operator overloading. see: https://github.com/ThePhD/sol2/issues/547
        sol::meta_function::addition, sol::overload( [](const vec3& v1, const vec3& v2) -> vec3 { return v1+v2; } ),
        sol::meta_function::subtraction, sol::overload( [](const vec3& v1, const vec3& v2) -> vec3 { return v1-v2; } ),
        sol::meta_function::multiplication, sol::overload(
            [](const vec3& v1, const vec3& v2) -> vec3 { return v1*v2; },
            [](const vec3& v1, float f) -> vec3 { return v1*f; },
            [](float f, const vec3& v1) -> vec3 { return f*v1; }
        )
    );

    engine.resources->register_resource_loader(std::make_unique<ScriptLoader>(lua));
}

void ScriptManager::run_script(std::string_view path) {
    auto maybe_script = engine.resources->get_resource<sol::protected_function>(path);
    if (!maybe_script.has_value()) {
        spdlog::error("Could not load script {}.", path);
    } else {
        // run the script specified by the path
        sol::protected_function script = *maybe_script.value();
        PCALL(script());
    }
}
