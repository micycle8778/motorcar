#include <filesystem>
#include <unordered_set>

#include "GLFW/glfw3.h"
#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>
#include <spdlog/spdlog.h>

#include "scripts.h"
#include "resources.h"
#include "engine.h"
#include "input.h"
#include "sound.h"
#include "ecs.h"
#include "components.h"

using namespace motorcar;
using Tables = std::vector<sol::table>;

#define TOKCAT2(t1, t2) t1 ## t2
#define TOKCAT(t1, t2) TOKCAT2(t1, t2)

namespace {
    struct Event {
        std::string name;
        Event(std::string name) : name(name) {}
    };

    struct CallInfo {
        const char* filename;
        int lineno;
    }; 

    template <typename ...Args>
    bool pcall(sol::protected_function f, Args&& ...args) {
        sol::protected_function_result result = f(std::forward<Args>(args)...);
        if (!result.valid()) {
            sol::error error = result;
            spdlog::error("Caught lua error: {}", error.what());
        }
        return result.valid();
    }

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

    void load_and_execute_script(Engine& engine, const std::filesystem::path& file_path, bool watch = true) {
        ScriptManager& script_manager = *engine.scripts;
        std::ifstream file_stream { file_path };
        std::string script_name = file_path.lexically_relative(std::filesystem::current_path()).string();
        if (file_stream.fail()) {
            SPDLOG_ERROR("failed opening file backing {}.", file_path.string());
            return;
        }

        std::string code { std::istreambuf_iterator<char>(file_stream), std::istreambuf_iterator<char>() };
        sol::load_result load_result = script_manager.lua.load(code, script_name);

        if (!load_result.valid()) {
            sol::error err = load_result;
            SPDLOG_ERROR("failed loading script {}: err.what(): {}", file_path.string(), err.what());
            return;
        }

        sol::protected_function script = load_result;

        for (auto [e, bound_to_script] : engine.ecs->query<Entity, BoundToScript>()) {
            if (bound_to_script->script_name == script_name) {
                engine.ecs->delete_entity(e);
            }
        }

        if (pcall(script)) {
            if (watch) {
                Engine* _engine = &engine;
                ResourceManager::watch_file(file_path, 
                    [=]() { 
                        _engine->ecs->command_queue.push_command([=]() {
                            load_and_execute_script(*_engine, file_path, false); 
                        });
                    }
                );
            }
        }
    }
}

