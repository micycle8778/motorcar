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

    auto watering_can = e.ecs->new_entity();
    e.ecs->emplace_native_component<motorcar::Sprite>(watering_can, "watercan.png");
    e.ecs->emplace_native_component<motorcar::Transform>(
            watering_can, 
            motorcar::Transform()
                .with_scale(motorcar::vec3(40))
                .with_position(motorcar::vec3(0, 0, 1))
    );

    e.ecs->flush_command_queue();

    e.run();

    return 0;
}
