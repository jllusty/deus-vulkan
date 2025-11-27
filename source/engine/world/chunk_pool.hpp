#pragma once

#include <span>
#include <vector>
#include <unordered_map>

#include "engine/world/chunk_data.hpp"

namespace engine::world {

class ChunkPool {
    // chunk pool
    std::vector<ChunkData> pool{};

    // free stack, filled with indices into the chunk pool at init
    std::vector<std::size_t> loadable{};

    // chunk coords -> chunk pool index
    std::unordered_map<Chunk, std::size_t, ChunkHash> chunkToLoaded{};

    // pool indices that are loaded
    std::vector<std::size_t> loaded{};

    // pool index -> location in loaded
    std::vector<std::size_t> loadedIndex{};

public:
    ChunkPool(const std::size_t capacity)
    {
        // pool of chunk data
        pool.resize(capacity);
        // pool index -> loaded list
        loadedIndex.resize(capacity);
        // we can at most load all of our chunks
        loaded.reserve(capacity);

        // populate loadable with indices into the pool
        loadable.reserve(capacity);
        for(std::size_t i = 0; i < pool.size(); ++i) {
            loadable.push_back(i);
        }
    }

    // load: request pointer to ChunkData for a write
    ChunkData* load(Chunk chunk) {
        // already loaded?
        if(chunkToLoaded.contains(chunk)) {
            return &pool[chunkToLoaded[chunk]];
        }

        // out of space?
        // todo: eviction notice
        if(loadable.empty()) {
            return nullptr;
        }
        std::size_t poolIndex = loadable.back();
        loadable.pop_back();

        // insert pool index into hash by chunk coordinates
        chunkToLoaded[chunk] = poolIndex;

        // update loaded list of chunks and
        loadedIndex[poolIndex] = loaded.size();
        loaded.push_back(poolIndex);

        ChunkData& chunkData = pool[poolIndex];
        chunkData.chunk = chunk;
        // chunkData.heights = ...
        return &chunkData;
    }

    void unload(Chunk chunk) {
        if(!chunkToLoaded.contains(chunk)) {
            // todo: log
            return;
        }
        std::size_t poolIndex = chunkToLoaded[chunk];
        std::size_t ldIndex = loadedIndex[poolIndex];

        // delist from the chunk coord -> pool index mapping
        chunkToLoaded.erase(chunk);

        // swap this chunk to be unloaded with the last loaded chunk
        // in the loaded list, so that we can pop it off
        std::size_t prevIndex = loaded.back();
        std::swap(loaded[ldIndex], loaded[prevIndex]);
        loaded.pop_back();

        // fix the pool index -> loaded list index for the swapped chunk
        loadedIndex[prevIndex] = ldIndex;

        // add the unloaded chunk pool index into the loadable pool indices
        loadable.push_back(poolIndex);
    }

    // todo: stream methods
};

}
