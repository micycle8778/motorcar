#include "GLFW/glfw3.h"
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

#include <spdlog/spdlog.h>

#include "engine.h"
#include "gfx.h"
#include "input.h"
using namespace motorcar;


const u8 InputManager::LEFT_CLICK;
const u8 InputManager::RIGHT_CLICK;
const u8 InputManager::MIDDLE_CLICK;
const u8 InputManager::SCROLL_WHEEL_DOWN;
const u8 InputManager::SCROLL_WHEEL_UP;

InputManager::InputManager(Engine& engine) 
        : engine(engine), 
        state(State {
            .keys_repeated_this_frame = { 0 },
            .keys_pressed_this_frame = { 0 },
            .keys_released_this_frame = { 0 },
        })
{
    glfwSetKeyCallback(engine.gfx->window, [](
            GLFWwindow* window,
            int key,
            int,
            int action,
            int
    ) {
        Engine& engine = *static_cast<Engine*>(glfwGetWindowUserPointer(window));
        InputManager& us = *engine.input;

        if (action == GLFW_RELEASE) {
            us.state.keys_released_this_frame[key] = true;
        } else {
            us.state.keys_repeated_this_frame[key] = true;
            us.state.keys_pressed_this_frame[key] = action == GLFW_PRESS;
        }
    });

    glfwSetMouseButtonCallback(engine.gfx->window, [](
            GLFWwindow* window,
            int button,
            int action,
            int
    ) {
        Engine& engine = *static_cast<Engine*>(glfwGetWindowUserPointer(window));
        InputManager& us = *engine.input;

        u8 key;
        switch (button) {
            case GLFW_MOUSE_BUTTON_LEFT:
                key = LEFT_CLICK;
            case GLFW_MOUSE_BUTTON_RIGHT:
                key = RIGHT_CLICK;
            case GLFW_MOUSE_BUTTON_MIDDLE:
                key = MIDDLE_CLICK;
        }

        if (action == GLFW_RELEASE) {
            us.state.keys_released_this_frame[key] = true;
        } else {
            us.state.keys_repeated_this_frame[key] = true;
            us.state.keys_pressed_this_frame[key] = action == GLFW_PRESS;
        }
    });

    glfwSetScrollCallback(engine.gfx->window, [](
            GLFWwindow* window,
            double,
            double yoffset
    ) {
        Engine& engine = *static_cast<Engine*>(glfwGetWindowUserPointer(window));
        InputManager& us = *engine.input;


        u8 key = yoffset < 0 ? SCROLL_WHEEL_DOWN : SCROLL_WHEEL_UP;
        us.state.keys_pressed_this_frame[key] = true;
    });
}

void InputManager::clear_key_buffers() {
    this->state.keys_repeated_this_frame.fill(false);
    this->state.keys_pressed_this_frame.fill(false);
    this->state.keys_released_this_frame.fill(false);

    double x, y;
    glfwGetCursorPos(engine.gfx->window, &x, &y);
    vec2 v = vec2((f32)x, (f32)y);

    state.mouse_motion = v - state.last_mouse_position;
    state.last_mouse_position = v;
}

bool InputManager::is_key_held_down(Key k) const {
    return glfwGetKey(engine.gfx->window, k.keycode) != GLFW_RELEASE;
}
bool InputManager::is_key_pressed_this_frame(Key k) const {
    return this->state.keys_pressed_this_frame[k.keycode];
}
bool InputManager::is_key_repeated_this_frame(Key k) const {
    return this->state.keys_repeated_this_frame[k.keycode];
}
bool InputManager::is_key_released_this_frame(Key k) const {
    return this->state.keys_released_this_frame[k.keycode];
}
vec2 InputManager::get_mouse_position() const {
    double x, y;
    glfwGetCursorPos(engine.gfx->window, &x, &y);
    return vec2((f32)x, (f32)y);
}
vec2 InputManager::get_mouse_motion_this_frame() const {
    return state.mouse_motion;
}

void InputManager::lock_mouse() {
    glfwSetInputMode(engine.gfx->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void InputManager::unlock_mouse() {
    glfwSetInputMode(engine.gfx->window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}
