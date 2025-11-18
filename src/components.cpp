#include "ecs.h"
#include <sol/sol.hpp>
#include "components.h"

using namespace motorcar;

void motorcar::register_components_to_lua(sol::state& state) {
    auto global_transform = state.new_usertype<GlobalTransform>("GlobalTransform", sol::constructors<>());

    global_transform["position"] = &GlobalTransform::position;

    global_transform["forward"] = &GlobalTransform::forward;
    global_transform["backward"] = &GlobalTransform::backward;
    global_transform["left"] = &GlobalTransform::left;
    global_transform["right"] = &GlobalTransform::right;
    global_transform["up"] = &GlobalTransform::up;
    global_transform["down"] = &GlobalTransform::down;

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

    transform["look_at"] = &Transform::look_at;
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

    state.new_usertype<GLTF>("GLTF",
        sol::constructors<GLTF(std::string)>(),
        "resource_path", &GLTF::resource_path
    );

    state.new_usertype<Albedo>("Albedo",
        sol::constructors<Albedo(vec4), Albedo(vec3)>(),
        "color", &Albedo::color
    );

    state.new_usertype<Text>("Text",
        sol::constructors<Text(std::string)>(),
        "text", &Text::text
    );

    state.new_usertype<Sprite3D>("Sprite3D",
        sol::constructors<Sprite3D(std::string)>(),
        "resource_path", &Sprite3D::resource_path
    );
}

void motorcar::register_components_to_ecs(ECSWorld &world) {
    world.register_component<Transform>();
    world.register_component<GlobalTransform>();
    world.register_component<Parent>();
    world.register_component<Velocity>();
    world.register_component<Sprite>();
    world.register_component<GLTF>();
    world.register_component<Albedo>();
    world.register_component<Text>();
    world.register_component<Sprite3D>();

    world.register_component<System>();
    world.register_component<EventHandler>();
    world.register_component<RenderSystem>();
    world.register_component<PhysicsSystem>();
    world.register_component<BoundToStage>();
    world.register_component<BoundToScript>();

    world.register_component<Camera>();
}
