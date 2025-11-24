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

    std::optional<VkInstance> instance{};

    std::span<VkPhysicalDevice> physicalDevices{};
    std::span<VkPhysicalDeviceProperties> physicalDeviceProps{};
    std::span<VkPhysicalDeviceMemoryProperties> physicalDeviceMemoryProps{};

    // a single physical device can be associated with multiple queues
    std::span<core::memory::ArrayOffset> queueFamilyPropertiesOffsets{};
    std::span<VkQueueFamilyProperties> queueFamilyProperties{};

    // logical devices
    std::span<VkDevice> devices;

    // logging
    core::log::Logger* pLogger{ nullptr };

public:
    Configurator() = delete;

    Configurator(core::memory::Region region, core::log::Logger* pLogger = nullptr)
        : allocator(region), pLogger(pLogger)
    {
        // todo: custom vulkan allocator, inject pLogger calls
    }

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
                logError("could not retrieve vulkan version >= 1.1");
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
            logError("could not get any available instance layers");
            return {};
        }

        instanceAvailableLayers = allocator.allocate<VkLayerProperties>(numLayers);

        result = vkEnumerateInstanceLayerProperties(
            &numLayers,
            instanceAvailableLayers.data()
        );

        if(result != VK_SUCCESS || instanceAvailableLayers.empty()) {
            logError("could not get any available instance layers");
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
            logError("could not get any available instance extensions");
            return {};
        }

        instanceAvailableExtensions = allocator.allocate<VkExtensionProperties>(numExtensions);

        result = vkEnumerateInstanceExtensionProperties(
            nullptr,
            &numExtensions,
            instanceAvailableExtensions.data()
        );

        if(result != VK_SUCCESS || instanceAvailableLayers.empty()) {
            logError("could not get any available instance layers");
        }

        return instanceAvailableExtensions;
    }

    [[nodiscard]] std::optional<const VkInstance> createInstance(
        const char* applicationName,
        const char* engineName,
        const core::u32 apiVersion,
        std::initializer_list<const char *> layerNames,
        std::initializer_list<const char *> extensionNames)
    {
        VkApplicationInfo appInfo{
            VK_STRUCTURE_TYPE_APPLICATION_INFO,
            nullptr,
            applicationName,
            0,
            engineName,
            apiVersion
        };

        // flags check
        core::u32 flags{ 0 };
        if(std::ranges::find(extensionNames, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) != layerNames.end()) {
            flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
            logInfo("extension VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME requested for new instance");
        }

        // Layers: []
        const core::u32 numLayers = layerNames.size();
        const char* const* ppLayerNames { layerNames.begin() };
        const core::u32 numExtensions = extensionNames.size();
        const char* const* ppExtensionNames { extensionNames.begin() };
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
            logError("could not create vulkan instance");
            return std::nullopt;
        }

        logInfo("created instance");
        instance = vulkanInstance;
        return instance;
    }

    // return true if succeeds, false if failure
    [[nodiscard]] const bool destroyInstance() noexcept {
        if(!instance.has_value()) {
            logError("attempt to destroy non-existent instance");
            return false;
        }

        if(!devices.empty()) {
            logError("attempt to destroy instance while references to logical devices exist");
            return false;
        }

        vkDestroyInstance(*instance, nullptr);
        logInfo("destroyed instance");
        return true;
    }

    std::span<const VkPhysicalDevice> enumeratePhysicalDevices() {
        if(!instance.has_value()) {
            logError("cannot enumerate physical devices without an instance");
            return {};
        }

        // enumerate physical devices
        core::u32 numPhysicalDevices = 0;
        VkResult enumeratePhysicalDevicesResult = vkEnumeratePhysicalDevices(
            *instance,
            &numPhysicalDevices,
            nullptr
        );

        if(enumeratePhysicalDevicesResult != VK_SUCCESS) {
            logError("could not enumerate physical devices for configured instance");
            return {};
        }
        if(numPhysicalDevices == 0) {
            // todo: log this
            logError("no physical devices found");
            return {};
        }

        physicalDevices = allocator.allocate<VkPhysicalDevice>(numPhysicalDevices);

        enumeratePhysicalDevicesResult = vkEnumeratePhysicalDevices(
            *instance,
            &numPhysicalDevices,
            physicalDevices.data()
        );

        if(enumeratePhysicalDevicesResult != VK_SUCCESS) {
            // todo: log this
            logError("could not enumerate physical devices for configured instance");
            return {};
        }
        if(physicalDevices.empty()) {
            logError("no physical devices found");
        }

        logInfo("enumerated %d physical devices");
        return physicalDevices;
    }

    std::span<const VkPhysicalDeviceProperties> enumeratePhysicalDeviceProperties() noexcept {
        if(physicalDevices.empty()) {
            logError("cannot enumerate physical device properties without enumerating physical devices");
            return {};
        }

        // get physical device properties
        physicalDeviceProps = allocator.allocate<VkPhysicalDeviceProperties>(physicalDevices.size());
        for(std::size_t deviceIndex = 0; deviceIndex < physicalDevices.size(); ++deviceIndex) {
            const VkPhysicalDevice& physicalDevice = physicalDevices[deviceIndex];

            vkGetPhysicalDeviceProperties(
                physicalDevice,
                &physicalDeviceProps[deviceIndex]
            );

            if(physicalDeviceProps.empty()) {
                logError("could not retrieve physical device properties for a physical device");
            }
        }
        return physicalDeviceProps;
    }

    std::span<const VkPhysicalDeviceMemoryProperties> enumeratePhysicalDeviceMemoryProperties() noexcept {
        if(physicalDevices.empty()) {
            logError("no physical devices enumerated");
        }

        physicalDeviceMemoryProps = allocator.allocate<VkPhysicalDeviceMemoryProperties>(physicalDevices.size());
        for(std::size_t deviceIndex = 0; deviceIndex < physicalDevices.size(); ++deviceIndex) {
            const VkPhysicalDevice& physicalDevice = physicalDevices[deviceIndex];
            vkGetPhysicalDeviceMemoryProperties(
                physicalDevice,
                &physicalDeviceMemoryProps[deviceIndex]
            );

            const VkPhysicalDeviceMemoryProperties& props = physicalDeviceMemoryProps[deviceIndex];
        }

        return physicalDeviceMemoryProps;
    }

    std::span<const VkQueueFamilyProperties> enumerateQueueFamilyProperties() noexcept {
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

        return queueFamilyProperties;
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
            pLogger->error("[%s]: %s", "vulkan/configurator", msg);
        }
    }

    void logDebug(const char* msg) {
        if(pLogger != nullptr) {
            pLogger->debug("[%s]: %s", "vulkan/configurator", msg);
        }
    }

    void logInfo(const char* msg) {
        if(pLogger != nullptr) {
            pLogger->info("[%s]: %s", "vulkan/configurator", msg);
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

};

}
