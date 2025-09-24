#include "engine.h"
using namespace motorcar;

Engine::Engine(const std::string& name) : 
    gfx(*this, name), input(*this), sound(*this) {
}


