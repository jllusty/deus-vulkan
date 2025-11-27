#pragma once

#include "core/types.hpp"

namespace engine::world {

struct int2 {
    core::i32 x{ 0 };
    core::i32 y{ 0 };
};

struct float2 {
    float x{ 0 };
    float y{ 0 };
};

constexpr inline float2 operator+(float2 a, float2 b) {
    return {
        .x = a.x + b.x,
        .y = a.y + b.y
    };
}

constexpr inline float2 operator-(float2 a, float2 b) {
    return {
        .x = a.x - b.x,
        .y = a.y - b.y
    };
}

}
