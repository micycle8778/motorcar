#include "soloud.h"
#include "soloud_wav.h"

#include "engine.h"
#include "spdlog/spdlog.h"
#include "sound.h"

using namespace motorcar;

SoundManager::SoundManager(Engine& engine) : engine(engine) {
    soloud.init();
}

void SoundManager::play_sound(std::string_view filename) {
    auto sound = engine.resources.get_resource<SoLoud::Wav>(filename);
    if (sound.has_value()) {
        soloud.play(**sound);
    } else {
        spdlog::error("Could not load sound '{}'", filename);
    }
}
