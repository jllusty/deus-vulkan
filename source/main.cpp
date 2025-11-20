// main.cpp
#include <cstddef>
#include <iostream>
#include <vector>

#include <vulkan/vulkan.h>
#include <SFML/Window.hpp>

#include "core/memory/base_allocator.hpp"
#include "core/memory/types.hpp"
#include "core/types.hpp"

#include "gfx/vulkan/device_explorer.hpp"


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
    core::memory::Region region = baseAllocator.reserve(1024 * 1024);
    gfx::vulkan::PhysicalDeviceEnumerator enumerator(vulkanInstance, region);

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
        std::cout << "old shit\n";
    }

    return 0;
}
