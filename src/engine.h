#pragma once

#include <memory>
#include <functional>
#include <optional>

namespace motorcar {
    class ResourceManager;
    class ECSWorld;
    class GraphicsManager;
    class InputManager;
    class SoundManager;
    class ScriptManager;

    struct Engine {
        std::shared_ptr<ScriptManager> scripts;
        std::shared_ptr<SoundManager> sound;
        std::shared_ptr<ResourceManager> resources;
        std::shared_ptr<GraphicsManager> gfx;
        std::shared_ptr<InputManager> input;
        std::shared_ptr<ECSWorld> ecs;

        std::optional<std::string> stage;
        std::optional<std::string> next_stage;

        double delta = 0.;
        double time_simulated_secs = 0.;
        bool keep_running = true;

        Engine(const std::string_view& game_name);

        // the engine should never change memory locations
        Engine(Engine&) = delete;
        Engine& operator=(Engine&) = delete;
        Engine(Engine&&) = delete;
        Engine& operator=(Engine&&) = delete;

        void run();
    };
}
