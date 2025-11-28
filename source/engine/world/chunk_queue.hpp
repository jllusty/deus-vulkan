#pragma once

#include <functional>
#include <queue>
#include <condition_variable>
#include <stop_token>
#include <mutex>

#include "engine/world/chunk.hpp"

namespace engine::world {

class ChunkQueue {
private:
    // todo: ring buffer + atomic
    std::queue<Chunk> jobs{};
    std::mutex mutex{};
    std::condition_variable_any cv{};

public:
    void push(Chunk job) {
        {
            std::lock_guard lock(mutex);
            jobs.push(std::move(job));
        }
        cv.notify_one();
    }

    // return false if stop is requested and there are no chunks to load
    bool pop(Chunk& out, std::stop_token st) {
        std::unique_lock lock(mutex);

        cv.wait(lock, st, [&] {
            return st.stop_requested() || !jobs.empty();
        });

        // stop request + no job
        if(st.stop_requested() && jobs.empty()) {
            return false;
        }

        out = std::move(jobs.front());
        jobs.pop();
        return true;
    }

    void notify_all() {
        cv.notify_all();
    }

};

}
