#pragma once

#include <optional>

#include "types.h"

namespace motorcar {
    struct Engine;
    class PhysicsManager {
        Engine& engine;

        public:
            PhysicsManager(Engine& engine);

            std::optional<std::pair<Entity, vec3>> cast_ray(vec3 origin, vec3 direction);
    };
}
