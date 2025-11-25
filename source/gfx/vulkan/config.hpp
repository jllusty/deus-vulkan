// gfx/vulkan/config.hpp:
//     accepts an instance request, returns an instance and a bunch of
#pragma once

#include <span>
#include <optional>

#include "core/memory/stack_allocator.hpp"
#include "core/types.hpp"
#include "core/memory/types.hpp"

#include "core/memory/stack_allocator.hpp"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace gfx::vulkan {

struct InstanceRequest {
    std::span<const char* const> requiredLayerNames;
    std::span<const char* const> requiredExtensionNames;
    std::span<const char* const> optionalLayerNames;
    std::span<const char* const> optionalExtensionNames;
};

// supports configuring a single vulkan instance
class Configurator {
    core::memory::StackAllocator allocator;

    // available version of vulkan api
    std::optional<core::u32> apiVersion{};

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

    // device-level extensions
    std::span<VkExtensionProperties> physicalDeviceExtensionProps{};

    // logging
    core::log::Logger& log;

public:
    static std::optional<Configurator> create(core::memory::Region region,
        InstanceRequest request,
        core::log::Logger& log) noexcept
    {
        // call private constructor
        Configurator config(region, log);

        // instance-level vulkan api
        std::optional<core::u32> availableInstanceAPI = config.queryAvailableInstanceVersion();
        if(!availableInstanceAPI) {
            log.error("vulkan/config","could not retrieve vulkan api version");
            return std::nullopt;
        }
        config.apiVersion = *availableInstanceAPI;

        // query, store, and return available instance-level layers and extensions
        std::span<const VkLayerProperties> layerProps = config.queryAvailableInstanceLayerProperties();
        std::span<const VkExtensionProperties> extensionProps = config.queryAvailableInstanceExtensionProperties();

        std::optional<const VkInstance> createdInstance = config.createInstance(
            "Vulkan Application",
            "deus-vulkan",
            *availableInstanceAPI,
            {},
            { VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME }
        );
        if(!createdInstance.has_value()) {
            log.error("vulkan/config", "failed to create vulkan instance");
            return std::nullopt;
        }

        // query, store, and return device properties, memory, and queues
        std::span<const VkPhysicalDevice> physicalDevices = config.enumeratePhysicalDevices();
        std::span<const VkPhysicalDeviceProperties> deviceProps = config.enumeratePhysicalDeviceProperties();
        std::span<const VkPhysicalDeviceMemoryProperties> deviceMemoryProps = config.enumeratePhysicalDeviceMemoryProperties();
        std::span<const VkQueueFamilyProperties> queueFamilyProps = config.enumerateQueueFamilyProperties();

        // return a config with a properly set instance, invalidate the local temporary config's instance
        return std::move(config);
    }

    Configurator() = delete;
    Configurator(const Configurator&) = delete;
    Configurator& operator=(const Configurator&) = delete;

    Configurator(Configurator&& other)
        : allocator(other.allocator), log(other.log)
    {
        apiVersion = other.apiVersion;

        // available layers and extensions for a VkInstance
        instanceAvailableLayers = other.instanceAvailableLayers;
        instanceAvailableExtensions = other.instanceAvailableExtensions;

        instance = std::move(other.instance);

        physicalDevices = other.physicalDevices;
        physicalDeviceProps = other.physicalDeviceProps;
        physicalDeviceMemoryProps = other.physicalDeviceMemoryProps;

        // a single physical device can be associated with multiple queues
        queueFamilyPropertiesOffsets = other.queueFamilyPropertiesOffsets;
        queueFamilyProperties = other.queueFamilyProperties;

        // device-level extensions
        physicalDeviceExtensionProps = other.physicalDeviceExtensionProps;

        // invalidate other's instance
        other.instance = std::nullopt;
    }
    Configurator& operator=(Configurator&& other) = delete;

    ~Configurator() {
        if(!instance.has_value()) {
            // this will happen once on startup (see create function)
            logInfo("attempt to destroy non-existent instance");
            return;
        }

        vkDestroyInstance(*instance, nullptr);
        logInfo("destroyed instance");
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

    // get device-level available extensions
    std::span<const VkExtensionProperties> getAvailableDeviceExtensionProperties(const VkPhysicalDevice physicalDevice) const noexcept {
        return physicalDeviceExtensionProps;
    }

    std::optional<const VkPhysicalDeviceProperties> getPhysicalDeviceProperties(const VkPhysicalDevice& physicalDevice) const noexcept {
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

    std::optional<const VkPhysicalDeviceMemoryProperties> getPhysicalDeviceMemoryProperties(const VkPhysicalDevice& physicalDevice) const noexcept {
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

    std::span<const VkQueueFamilyProperties> getQueueFamilyProperties(const VkPhysicalDevice& physicalDevice) const noexcept {
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
    Configurator(core::memory::Region region, core::log::Logger& log)
        : allocator(region), log(log)
    {
        // todo: custom vulkan allocator callbacks, inject pLogger calls
    }

    static void allocationNotification(void* pUserData, size_t size,
            VkInternalAllocationType allocationType,
            VkSystemAllocationScope allocationScope)
    {
        std::cout << "[vulkan/internal]: allocating " << size << " bytes\n";
        std::cout.flush();
    }

    static void freeNotification(void* pUserData, size_t size,
            VkInternalAllocationType allocationType,
            VkSystemAllocationScope allocationScope)
    {
        std::cout << "[vulkan/internal]: freeing " << size << " bytes\n";
        std::cout.flush();
    }

    // log convenience
    template<typename... Args>
    void logError(const char* msg, Args... args) {
        log.error("vulkan/configurator", msg, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void logDebug(const char* msg, Args... args) {
        log.debug("vulkan/configurator", msg, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void logInfo(const char* msg, Args... args) {
        log.info("vulkan/configurator", msg, std::forward<Args>(args)...);
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

        logInfo("enumerated %d physical devices", numPhysicalDevices);
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

    // there is no way in vulkan 1.0 to even ask if 1.0 is supported
    // we just have to infer based on the lack of vkEnumerateInstanceVersion
    // which was introduced in 1.1
    std::optional<const core::u32> queryAvailableInstanceVersion() noexcept {
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

    std::span<const VkLayerProperties> queryAvailableInstanceLayerProperties() noexcept {
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

    std::span<const VkExtensionProperties> queryAvailableInstanceExtensionProperties() noexcept {
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
        VkAllocationCallbacks allocationCallbacks {
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            &Configurator::allocationNotification,
            &Configurator::freeNotification
        };
        VkInstance vulkanInstance;
        VkResult res = vkCreateInstance(
            &createInfo,
            nullptr, // &allocationCallbacks,
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

};

}
