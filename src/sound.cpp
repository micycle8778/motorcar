#include <soloud.h>
#include <soloud_wav.h>
#include <spdlog/spdlog.h>

#include "engine.h"
#include "sound.h"
#include "resources.h"

using namespace motorcar;

namespace {
    /*
    namespace SoLoud
    {
        enum SOLOUD_ERRORS
        {
            SO_NO_ERROR       = 0, // No error
            INVALID_PARAMETER = 1, // Some parameter is invalid
            FILE_NOT_FOUND    = 2, // File not found
            FILE_LOAD_FAILED  = 3, // File found, but could not be loaded
            DLL_NOT_FOUND     = 4, // DLL not found, or wrong DLL
            OUT_OF_MEMORY     = 5, // Out of memory
            NOT_IMPLEMENTED   = 6, // Feature not implemented
            UNKNOWN_ERROR     = 7  // Other error
        };
    };
    */
    struct SoLoudWavLoader : public ILoadResources {
        std::optional<Resource> load_resource(const std::filesystem::path& path, std::ifstream& file_stream) override {
            auto data = get_data_from_file_stream(file_stream);
            SoLoud::Wav sound;
            auto error = sound.loadMem(data.data(), data.size(), false, false);

            if (error) {
                spdlog::trace("Failed to load audio file {}. (errorno: {})", path.string(), error);
                return {};
            } else {
                return std::move(sound);
            }
        }
    };
}

SoundManager::SoundManager(Engine& engine) : engine(engine) {
    soloud.init();
    engine.resources->register_resource_loader(std::make_unique<SoLoudWavLoader>());
}

void SoundManager::play_sound(std::string_view filename) {
    auto sound = engine.resources->get_resource<SoLoud::Wav>(filename);
    if (sound.has_value()) {
        soloud.play(**sound);
    } else {
        spdlog::error("Could not load sound '{}'", filename);
    }
}

SoundManager::~SoundManager() {
    soloud.deinit();
}
