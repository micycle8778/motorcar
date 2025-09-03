#pragma once

#include <array>
#include <cctype>
#include "GLFW/glfw3.h"

namespace motorcar {
    struct Engine;
    class GraphicsManager;

    struct Key {
        int keycode;

        constexpr Key(int glfw_keycode) : keycode(glfw_keycode) {}
        constexpr Key(char c) : keycode(std::toupper(c)) {}
    };

    // TODO: mouse input
    // TODO: controller input
    // TODO: input action abstraction
    class InputManager {
        friend class GraphicsManager;

        Engine& engine;
        std::array<bool, GLFW_KEY_LAST + 1> keys_pressed_this_frame;
        std::array<bool, GLFW_KEY_LAST + 1> keys_released_this_frame;

        void clear_key_buffers();
        
    public:
        InputManager(Engine& engine);

        bool is_key_held_down(Key k);
        bool is_key_pressed_this_frame(Key k);
        bool is_key_released_this_frame(Key k);
    };
}
