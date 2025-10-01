#include <sol/sol.hpp>

namespace motorcar {
    struct Engine;

    class ScriptManager {
        sol::state lua;
        Engine& engine;

        public:
            ScriptManager(Engine& engine);

            void run_script(std::string_view path);

            // script manager's memory address should never change
            ScriptManager(ScriptManager&) = delete;
            ScriptManager& operator=(ScriptManager&) = delete;
            ScriptManager(ScriptManager&&) = delete;
            ScriptManager& operator=(ScriptManager&&) = delete;
    };
}