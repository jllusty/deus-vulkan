// gfx/vulkan/config.hpp:
//     accepts an instance request, returns an instance and a bunch of
#pragma once

#include <vector>
#include <string>
#include <span>
#include <optional>

#include "core/types.hpp"
#include "core/log/logging.hpp"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace gfx::vulkan {

struct PhysicalDeviceHandle {
    std::size_t id{};
};

struct InstanceRequest {
    std::initializer_list<const char*> requiredLayerNames;
    std::initializer_list<const char*> requiredExtensionNames;
    std::initializer_list<const char*> optionalLayerNames;
    std::initializer_list<const char*> optionalExtensionNames;
};

// supports configuring a single vulkan instance
class Configurator {
    // available version of vulkan api
    std::optional<core::u32> apiVersion{};

    // available layers and extensions for a VkInstance
    std::vector<VkLayerProperties> instanceAvailableLayers{};
    std::vector<VkExtensionProperties> instanceAvailableExtensions{};

    // vulkan instance
    std::optional<VkInstance> instance{};

    // handle for physical devices - just an index to these internal arrays
    std::vector<PhysicalDeviceHandle> physicalDeviceHandles{};
    // enumerated physical devices and their properties
    std::vector<VkPhysicalDevice> physicalDevices{};
    std::vector<VkPhysicalDeviceProperties> physicalDeviceProps{};
    std::vector<VkPhysicalDeviceMemoryProperties> physicalDeviceMemoryProps{};

    // a single physical device can be associated with multiple queues
    std::vector<std::vector<VkQueueFamilyProperties>> queueFamilyProperties{};

    // layers/extension names to request from vulkan on instance creation
    std::vector<std::string> instanceRequestedLayers{};
    std::vector<std::string> instanceRequestedExtensions{};

    // device-level extensions
    std::vector<VkExtensionProperties> physicalDeviceExtensionProps{};

    // logging
    core::log::Logger& log;

public:
    static std::optional<Configurator> create(InstanceRequest request,
        core::log::Logger& log) noexcept
    {
        Configurator config(log);

        // instance-level vulkan api
        config.enumerateAvailableInstanceVersion();
        if(!config.apiVersion.has_value()) {
            config.logError("could not retrieve vulkan api version");
            return std::nullopt;
        }

        // enumerate, store, and return available instance-level layers and extensions
        config.enumerateAvailableInstanceLayerProperties();
        config.enumerateAvailableInstanceExtensionProperties();

        // enumerate, store, and return requestable instance-level layer names
        config.enumerateRequestableLayerNames(
            request.requiredLayerNames, request.optionalLayerNames
        );
        config.enumerateRequestableExtensionNames(
            request.requiredExtensionNames, request.optionalExtensionNames
        );

        config.createInstance(
            "Vulkan Application",
            "deus-vulkan"
        );
        if(!config.instance.has_value()) {
            config.logError("failed to create vulkan instance");
            return std::nullopt;
        }

        // enumerate, store, and return device properties, memory, and queues
        config.enumeratePhysicalDevices();
        config.enumeratePhysicalDeviceProperties();
        config.enumeratePhysicalDeviceMemoryProperties();
        config.enumerateQueueFamilyProperties();

        // return a config with a properly set instance, invalidate the local temporary config's instance
        return std::move(config);
    }

    Configurator() = delete;
    Configurator(const Configurator&) = delete;
    Configurator& operator=(const Configurator&) = delete;

