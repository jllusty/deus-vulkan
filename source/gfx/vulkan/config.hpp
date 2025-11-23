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
    std::span<VkExtensionProperties> instanceAvailableExtensions{};

    VkInstance instance{};

    std::span<VkPhysicalDevice> physicalDevices{};
    std::span<VkPhysicalDeviceProperties> physicalDeviceProps{};
    std::span<VkPhysicalDeviceMemoryProperties> physicalDeviceMemoryProps{};

    // a single device can be associated with multiple queues
    std::span<core::memory::ArrayOffset> queueFamilyPropertiesOffsets{};
    std::span<VkQueueFamilyProperties> queueFamilyProperties{};

    // logging
    core::log::Logger* pLogger{ nullptr };

public:
    Configurator() = delete;

    Configurator(core::memory::Region region, core::log::Logger* pLogger = nullptr)
        : allocator(region), pLogger(pLogger)
    {

    }

    // note that we do not return optional here
    // there is no way in vulkan 1.0 to even ask if 1.0 is supported
    // we just have to infer based on the lack of vkEnumerateInstanceVersion
    // which was introduced in 1.0
    std::optional<const core::u32> getAvailableInstanceVersion() noexcept {
        // we know the signture of vkEnumerateInstanceVersion, should it exist
        PFN_vkVoidFunction vkEnumerateInstanceVersionPtr = vkGetInstanceProcAddr(
            nullptr,
            "vkEnumerateInstanceVersion"
        );

        if(vkEnumerateInstanceVersionPtr != nullptr) {
            core::u32 version{ 0 };
            VkResult result =
                reinterpret_cast<VkResult(VKAPI_PTR*)(core::u32*)>
                (vkEnumerateInstanceVersionPtr)(&version);

            if(result != VK_SUCCESS) {
                logError("[vulkan/configurator]: could not retrieve vulkan version >= 1.1");
                return std::nullopt;
            }

            return version;
        }

        // we must assume that if we did not get a valid function pointer from vulkan on
        // the first command call, it is because that function is not defined and we
        // are working with vulkan 1.0
        return VK_VERSION_1_0;
    }

    std::span<const VkLayerProperties> getAvailableInstanceLayerProperties() noexcept {
        if(!instanceAvailableLayers.empty()) {
            return instanceAvailableLayers;
        }

        core::u32 numLayers{ 0 };
        VkResult result = vkEnumerateInstanceLayerProperties(
            &numLayers,
            nullptr
        );

        if(result != VK_SUCCESS || numLayers == 0) {
            logError("[vulkan/configurator]: could not get any available instance layers");
            return {};
        }

        instanceAvailableLayers = allocator.allocate<VkLayerProperties>(numLayers);

        result = vkEnumerateInstanceLayerProperties(
            &numLayers,
            instanceAvailableLayers.data()
        );

        if(result != VK_SUCCESS || instanceAvailableLayers.empty()) {
            logError("[vulkan/configurator]: could not get any available instance layers");
        }

        return instanceAvailableLayers;
    }

    std::span<const VkExtensionProperties> getAvailableInstanceExtensionProperties() noexcept {
        if(!instanceAvailableExtensions.empty()) {
            return instanceAvailableExtensions;
        }

        core::u32 numExtensions{ 0 };
        VkResult result = vkEnumerateInstanceExtensionProperties(
            nullptr,
            &numExtensions,
            nullptr
        );

        if(result != VK_SUCCESS || numExtensions == 0) {
            logError("[vulkan/configurator]: could not get any available instance extensions");
            return {};
        }

        instanceAvailableExtensions = allocator.allocate<VkExtensionProperties>(numExtensions);

        result = vkEnumerateInstanceExtensionProperties(
            nullptr,
            &numExtensions,
            instanceAvailableExtensions.data()
        );

        if(result != VK_SUCCESS || instanceAvailableLayers.empty()) {
            logError("[vulkan/configurator]: could not get any available instance layers");
        }

        return instanceAvailableExtensions;
    }

    [[nodiscard]] std::optional<const VkInstance> createInstance(
        const char* applicationName,
        const char* engineName,
        const core::u32 apiVersion,
        std::span<const VkLayerProperties> layerProperties,
        std::span<const VkExtensionProperties> extensionProperties)
    {
        VkApplicationInfo appInfo{
            VK_STRUCTURE_TYPE_APPLICATION_INFO,
            nullptr,
            applicationName,
            0,
            engineName,
            apiVersion
        };

        // vulkan instance create info

        // Layers: []
        const core::u32 numLayers{ 0 };
        const char* const* ppLayerNames { nullptr };
        // Optional Extensions: [VK_KHR_portability_enumeration]
        //
        // 1. VK_KHR_portability_enumeration: used for Mac OS X development environment.
        // -> enumerate available Vulkan Portability-compliant physical devices and groups
        // in addition to the Vulkan physical devices and groups that are enumerated by default.
        const core::u32 numExtensions{ 1 };
        const char* const ppExtensionNames[] { VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME };
        const VkInstanceCreateInfo createInfo {
            VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,             // sType
            nullptr,                                            // pNext
            VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR,   // flags
            &appInfo,                                           // pApplicationInfo
            numLayers,                                          // enabledLayerCount
            ppLayerNames,                                       // ppEnabledLayerNames
            numExtensions,                                      // enabledExtensionCount
            ppExtensionNames                                    // ppEnabledExtensionNames
        };

        // Create Vulkan Instance
        const VkAllocationCallbacks* pHostMemoryAllocator = nullptr; // use Vulkan's internal allocator
        VkInstance vulkanInstance;
        VkResult res = vkCreateInstance(
            &createInfo,
            pHostMemoryAllocator,
            &vulkanInstance
        );

        if(res != VK_SUCCESS) {
            logError("[vulkan/configurator]: could not create vulkan instance");
            return std::nullopt;
        }

        instance = vulkanInstance;
        return instance;
    }

    void enumeratePhysicalDevicesAndQueues() {
        enumeratePhysicalDevices();
        enumeratePhysicalDeviceProperties();
        enumeratePhysicalDeviceMemoryProperties();
        enumerateQueueFamilyProperties();
    }

    constexpr std::span<const VkPhysicalDevice> getPhysicalDevices() const noexcept {
        return physicalDevices;
    }

    // methods for querying devices and or device/queue properties
    // todo: pick a device based on optional and required features
    std::optional<const VkPhysicalDevice> getBestPhysicalDevice() const noexcept {
        if(physicalDevices.empty()) {
            return std::nullopt;
        }
        // just return the first one for now
        return physicalDevices.front();
    }

    std::optional<const VkPhysicalDeviceProperties> getPhysicalDeviceProperties(const VkPhysicalDevice& physicalDevice) noexcept {
        if(physicalDevices.empty()) {
            return std::nullopt;
        }

        // what is the queue family properties offset corresponding to that physical device?
        std::optional<const std::size_t> deviceIndex = getPhysicalDeviceIndex(physicalDevice);
        if(!deviceIndex.has_value()) {
            return std::nullopt;
        }

        return physicalDeviceProps[*deviceIndex];
    }

    std::optional<const VkPhysicalDeviceMemoryProperties> getPhysicalDeviceMemoryProperties(const VkPhysicalDevice& physicalDevice) {
        if(physicalDevices.empty()) {
            return std::nullopt;
        }

        // what is the queue family properties offset corresponding to that physical device?
        std::optional<const std::size_t> deviceIndex = getPhysicalDeviceIndex(physicalDevice);
        if(!deviceIndex.has_value()) {
            return std::nullopt;
        }

        return physicalDeviceMemoryProps[*deviceIndex];
    }

    std::span<const VkQueueFamilyProperties> getQueueFamilyProperties(const VkPhysicalDevice& physicalDevice) const {
        if(physicalDevices.empty()) {
            return {};
        }

        std::optional<const std::size_t> deviceIndex = getPhysicalDeviceIndex(physicalDevice);

        if(!deviceIndex.has_value()) {
            return {};
        }

        // what is the queue family properties offset corresponding to that physical device?
        const std::size_t offset = queueFamilyPropertiesOffsets[*deviceIndex].offset;
        const std::size_t length = queueFamilyPropertiesOffsets[*deviceIndex].length;

        return queueFamilyProperties.subspan(offset, length);
    }

private:
    // log convenience
    void logError(const char* msg) {
        if(pLogger != nullptr) {
            pLogger->log<core::log::Level::error>(msg);
        }
    }

    void logDebug(const char* msg) {
        if(pLogger != nullptr) {
            pLogger->log<core::log::Level::debug>(msg);
        }
    }

    void logInfo(const char* msg) {
        if(pLogger != nullptr) {
            pLogger->log<core::log::Level::info>(msg);
        }
    }

    // get physical device index
    std::optional<const std::size_t> getPhysicalDeviceIndex(const VkPhysicalDevice& physicalDevice) const noexcept {
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
            return std::nullopt;
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

        // std::cout << "[vulkan/device_explorer]: found " << numPhysicalDevices << " physical devices\n";
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
                << " queues on the physical device " << deviceIndex << "\n";

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
