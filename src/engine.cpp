#include "types.h"
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

#include <GLFW/glfw3.h>

#include "input.h"
#include "gfx.h"
#include "sound.h"
#include "resources.h"
#include "engine.h"
#include "scripts.h"
#include "ecs.h"
#include "components.h"

using namespace motorcar;

namespace {
    template <typename SystemType>
    void run_systems(ECSWorld& world) {
        auto it = world.query<System,SystemType>();
        auto v = std::vector(it.begin(), it.end());
        std::sort(v.begin(), v.end(), [](auto& l, auto& r) {
            return std::get<System*>(l)->priority < std::get<System*>(r)->priority;
        });

        for (auto [system, _] : v) {
            if (system->callback)
                system->callback();
        }
    }
}

Engine::Engine(const std::string_view& name) {
    resources = std::make_shared<ResourceManager>();
    gfx = std::make_shared<GraphicsManager>(*this, name);
    input = std::make_shared<InputManager>(*this);
    sound = std::make_shared<SoundManager>(*this);
    ecs = std::make_shared<ECSWorld>();
    scripts = std::make_shared<ScriptManager>(*this);

    register_components_to_lua(scripts->lua);
    register_components_to_ecs(*ecs);
}

void Engine::run() {
    const f32 SIMULATION_FREQ = 60;
    const u32 MAX_PHYSICS_STEPS = 4;
    const f32 PHYSICS_DELTA = 1. / SIMULATION_FREQ;

    scripts->load_plugins();
    ecs->flush_command_queue();

    stage = "init";
    scripts->load_stage(stage.value());
    ecs->flush_command_queue();

    f32 lastRenderUpdateTimestamp = glfwGetTime();
    while (!gfx->window_should_close() && keep_running) {
        next_stage = {};

        // remember input state for render systems
        InputManager::State input_state = input->state;

        u32 physics_step_allowance = MAX_PHYSICS_STEPS;
        while (glfwGetTime() > time_simulated_secs && physics_step_allowance > 0) {
            delta = PHYSICS_DELTA;
            if (physics_step_allowance == 0) {
                delta = std::max(PHYSICS_DELTA, (f32)(glfwGetTime() - time_simulated_secs));
            }

            run_systems<PhysicsSystem>(*ecs);

            ecs->flush_command_queue();
            input->clear_key_buffers();
            time_simulated_secs += PHYSICS_DELTA;
            physics_step_allowance--;
        }

        time_simulated_secs = std::max(time_simulated_secs, glfwGetTime());

        // setup input for render systems
        input->state = input_state;

        delta = glfwGetTime() - lastRenderUpdateTimestamp;
        lastRenderUpdateTimestamp = glfwGetTime();
        run_systems<RenderSystem>(*ecs);

        ecs->flush_command_queue();
        input->clear_key_buffers();
        gfx->draw(); // input->state gets updated here

        if (next_stage.has_value()) {
            std::string& current_stage = stage.value();
            for (auto [e, stage] : ecs->query<Entity, BoundToStage>()) {
                if (stage->stage_name == current_stage) {
                    ecs->delete_entity(e);
                }
            }

            stage = next_stage;
            scripts->load_stage(next_stage.value());
            ecs->flush_command_queue();
        }

        // free all the memory we used this frame
        ecs->ocean.reset();
    }
}

