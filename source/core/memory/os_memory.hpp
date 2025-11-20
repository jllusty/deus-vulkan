// os_memory.hpp: basic layer over OS-specific functions for requesting virtual memory

#pragma once

#include <cassert>
#include <cstddef>

#include <span>

// posix
#include <sys/mman.h>
#include <unistd.h>

namespace core::memory::os {

// read/write span
using OsAddressSpace = std::span<std::byte>;

class OsAddressSpaceAllocator {
    std::size_t page_size { 0 };

    // query sysconf to get page size
    [[nodiscard]] std::size_t queryPageSize() const {
        const long rc = ::sysconf(_SC_PAGESIZE);
        if(rc == -1) {
            // todo: handle error
            assert(false && "sysconf failure: cannot get pagesize");
        }

        return static_cast<std::size_t>(rc);
    }

public:
    OsAddressSpaceAllocator()
        : page_size(queryPageSize())
    {}

    // Condition: rounds requested number of bytes up to a multiple of page size
    [[nodiscard]] OsAddressSpace reserve(std::size_t bytesRequested) const {
        // round up to next page boundary
        const std::size_t ps = page_size;
        const std::size_t bytesRounded = (bytesRequested + ps - 1) & ~(ps - 1);
        void* pBase = ::mmap(
            nullptr,                        // void addr[]
            bytesRounded,                   // size_t length
            PROT_READ | PROT_WRITE,         // int prot
            MAP_PRIVATE | MAP_ANONYMOUS,    // int flags
            -1,                             // int fd
            0                               // off_t offset
        );
        // note: fd must be -1 in some implementations for MAP_ANONYMOUS

        if(pBase == MAP_FAILED) {
            // todo: handle this (query errno)
            assert(false && "mmap failed in os_reserve_memory");
        }

        return std::span<std::byte>(static_cast<std::byte*>(pBase), bytesRounded);
    }

    void release(OsAddressSpace addressSpace) const {
        if(addressSpace.data() != nullptr) {
            ::munmap(static_cast<void*>(addressSpace.data()), addressSpace.size());
        }
        else {
            // todo: handle this
            assert(false && "attempt to free nullptr");
        }
    }
};

}
