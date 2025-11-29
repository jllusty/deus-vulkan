#pragma once

#include <array>

#include "engine/world/chunk.hpp"

namespace engine::world {

enum class ChunkStatus : core::u32 {
    Unloaded = 0,
    Loading = 1,
    Loaded = 2
};

struct ChunkData {
    // chunk coordinate
    Chunk chunk{};
    // height map
    std::array<core::i16, CHUNK_RESOLUTION * CHUNK_RESOLUTION> heights{};
};

struct ChunkTOC {
    // chunk coordinate
    core::i32 chunkX{};
    core::i32 chunkZ{};
    // file offset
    core::u64 offset{};
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
