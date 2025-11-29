// chonker.hpp: defines the Chonker class which manages a ChunkPool and ChunkQueue to give
//     rendering logic an easy way to request/fetch chunks to load that will trigger async file reads
//     from worker threads all managed internally to this class
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

    // chunk reading offsets
    std::unordered_map<Chunk, std::size_t, ChunkHash> fileOffsets;

    // worker threads for reading chunks
    std::vector<std::jthread> workers;

public:
    Chonker(const std::size_t chunkPoolCapacity)
        : pool(chunkPoolCapacity)
    {
        // read the fileOffsets
        readOffsets();

        // spawn the chunking system worker threads
        const int num_workers = 1;
        workers.reserve(num_workers);

        for (std::size_t i = 0; i < num_workers; ++i) {
            workers.emplace_back(
                [this](std::stop_token st, std::size_t id){
                    this->worker(st, id);
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

        // no data in negative chunks for now
        // we currently use the chunk coords to map to specific file
        // offsets, but we could something else
        if(c.x < 0 or c.z < 0) {
            return;
        }
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

private:
    // initialize chunk -> file offsets for worker reads
    void readOffsets() noexcept {
        const char * chunkFilename = "assets/N40W106.chunk";
        std::ifstream fin(chunkFilename, std::ios::binary);

        // read number of chunks
        std::uint64_t numChunks{};
        fin.read(reinterpret_cast<char*>(&numChunks), sizeof(numChunks));

        std::cout << "chonker: gonna read " << numChunks << " chunks... \n";

        // read TOC for each entry, store in map
        ChunkTOC chunkTOC{};
        for(std::uint64_t c = 0; c < numChunks; ++c) {
            fin.read(reinterpret_cast<char*>(&chunkTOC), sizeof(chunkTOC));
            Chunk chunk {
                .x = chunkTOC.chunkX,
                .z = chunkTOC.chunkZ
            };
            fileOffsets[chunk] = chunkTOC.offset;
        }
    }

    // worker thread function (called from lambda)
    void worker(std::stop_token st, std::size_t workerThreadID) noexcept {
        Chunk c{};
        // wait until we are notified to pop a chunk off the queue
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

            const char * filename = "assets/N40W106.chunk";
            std::ifstream fin(filename, std::ios_base::binary);

            // set offset to chunk coordinate
            std::size_t offset = fileOffsets.at(data.chunk);
            fin.seekg(offset, std::ios::beg);
            // read all data into the chunk
            fin.read(reinterpret_cast<char*>(data.heights.data()), sizeof(int16_t) * data.heights.size());

            // mark chunk c fully loaded
            pool.setChunkStatus(c, ChunkStatus::Loaded);
        }
        printf("Worker %lu exiting\n", workerThreadID);
    }
};

}
