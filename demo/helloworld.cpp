#include "types.h"
#include <spdlog/spdlog.h>
#include <engine.h>
#include <sound.h>
#include <input.h>
#include <vector>

int main() {
    spdlog::set_level(spdlog::level::trace);

    int press_count = 0;
    int release_count = 0;

    std::vector<motorcar::Sprite> sprites = {
        // motorcar::Sprite {
        //     .position = motorcar::vec2(0.1, 0.1),
        //     .scale = motorcar::vec2(0.1, 0.1),
        //     .depth = 1.,
        //     .texture_path = "michael-day.png"
        // },
        motorcar::Sprite {
            .depth = 0.,
            .texture_path = "watercan.png"
        },
    };

    motorcar::Engine e("helloworld");
    e.run([&]() {
        if (e.input->is_key_pressed_this_frame('`')) {
            press_count++;
            spdlog::debug("tilde pressed {} times", press_count);
        }

        if (e.input->is_key_repeated_this_frame('f')) {
            spdlog::debug("respects paid");
        }

        if (e.input->is_key_released_this_frame(' ')) {
            release_count++;
            spdlog::debug("space released {} times", release_count);
            e.sound->play_sound("doo-doo.mp3");
        }

        if (e.input->is_key_held_down("f3")) {
            spdlog::debug("f3");
        }
    }, sprites);

    return 0;
}
