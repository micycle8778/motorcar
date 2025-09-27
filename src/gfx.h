#pragma once

#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "types.h"

struct GLFWwindow;

namespace motorcar {
    const int DEFAULT_WIDTH  = 1280;
    const int DEFAULT_HEIGHT = 720;

    struct Engine;

    class GraphicsManager {
        friend class InputManager; // friend :)

        struct WebGPUState;

        GLFWwindow* window;
        std::shared_ptr<WebGPUState> webgpu;
        Engine& engine;

        public:
            GraphicsManager(
                    Engine& engine, 
                    const std::string& window_name, 
                    int window_width = DEFAULT_WIDTH, 
                    int window_height = DEFAULT_HEIGHT
            );
            bool window_should_close();
            void draw(std::span<const Sprite> sprites);

            GraphicsManager(GraphicsManager&) = delete;
            GraphicsManager& operator=(GraphicsManager&) = delete;

            ~GraphicsManager();
    };
}
