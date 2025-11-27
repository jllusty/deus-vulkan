#pragma once

#include "core/memory/types.hpp"

namespace core::memory {

// bump allocator for fixed type
template<typename T>
class ArenaAllocator {
    Region region{};
    std::size_t offset{ 0 };

public:
    ArenaAllocator() {}

    ArenaAllocator(Region region)
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

        return reinterpret_cast<T*>(region.data() + current);
    }

    [[nodiscard]] std::span<T> allocate(std::size_t elements) {
        if(offset + elements > region.size()) {
            // todo: log this
            return {};
        }
        const std::size_t current = offset;
        offset += elements;

        return std::span<T>(
            reinterpret_cast<T*>(region.data() + current),
            elements
        );
    }

    // note: clears all data in the allocator
    void clear() {
        offset = 0;
    }
};

}
