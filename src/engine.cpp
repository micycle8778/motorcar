#include <iostream>
#include <stdexcept>
#include "GLFW/glfw3.h"
#include "engine.h"
using namespace motorcar;

GraphicsManager::GraphicsManager(
        Engine& engine,
        const std::string& window_name,
        int window_width,
        int window_height
) : engine(engine) {
    if (!glfwInit()) {
        const char* error;
        glfwGetError(&error);
        throw std::runtime_error(std::string(error));
    }

    // We don't want GLFW to set up a graphics API.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // Create the window.
    window = glfwCreateWindow(window_width, window_height, window_name.c_str(), nullptr, nullptr);
    if( !window )
    {
        const char* error;
        glfwGetError(&error);
        glfwTerminate();
        throw std::runtime_error(std::string(error));
    }
    glfwSetWindowAspectRatio(window, window_width, window_height);

    glfwSetWindowUserPointer(window, &engine);
}

bool GraphicsManager::window_should_close() {
    return glfwWindowShouldClose(window);
}

void GraphicsManager::draw() {
    engine.input.clear_key_buffers();
    glfwPollEvents();
}

GraphicsManager::~GraphicsManager() {
    glfwDestroyWindow(window);
    glfwTerminate();
}

Engine::Engine(const std::string& name) : gfx(*this, name), input(*this) {
}


