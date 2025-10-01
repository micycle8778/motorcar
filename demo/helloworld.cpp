#include <vector>

#include <spdlog/spdlog.h>

#include <engine.h>
#include <sound.h>
#include <input.h>
#include <types.h>
#include <scripts.h>

int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::trace);

    int press_count = 0;
    int release_count = 0;

    motorcar::Engine e("helloworld", {
        motorcar::Sprite {
            .position = motorcar::vec2(0),
            .scale = motorcar::vec2(25),
            // .scale = motorcar::vec2(0.1, 0.1),
            .depth = .3,
            .texture_path = "bowser.png"
        },
        motorcar::Sprite {
            .position = motorcar::vec2(30),
            .scale = motorcar::vec2(45),
            .depth = .1,
            .texture_path = "watercan.png"
        },
        motorcar::Sprite {
            .position = motorcar::vec2(-50),
            .scale = motorcar::vec2(16),
            .depth = .2,
            .texture_path = "michael-day.png"
        },
        motorcar::Sprite {
            .position = motorcar::vec2(-25, -20),
            .scale = motorcar::vec2(32),
            .depth = .2,
            .texture_path = "insect.png"
        },
    });
    e.run([&]() {
        e.scripts->run_script("hello.lua");

        // e.sprites[0].position.x += 0.1;

        // if (e.input->is_key_pressed_this_frame('`')) {
        //     press_count++;
        //     SPDLOG_DEBUG("tilde pressed {} times", press_count);
        // }

        // if (e.input->is_key_repeated_this_frame('f')) {
        //     SPDLOG_DEBUG("respects paid");
        // }

        // if (e.input->is_key_released_this_frame("space")) {
        //     release_count++;
        //     SPDLOG_DEBUG("space released {} times", release_count);
        //     e.sound->play_sound("doo-doo.mp3");
        //     e.scripts->run_script("hello.lua");
        // }

        // if (e.input->is_key_held_down("f3")) {
        //     SPDLOG_DEBUG("f3");
        // }
    });

    return 0;
}
