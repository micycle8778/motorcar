#pragma once

#include <chrono>
#include <string>

#include "input.h"
#include "gfx.h"

namespace motorcar {
    struct Engine {
        GraphicsManager gfx;
        InputManager input;
        
        Engine(const std::string& game_name);
        
        template <typename Callback>
        void run(Callback callback);

        Engine(Engine&) = delete;
        Engine& operator=(Engine&) = delete;
    };
}

void motorcar::Engine::run(auto callback) {
    std::chrono::time_point<std::chrono::steady_clock> frame_start;
    while (!gfx.window_should_close()) {
        frame_start = std::chrono::steady_clock::now();

        callback();
        gfx.draw();

        while (true) {
            auto duration = std::chrono::steady_clock::now() - frame_start;
            auto micros = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
            if (micros >= 16667) break;
        }
    }
}
