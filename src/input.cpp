#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

#include "engine.h"
#include "gfx.h"
#include "input.h"
using namespace motorcar;

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
}

void InputManager::clear_key_buffers() {
    this->state.keys_repeated_this_frame.fill(false);
    this->state.keys_pressed_this_frame.fill(false);
    this->state.keys_released_this_frame.fill(false);
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
