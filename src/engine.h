#pragma once

#include <chrono>
#include <utility>
#include <string>

#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

#include "input.h"

namespace motorcar {
    const int DEFAULT_WIDTH  = 1280;
    const int DEFAULT_HEIGHT = 720;

    struct Engine;
    class InputManager;

    class GraphicsManager {
        friend class InputManager; // friend :)
        GLFWwindow* window;
        Engine& engine;

        public:
            GraphicsManager(
                    Engine& engine, 
                    const std::string& window_name, 
                    int window_width = DEFAULT_WIDTH, 
                    int window_height = DEFAULT_HEIGHT
            );
            bool window_should_close();
            void draw();

            GraphicsManager(GraphicsManager&) = delete;
            GraphicsManager& operator=(GraphicsManager&) = delete;

            ~GraphicsManager();
    };

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