#pragma once

#include <fstream>
#include <span>

#include "engine/world/chunk.hpp"
#include "engine/world/chunk_data.hpp"

namespace engine::world
{

class Chonker {
    ChunkData chunkData;

public:
    Chonker() {}

    // load all chunks into memory at once, for now
    void generate() noexcept {
        const char * filename = "assets/N40W106.hgt";
        std::ifstream fin(filename, std::ios_base::binary);
    }

    // query chunk
    const ChunkData& getChunk(Chunk chunk) const noexcept {

    }

    // loaded chunks
    std::span<const ChunkData> getChunks() {

    }
};

}
