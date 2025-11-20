// gfx/vulkan/describe.hpp: describe all of our instances, devices, and
//     queues available on this device
#pragma once

#include <cassert>

#include <iostream>
#include <iterator>
#include <span>

#include "core/memory/stack_allocator.hpp"
#include "core/types.hpp"
#include "core/memory/types.hpp"

#include "core/memory/stack_allocator.hpp"

#include <vulkan/vulkan.h>

// associate each VkPhysicalDevice with the offsets corresponding to
// their properties


namespace gfx::vulkan {

class PhysicalDeviceEnumerator {
    core::memory::StackAllocator allocator;

    VkInstance instance{};

    std::span<VkPhysicalDevice> physicalDevices{};
    std::span<VkPhysicalDeviceProperties> physicalDeviceProps{};
    std::span<VkPhysicalDeviceMemoryProperties> physicalDeviceMemoryProps{};

    std::span<core::memory::ArrayOffset> queueFamilyPropertiesOffsets{};
    std::span<VkQueueFamilyProperties> queueFamilyProperties{};

public:
    PhysicalDeviceEnumerator() = delete;

    PhysicalDeviceEnumerator(VkInstance instance, core::memory::Region region)
        : instance(instance), allocator(region)
    {
        enumeratePhysicalDevices();
        enumeratePhysicalDeviceProperties();
        enumeratePhysicalDeviceMemoryProperties();
        enumerateQueueFamilyProperties();
    }

    constexpr std::size_t getNumPhysicalDevices() const noexcept {
        return physicalDevices.size();
    }

    // methods for querying devices and or device/queue properties
    // todo: pick a device based on optional and required features
    const VkPhysicalDevice getBestPhysicalDevice() const {
        if(physicalDevices.empty()) {
            assert(false && "no physical devices found\n");
        }
        // just return the first one for now
        return physicalDevices.front();
    }

    std::span<const VkQueueFamilyProperties> getQueueFamilyProperties(const VkPhysicalDevice& physicalDevice) const {
        if(physicalDevices.empty()) {
            assert(false && "no physical devices found to match on\n");
        }

        // what is the queue family properties offset corresponding to that physical device?
        std::size_t offset{ 0 }, length{ 0 };
        for(std::size_t deviceIndex = 0; deviceIndex < physicalDevices.size(); ++deviceIndex) {
            if(physicalDevice == physicalDevices[deviceIndex]) {
                offset = queueFamilyPropertiesOffsets[deviceIndex].offset;
                length = queueFamilyPropertiesOffsets[deviceIndex].length;
            }
        }

        return queueFamilyProperties.subspan(offset, length);
    }

private:
    // todo: these should be called "initialize / query" instead of enumerate, probably
    void enumeratePhysicalDevices() {
        // enumerate physical devices
        core::u32 numPhysicalDevices = 0;
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

        physicalDevices = allocator.allocate<VkPhysicalDevice>(numPhysicalDevices);

        enumeratePhysicalDevicesResult = vkEnumeratePhysicalDevices(
            instance,
            &numPhysicalDevices,
            physicalDevices.data()
        );

        if(enumeratePhysicalDevicesResult != VK_SUCCESS) {
            // todo: log this
            assert(false && "[vulkan/device_explorer]: could not enumerate physical devices for provided instance");
        }
        if(physicalDevices.empty()) {
            assert(false && "[vulkan/device_explorer]: vulkan did not return any physical device handles");
        }

        std::cout << "[vulkan/device_explorer]: found " << numPhysicalDevices << " physical devices\n";
    }