    Configurator(Configurator&& other)
        : log(other.log)
    {
        apiVersion = std::move(other.apiVersion);

        // available layers and extensions for a VkInstance
        instanceAvailableLayers = other.instanceAvailableLayers;
        instanceAvailableExtensions = other.instanceAvailableExtensions;

        instance = std::move(other.instance);

        physicalDeviceHandles = other.physicalDeviceHandles;
        physicalDevices = other.physicalDevices;
        physicalDeviceProps = other.physicalDeviceProps;
        physicalDeviceMemoryProps = other.physicalDeviceMemoryProps;

        // a single physical device can be associated with multiple queues
        queueFamilyProperties = other.queueFamilyProperties;

        // device-level extensions
        physicalDeviceExtensionProps = other.physicalDeviceExtensionProps;

        // enabled extensions layers and extensions
        instanceRequestedLayers = other.instanceRequestedLayers;
        instanceRequestedExtensions = other.instanceRequestedExtensions;

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

    constexpr std::optional<const core::u32> getVulkanAPI() const noexcept {
        return apiVersion;
    }

    constexpr std::optional<const VkInstance> getVulkanInstance() const noexcept {
        return instance;
    }

    constexpr std::optional<const VkPhysicalDevice> getVulkanPhysicalDevice(const PhysicalDeviceHandle physicalDevice) const noexcept {
        return physicalDevices.at(physicalDevice.id);
    }

    constexpr std::span<const PhysicalDeviceHandle> getPhysicalDevices() const noexcept {
        return physicalDeviceHandles;
    }

    // methods for enumerateing devices and or device/queue properties
    // todo: pick a device based on optional and required features
    std::optional<const PhysicalDeviceHandle> getBestPhysicalDevice() const noexcept {
        if(physicalDevices.empty()) {
            return std::nullopt;
        }
        // just return the first one for now
        return physicalDeviceHandles.front();
    }

    // get device-level available extensions
    std::optional<const VkExtensionProperties> getAvailableDeviceExtensionProperties(const PhysicalDeviceHandle& physicalDevice) const noexcept {
        if(physicalDeviceExtensionProps.empty()) {
            return std::nullopt;
        }
        return physicalDeviceExtensionProps.at(physicalDevice.id);
    }

    std::optional<const VkPhysicalDeviceProperties> getPhysicalDeviceProperties(const PhysicalDeviceHandle& physicalDevice) const noexcept {
        if(physicalDeviceProps.empty()) {
            return std::nullopt;
        }

        // what is the queue family properties offset corresponding to that physical device?
        return physicalDeviceProps.at(physicalDevice.id);
    }

    std::optional<const VkPhysicalDeviceMemoryProperties> getPhysicalDeviceMemoryProperties(const PhysicalDeviceHandle& physicalDevice) const noexcept {
        if(physicalDeviceMemoryProps.empty()) {
            return std::nullopt;
        }

        return physicalDeviceMemoryProps.at(physicalDevice.id);
    }

    std::span<const VkQueueFamilyProperties> getQueueFamilyProperties(const PhysicalDeviceHandle& physicalDevice) const noexcept {
        if(queueFamilyProperties.empty()) {
            return {};
        }

        return queueFamilyProperties.at(physicalDevice.id);
    }

    std::span<const std::string> getEnabledExtensionNames() const noexcept {
        return instanceRequestedExtensions;
    }

    std::span<const std::string> getEnabledLayerNames() const noexcept {
        return instanceRequestedLayers;
    }

private:
    Configurator(core::log::Logger& log)
        : log(log)
    {
        // todo: custom vulkan allocator callbacks, inject pLogger calls
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

    void enumeratePhysicalDevices() {
        if(!instance.has_value()) {
            logError("cannot enumerate physical devices without an instance");
            return;
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
            return;
        }
        if(numPhysicalDevices == 0) {
            // todo: log this
            logError("no physical devices found");
            return;
        }

        physicalDevices.resize(numPhysicalDevices);
        enumeratePhysicalDevicesResult = vkEnumeratePhysicalDevices(
            *instance,
            &numPhysicalDevices,
            physicalDevices.data()
        );

        if(enumeratePhysicalDevicesResult != VK_SUCCESS) {
            logError("could not enumerate physical devices for configured instance");
            return;
        }
        if(physicalDevices.empty()) {
            logError("no physical devices found");
        }

        // create list of physical device handles
        for(std::size_t id = 0; id < numPhysicalDevices; ++id) {
            physicalDeviceHandles.push_back({.id = id});
        }

        logInfo("enumerated %d physical devices", numPhysicalDevices);
    }

    void enumeratePhysicalDeviceProperties() noexcept {
        if(physicalDevices.empty()) {
            logError("cannot enumerate physical device properties without first enumerating physical devices");
            return;
        }

        // get physical device properties
        physicalDeviceProps.resize(physicalDevices.size());
        for(const PhysicalDeviceHandle& physicalDeviceHandle : physicalDeviceHandles) {
            const VkPhysicalDevice physicalDevice = physicalDevices.at(physicalDeviceHandle.id);
            vkGetPhysicalDeviceProperties(
                physicalDevice,
                &physicalDeviceProps[physicalDeviceHandle.id]
            );

            if(physicalDeviceProps.empty()) {
                logError("could not retrieve physical device properties for a physical device");
            }
        }
    }

    void enumeratePhysicalDeviceMemoryProperties() noexcept {
        if(physicalDevices.empty()) {
            logError("cannot enumerate physical device memory properties without first enumerating physical devices");
        }

        physicalDeviceMemoryProps.resize(physicalDevices.size());
        for(const PhysicalDeviceHandle& physicalDeviceHandle : physicalDeviceHandles) {
            const VkPhysicalDevice physicalDevice = physicalDevices.at(physicalDeviceHandle.id);
            vkGetPhysicalDeviceMemoryProperties(
                physicalDevice,
                &physicalDeviceMemoryProps[physicalDeviceHandle.id]
            );
        }
    }

    void enumerateQueueFamilyProperties() noexcept {
        queueFamilyProperties.resize(physicalDevices.size());
        for(const PhysicalDeviceHandle& physicalDeviceHandle : physicalDeviceHandles) {
            const VkPhysicalDevice physicalDevice = physicalDevices.at(physicalDeviceHandle.id);

            core::u32 numQueueFamilyProperties{ 0 };
            vkGetPhysicalDeviceQueueFamilyProperties(
                physicalDevice,
                &numQueueFamilyProperties,
                nullptr
            );

            queueFamilyProperties.at(physicalDeviceHandle.id).resize(numQueueFamilyProperties);

            vkGetPhysicalDeviceQueueFamilyProperties(
                physicalDevice,
                &numQueueFamilyProperties,
                queueFamilyProperties[physicalDeviceHandle.id].data()
            );
        }
    }

    // there is no way in vulkan 1.0 to even ask if 1.0 is supported
    // we just have to infer based on the lack of vkEnumerateInstanceVersion
    // which was introduced in 1.1
    void enumerateAvailableInstanceVersion() noexcept {
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
                return;
            }

            logInfo("vkEnumerateInstanceVersion returned vulkan %d.%d",
                VK_VERSION_MAJOR(version),
                VK_VERSION_MINOR(version)
            );
            apiVersion = version;
            return;
        }

        logInfo("vkEnumerateInstanceVersion does not exist, using vulkan 1.0");

        // we must assume that if we did not get a valid function pointer from vulkan on
        // the first command call, it is because that function is not defined and we
        // are working with vulkan 1.0
        apiVersion = VK_API_VERSION_1_0;
    }

