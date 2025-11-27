#pragma once

#include <array>

#include "engine/world/chunk.hpp"

namespace engine::world {

struct ChunkData {
    Chunk chunk{};

    std::array<float, CHUNK_RESOLUTION * CHUNK_RESOLUTION> heights{};
};

inline float sampleChunkDataHeights(ChunkData& chunkData, int2 sampleCoords) {
    return chunkData.heights[
        sampleCoords.y * CHUNK_RESOLUTION + sampleCoords.x
    ];
}

struct ChunkHash {
    std::size_t operator()(const Chunk& chunk) const noexcept {
        return (static_cast<std::size_t>(chunk.x) << 32) | static_cast<std::size_t>(chunk.z);
    }
};

}
