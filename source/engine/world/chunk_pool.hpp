#pragma once

#include <atomic>
#include <optional>
#include <span>
#include <vector>
#include <unordered_map>

#include "engine/world/chunk_data.hpp"

namespace engine::world {

class ChunkPool {
    // chunk pool
    std::vector<ChunkData> pool{};

    // loaded
    std::vector<std::atomic<ChunkStatus>> status{};

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
        : pool(capacity),
          status(capacity),
          loadedIndex(capacity)
    {
        // set all chunks unloaded
        for(std::size_t i = 0; i < status.size(); ++i) {
            status[i] = ChunkStatus::Unloaded;
        }
        // we can at most load all of our chunks
        loaded.reserve(capacity);

        // populate loadable with indices into the pool
        loadable.reserve(capacity);
        for(std::size_t i = 0; i < pool.size(); ++i) {
            loadable.push_back(i);
        }
    }

    // load: request index to ChunkData pool for later reads
    void request(Chunk chunk) {
        // out of space?
        // todo: eviction notice
        if(loadable.empty()) {
            return;
        }
        std::size_t poolIndex = loadable.back();
        loadable.pop_back();

        // insert pool index into hash by chunk coordinates
        chunkToLoaded[chunk] = poolIndex;

        // update loaded list of chunks and
        loadedIndex[poolIndex] = loaded.size();
        loaded.push_back(poolIndex);

        // update status to loading
        status[poolIndex].store(ChunkStatus::Loading, std::memory_order_release);
        return;
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

        // update status to unloaded
        status[poolIndex].store(ChunkStatus::Unloaded, std::memory_order_release);
    }

    std::span<const std::size_t> getRequestedChunkIds() const noexcept {
        return loaded;
    }

    ChunkStatus getChunkStatus(Chunk chunk) const noexcept {
        // if we haven't loaded the chunk yet, just return unloaded
        if(!chunkToLoaded.contains(chunk)) {
            return ChunkStatus::Unloaded;
        }
        std::size_t poolIndex = chunkToLoaded.at(chunk);
        return status[poolIndex].load(std::memory_order_acquire);
    }

    void setChunkStatus(Chunk chunk, ChunkStatus s) noexcept {
        if(!chunkToLoaded.contains(chunk)) {
            return;
        }
        std::size_t poolIndex = chunkToLoaded.at(chunk);
        status[poolIndex].store(s, std::memory_order_release);
    }

    std::optional<std::size_t> getPoolIndex(Chunk chunk) const noexcept {
        if(chunkToLoaded.contains(chunk)) {
            return chunkToLoaded.at(chunk);
        }
        return std::nullopt;
    }

    ChunkData& getChunkData(std::size_t poolIndex) noexcept {
        return pool[poolIndex];
    }
};

}