    void enumerateAvailableInstanceLayerProperties() noexcept {
        core::u32 numLayers{ 0 };
        VkResult result = vkEnumerateInstanceLayerProperties(
            &numLayers,
            nullptr
        );

        if(result != VK_SUCCESS || numLayers == 0) {
            logError("could not get any available instance layers");
            return;
        }

        instanceAvailableLayers.resize(numLayers);

        result = vkEnumerateInstanceLayerProperties(
            &numLayers,
            instanceAvailableLayers.data()
        );

        if(result != VK_SUCCESS || instanceAvailableLayers.empty()) {
            logError("could not get any available instance layers");
        }
    }

    void enumerateAvailableInstanceExtensionProperties() noexcept {
        core::u32 numExtensions{ 0 };
        VkResult result = vkEnumerateInstanceExtensionProperties(
            nullptr,
            &numExtensions,
            nullptr
        );

        if(result != VK_SUCCESS || numExtensions == 0) {
            logError("could not get any available instance extensions");
        }

        instanceAvailableExtensions.resize(numExtensions);

        result = vkEnumerateInstanceExtensionProperties(
            nullptr,
            &numExtensions,
            instanceAvailableExtensions.data()
        );

        if(result != VK_SUCCESS || instanceAvailableLayers.empty()) {
            logError("could not get any available instance layers");
        }
    }

