#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

#include <GLFW/glfw3.h>

#include "input.h"
#include "gfx.h"
#include "sound.h"
#include "resources.h"
#include "engine.h"
#include "scripts.h"

using namespace motorcar;

Engine::Engine(const std::string_view& name, std::initializer_list<Sprite> sprites) {
    this->sprites = sprites;

    resources = std::make_shared<ResourceManager>();
    gfx = std::make_shared<GraphicsManager>(*this, name);
    input = std::make_shared<InputManager>(*this);
    sound = std::make_shared<SoundManager>(*this);
    scripts = std::make_shared<ScriptManager>(*this);
}

void Engine::run(std::function<void()> callback) {
#define SIMULATION_FREQ 60
    while (!gfx->window_should_close() && keep_running) {
        while (glfwGetTime() > time_simulated_secs) {
            callback();
            input->clear_key_buffers();
            time_simulated_secs += (1. / SIMULATION_FREQ);
        }

        gfx->draw(sprites);
    }
#undef SIMULATION_FREQ
}

