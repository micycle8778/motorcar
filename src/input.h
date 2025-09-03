#pragma once

#include <array>
#include "GLFW/glfw3.h"

namespace motorcar {
    struct Engine;
    class GraphicsManager;

    class invalid_key_error : public std::exception {
        std::string error_msg;
        public:
            invalid_key_error(const std::string_view& key) {
                error_msg = std::format("invalid key '{}' passed to constructor of motorcar::Key", key);
            }

            invalid_key_error(int key) {
                error_msg = std::format("invalid key '{}' passed to constructor of motorcar::Key", key);
            }

            const char* what() const noexcept override {
                return error_msg.c_str();
            }
    };

    struct Key {
        int keycode;

        constexpr Key(int glfw_keycode);
        constexpr Key(std::string_view s);
        constexpr Key(const char* c) { *this = std::string_view(c); }
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

        bool is_key_held_down(Key k) const;
        bool is_key_pressed_this_frame(Key k) const;
        bool is_key_released_this_frame(Key k) const;
    };
}

#include "input.ipp"