#pragma once

#include <string>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace motorcar {
    const int DEFAULT_WIDTH  = 1280;
    const int DEFAULT_HEIGHT = 720;

    struct Engine;
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
}
