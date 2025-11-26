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
    std::initializer_list<const char*> requiredLayerNames;
    std::initializer_list<const char*> requiredExtensionNames;
    std::initializer_list<const char*> optionalLayerNames;
    std::initializer_list<const char*> optionalExtensionNames;
};

// supports configuring a single vulkan instance
class Configurator {
    core::memory::StackAllocator allocator;

    // available version of vulkan api
    std::optional<core::u32> apiVersion{};

    // available layers and extensions for a VkInstance
    std::span<VkLayerProperties> instanceAvailableLayers{};
    std::span<VkExtensionProperties> instanceAvailableExtensions{};

    // vulkan instance
    std::optional<VkInstance> instance{};

    // enumerated physical devices and their properties
    std::span<VkPhysicalDevice> physicalDevices{};
    std::span<VkPhysicalDeviceProperties> physicalDeviceProps{};
    std::span<VkPhysicalDeviceMemoryProperties> physicalDeviceMemoryProps{};

    // a single physical device can be associated with multiple queues
    std::span<core::memory::ArrayOffset> queueFamilyPropertiesOffsets{};
    std::span<VkQueueFamilyProperties> queueFamilyProperties{};

    // layers/extension names to request from vulkan on instance creation
    // contigous sequence of char ptrs to pass to vulkan
    std::span<char*> instanceRequestedLayers{};
    std::span<char*> instanceRequestedExtensions{};
    // underlying data for the requested layer and extension names
    std::span<char> instanceRequestedLayersData{};
    std::span<char> instanceRequestedExtensionsData{};

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
        std::optional<core::u32> availableInstanceAPI = config.enumerateAvailableInstanceVersion();
        if(!availableInstanceAPI) {
            config.logError("could not retrieve vulkan api version");
            return std::nullopt;
        }
        config.apiVersion = *availableInstanceAPI;

        // enumerate, store, and return available instance-level layers and extensions
        std::span<const VkLayerProperties> layerProps = config.enumerateAvailableInstanceLayerProperties();
        std::span<const VkExtensionProperties> extensionProps = config.enumerateAvailableInstanceExtensionProperties();

        // enumerate, store, and return requestable instance-level layer names
        std::span<char* const> requestableLayerNames = config.enumerateRequestableLayerNames(
            request.requiredLayerNames, request.optionalLayerNames
        );
        std::span<char* const> requestableExtensionNames = config.enumerateRequestableExtensionNames(
            request.requiredExtensionNames, request.optionalExtensionNames
        );

        std::optional<const VkInstance> createdInstance = config.createInstance(
            "Vulkan Application",
            "deus-vulkan"
        );
        if(!createdInstance.has_value()) {
            config.logError("failed to create vulkan instance");
            return std::nullopt;
        }

        // enumerate, store, and return device properties, memory, and queues
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

        // enabled extensions layers and extensions
        instanceRequestedLayers = other.instanceRequestedLayers;
        instanceRequestedLayersData = other.instanceRequestedLayersData;
        instanceRequestedExtensions = other.instanceRequestedExtensions;
        instanceRequestedExtensionsData = other.instanceRequestedExtensionsData;

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

    // methods for enumerateing devices and or device/queue properties
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

    std::span<char* const> getEnabledExtensionNames() const noexcept {
        return instanceRequestedExtensions;
    }

    std::span<char* const> getEnabledLayerNames() const noexcept {
        return instanceRequestedLayers;
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
    std::optional<const core::u32> enumerateAvailableInstanceVersion() noexcept {
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

            logInfo("vkEnumerateInstanceVersion returned vulkan %d.%d",
                VK_VERSION_MAJOR(version),
                VK_VERSION_MINOR(version)
            );
            return version;
        }
        else {
            logInfo("vkEnumerateInstanceVersion does not exist, using vulkan 1.0");
        }

        // we must assume that if we did not get a valid function pointer from vulkan on
        // the first command call, it is because that function is not defined and we
        // are working with vulkan 1.0
        return VK_VERSION_1_0;
    }

