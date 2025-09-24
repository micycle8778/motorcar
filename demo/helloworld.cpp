#include "input.h"
#include "soloud.h"
#include "soloud_wav.h"

#include <engine.h>

int main() {
    spdlog::set_level(spdlog::level::trace);

    int press_count = 0;
    int release_count = 0;

    motorcar::Engine e("helloworld");
    e.run([&]() {
        // spdlog::trace("lambda start");

        if (e.input.is_key_pressed_this_frame('`')) {
            press_count++;
            spdlog::debug("tilde pressed {} times", press_count);
        }

        if (e.input.is_key_repeated_this_frame('f')) {
            spdlog::debug("respects paid");
        }

        if (e.input.is_key_released_this_frame(' ')) {
            release_count++;
            spdlog::debug("space released {} times", release_count);
            e.sound.play_sound("doo-doo.mp3");
        }

        if (e.input.is_key_held_down("f3")) {
            spdlog::debug("f3");
        }

        // spdlog::trace("lambda end");
    });

    return 0;
}
