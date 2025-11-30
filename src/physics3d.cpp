#include "glm/ext/matrix_transform.hpp"
#include <optional>
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
#include "physics3d.h"
#include "engine.h"

using namespace motorcar;

template <typename T>
vec3 ai2glm(T v) {
    return vec3(v.x, v.y, v.z);
}

AABB::AABB(aiAABB ai_aabb) {
    vec3 min = ai2glm(ai_aabb.mMin);
    vec3 max = ai2glm(ai_aabb.mMax);

    center = glm::mix(min, max, 0.5);
    half_size = (max - min) * 0.5f;
}

struct OBB {
    // i need three normals and 8 points
    std::array<vec3, 8> points;
    std::array<vec3, 3> normals;

    vec3 center;
    
    OBB(AABB aabb, mat4 _model_matrix, mat3 normal_matrix) {
        mat4 model_matrix = glm::scale(glm::translate(_model_matrix, aabb.center), aabb.half_size);
        // mat4 model_matrix = _model_matrix * local;
        // mat4 model_matrix = glm::translate(glm::scale(_model_matrix, aabb.half_size), aabb.center);
        // mat4 model_matrix = glm::scale(_model_matrix, aabb.half_size);
        // model_matrix = glm::translate(model_matrix, aabb.center);

        normals = {
            normal_matrix * vec3(1, 0, 0),
            normal_matrix * vec3(0, 1, 0),
            normal_matrix * vec3(0, 0, 1)
        };

        center = model_matrix * vec4(0, 0, 0, 1);

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


    struct KinematicBody {
        KinematicBody() {}
        KinematicBody(sol::object) {}
    };
    COMPONENT_TYPE_TRAIT(KinematicBody, "kinematic_body");

    struct TriggerBody {
        TriggerBody() {}
        TriggerBody(sol::object) {}
    };
    COMPONENT_TYPE_TRAIT(TriggerBody, "trigger_body");
}

struct TransformedBody {
    OBB obb;
    f32 radius; // radius of the broad phase circle collider