    std::span<const VkLayerProperties> enumerateAvailableInstanceLayerProperties() noexcept {
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

    std::span<const VkExtensionProperties> enumerateAvailableInstanceExtensionProperties() noexcept {
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

    // check what required names we need to encode in our instance creation request
    // as well as what optional names we could encode
    std::span<char* const> enumerateRequestableLayerNames(
        std::initializer_list<const char*> requiredLayerNames,
        std::initializer_list<const char*> optionalLayerNames
    )
    {
        // count the number of required layers we can use for instance specification: if we can't use them, failover
        std::size_t numLayersToRequest{ 0 };
        for(const char * name : requiredLayerNames) {
            bool requiredLayerNameUsed = false;
            for(const VkLayerProperties prop : instanceAvailableLayers) {
                if(std::strcmp(prop.layerName,name) == 0) {
                    ++numLayersToRequest;
                    requiredLayerNameUsed = true;
                }
            }
            if(!requiredLayerNameUsed) {
                logError("could not use requested optional layer '%s' for instance creation", name);
                return {};
            }
        }

        // count the number of usable layers
        // optional layers: log to info if we can't use them, but do not failover
        for(const char * name : optionalLayerNames) {
            bool optionalLayerNameUsable{ false };
            for(const VkLayerProperties prop : instanceAvailableLayers) {
                if(std::strcmp(prop.layerName,name) == 0) {
                    ++numLayersToRequest;
                    optionalLayerNameUsable = true;
                }
            }
            if(!optionalLayerNameUsable) {
                // todo: better name for logging in this stage
                logInfo("could not use requested optional layer '%s' for instance creation", name);
            }
        }

        // allocate space for the layers to request at instance creation
        instanceRequestedLayers = allocator.allocate<char*>(numLayersToRequest);
        instanceRequestedLayersData = allocator.allocate<char>(
            numLayersToRequest * VK_MAX_EXTENSION_NAME_SIZE
        );
        std::size_t ptrIndex{ 0 };
        std::size_t writeIndex{ 0 };
        // write the required layers
        for(const char * name : requiredLayerNames) {
            instanceRequestedLayers[ptrIndex] = instanceRequestedLayersData.data() + writeIndex;
            std::strcpy(
                instanceRequestedLayers[ptrIndex],
                name
            );
            ++ptrIndex;
            writeIndex += VK_MAX_EXTENSION_NAME_SIZE;
        }
        // write the optional layers we support
        for(const char * name : optionalLayerNames) {
            // just loop back through the supported layers
            for(const VkLayerProperties prop : instanceAvailableLayers) {
                if(std::strcmp(prop.layerName, name) == 0) {
                    instanceRequestedLayers[ptrIndex] = instanceRequestedLayersData.data() + writeIndex;
                    std::strcpy(
                        instanceRequestedLayers[ptrIndex],
                        name
                    );
                    ++ptrIndex;
                    writeIndex += VK_MAX_EXTENSION_NAME_SIZE;
                }
            }
        }

        return instanceRequestedLayers;
    }

    // check what required names we need to encode in our instance creation request
    // as well as what optional names we could encode
    std::span<char* const> enumerateRequestableExtensionNames(
        std::initializer_list<const char*> requiredExtensionNames,
        std::initializer_list<const char*> optionalExtensionNames
    )
    {
        // count the number of required extensions we can use for instance specification: if we can't use them, failover
        std::size_t numExtensionsToRequest{ 0 };
        for(const char * name : requiredExtensionNames) {
            bool requiredExtensionNameUsed = false;
            for(const VkExtensionProperties prop : instanceAvailableExtensions) {
                if(std::strcmp(prop.extensionName,name) == 0) {
                    ++numExtensionsToRequest;
                    requiredExtensionNameUsed = true;
                }
            }
            if(!requiredExtensionNameUsed) {
                log.error("vulkan/config-create","could not use requested optional extension '%s' for instance creation", name);
                return {};
            }
        }

        // count the number of usable extensions
        // optional extensions: log to info if we can't use them, but do not failover
        for(const char * name : optionalExtensionNames) {
            bool optionalExtensionNameUsable{ false };
            for(const VkExtensionProperties prop : instanceAvailableExtensions) {
                if(std::strcmp(prop.extensionName,name) == 0) {
                    ++numExtensionsToRequest;
                    optionalExtensionNameUsable = true;
                }
            }
            if(!optionalExtensionNameUsable) {
                // todo: better name for logging in this stage
                log.info("vulkan/config-create","could not use requested optional extension '%s' for instance creation", name);
            }
        }

        // allocate space for the extensions to request at instance creation
        instanceRequestedExtensions = allocator.allocate<char*>(numExtensionsToRequest);
        instanceRequestedExtensionsData = allocator.allocate<char>(
            numExtensionsToRequest * VK_MAX_EXTENSION_NAME_SIZE
        );
        std::size_t ptrIndex{ 0 };
        std::size_t writeIndex{ 0 };
        // write the required extensions
        for(const char * name : requiredExtensionNames) {
            instanceRequestedExtensions[ptrIndex] = instanceRequestedExtensionsData.data() + writeIndex;
            std::strcpy(
                instanceRequestedExtensions[ptrIndex],
                name
            );
            ++ptrIndex;
            writeIndex += VK_MAX_EXTENSION_NAME_SIZE;
        }

        // write the optional extensions we support
        for(const char * name : optionalExtensionNames) {
            // just loop back through the supported Extensions
            for(const VkExtensionProperties prop : instanceAvailableExtensions) {
                if(std::strcmp(prop.extensionName, name) == 0) {
                    instanceRequestedExtensions[ptrIndex] = instanceRequestedExtensionsData.data() + writeIndex;
                    std::strcpy(
                        instanceRequestedExtensions[ptrIndex],
                        name
                    );
                    ++ptrIndex;
                    writeIndex += VK_MAX_EXTENSION_NAME_SIZE;
                }
            }
        }

        return instanceRequestedExtensions;
    }

    [[nodiscard]] std::optional<const VkInstance> createInstance(
        const char* applicationName,
        const char* engineName)
    {
        VkApplicationInfo appInfo{
            VK_STRUCTURE_TYPE_APPLICATION_INFO,
            nullptr,
            applicationName,
            0,
            engineName,
            *apiVersion
        };

        // flags check
        core::u32 flags{ 0 };
        for(const char * extensionName : instanceRequestedExtensions) {
            if(strcmp(extensionName, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0) {
                flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
                logInfo("extension VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME requested for new instance: portability bit set for instance creation flags");
            }
        }

        const VkInstanceCreateInfo createInfo {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = flags,
            .pApplicationInfo = &appInfo,
            .enabledLayerCount = static_cast<core::u32>(instanceRequestedLayers.size()),
            .ppEnabledLayerNames = instanceRequestedLayers.data(),
            .enabledExtensionCount = static_cast<core::u32>(instanceRequestedExtensions.size()),
            .ppEnabledExtensionNames = instanceRequestedExtensions.data()
        };

        // Create Vulkan Instance
        // todo: pass an allocator
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
