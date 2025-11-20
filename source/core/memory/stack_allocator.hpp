#pragma once

#include <cassert>
#include <cstddef>

#include "core/memory/types.hpp"

namespace core::memory {

// stack allocator for variable-length allocations
class StackAllocator {
    Region region{};

    // byte offset
    std::size_t offset{ 0 };

public:
    StackAllocator() = delete;

    StackAllocator(Region region)
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

        return reinterpret_cast<T*>(region.data() + current);
    }

    template<typename T>
    [[nodiscard]] std::span<T> allocate(std::size_t elements) {
        if(offset + elements * sizeof(T) > region.size()) {
            // todo: log this
            assert(false && "cannot reserve enough space");
        }
        const std::size_t current = offset;
        offset += elements * sizeof(T);

        return std::span<T>(
            reinterpret_cast<T*>(region.data() + current),
            elements
        );
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
