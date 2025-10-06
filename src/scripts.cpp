#include <ranges>
#include <stdexcept>
#include <unordered_set>
#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include <spdlog/spdlog.h>

#include "scripts.h"
#include "resources.h"
#include "engine.h"
#include "input.h"
#include "sound.h"
#include "ecs.h"

using namespace motorcar;

namespace {
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
    engine_namespace["sprites"] = std::ref(engine.sprites);


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
    ecs_namespace.set_function("for_each", [&](sol::table components, sol::function callback) {
        if (components.size() == 0) {
            callback();
            return;
        }

        // TODO: is this desirable?
        if (components.size() == 1 && components[1] == "entity") {
            throw std::runtime_error("component list of just 'entity' not supported.");
        }

        std::vector<std::string> component_names;
        for (size_t idx = 1; idx <= components.size(); idx++) {
            component_names.push_back(components[idx].get<std::string>());
        }

        auto view = component_names | std::views::filter([](auto s) { return s != "entity"; });
        auto first_component_name = *view.begin();
        std::vector<Entity> starting_entites;

        if (engine.ecs->native_component_exists(first_component_name)) {
            starting_entites = engine.ecs->get_entities_from_native_component_name(first_component_name);
        } else if (engine.ecs->lua_storage[first_component_name].valid()) {
            engine.ecs->lua_storage[first_component_name].get<sol::table>().for_each([&](sol::object e, sol::object) {
                starting_entites.push_back(e.as<Entity>());
            });
        } else {
            SPDLOG_WARN("requested component {} doesn't exist in ECS.", first_component_name);
            return;
        }
        
        std::unordered_set<Entity> entities(starting_entites.begin(), starting_entites.end());

        std::vector<Entity> to_remove;
        for (auto component_name : component_names | std::views::drop(1)) {
            if (component_name == "entity") continue;
            for (Entity e : entities) {
                bool is_native_component = engine.ecs->native_component_exists(component_name);
                bool is_lua_component = engine.ecs->lua_storage[component_name].valid();

                // if the component doesn't exist in the ecs, give up
                if (!is_native_component && !is_lua_component) {
                    to_remove.push_back(e);
                // if the component does exist in the native storage, but this entity doesn't have it, filter
                } else if (is_native_component && !engine.ecs->entity_has_native_component(e, component_name)) {
                    to_remove.push_back(e);
                // ditto for lua
                } else if (is_lua_component && !engine.ecs->lua_storage[component_name][e].valid()) {
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
                bool is_native_component = engine.ecs->native_component_exists(component);

                if (component == "entity") {
                    argument[component] = e;
                } else {
                    if (is_native_component) {
                        argument[component] = engine.ecs->get_native_component_as_lua_object(e, component, lua);
                    } else {
                        argument[component] = engine.ecs->lua_storage[component][e];
                    }
                }
            }
            callback(argument);
        }
    });

    lua.new_usertype<Sprite>("Sprite",
        sol::constructors<Sprite(vec2, vec2, f32, std::string)>(),
        "position", &Sprite::position,
        "scale", &Sprite::scale,
        "depth", &Sprite::depth,
        "texture_path", &Sprite::texture_path
    );

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
        auto script = *maybe_script.value();
        sol::protected_function_result result = script(); 

        if (!result.valid()) {
            sol::error error = result;
            spdlog::error("Caught lua error: {}", error.what());
        }
    }
}
