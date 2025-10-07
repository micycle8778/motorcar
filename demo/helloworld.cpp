#include <sol/raii.hpp>
#include <sol/types.hpp>
#include <vector>

#include <spdlog/spdlog.h>

#include <engine.h>
#include <sound.h>
#include <input.h>
#include <types.h>
#include <scripts.h>
#include <ecs.h>
#include <components.h>

int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::trace);

    motorcar::Engine e("helloworld");
    // motorcar::Engine e("helloworld", {
    //     motorcar::Sprite {
    //         .position = motorcar::vec2(0),
    //         .scale = motorcar::vec2(25),
    //         // .scale = motorcar::vec2(0.1, 0.1),
    //         .depth = .3,
    //         .texture_path = "bowser.png"
    //     },
    //     motorcar::Sprite {
    //         .position = motorcar::vec2(30),
    //         .scale = motorcar::vec2(45),
    //         .depth = .1,
    //         .texture_path = "watercan.png"
    //     },
    //     motorcar::Sprite {
    //         .position = motorcar::vec2(-50),
    //         .scale = motorcar::vec2(16),
    //         .depth = .2,
    //         .texture_path = "michael-day.png"
    //     },
    //     motorcar::Sprite {
    //         .position = motorcar::vec2(-25, -20),
    //         .scale = motorcar::vec2(32),
    //         .depth = .2,
    //         .texture_path = "insect.png"
    //     },
    // });

    auto render_system = e.ecs->new_entity();
    e.ecs->emplace_native_component<motorcar::System>(render_system, [&]() {
        e.scripts->run_script("render.lua");
    }, 0);
    e.ecs->emplace_native_component<motorcar::RenderSystem>(render_system);

    auto physics_system = e.ecs->new_entity();
    e.ecs->emplace_native_component<motorcar::System>(physics_system, [&]() {
        e.scripts->run_script("physics.lua");
    }, 0);
    e.ecs->emplace_native_component<motorcar::PhysicsSystem>(physics_system);

    auto watering_can = e.ecs->new_entity();
    e.ecs->emplace_native_component<motorcar::Sprite>(watering_can, "watercan.png");
    e.ecs->emplace_native_component<motorcar::Transform>(watering_can, motorcar::Transform().with_scale(motorcar::vec3(40)));

    e.ecs->flush_command_queue();

    e.scripts->run_script("first.lua");
    e.run();
    // e.run([&]() {
    //     e.scripts->run_script("hello.lua");
    // });

    return 0;
}
