#pragma once

#include <span>
#include <cstddef>

namespace core::memory {

// read/write regions of operation for subsystem allocators
//
// note that we do not free this memory individually, you ask for it once upfront
// and have it for the lifetime of the program for subsystem carving
//
// core::memory::BaseAllocator returns these on allocation calls
using Region = std::span<std::byte>;

// useful for indexing arrays of type T and a given length
struct ArrayOffset {
    std::size_t offset{ 0 };
    std::size_t length{ 0 };
};

}
