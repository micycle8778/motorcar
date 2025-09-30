#pragma once

#include <memory>
#include <functional>
#include <span>
#include <string>

#include "types.h"


namespace motorcar {
    class ResourceManager;
    class GraphicsManager;
    class InputManager;
    class SoundManager;
    class ScriptManager;

    struct Engine {
        std::shared_ptr<SoundManager> sound;
        std::shared_ptr<ResourceManager> resources;
        std::shared_ptr<GraphicsManager> gfx;
        std::shared_ptr<InputManager> input;
        std::shared_ptr<ScriptManager> scripts;

        double time_simulated_secs = 0.;
        bool keep_running = true;

        Engine(const std::string_view& game_name);

        // the engine should never change memory locations
        Engine(Engine&) = delete;
        Engine& operator=(Engine&) = delete;
        Engine(Engine&&) = delete;
        Engine& operator=(Engine&&) = delete;

        void run(std::function<void()> callback, std::vector<Sprite>& sprites);
    };
}
