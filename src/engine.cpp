#include <iostream>
#include <stdexcept>
#include "GLFW/glfw3.h"
#include "engine.h"
using namespace motorcar;

Engine::Engine(const std::string& name) : gfx(*this, name), input(*this) {
}


