#include <sol/sol.hpp>

namespace motorcar {
    struct Engine;

    class ScriptManager {
        Engine& engine;

        public:
            sol::state lua;

            ScriptManager(Engine& engine);

            void run_script(std::string_view path);
            void load_plugins();
            void load_stage(std::string_view stage_name);

            // script manager's memory address should never change
            ScriptManager(ScriptManager&) = delete;
            ScriptManager& operator=(ScriptManager&) = delete;
            ScriptManager(ScriptManager&&) = delete;
            ScriptManager& operator=(ScriptManager&&) = delete;
    };
}
