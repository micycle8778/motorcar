#include <sol/sol.hpp>

namespace motorcar {
    struct Engine;

    class ScriptManager {
        sol::state lua;
        Engine& engine;

        public:
            ScriptManager(Engine& engine);

            // script manager's memory address should never change
            ScriptManager(ScriptManager&) = delete;
            ScriptManager& operator=(ScriptManager&) = delete;
            ScriptManager(ScriptManager&&) = delete;
            ScriptManager& operator=(ScriptManager&&) = delete;
    };
}