#pragma once
#include "ecs.h"
#include "types.h"
#include <sol/forward.hpp>
#include <sol/sol.hpp>
#include <stdexcept>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#define COMPONENT_TYPE_TRAIT(type, name) \
    template <>\
    struct ComponentTypeTrait<type> {\
        constexpr static bool value = true;\
        constexpr static std::string_view component_name = name;\
    }

#define NOT_LUA_CONSTRUCTABLE(type) \
    type(sol::object) {\
        throw std::runtime_error(#type " is not constructable from lua object!");\
    }

#define DEFAULT_LUA_CONSTRUCTABLE(type) \
    type(sol::object object) {\
        *this = object.as<type>(); \
    } \

namespace motorcar {
    struct Transform {
        vec3 position = vec3(0);
        quat rotation = quat(1, 0, 0, 0);
        vec3 scale = vec3(1);

        constexpr Transform() {}
        DEFAULT_LUA_CONSTRUCTABLE(Transform);

        constexpr Transform with_position(vec3 position) const {
            Transform ret;
            ret.position = position;
            ret.rotation = rotation;
            ret.scale = scale;

            return ret;
        }
        constexpr Transform with_rotation(quat rotation) const {
            Transform ret;
            ret.position = position;
            ret.rotation = rotation;
            ret.scale = scale;

            return ret;
        }
        // Set rotation from Euler angles (radians)
        Transform with_rotation_euler(vec3 euler) const {
            Transform ret;
            ret.position = position;
            ret.rotation = glm::quat(euler);
            ret.scale = scale;
            return ret;
        }

        constexpr Transform with_scale(vec3 scale) const {
            Transform ret;
            ret.position = position;
            ret.rotation = rotation;
            ret.scale = scale;

            return ret;
        }

        Transform translated(vec3 translation) const {
            Transform ret;
            ret.position = position + translation;
            ret.rotation = rotation;
            ret.scale = scale;

            return ret;
        }
        Transform scaled(vec3 scale) const {
            Transform ret;
            ret.position = position;
            ret.rotation = rotation;
            ret.scale = this->scale * scale;

            return ret;
        }
        Transform rotated_quat(quat rotation) const {
            Transform ret;
            ret.position = position;
            ret.rotation = rotation * this->rotation;
            ret.scale = scale;

            return ret;
        }
        // Rotate by an Euler vector (radians). Construct a quaternion from the euler angles
        // and apply it (pre-multiplied) to the existing rotation.
        Transform rotated_euler(vec3 euler) const {
            Transform ret;
            ret.position = position;
            quat q = glm::quat(euler);
            ret.rotation = q * this->rotation;
            ret.scale = scale;

            return ret;
        }
        Transform rotated(vec3 axis, f32 rads) const {
            Transform ret;
            ret.position = position;
            ret.rotation = glm::rotate(rotation, rads, axis);
            ret.scale = scale;

            return ret;
        }

        // TODO: cache this
        mat4 model_matrix() const {
            mat4 ret = {1};
            ret = glm::translate(ret, position);
            ret = ret * mat4(rotation);
            ret = glm::scale(ret, scale);

            return ret;
        }

        // TODO: cache this
        mat3 normal_matrix() const {
            return mat3(rotation);
        }

        void look_at(vec3 target) {
            vec3 forward = vec3(0, 0, -1);
            vec3 direction = glm::normalize(target - position);
            rotation = glm::rotation(forward, direction);
        }

        void translate_by(vec3 translation) {
            *this = translated(translation);
        }
        void scale_by(vec3 scale) {
            *this = scaled(scale);
        }
        void rotate_by_quat(quat rotation) {
            *this = rotated_quat(rotation);
        }
        void rotate_by(vec3 axis, f32 rads) {
            *this = rotated(axis, rads);
        }
        // Mutating Euler-based rotation (radians)
        void rotate_by_euler(vec3 euler) {
            *this = rotated_euler(euler);
        }
        // Set rotation directly from Euler angles (radians)
        void set_rotation_euler(vec3 euler) {
            rotation = glm::quat(euler);
        }

        // Read back Euler angles (radians)
        vec3 euler_angles() const {
            return glm::eulerAngles(rotation);
        }
    };
    COMPONENT_TYPE_TRAIT(Transform, "transform");

    struct GlobalTransform {
        mat4 model;
        mat3 normal;
        GlobalTransform(mat4 model, mat3 normal) : model(model), normal(normal) {}
        NOT_LUA_CONSTRUCTABLE(GlobalTransform);

        vec3 position() const { return model * vec4(vec3(0), 1); }

        vec3 forward() const { return normal * vec3(0, 0, -1); }
        vec3 backward() const { return normal * vec3(0, 0, 1); }
        vec3 left() const { return normal * vec3(-1, 0, 0); }
        vec3 right() const { return normal * vec3(1, 0, 0); }
        vec3 up() const { return normal * vec3(0, 1, 0); }
        vec3 down() const { return normal * vec3(0, -1, 0); }
    };
    COMPONENT_TYPE_TRAIT(GlobalTransform, "global_transform");

    struct Parent {
        Entity parent;
        Parent(Entity parent) : parent(parent) {}
        Parent(sol::object object) {
            if (object.is<Parent>()) {
                *this = object.as<Parent>();
            } else if (object.is<Entity>())
                *this = Parent(object.as<Entity>());
            else {
                throw std::runtime_error("object is not convertible to Parent");
            }
        }
    };
    COMPONENT_TYPE_TRAIT(Parent, "parent");

    struct Velocity {
        vec3 v;

        constexpr Velocity(vec3 v) : v(v) {}
        Velocity(sol::object object) {
            if (object.is<vec3>()) {
                v = object.as<vec3>();
            } else if (object.is<Velocity>()) {
                *this = object.as<Velocity>();
            } else {
                throw std::runtime_error("object is not convertible to Velocity");
            }
        }
    };
    COMPONENT_TYPE_TRAIT(Velocity, "velocity");

    struct Sprite {
        std::string resource_path;
        constexpr Sprite(std::string resource_path) : resource_path(resource_path) {}
        Sprite(sol::object object) {
            if (object.is<std::string>()) {
                resource_path = object.as<std::string>();
            } else if (object.is<Sprite>()) {
                *this = object.as<Sprite>();
            } else {
                throw std::runtime_error("object is not convertible to Sprite");
            }
        }
    };
    COMPONENT_TYPE_TRAIT(Sprite, "sprite");

    struct System {
        std::function<void()> callback;
        size_t priority;

        System(std::function<void()> callback, size_t priority) : callback(callback), priority(priority) {}
        NOT_LUA_CONSTRUCTABLE(System)
    };
    COMPONENT_TYPE_TRAIT(System, "::system");

    struct EventHandler {
        std::function<void(sol::object)> callback;
        std::string event_name;

        EventHandler(std::function<void(sol::object)> callback, std::string event_name) : 
            callback(callback), event_name(event_name) {}
        NOT_LUA_CONSTRUCTABLE(EventHandler)
    };
    COMPONENT_TYPE_TRAIT(EventHandler, "::event_handler");

    struct RenderSystem {
        RenderSystem() {}
        NOT_LUA_CONSTRUCTABLE(RenderSystem)
    };
    COMPONENT_TYPE_TRAIT(RenderSystem, "::render_system");

    struct PhysicsSystem {
        PhysicsSystem() {}
        NOT_LUA_CONSTRUCTABLE(PhysicsSystem)
    };
    COMPONENT_TYPE_TRAIT(PhysicsSystem, "::physics_system");

    struct BoundToStage {
        std::string stage_name;
        BoundToStage(std::string stage_name) : stage_name(stage_name) {}
        NOT_LUA_CONSTRUCTABLE(BoundToStage)
    };
    COMPONENT_TYPE_TRAIT(BoundToStage, "::bound_to_stage");

    struct BoundToScript {
        std::string script_name;
        BoundToScript(std::string script_name) : script_name(script_name) {}
        NOT_LUA_CONSTRUCTABLE(BoundToScript)
    };
    COMPONENT_TYPE_TRAIT(BoundToScript, "::bound_to_script");

    struct GLTF {
        std::string resource_path;
        GLTF(std::string resource_path) : resource_path(resource_path) {}

        GLTF(sol::object object) {
            if (object.is<std::string>()) {
                resource_path = object.as<std::string>();
            } else if (object.is<GLTF>()) {
                *this = object.as<GLTF>();
            } else {
                throw std::runtime_error("object is not convertible to GLTF");
            }
        }
    };
    COMPONENT_TYPE_TRAIT(GLTF, "gltf");

    struct Albedo {
        vec4 color;
        Albedo(vec4 color) : color(color) {}
        Albedo(vec3 color) : color(vec4(color, 1)) {}

        Albedo(sol::object object) {
            if (object.is<vec4>()) {
                color = object.as<vec4>();
            } else if (object.is<vec3>()) {
                color = vec4(object.as<vec3>(), 1);
            } else if (object.is<Albedo>()) {
                *this = object.as<Albedo>();
            } else {
                throw std::runtime_error("object is not convertible to Albedo");
            }
        }
    };
    COMPONENT_TYPE_TRAIT(Albedo, "albedo");

    struct Camera {
        Camera() {}
        Camera(sol::object) {}
    };
    COMPONENT_TYPE_TRAIT(Camera, "camera");

    struct Text {
        std::string text;

        Text(std::string s) : text(s) {}
        Text(sol::object object) {
            if (object.is<std::string>()) {
                text = object.as<std::string>();
            } else if (object.is<Text>()) {
                *this = object.as<Text>();
            } else {
                throw std::runtime_error("object is not convertible to Text");
            }
        }
    };
    COMPONENT_TYPE_TRAIT(Text, "text");

    struct Sprite3D {
        std::string resource_path;
        Sprite3D(std::string resource_path) : resource_path(resource_path) {}
        Sprite3D(sol::object object) {
            if (object.is<std::string>()) {
                resource_path = object.as<std::string>();
            } else if (object.is<Sprite3D>()) {
                *this = object.as<Sprite3D>();
            } else {
                throw std::runtime_error("object is not convertible to Sprite3D");
            }
        }
    };
    COMPONENT_TYPE_TRAIT(Sprite3D, "sprite3d");

    struct Light {
        vec3 ambient;
        vec3 diffuse;
        vec3 specular;
        f32 distance;

        Light(vec3 color, f32 distance, f32 ambient_power = 0.25f) : 
            Light(color * ambient_power, color, color, distance) {}
        Light(vec3 ambient, vec3 diffuse, vec3 specular, f32 distance) :
            ambient(ambient),
            diffuse(diffuse),
            specular(specular),
            distance(distance)
        {}

        DEFAULT_LUA_CONSTRUCTABLE(Light);
    };
    COMPONENT_TYPE_TRAIT(Light, "light");
    
    void register_components_to_lua(sol::state& state);
    void register_components_to_ecs(ECSWorld& world);
}
