#pragma once

#include "core/memory/types.hpp"

namespace core::memory {

// bump allocator for fixed size
template<typename T>
class ArenaAllocator {
    MemoryRegion region{ nullptr, nullptr };
    std::size_t offset{ 0 };

public:
    ArenaAllocator() = delete;

    ArenaAllocator(MemoryRegion region)
        : region(region)
    {}

    [[nodiscard]] T* allocate() {
        // containment check
        if(offset + 1 > region.size()) {
            // todo: log this
            return nullptr;
        }
        const std::size_t current = offset;
        offset += 1;

        return reinterpret_cast<T*>(region.pStart + current);
    }

    [[nodiscard]] T* allocate(std::size_t elements) {
        if(offset + elements > region.size()) {
            // todo: log this
            return nullptr;
        }
        const std::size_t current = offset;
        offset += elements;

        return reinterpret_cast<T*>(region.pStart + current);
    }

    // note: clears all data in the allocator
    void clear() {
        offset = 0;
    }
};

}
