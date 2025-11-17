#pragma once

#include <cstddef>

namespace core::memory {

// read/write regions of operation for subsystem allocators
// half open intervals: [MemoryRegion.pStart, MemoryRegion.pEnd)
//
// note that we do not free this memory individually, you ask for it once upfront
// and have it for the lifetime of the program for subsystem carving
//
// core::memory::BaseAllocator returns these on allocation calls
struct MemoryRegion {
    std::byte* pStart;
    std::byte* pEnd;

    [[nodiscard]] constexpr std::size_t size() const noexcept {
        return static_cast<std::size_t>(pEnd - pStart);
    }

    [[nodiscard]] constexpr bool contains(std::byte* ptr) const noexcept {
        return (pStart <= ptr) && (ptr < pEnd);
    }
};

}
