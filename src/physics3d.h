#pragma once

#include <optional>

#include "types.h"
#include "components.h"

namespace motorcar {
    struct Collider {
        AABB aabb; // AABB when the model has no rotation
        f32 radius; // radius of the broad phase circle collider

        Collider(AABB aabb) : 
            aabb(aabb), radius(glm::length(aabb.half_size))
        {}
        DEFAULT_LUA_CONSTRUCTABLE(Collider);
    };

    struct Body {
        // BodyType body; // static, trigger, kinematic // is static needed?
        Collider collider; // TODO: many colliders
        Body(Collider collider) : collider(collider) {}
        Body(AABB aabb) : collider(aabb) {}
        Body(sol::object object) : collider(Collider(AABB())) {
            *this = object.as<Body>();
        }
    };
    COMPONENT_TYPE_TRAIT(Body, "body");

    struct Engine;
    class PhysicsManager {
        Engine& engine;

        public:
            PhysicsManager(Engine& engine);

            std::optional<std::pair<Entity, vec3>> cast_ray(vec3 origin, vec3 direction, Entity excluded);
    };
}
