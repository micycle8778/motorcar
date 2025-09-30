#pragma once

#include <array>
#include <format>
#include <ranges>
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
        consteval Key(std::string_view s);
        consteval Key(const char* s) : Key(std::string_view(s)) {}

        static constexpr Key key_from_string(std::string_view s);
    };

    // TODO: mouse input
    // TODO: controller input
    // TODO: input action abstraction
    class InputManager {
        friend class GraphicsManager;
        friend struct Engine;

        Engine& engine;
        std::array<bool, GLFW_KEY_LAST + 1> keys_repeated_this_frame;
        std::array<bool, GLFW_KEY_LAST + 1> keys_pressed_this_frame;
        std::array<bool, GLFW_KEY_LAST + 1> keys_released_this_frame;

        void clear_key_buffers();
        
    public:
        InputManager(Engine& engine);

        bool is_key_held_down(Key k) const;
        bool is_key_pressed_this_frame(Key k) const;
        bool is_key_repeated_this_frame(Key k) const;
        bool is_key_released_this_frame(Key k) const;
    };
}

namespace motorcar {
    namespace {
        constexpr bool valid_glfw_keycode(int keycode) {
            // yoinked from glfwGetKey
            return !(keycode < GLFW_KEY_SPACE || keycode > GLFW_KEY_LAST);
        }

        constexpr bool is_not_whitespace(int c) {
            return !(c == ' ' || c == '-' || c == '_');
        }

        template <typename T>
        constexpr T ctoupper(T c) {
            return (c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : c;
        }
    }

    constexpr Key Key::key_from_string(std::string_view s) {
        int keycode = -1;
        if (s.length() == 1) {
            return Key(s[0]);
        } else {
            // create a new string.
            // this new string should be all uppercase and only contain letters & numbers
            //
            // this filter effectively allows us to support "KP3", "kp3", "kp_3", "kp 3", etc.
            // because this constructor is constexpr, we probably won't pay for this complexity at runtime.
            // also, this is gonna be super useful for the scripting api
            auto it =
                s
                | std::views::filter(is_not_whitespace)
                | std::views::transform(ctoupper<int>);
            std::string new_s(it.begin(), it.end());

            if (new_s.length() == 1) {
                return Key(s[0]);
            }

            #define KEY_F_COND(num) if (new_s == ("F" #num)) keycode = GLFW_KEY_F##num; else
            #define KEY_KP_COND(num) if (new_s == ("KP" #num)) keycode = GLFW_KEY_KP_##num; else
            #define KEY_MODIFIER_COND(side, name) if (new_s == (#side #name)) keycode = GLFW_KEY_##side _ ##name; else

            if (new_s == "SPACE") keycode = GLFW_KEY_SPACE; else
            KEY_F_COND(1);
            KEY_F_COND(2);
            KEY_F_COND(3);
            KEY_F_COND(4);
            KEY_F_COND(5);
            KEY_F_COND(6);
            KEY_F_COND(7);
            KEY_F_COND(8);
            KEY_F_COND(9);
            KEY_F_COND(10);
            KEY_F_COND(11);
            KEY_F_COND(12);
            KEY_F_COND(13);
            KEY_F_COND(14);
            KEY_F_COND(15);
            KEY_F_COND(16);
            KEY_F_COND(17);
            KEY_F_COND(18);
            KEY_F_COND(19);
            KEY_F_COND(20);
            KEY_F_COND(21);
            KEY_F_COND(22);
            KEY_F_COND(23);
            KEY_F_COND(24);
            KEY_F_COND(25);

            KEY_KP_COND(0);
            KEY_KP_COND(1);
            KEY_KP_COND(2);
            KEY_KP_COND(3);
            KEY_KP_COND(4);
            KEY_KP_COND(5);
            KEY_KP_COND(6);
            KEY_KP_COND(7);
            KEY_KP_COND(8);
            KEY_KP_COND(9);

            KEY_KP_COND(DECIMAL);
            KEY_KP_COND(DIVIDE);
            KEY_KP_COND(MULTIPLY);
            KEY_KP_COND(SUBTRACT);
            KEY_KP_COND(ADD);
            KEY_KP_COND(ENTER);
            KEY_KP_COND(EQUAL);
            
            #undef KEY_MODIFIER_COND
            #undef KEY_KP_COND
            #undef KEY_F_COND
        }

        if (keycode == -1) throw invalid_key_error(s);
        return Key(keycode);
    }

    constexpr Key::Key(int glfw_keycode) : keycode(ctoupper(glfw_keycode)) {
        if (!valid_glfw_keycode(glfw_keycode))
            throw invalid_key_error(glfw_keycode);
    }

    consteval Key::Key(std::string_view s) : keycode(-1) {
        *this = key_from_string(s);
    }

    static_assert(Key("kp_a_dd").keycode == GLFW_KEY_KP_ADD);
    static_assert(Key("kp a_dd").keycode == GLFW_KEY_KP_ADD);
    static_assert(Key("kpAdd").keycode == GLFW_KEY_KP_ADD);
    static_assert(Key("6").keycode == GLFW_KEY_6);
    static_assert(Key("6 ").keycode == GLFW_KEY_6);

    static_assert(Key("`").keycode == GLFW_KEY_GRAVE_ACCENT);
    static_assert(Key("` ").keycode == GLFW_KEY_GRAVE_ACCENT);

}
