#pragma once

#include <cassert>

#include <iostream>
#include <fstream>
#include <thread>

#include "engine/world/chunk.hpp"
#include "engine/world/chunk_data.hpp"
#include "engine/world/chunk_pool.hpp"
#include "engine/world/chunk_queue.hpp"

namespace engine::world
{

class Chonker {
    // chunk pool arena allocator, with a loaded list
    ChunkPool pool;
    // async pub/sub queue for worker threads
    ChunkQueue queue;

    // worker threads for reading chunks
    std::vector<std::jthread> workers;

public:
    Chonker(const std::size_t chunkPoolCapacity)
        : pool(chunkPoolCapacity)
    {
        // spawn the chonker threads
        const int num_workers = 1;
        workers.reserve(num_workers);

        for (int i = 0; i < num_workers; ++i) {
            workers.emplace_back(
                [this](std::stop_token st, int id){
                    Chunk c{};
                    while (this->queue.pop(c, st)) {
                        // load chunk c
                        printf("chonker: chunk (%d,%d) requested\n",c.x,c.z);
                        // get reference from thread pool
                        std::optional<std::size_t> poolIndexOpt = pool.getPoolIndex(c);
                        if(!poolIndexOpt.has_value()) {
                            // todo: very bad
                            assert(false && "very bad");
                        }
                        std::size_t poolIndex = *poolIndexOpt;

                        ChunkData& data = pool.getChunkData(poolIndex);

                        data.heights[0] = c.x;
                        data.heights[1] = c.z;

                        //const char * filename = "assets/N40W106.hgt";
                        //std::ifstream fin(filename, std::ios_base::binary);

                        // mark chunk c fully loaded
                        pool.setChunkStatus(c, ChunkStatus::Loaded);
                    }
                    std::cout << "Worker " << id << " exiting\n";
                },
                i
            );
        }
    }

    ~Chonker() {
        // stop all
        queue.notify_all();
        // wake any sleeping threads
        for (auto& w : workers) {
            w.request_stop();
        }
    }

    void request(Chunk c) noexcept {
        // todo: check if we've already loaded it

        // allocate space in the pool arena allocator, init ChunkData
        pool.request(c);
        queue.push(c);
    }

    ChunkStatus getStatus(Chunk c) noexcept {
        return pool.getChunkStatus(c);
    }

    ChunkData* fetch(Chunk c) noexcept {
        // check that this chunk has valid loaded ChunkData
        if(getStatus(c) == ChunkStatus::Unloaded) {
            return nullptr;
        }
        // get the pool index
        std::size_t poolIndex = *pool.getPoolIndex(c);
        // return ptr into the pool
        // todo: safety of returning a ptr into an std::vector?
        return &pool.getChunkData(poolIndex);
    }

    void generate() noexcept {
        const char * filename = "assets/N40W106.hgt";
        std::ifstream fin(filename, std::ios_base::binary);
    }
};

}
