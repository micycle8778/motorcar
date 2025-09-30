#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include "scripts.h"
#include "resources.h"

using namespace motorcar;

namespace {
    class ScriptLoader : public ILoadResources {
        sol::state& lua;

        virtual std::optional<Resource> load_resource(const std::filesystem::path& path, std::ifstream& file_stream) {
            
        };
    };
}

ScriptManager::ScriptManager(Engine& engine) : engine(engine) {
    lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::table);
}