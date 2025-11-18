#pragma once

#include <cstddef>

#include "core/memory/types.hpp"

namespace core::memory {

// heap allocator for variable-length allocations
class HeapAllocator {
    MemoryRegion region{ nullptr, nullptr };
    std::size_t offset{ 0 };

public:
    HeapAllocator() = delete;

    HeapAllocator(MemoryRegion region)
        : region(region)
    {}

    template<typename T>
    [[nodiscard]] T* allocate() {
        // containment check
        if(offset + sizeof(T) > region.size()) {
            // todo: log this
            return nullptr;
        }
        const std::size_t current = offset;
        offset += sizeof(T);

        return reinterpret_cast<T*>(region.pStart + current);
    }

    template<typename T>
    [[nodiscard]] T* allocate(std::size_t elements) {
        if(offset + elements * sizeof(T) > region.size()) {
            // todo: log this
            return nullptr;
        }
        const std::size_t current = offset;
        offset += elements * sizeof(T);

        return reinterpret_cast<T*>(region.pStart + current);
    }

    // note: clears all data in the allocator
    void clear() {
        offset = 0;
    }
};

}
