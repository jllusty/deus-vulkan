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
    // status
    // todo: list of atomic<bool> in the ChunkPool instead
    ChunkStatus status{ ChunkStatus::Unloaded };
    // height map (should be u16, but using i32 for testing)
    std::array<core::i32, CHUNK_RESOLUTION * CHUNK_RESOLUTION> heights{};
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
