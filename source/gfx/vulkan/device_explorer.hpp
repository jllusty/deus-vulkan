// gfx/vulkan/describe.hpp: describe all of our instances, devices, and
//     queues available on this device with a PhysicalDeviceExplorer
#pragma once

#include <cassert>

#include "core/memory/stack_allocator.hpp"
#include "core/types.hpp"
#include "core/memory/types.hpp"

#include "core/memory/stack_allocator.hpp"

#include <vulkan/vulkan.h>

namespace gfx::vulkan {

class PhysicalDeviceEnumerator {
    core::memory::StackAllocator allocator;

    VkPhysicalDevice* pPhysicalDevices{ nullptr };

public:
    PhysicalDeviceEnumerator() = delete;

    PhysicalDeviceEnumerator(VkInstance instance, core::memory::MemoryRegion region)
        : allocator(region)
    {
        // get physical devices
        core::u32 numPhysicalDevices{ 0 };
        VkResult enumeratePhysicalDevicesResult = vkEnumeratePhysicalDevices(
            instance,
            &numPhysicalDevices,
            nullptr
        );

        if(enumeratePhysicalDevicesResult != VK_SUCCESS) {
            // todo: log this
            assert(false && "[vulkan/device_explorer]: could not enumerate physical devices for provided instance");
        }
        if(numPhysicalDevices == 0) {
            // todo: log this
            assert(false && "[vulkan/device_explorer]: zero vulkan physical devices for provided instance");
        }

        pPhysicalDevices = allocator.allocate<VkPhysicalDevice>(numPhysicalDevices);

        enumeratePhysicalDevicesResult = vkEnumeratePhysicalDevices(
            instance,
            &numPhysicalDevices,
            pPhysicalDevices
        );

        if(enumeratePhysicalDevicesResult != VK_SUCCESS) {
            // todo: log this
            assert(false && "[vulkan/device_explorer]: could not enumerate physical devices for provided instance");
        }
    }
};

}
