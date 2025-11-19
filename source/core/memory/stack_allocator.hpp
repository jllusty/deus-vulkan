#pragma once

#include <cstddef>

#include "core/memory/types.hpp"

namespace core::memory {

// stack allocator for variable-length allocations
class StackAllocator {
    MemoryRegion region{ nullptr, nullptr };
    // byte offset
    std::size_t offset{ 0 };

public:
    StackAllocator() = delete;

    StackAllocator(MemoryRegion region)
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

    constexpr std::size_t size() const {
        return offset;
    }

    // note: clears all data in the allocator
    void clear() {
        offset = 0;
    }
};

}
