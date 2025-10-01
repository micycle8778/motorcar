#pragma once

#include <soloud.h>
#include <string_view>
namespace motorcar {
    struct Engine;
    class SoundManager {
        Engine& engine;
        SoLoud::Soloud soloud;

        public:
            SoundManager(Engine& engine);
            ~SoundManager();

            void play_sound(std::string_view path);
    };
}
