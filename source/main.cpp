// main.cpp
#include <cstddef>
#include <iostream>

#include <sys/wait.h>
#include <vulkan/vulkan.h>
#include <SFML/Window.hpp>

#include "core/types.hpp"
#include "core/memory/base_allocator.hpp"
#include "core/memory/types.hpp"
#include "core/log/logging.hpp"

#include "gfx/vulkan/config.hpp"

int main()
{
    // Application Info: Describes the application
    core::u32 vulkanApiVersion = VK_MAKE_API_VERSION(0,1,0,0);
    const VkApplicationInfo applicationInfo {
        VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,     // sType
        nullptr,                                    // pNext
        "VulkanApplication",                        // pApplicationName
        0,                                          // applicationVersion
        "Deus-Vulkan",                              // pEngineName
        0,                                          // engineVersion
        vulkanApiVersion                            // apiVersion
    };

    // vulkan instance create info
    // Layers: []
    const core::u32 numLayers{ 0 };
    const char* const* ppLayerNames { nullptr };
    // Extensions: [VK_KHR_portability_enumeration]
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
        &applicationInfo,                                   // pApplicationInfo
        numLayers,                                          // enabledLayerCount
        ppLayerNames,                                       // ppEnabledLayerNames
        numExtensions,                                      // enabledExtensionCount
        ppExtensionNames                                    // ppEnabledExtensionNames
    };

    // Create Vulkan Instance
    const VkAllocationCallbacks* pHostMemoryAllocator = nullptr; // use Vulkan's internal allocator
    VkInstance vulkanInstance;
    VkResult instanceCreationResult = vkCreateInstance(
        &createInfo,
        pHostMemoryAllocator,
        &vulkanInstance
    );

    std::cout << "[vulkan]: vkcreateInstance result: " << instanceCreationResult << "\n";

    // ArenaAllocator Use: Subsystem-level Memory Management
    // top level: reserve 4 MB from the OS
    core::memory::BaseAllocator baseAllocator(4 * 1024 * 1024);

    // subsystem call: create a memory region with the right size from the shared base allocator
    core::memory::Region regionLog = baseAllocator.reserve(1024 * 1024);
    core::memory::Region regionVulkanConfig = baseAllocator.reserve(1024 * 1024);

    // Logging
    core::log::Logger logger(regionLog);

    gfx::vulkan::Configurator enumerator(regionVulkanConfig);
    core::log::Logger* pLogger = &logger;

    pLogger->log(core::log::debug("I have issues"));
    pLogger->log(core::log::error("[core/memory/stack_allocator]: out of capacity"));

    enumerator.setInstance(vulkanInstance);

    enumerator.enumeratePhysicalDevicesAndQueues();

    std::size_t numPhysicalDevices = enumerator.getNumPhysicalDevices();

    // for now, just pick the first available device
    VkPhysicalDevice physicalDevice = enumerator.getBestPhysicalDevice();

    std::span<const VkQueueFamilyProperties> queueFamilyProps = enumerator.getQueueFamilyProperties(physicalDevice);

    float priority = 1.0f;

    VkDeviceQueueCreateInfo deviceQueueCreateInfo {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        nullptr,
        0,
        0,
        1,
        &priority
    };

    // todo: require features
    //VkPhysicalDeviceFeatures pEnabledFeatures;

    VkDeviceCreateInfo logicalDeviceCreateInfo {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        nullptr,
        0,
        1,
        &deviceQueueCreateInfo,
        0,
        nullptr,
        0,
        nullptr,
        nullptr
    };

    VkDevice logicalDevice{};
    VkResult result = vkCreateDevice(
        physicalDevice,
        &logicalDeviceCreateInfo,
        nullptr,
        &logicalDevice
    );

    if(result != VK_SUCCESS) {
        std::cout << "failed to create logical device\n";
    }

    // layers
    core::u32 propertyCount{ 0 };
    result = vkEnumerateInstanceLayerProperties(
        &propertyCount,
        nullptr //&layerProperties
    );

    std::vector<VkLayerProperties> layerProperties(propertyCount);

    result = vkEnumerateInstanceLayerProperties(
        &propertyCount,
        layerProperties.data()
    );

    core::u32 numDeviceLayerProperties{ 0 };
    result = vkEnumerateDeviceLayerProperties(
        physicalDevice,
        &numDeviceLayerProperties,
        nullptr
    );

    layerProperties.resize(numDeviceLayerProperties);
    result = vkEnumerateDeviceLayerProperties(
        physicalDevice,
        &numDeviceLayerProperties,
        layerProperties.data()
    );

    core::u32 pCount{ 0 };
    std::vector<VkExtensionProperties> instanceExtensionProps;
    result = vkEnumerateInstanceExtensionProperties(
        nullptr,
        &pCount,
        nullptr
    );

    instanceExtensionProps.resize(pCount);
    result = vkEnumerateInstanceExtensionProperties(
        nullptr,
        &pCount,
        instanceExtensionProps.data()
    );

    std::vector<VkExtensionProperties> deviceExtensionProps;
    result = vkEnumerateDeviceExtensionProperties(
        physicalDevice,
        nullptr,
        &pCount,
        nullptr
    );

    deviceExtensionProps.resize(pCount);
    result = vkEnumerateDeviceExtensionProperties(
        physicalDevice,
        nullptr,
        &pCount,
        deviceExtensionProps.data()
    );

    PFN_vkVoidFunction instanceProcAddr = vkGetInstanceProcAddr(
        vulkanInstance,
        ""
    );

    return 0;
}
