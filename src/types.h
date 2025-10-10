#pragma once
#include "glm/glm.hpp"
#include <glm/gtc/quaternion.hpp>
#include <cstdint>
#include <string>

namespace motorcar {
    typedef uint8_t u8;

    typedef uint32_t u32;
    typedef uint64_t u64;

    typedef int32_t i32;
    typedef int64_t i64;

    // WARN: The C standard does *not* guarantee these are 32-bit and 64-bit.j
    // C++23 has <stdfloat> for this, but we're not writing in C++23, are we?
    typedef float f32;
    typedef double f64;

    typedef glm::vec2 vec2;
    typedef glm::vec3 vec3;
    typedef glm::vec4 vec4;
    typedef glm::quat quat;

    typedef glm::mat2 mat2;
    typedef glm::mat3 mat3;
    typedef glm::mat4 mat4;

    typedef size_t Entity;
}
