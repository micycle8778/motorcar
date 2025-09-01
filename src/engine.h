#pragma once

#include <utility>
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#include <string>


namespace motorcar {
    const int DEFAULT_WIDTH  = 1280;
    const int DEFAULT_HEIGHT = 720;

    class GraphicsManager {
        GLFWwindow* window;

        public:
            GraphicsManager(const std::string& window_name, int window_width = DEFAULT_WIDTH, int window_height = DEFAULT_HEIGHT);
            bool window_should_close();
            void draw();

            GraphicsManager(GraphicsManager&) = delete;
            GraphicsManager& operator=(GraphicsManager&) = delete;

            ~GraphicsManager();
    };

    class Engine {
        GraphicsManager graphics_manager;

        public:
            Engine(const std::string& game_name);
            void run();

            Engine(Engine&) = delete;
            Engine& operator=(Engine&) = delete;
    };
}