    // check what required names we need to encode in our instance creation request
    // as well as what optional names we could encode
    void enumerateRequestableLayerNames(
        std::initializer_list<const char*> requiredLayerNames,
        std::initializer_list<const char*> optionalLayerNames
    )
    {
        // ensure we can use the user-specified required layers we can use for instance specification
        // if we can't use them, failover
        for(const char * name : requiredLayerNames) {
            bool requiredLayerNameUsed = false;
            for(const VkLayerProperties prop : instanceAvailableLayers) {
                if(std::strcmp(prop.layerName,name) == 0) {
                    instanceRequestedLayers.push_back(name);
                    requiredLayerNameUsed = true;
                }
            }
            if(!requiredLayerNameUsed) {
                logError("could not use requested required layer '%s' for instance creation", name);
                instanceRequestedLayers.resize(0);
                return;
            }
        }

        // check if  we can use the user-specified optional layers we can use for instance specification
        // if we can't use them, just report it
        for(const char * name : optionalLayerNames) {
            bool optionalLayerNameUsable{ false };
            for(const VkLayerProperties prop : instanceAvailableLayers) {
                if(std::strcmp(prop.layerName,name) == 0) {
                    instanceRequestedLayers.push_back(name);
                    optionalLayerNameUsable = true;
                }
            }
            if(!optionalLayerNameUsable) {
                logInfo("could not use requested optional layer '%s' for instance creation", name);
            }
        }
    }

    // check what required names we need to encode in our instance creation request
    // as well as what optional names we could encode
    void enumerateRequestableExtensionNames(
        std::initializer_list<const char*> requiredExtensionNames,
        std::initializer_list<const char*> optionalExtensionNames
    )
    {
        // ensure we can use the user-specified required extensions we can use for instance specification
        // if we can't use them, failover
        for(const char * name : requiredExtensionNames) {
            bool requiredExtensionNameUseable = false;
            for(const VkExtensionProperties prop : instanceAvailableExtensions) {
                if(std::strcmp(prop.extensionName,name) == 0) {
                    instanceRequestedExtensions.push_back(name);
                    requiredExtensionNameUseable = true;
                }
            }
            if(!requiredExtensionNameUseable) {
                logError("could not use requested required extension '%s' for instance creation", name);
                instanceRequestedExtensions.resize(0);
                return;
            }
        }

        // check if  we can use the user-specified optional extensions we can use for instance specification
        // if we can't use them, just report it
        for(const char * name : optionalExtensionNames) {
            bool optionalExtensionNameUsable{ false };
            for(const VkExtensionProperties prop : instanceAvailableExtensions) {
                if(std::strcmp(prop.extensionName,name) == 0) {
                    instanceRequestedExtensions.push_back(name);
                    optionalExtensionNameUsable = true;
                }
            }
            if(!optionalExtensionNameUsable) {
                logInfo("could not use requested optional extension '%s' for instance creation", name);
            }
        }
    }

    void createInstance(
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
        for(const std::string& extensionName : instanceRequestedExtensions) {
            if(strcmp(extensionName.c_str(), VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0) {
                flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
                logInfo("extension VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME requested for new instance: portability bit set for instance creation flags");
            }
        }

        // intermediate buffers of const char *s
        std::vector<const char*> instanceRequestedLayerPtrs{};
        for(std::string& str : instanceRequestedLayers) {
            instanceRequestedLayerPtrs.push_back(str.c_str());
        }
        std::vector<const char*> instanceRequestedExtensionPtrs{};
        for(std::string& str : instanceRequestedExtensions) {
            instanceRequestedExtensionPtrs.push_back(str.c_str());
        }

        const VkInstanceCreateInfo createInfo {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = flags,
            .pApplicationInfo = &appInfo,
            .enabledLayerCount = static_cast<core::u32>(instanceRequestedLayers.size()),
            .ppEnabledLayerNames = instanceRequestedLayerPtrs.data(),
            .enabledExtensionCount = static_cast<core::u32>(instanceRequestedExtensions.size()),
            .ppEnabledExtensionNames = instanceRequestedExtensionPtrs.data()
        };

        // Create Vulkan Instance
        const VkAllocationCallbacks* pHostMemoryAllocator = nullptr; // use Vulkan's internal allocator
        VkInstance vulkanInstance;
        VkResult res = vkCreateInstance(
            &createInfo,
            nullptr, // &allocationCallbacks,
            &vulkanInstance
        );

        if(res != VK_SUCCESS) {
            logError("could not create vulkan instance");
            return;
        }

        logInfo("created instance");
        instance = vulkanInstance;
    }

};

}
