// gfx/vulkan/config.hpp:
//     provide methods to create instances, enumerate physical devices, and
//     create logical devices. provide read access to layers/extensions at the
//     instance level and extensions at the device level
#pragma once

#include <cassert>

#include <iostream>
#include <span>

#include "core/memory/stack_allocator.hpp"
#include "core/types.hpp"
#include "core/memory/types.hpp"

#include "core/memory/stack_allocator.hpp"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace gfx::vulkan {

// supports configuring a single vulkan instance
class Configurator {
    core::memory::StackAllocator allocator;

    // available layers and extensions for a VkInstance
    std::span<VkLayerProperties> instanceAvailableLayers{};
    std::span<VkLayerProperties> instanceAvailableExtensions{};

    VkInstance instance{};

    std::span<VkPhysicalDevice> physicalDevices{};
    std::span<VkPhysicalDeviceProperties> physicalDeviceProps{};
    std::span<VkPhysicalDeviceMemoryProperties> physicalDeviceMemoryProps{};

    // a single device can be associated with multiple queues
    std::span<core::memory::ArrayOffset> queueFamilyPropertiesOffsets{};
    std::span<VkQueueFamilyProperties> queueFamilyProperties{};

public:
    Configurator() = delete;

    Configurator(core::memory::Region region)
        : allocator(region)
    {}

    // @TODO: delete
    void setInstance(VkInstance instance) {
        this->instance = instance;
    }

    void enumeratePhysicalDevicesAndQueues() {
        enumeratePhysicalDevices();
        enumeratePhysicalDeviceProperties();
        enumeratePhysicalDeviceMemoryProperties();
        enumerateQueueFamilyProperties();
    }

    std::span<const VkLayerProperties> getAvailableInstanceLayerProperties() {
        if(!instanceAvailableLayers.empty()) {
            return instanceAvailableLayers;
        }

        core::u32 numLayers{ 0 };
        VkResult result = vkEnumerateInstanceLayerProperties(
            &numLayers,
            nullptr
        );

        if(result != VK_SUCCESS) {
            return {};
        }

        assert(false && "come back mr. frodo");
        return {};
    }

    constexpr std::size_t getNumPhysicalDevices() const noexcept {
        return physicalDevices.size();
    }

    constexpr std::span<const VkPhysicalDevice> getPhysicalDevices() const noexcept {
        return physicalDevices;
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

    const VkPhysicalDeviceProperties getPhysicalDeviceProperties(const VkPhysicalDevice& physicalDevice) {
        if(physicalDevices.empty()) {
            assert(false && "no physical devices found to match on\n");
        }

        // what is the queue family properties offset corresponding to that physical device?
        const std::size_t deviceIndex = getPhysicalDeviceIndex(physicalDevice);

        return physicalDeviceProps[deviceIndex];
    }

    const VkPhysicalDeviceMemoryProperties getPhysicalDeviceMemoryProperties(const VkPhysicalDevice& physicalDevice) {
        if(physicalDevices.empty()) {
            assert(false && "no physical devices found to match on\n");
        }

        // what is the queue family properties offset corresponding to that physical device?
        const std::size_t deviceIndex = getPhysicalDeviceIndex(physicalDevice);

        return physicalDeviceMemoryProps[deviceIndex];
    }

    std::span<const VkQueueFamilyProperties> getQueueFamilyProperties(const VkPhysicalDevice& physicalDevice) const {
        if(physicalDevices.empty()) {
            assert(false && "no physical devices found to match on\n");
        }

        const std::size_t deviceIndex = getPhysicalDeviceIndex(physicalDevice);

        // what is the queue family properties offset corresponding to that physical device?
        const std::size_t offset = queueFamilyPropertiesOffsets[deviceIndex].offset;
        const std::size_t length = queueFamilyPropertiesOffsets[deviceIndex].length;

        return queueFamilyProperties.subspan(offset, length);
    }

private:
    // get physical device index
    const std::size_t getPhysicalDeviceIndex(const VkPhysicalDevice& physicalDevice) const {
        std::size_t offset{ 0 };
        bool foundOffset = false;
        for(std::size_t deviceIndex = 0; deviceIndex < physicalDevices.size(); ++deviceIndex) {
            if(physicalDevice == physicalDevices[deviceIndex]) {
                offset = deviceIndex;
                foundOffset = true;
                break;
            }
        }
        if(!foundOffset) {
            assert(false && "no matching device index for the provided physical device");
        }
        return offset;
    }

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
