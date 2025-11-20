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
    os::OsAddressSpaceAllocator allocator{};
    os::OsAddressSpace addr{};

    // bump index
    std::size_t offset{ 0 };

public:
    // do not instantiate without a specified size
    BaseAllocator() = delete;

    // bytesRequested: the os layer allocator will request full pages and always
    //     give at least that much memory
    BaseAllocator(std::size_t bytesRequested)
        : addr(allocator.reserve(bytesRequested))
    {}

    ~BaseAllocator() {
        // free the address space back to the OS
        allocator.release(addr);
    }

    std::size_t getBytesAllocated() const {
        return addr.size();
    }

    Region reserve(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t)) {
        std::size_t current = offset;

        // move current offset to next alignment boundary
        std::size_t aligned = (current + alignment - 1) & ~(alignment - 1);

        // containment check
        if(aligned + bytes > addr.size()) {
            // todo: handle this
            assert(false && "BaseAllocator: out of memory");
        }

        // move offset to next region of free memory after this allocation
        offset = aligned + bytes;
        return addr.subspan(aligned, offset);
    }
};

}