    TransformedBody(Body body, mat4 model_matrix, mat3 normal_matrix) :
        obb(body.collider.aabb, model_matrix, normal_matrix),
        radius(body.collider.radius)
    {}
};

std::optional<vec3> bodies_overlap(TransformedBody a, TransformedBody b) {
    f32 min_radius = a.radius + b.radius;

    // broad check
    if (glm::distance(a.obb.center, b.obb.center) > min_radius) {
        return {};
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

    vec3 displacement = vec3(INFINITY);
    for (vec3 normal : normals) {
        // unlikely because like what are the chances with f32's precision
        if (normal == vec3(0.)) [[unlikely]] continue;

        normal = glm::normalize(normal);

        auto [a_min, a_max] = a.obb.squish(normal);
        auto [b_min, b_max] = b.obb.squish(normal);

        if (!(a_max >= b_min && b_max >= a_min)) {
            return {};
        }

        vec3 potential_displacement = normal * (std::min(a_max, b_max) - std::max(a_min, b_min));
        if (glm::length(potential_displacement) < glm::length(displacement)) {
            displacement = potential_displacement;
        }
    }

    // SPDLOG_DEBUG("collision");
    return displacement;
};

PhysicsManager::PhysicsManager(Engine& engine) : engine(engine) {
    sol::state& lua = engine.scripts->lua;

    lua.new_usertype<AABB>("AABB", sol::constructors<AABB(), AABB(vec3, vec3)>(),
        "center", &AABB::center,
        "half_size", &AABB::half_size
    );
    lua.new_usertype<Collider>("Collider", sol::constructors<Collider(AABB)>());
    lua.new_usertype<Body>("Body", sol::constructors<Body(Collider), Body(AABB)>());
    lua.new_usertype<CollidingWith>("CollidingWith", sol::constructors(),
        "entities", &CollidingWith::entities
    );

    sol::table physics_namespce = lua["Physics"].force();
    physics_namespce.set_function("cast_ray", [&](vec3 origin, vec3 direction, std::optional<Entity> excluded) -> sol::object {
            Entity e = excluded.has_value() ? *excluded : -1;
            auto ret = cast_ray(origin, direction, e);

            if (ret.has_value()) {
                sol::table t = sol::table(lua, sol::create);
                t["entity"] = std::get<Entity>(*ret);
                t["position"] = std::get<vec3>(*ret);
                return t;
            }
            return sol::nil;
    });

    engine.ecs->register_component<Body>();
    engine.ecs->register_component<CollidingWith>();
    engine.ecs->register_component<KinematicBody>();
    engine.ecs->register_component<TriggerBody>();

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
                if (auto v = bodies_overlap(ith_body, jth_body)) {
                    colliding_with_table[ith_entity].push_back(jth_entity);
                    colliding_with_table[jth_entity].push_back(ith_entity);

                    bool one_is_trigger =
                        engine.ecs->entity_has_native_component<TriggerBody>(ith_entity) ||
                        engine.ecs->entity_has_native_component<TriggerBody>(jth_entity);

                    // if neither is a trigger, then we should try to bump out kinematic bodies
                    if (!one_is_trigger) { 
                        vec3 displacement = v.value();
                        if (glm::dot(displacement, jth_body.obb.center - ith_body.obb.center) < 0) {
                            displacement = -displacement;
                        }

                        if (engine.ecs->entity_has_native_component<KinematicBody>(ith_entity)) {
                            engine.ecs->get_native_component<Transform>(ith_entity).value()->position -= displacement;
                        } else if (engine.ecs->entity_has_native_component<KinematicBody>(jth_entity)) {
                            engine.ecs->get_native_component<Transform>(jth_entity).value()->position += displacement;
                        }
                    }
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

std::optional<std::pair<Entity, vec3>> PhysicsManager::cast_ray(vec3 origin, vec3 direction, Entity excluded) {
    auto it = engine.ecs->query<Entity, GlobalTransform, Body>();

    Entity ret_e = -1;
    f32 ret_t = INFINITY;

    for (auto [entity, gt, body] : it) {
        if (entity == excluded) continue;
        // the colliders are AABB, so we're gonna raycast against those
        // first, we have to transform the ray into model space
        // assuming the ray lives in world space, this is inverse(M) * origin and inverse(N) * direction

        // https://gdbooks.gitbooks.io/3dcollisions/content/Chapter3/raycast_aabb.html

        AABB aabb = body->collider.aabb;
        vec3 aabb_min = aabb.center - aabb.half_size;
        vec3 aabb_max = aabb.center + aabb.half_size;

        vec3 _src = glm::inverse(gt->model) * vec4(origin, 1);
        vec3 _dir = glm::inverse(gt->normal) * direction;

        // compute time of flight for each plane
        vec3 t_min_v = vec3(
                (aabb_min.x - _src.x) / _dir.x,
                (aabb_min.y - _src.y) / _dir.y,
                (aabb_min.z - _src.z) / _dir.z
        );
        vec3 t_max_v = vec3(
                (aabb_max.x - _src.x) / _dir.x,
                (aabb_max.y - _src.y) / _dir.y,
                (aabb_max.z - _src.z) / _dir.z
        );

        // compute min and max on each plane
        vec3 t_min2 = glm::min(t_min_v, t_max_v);
        vec3 t_max2 = glm::max(t_min_v, t_max_v);
        // compute biggest min and smallest max
        f32 t_min = std::max(std::max(t_min2.x, t_min2.y), t_min2.z);
        f32 t_max = std::min(std::min(t_max2.x, t_max2.y), t_max2.z);

        if (t_max < 0) continue;
        if (t_min > t_max) continue;

        f32 t = t_min < 0 ? t_max : t_min;
        if (t < ret_t) {
            ret_t = t;
            ret_e = entity;
        }

        // if (t_min < 0) {
        //     ret_e = entity;
        //     ret_t = t_max;
        //     return std::make_pair(entity, origin + direction * t_max);
        // }
        // return std::make_pair(entity, origin + direction * t_min);
    }
    
    if (ret_e == (Entity)-1) {
        return {};
    } else {
        return std::make_pair(ret_e, origin + direction * ret_t);
    }
}
