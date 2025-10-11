#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

#include <cmath>
#include <ranges>
#include <sol/raii.hpp>
#include <utility>

#include "glm/geometric.hpp"
#include "assimp/aabb.h"

#include <spdlog/spdlog.h>

#include "scripts.h"
#include "types.h"
#include "components.h"
#include "physics3d.h"
#include "engine.h"

using namespace motorcar;

template <typename T>
vec3 ai2glm(T v) {
    return vec3(v.x, v.y, v.z);
}

struct AABB {
    vec3 center;
    vec3 half_size;

    AABB() : center(vec3(0.)), half_size(vec3(1.)) {}
    AABB(vec3 center, vec3 half_size) : center(center), half_size(half_size) {}
    AABB(aiAABB ai_aabb) {
        vec3 min = ai2glm(ai_aabb.mMin);
        vec3 max = ai2glm(ai_aabb.mMax);

        center = glm::mix(min, max, 0.5);
        half_size = (max - min) * 0.5f;
    };
};

struct OBB {
    // i need three normals and 8 points
    std::array<vec3, 8> points;
    std::array<vec3, 3> normals;

    vec3 center;
    
    OBB(AABB aabb, mat4 _model_matrix, mat3 normal_matrix) {
        mat4 model_matrix = glm::scale(_model_matrix, aabb.half_size);

        normals = {
            normal_matrix * vec3(1, 0, 0),
            normal_matrix * vec3(0, 1, 0),
            normal_matrix * vec3(0, 0, 1)
        };

        center = model_matrix * vec4(aabb.center, 1);

        points = {
            vec3(model_matrix * vec4(vec3(  1,  1,  1 ), 1)),
            vec3(model_matrix * vec4(vec3(  1,  1, -1 ), 1)),
            vec3(model_matrix * vec4(vec3(  1, -1,  1 ), 1)),
            vec3(model_matrix * vec4(vec3(  1, -1, -1 ), 1)),
            vec3(model_matrix * vec4(vec3( -1,  1,  1 ), 1)),
            vec3(model_matrix * vec4(vec3( -1,  1, -1 ), 1)),
            vec3(model_matrix * vec4(vec3( -1, -1,  1 ), 1)),
            vec3(model_matrix * vec4(vec3( -1, -1, -1 ), 1)),
        };
    }

    // computes the minimum and maximum across an axis.
    // think of it as squishing the shape along the axis and looking where it sits.
    //
    // i think math nerds call this the projection distance or something, but squish
    // is a funnier method name
    std::pair<f32, f32> squish(vec3 normal) {
        f32 ret_min = INFINITY;
        f32 ret_max = -INFINITY;

        for (vec3 point : points) {
            f32 x = glm::dot(point, normal);
            ret_min = std::min(ret_min, x);
            ret_max = std::max(ret_max, x);
        }

        return std::make_pair(ret_min, ret_max);
    }
};

namespace motorcar {
    struct CollidingWith {
        std::vector<Entity> entities;
        CollidingWith(std::vector<Entity> entities) : entities(entities) {}
        NOT_LUA_CONSTRUCTABLE(CollidingWith);
    };
    COMPONENT_TYPE_TRAIT(CollidingWith, "colliding_with");

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
}

struct TransformedBody {
    OBB obb;
    f32 radius; // radius of the broad phase circle collider

    TransformedBody(Body body, mat4 model_matrix, mat3 normal_matrix) :
        obb(body.collider.aabb, model_matrix, normal_matrix),
        radius(body.collider.radius)
    {}
};

bool bodies_overlap(TransformedBody a, TransformedBody b) {
    f32 min_radius = a.radius + b.radius;

    // broad check
    if (glm::distance(a.obb.center, b.obb.center) > min_radius) {
        return false;
    }

    std::array<vec3, 15> normals = {
        a.obb.normals[0],
        a.obb.normals[1],
        a.obb.normals[2],

        b.obb.normals[0],
        b.obb.normals[1],
        b.obb.normals[2],

        glm::cross(a.obb.normals[0], b.obb.normals[0]),
        glm::cross(a.obb.normals[0], b.obb.normals[1]),
        glm::cross(a.obb.normals[0], b.obb.normals[2]),

        glm::cross(a.obb.normals[1], b.obb.normals[0]),
        glm::cross(a.obb.normals[1], b.obb.normals[1]),
        glm::cross(a.obb.normals[1], b.obb.normals[2]),

        glm::cross(a.obb.normals[2], b.obb.normals[0]),
        glm::cross(a.obb.normals[2], b.obb.normals[1]),
        glm::cross(a.obb.normals[2], b.obb.normals[2]),
    };

    for (vec3 normal : normals) {
        // unlikely because like what are the chances with f32's precision
        if (normal == vec3(0.)) [[unlikely]] continue;

        auto [a_min, a_max] = a.obb.squish(normal);
        auto [b_min, b_max] = b.obb.squish(normal);

        if (!(a_max >= b_min && b_max >= a_min)) {
            return false;
        }
    }

    return true;
};

PhysicsManager::PhysicsManager(Engine& engine) : engine(engine) {
    sol::state& lua = engine.scripts->lua;

    lua.new_usertype<AABB>("AABB", sol::constructors<AABB(), AABB(vec3, vec3)>());
    lua.new_usertype<Collider>("Collider", sol::constructors<Collider(AABB)>());
    lua.new_usertype<Body>("Body", sol::constructors<Body(Collider), Body(AABB)>());
    lua.new_usertype<CollidingWith>("CollidingWith", sol::constructors(),
        "entities", &CollidingWith::entities
    );

    engine.ecs->register_component<Body>();
    engine.ecs->register_component<CollidingWith>();

    Entity collision_system = engine.ecs->new_entity();
    engine.ecs->emplace_native_component<PhysicsSystem>(collision_system);
    engine.ecs->emplace_native_component<System>(collision_system, [&]() {
        auto it = engine.ecs->query<Entity, GlobalTransform, Body>() | std::views::transform([](auto t) {
            return std::make_pair(std::get<0>(t), TransformedBody(*std::get<2>(t), std::get<1>(t)->model, std::get<1>(t)->normal));
        });

        std::unordered_map<Entity, std::vector<Entity>> colliding_with_table;
        auto bodies = std::vector(it.begin(), it.end());

        for (u32 idx = 0; idx < bodies.size(); idx++) {
            auto [ith_entity, ith_body] = bodies[idx];
            for (u32 jdx = idx + 1; jdx < bodies.size(); jdx++) {
                auto [jth_entity, jth_body] = bodies[jdx];
                if (bodies_overlap(ith_body, jth_body)) {
                    colliding_with_table[ith_entity].push_back(jth_entity);
                    colliding_with_table[jth_entity].push_back(ith_entity);
                }
            }
        }

        // clear CollidingWith
        for (auto [entity, _] : engine.ecs->query<Entity, CollidingWith>()) {
            engine.ecs->remove_native_component_from_entity<CollidingWith>(entity);
        }

        // now lets insert them all
        for (auto& [entity, colliding_with] : colliding_with_table) {
            if (!colliding_with.empty()) {
                engine.ecs->emplace_native_component<CollidingWith>(entity, std::move(colliding_with));
            }
        }
    }, 0);
}
