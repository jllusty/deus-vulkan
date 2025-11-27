#pragma once

#include <algorithm>

#include "engine/world/types.hpp"

namespace engine::world {

// size of chunks in world space
constexpr const core::i32 CHUNK_SIZE = 16;

// sample resolution of chunks
constexpr const core::i32 CHUNK_RESOLUTION = 17;

// chunk coordinate
struct Chunk {
    core::i32 x{ 0 };
    core::i32 z{ 0 };

    bool operator==(const Chunk& other) const noexcept {
        return (x == other.x) and (z == other.z);
    }
};

// local coordinates inside chunk
struct ChunkLocal {
    Chunk chunk{};
    float2 local{};
};

// horizontal world space (x,...,z) -> chunk coordinate
inline Chunk worldPositionXZToChunk(float2 worldPositionXZ) noexcept {
    return {
        .x = static_cast<core::i32>(std::floor(worldPositionXZ.x / CHUNK_SIZE)),
        .z = static_cast<core::i32>(std::floor(worldPositionXZ.y / CHUNK_SIZE))
    };
}


// chunk coordinate -> origin of chunk in horizontal world space (x,y,...)
inline float2 chunkToWorldPositionXZ(Chunk chunk) noexcept {
    return {
        .x = static_cast<float>(chunk.x * CHUNK_SIZE),
        .y = static_cast<float>(chunk.z * CHUNK_SIZE)
    };
}

inline ChunkLocal worldPositionXZToChunkLocal(float2 worldPositionXZ) noexcept {
    const Chunk chunk = worldPositionXZToChunk(worldPositionXZ);
    const float2 origin = chunkToWorldPositionXZ(chunk);
    return {
        .chunk = chunk,
        .local = worldPositionXZ - origin
    };
}

inline float2 chunkLocalPositionToWorldPositionXZ(ChunkLocal chunkLocal) noexcept {
    return chunkToWorldPositionXZ(chunkLocal.chunk) + chunkLocal.local;
}

inline int2 chunkLocalPositionToSample(float2 chunkLocalPositionXZ) noexcept {
    float spacing = static_cast<float>(CHUNK_SIZE) / static_cast<float>(CHUNK_RESOLUTION - 1);
    return {
        .x = std::clamp(static_cast<core::i32>(chunkLocalPositionXZ.x / spacing), 0, CHUNK_RESOLUTION - 1),
        .y = std::clamp(static_cast<core::i32>(chunkLocalPositionXZ.y / spacing), 0, CHUNK_RESOLUTION - 1)
    };
}

}
