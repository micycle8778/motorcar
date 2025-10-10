#include "ecs.h"
#include <sol/sol.hpp>
#include "components.h"

using namespace motorcar;

void motorcar::register_components_to_lua(sol::state& state) {
    auto transform = state.new_usertype<Transform>("Transform",
        sol::constructors<Transform()>(),
        "position", &Transform::position,
        "rotation", &Transform::rotation,
        "scale", &Transform::scale
    );

    transform["with_position"] = &Transform::with_position;
    transform["with_rotation"] = &Transform::with_rotation;
    transform["with_scale"] = &Transform::with_scale;

    transform["translated"] = &Transform::translated;
    transform["scaled"] = &Transform::scaled;
    transform["rotated_quat"] = &Transform::rotated_quat;
    transform["rotated"] = &Transform::rotated;

    transform["translate_by"] = &Transform::translate_by;
    transform["scale_by"] = &Transform::scale_by;
    transform["rotate_by_quat"] = &Transform::rotate_by_quat;
    transform["rotate_by"] = &Transform::rotate_by;

    state.new_usertype<Velocity>("Velocity",
        sol::constructors<Velocity(vec3)>(),
        "v", &Velocity::v
    );

    state.new_usertype<Sprite>("Sprite",
        sol::constructors<Sprite(std::string)>(),
        "resource_path", &Sprite::resource_path
    );
}

void motorcar::register_components_to_ecs(ECSWorld &world) {
    world.register_component<Transform>();
    world.register_component<Velocity>();
    world.register_component<Sprite>();

    world.register_component<System>();
    world.register_component<EventHandler>();
    world.register_component<RenderSystem>();
    world.register_component<PhysicsSystem>();
    world.register_component<Stage>();
}
