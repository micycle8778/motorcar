#pragma once

#include <string>

#include "GLFW/glfw3.h"

#include "input.h"
#include "gfx.h"

namespace motorcar {
    struct Engine {
        GraphicsManager gfx;
        InputManager input;

        double time_simulated_secs = 0.;

        Engine(const std::string& game_name);
        
        template <typename Callback>
        void run(Callback callback);

        Engine(Engine&) = delete;
        Engine& operator=(Engine&) = delete;
    };
}

void motorcar::Engine::run(auto callback) {
    while (!gfx.window_should_close()) {

#define SIMULATION_FREQ 5

        while (glfwGetTime() > time_simulated_secs) {
            callback();
            time_simulated_secs += (1. / SIMULATION_FREQ);
        }

        gfx.draw();
    }
}