ScriptManager::ScriptManager(Engine& engine) : engine(engine) {
    lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::table, sol::lib::string);
    engine.ecs->lua_storage = sol::table(lua, sol::new_table());

    sol::table input_namespace = lua["Input"].force();
    #define INPUT_METHOD(name) input_namespace.set_function(#name, [&](std::string key_name) { return engine.input->name(Key::key_from_string(key_name)); })
    INPUT_METHOD(is_key_held_down);
    INPUT_METHOD(is_key_pressed_this_frame);
    INPUT_METHOD(is_key_repeated_this_frame);
    INPUT_METHOD(is_key_released_this_frame);
    #undef INPUT_METHOD
    input_namespace.set_function("get_mouse_position", [&]() { return engine.input->get_mouse_position(); });
    input_namespace.set_function("get_mouse_motion_this_frame", [&]() { return engine.input->get_mouse_motion_this_frame(); });
    input_namespace.set_function("lock_mouse", [&]() { engine.input->lock_mouse(); });
    input_namespace.set_function("unlock_mouse", [&]() { engine.input->unlock_mouse(); });


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


    sol::table stages_namespace = lua["Stages"].force();
    stages_namespace.set_function("change_to", [&](std::string stage_name, sol::object props) {
        engine.next_stage = stage_name;

        sol::state* _lua = &lua;
        engine.ecs->command_queue.push_command([=]() {
            (*_lua)["Stages"]["props"] = props;
        });
    });

    sol::table sound_namespace = lua["Sound"].force();
    sound_namespace.set_function("play_sound", [&](std::string sound_name) {
        engine.sound->play_sound(sound_name);
    });
    sound_namespace.set_function("play_music", [&](std::string sound_name) {
        engine.sound->play_music(sound_name);
    });


    sol::table resources_namespace = lua["Resources"].force();
    resources_namespace.set_function("load_resource", [&](std::string resource_path) {
        engine.resources->load_resource(resource_path);
    });


    sol::table engine_namespace = lua["Engine"].force();
    engine_namespace.set_function("quit", [&]() {
        engine.keep_running = false;
    });
    engine_namespace.set_function("clock", []() {
        return glfwGetTime();
    });
    engine_namespace.set_function("delta", [&]() {
        return engine.delta;
    });


    sol::table ecs_namespace = lua["ECS"].force();
    ecs_namespace.set_function("new_entity", [&]() {
        Entity ret = engine.ecs->new_entity();
        if (engine.stage.has_value())
            engine.ecs->emplace_native_component<BoundToStage>(ret, engine.stage.value());
        return ret;
    });
    ecs_namespace.set_function("delete_entity", [&](Entity e) {
        engine.ecs->delete_entity(e);
    });
    ecs_namespace.set_function("get_component", [&](Entity e, std::string component) {
        return engine.ecs->get_native_component_as_lua_object(e, component, lua);
    });
    ecs_namespace.set_function("get_ecs", [&]() {
            return engine.ecs->lua_storage;
    });
    ecs_namespace.set_function("remove_component_from_entity", [&](Entity e, std::string component) {
        bool is_native_component = engine.ecs->native_component_exists(component);
        bool is_lua_component = engine.ecs->lua_storage[component].valid();

        if (is_native_component) {
            engine.ecs->remove_native_component_from_entity(e, component);
        } else if (is_lua_component) {
            ECSWorld* ecs = engine.ecs.get();
            engine.ecs->command_queue.push_command([=]() {
                ecs->lua_storage[component][e] = sol::nil;
            });
        }
    });
    ecs_namespace.set_function("register_component", [&](std::string component) {
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
            pcall(callback, argument);
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
        if (engine.stage.has_value()) {
            engine.ecs->emplace_native_component<BoundToStage>(e, engine.stage.value());
        }

        auto call_info = get_debug_info(lua).value();
        engine.ecs->emplace_native_component<BoundToScript>(e, call_info.filename);

#define STRCMP(object, s) (object.is<std::string>() && object.as<std::string>() == s)
        if (!lifecycle.valid() || STRCMP(lifecycle, "physics")) {
            engine.ecs->emplace_native_component<PhysicsSystem>(e);
        } else if (STRCMP(lifecycle, "render")) {
            engine.ecs->emplace_native_component<RenderSystem>(e);
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
                    pcall(callback, event_payload);
                }, lifecycle.as<Event>().name);
            } else {
                engine.ecs->emplace_native_component<System>(e, [=]() {
                    pcall(callback);
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
                        pcall(callback, argument, event_payload);
                    }
                }, lifecycle.as<Event>().name);
            } else {
                engine.ecs->emplace_native_component<System>(e, [=]() {
                    for (sol::table argument : query(*state, *ecs, queries)) {
                        pcall(callback, argument);
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
                        pcall(callback, sol::as_args(args), event_payload);
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
                        pcall(callback, sol::as_args(args));
                    }); 
                }, 0); // TODO: priority
            }
        }
    });
    ecs_namespace.set_function("fire_event", [&](std::string event_name, sol::object event_payload) {
        engine.ecs->fire_event(event_name, event_payload);
    });


    // seed once
    static bool _random_seeded = ([](){
        std::srand(static_cast<unsigned>(std::time(nullptr)));
        return true;
    })();

    sol::table random_namespace = lua["Random"].force();
    random_namespace.set_function("randi", []() -> int {
        return std::rand();
    });
    random_namespace.set_function("randf", []() -> f32 {
        return static_cast<f32>(std::rand()) / static_cast<f32>(RAND_MAX);
    });
    random_namespace.set_function("randi_range", [](int a, int b) -> int {
        if (a > b) std::swap(a, b);
        // inclusive range [a, b]
        if (a == b) return a;
        int range = b - a + 1;
        return a + (std::rand() % range);
    });
    random_namespace.set_function("randf_range", [](f32 a, f32 b) -> f32 {
        if (a > b) std::swap(a, b);
        if (a == b) return a;
        f32 t = static_cast<f32>(std::rand()) / static_cast<f32>(RAND_MAX);
        return a + t * (b - a);
    });
    random_namespace.set_function("pick_random", [](sol::table t) -> sol::object {
        auto n = t.size();
        if (n == 0) return sol::nil;
        size_t idx = 1 + (std::rand() % n); // Lua is 1-based
        return t[idx];
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

    auto v3 = lua.new_usertype<vec3>("vec3",
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
    v3.set_function("normalized", [](vec3 v) { return glm::normalize(v); });
    v3.set_function("distance_to", [](vec3 v1, vec3 v2) { return glm::distance(v1, v2); });

    auto q = lua.new_usertype<quat>("quat", sol::constructors<quat(), quat(vec3, vec3)>());
    q.set_function("axis", [](quat q) { return glm::axis(q); });
    q.set_function("angle", [](quat q) { return glm::angle(q); });

    engine.resources->register_resource_loader(std::make_unique<ScriptLoader>(lua));
}

void ScriptManager::load_plugins() {
    // TODO: hot reload
    auto cwd = std::filesystem::current_path();
    auto plugins_path = cwd / "plugins";

    if (std::filesystem::exists(plugins_path)) {
        for (auto& entry : std::filesystem::recursive_directory_iterator(plugins_path)) {
            if (entry.is_regular_file() and entry.path().extension() == ".lua") {
                load_and_execute_script(engine, entry.path());
            }
        }
    }
}

void ScriptManager::load_stage(std::string_view stage_name) {
    // TODO: hot reload
    auto cwd = std::filesystem::current_path();
    auto stages_path = cwd / "stages";
    
    auto first_script = stages_path / std::format("{}.lua", stage_name);
    auto stage_path = stages_path / stage_name;

    bool ran_a_script = false;

    if (std::filesystem::exists(first_script)) {
        ran_a_script = true;
        load_and_execute_script(engine, first_script);
    }

    if (std::filesystem::exists(stage_path)) {
        for (auto& entry : std::filesystem::recursive_directory_iterator(stage_path)) {
            if (entry.is_regular_file() and entry.path().extension() == ".lua") {
                ran_a_script = true;
                load_and_execute_script(engine, entry.path());
            }
        }
    }

    if (!ran_a_script) {
        SPDLOG_ERROR("changed stage to {}, but no scripts were run to change the stage.", stage_name);
    }
}
