// base_allocator.hpp: layer over the os memory layer, used to request regions to implement other
//     subsystem allocators on
#pragma once

#include "core/memory/os_memory.hpp"
#include "core/memory/types.hpp"

#include <cstddef>

namespace core::memory {

// root linear / bump allocator for subsystems
//
// uniqueness: there should only be one of these lying around
class BaseAllocator {
    // os layer
    OsArenaAllocator allocator{};
    OsArena arena{ nullptr, 0 };

    // bump index
    std::size_t offset{ 0 };

public:
    // do not instantiate without a specified size
    BaseAllocator() = delete;

    // bytesRequested: the os layer allocator will request full pages and always
    //     give at least that much memory
    BaseAllocator(std::size_t bytesRequested)
        : arena(allocator.reserve(bytesRequested))
    {}

    ~BaseAllocator() {
        allocator.release(arena);
    }

    std::size_t getBytesAllocated() const {
        return arena.bytes;
    }

    MemoryRegion reserve(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t)) {
        std::size_t current = offset;

        // move current offset to next alignment boundary
        std::size_t aligned = (current + alignment - 1) & ~(alignment - 1);

        // containment check
        if(aligned + bytes > arena.bytes) {
            // todo: handle this
            assert(false && "BaseAllocator: out of memory");
        }

        // move offset to next region of free memory after this allocation
        offset = aligned + bytes;

        return { arena.pBase + aligned, arena.pBase + offset};
    }
};

}
