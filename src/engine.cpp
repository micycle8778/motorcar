#include <chrono>
#include <iostream>
#include <stdexcept>
#include "engine.h"
#include "GLFW/glfw3.h"
using namespace motorcar;

GraphicsManager::GraphicsManager(
    const std::string& window_name,
    int window_width,
    int window_height
) {
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
}

bool GraphicsManager::window_should_close() {
    return glfwWindowShouldClose(window);
}

void GraphicsManager::draw() {
}

GraphicsManager::~GraphicsManager() {
    glfwDestroyWindow(window);
    glfwTerminate();
}

Engine::Engine(const std::string& name) : graphics_manager(name) {
}

void Engine::run() {
    std::chrono::time_point<std::chrono::steady_clock> frame_start;
    while (!graphics_manager.window_should_close()) {
        frame_start = std::chrono::steady_clock::now();

        // std::cout << "frame" << std::endl;

        graphics_manager.draw();

        while (true) {
            auto duration = std::chrono::steady_clock::now() - frame_start;
            auto micros = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
            if (micros >= 16667) break;
        }
    }
}