    void enumeratePhysicalDeviceProperties() {
        // get physical device properties
        physicalDeviceProps = allocator.allocate<VkPhysicalDeviceProperties>(physicalDevices.size());
        for(std::size_t deviceIndex = 0; deviceIndex < physicalDevices.size(); ++deviceIndex) {
            const VkPhysicalDevice& physicalDevice = physicalDevices[deviceIndex];

            vkGetPhysicalDeviceProperties(
                physicalDevice,
                &physicalDeviceProps[deviceIndex]
            );

            const VkPhysicalDeviceProperties& props = physicalDeviceProps[deviceIndex];

            core::u32 major = VK_API_VERSION_MAJOR(props.apiVersion);
            core::u32 minor = VK_API_VERSION_MINOR(props.apiVersion);

            // todo: log more attr
            std::cout << "[vulkan/device_explorer]: got device properties: " <<
                "\n\tdeviceName: " << props.deviceName <<
                "\n\tapiVersion: " << major << "." << minor <<
                "\n\tdeviceID: " << props.deviceID <<
                "\n\tdeviceType: " << props.deviceType <<
                "\n\tdriverVersion: " << props.driverVersion << "\n";
        }
    }

    void enumeratePhysicalDeviceMemoryProperties() {
        physicalDeviceMemoryProps = allocator.allocate<VkPhysicalDeviceMemoryProperties>(physicalDevices.size());
        for(std::size_t deviceIndex = 0; deviceIndex < physicalDevices.size(); ++deviceIndex) {
            const VkPhysicalDevice& physicalDevice = physicalDevices[deviceIndex];
            vkGetPhysicalDeviceMemoryProperties(
                physicalDevice,
                &physicalDeviceMemoryProps[deviceIndex]
            );

            const VkPhysicalDeviceMemoryProperties& props = physicalDeviceMemoryProps[deviceIndex];

            // todo: list memory heap types
            std::cout << "got device memory properties:\n" <<
                "\t memoryTypeCount: " << props.memoryTypeCount << "\n"
                "\t memoryHeapCount: " << props.memoryHeapCount << "\n";
        }
    }

    void enumerateQueueFamilyProperties() {
        // prefetch: get the total size
        std::size_t totalNumQueueFamilyProperties{ 0 };
        for(std::size_t deviceIndex = 0; deviceIndex < physicalDevices.size(); ++deviceIndex) {
            core::u32 numQueueFamilyProperties{ 0 };
            vkGetPhysicalDeviceQueueFamilyProperties(
                physicalDevices[deviceIndex],
                &numQueueFamilyProperties,
                nullptr
            );
            totalNumQueueFamilyProperties += static_cast<std::size_t>(numQueueFamilyProperties);
        }

        // allocate index array header
        queueFamilyPropertiesOffsets = allocator.allocate<core::memory::ArrayOffset>(physicalDevices.size());

        // allocate actual space for the family properties
        queueFamilyProperties = allocator
            .allocate<VkQueueFamilyProperties>(totalNumQueueFamilyProperties);

        core::u32 currentNumQueueFamilyProperties{ 0 };
        for(std::size_t deviceIndex = 0; deviceIndex < physicalDevices.size(); ++deviceIndex) {
            const VkPhysicalDevice& physicalDevice = physicalDevices[deviceIndex];

            // populate index array and allocator
            core::u32 numQueueFamilyProperties{ 0 };
            vkGetPhysicalDeviceQueueFamilyProperties(
                physicalDevice,
                &numQueueFamilyProperties,
                nullptr
            );

            std::cout << "[vulkan/device_explorer]: there are " << numQueueFamilyProperties
                << " queues on the physical device " << deviceIndex;

            // set header offset and compute write location
            core::memory::ArrayOffset& arrayOffset = queueFamilyPropertiesOffsets[deviceIndex];

            arrayOffset.offset = currentNumQueueFamilyProperties;
            arrayOffset.length = numQueueFamilyProperties;

            VkQueueFamilyProperties& familyProps = queueFamilyProperties[currentNumQueueFamilyProperties];

            vkGetPhysicalDeviceQueueFamilyProperties(
                physicalDevice,
                &numQueueFamilyProperties,
                &familyProps
            );

            // increment count
            currentNumQueueFamilyProperties += numQueueFamilyProperties;
        }
    }
};

}
