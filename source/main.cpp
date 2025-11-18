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

    // enumerate our physical devices
    //
    // the first call just queries the number of physical devices so we
    // can reserve that space on our side.
    //
    // the second call retries that many VkPhysicalDevices
    core::u32 numPhysicalDevices{ 0 };
    VkPhysicalDevice pPhysicalDevices{ nullptr };
    VkResult enumeratePhysicalDevicesResult = vkEnumeratePhysicalDevices(
        vulkanInstance,
        &numPhysicalDevices,
        nullptr
    );

    if(enumeratePhysicalDevicesResult != VK_SUCCESS) {
        assert(false && "[vulkan]: could not enumerate physical devices");
    }

    std::cout << "[vulkan]: physical devices result: " << enumeratePhysicalDevicesResult << "\n";
    std::cout << "[vulkan]: physical devices: " << numPhysicalDevices << "\n";

    // just use STL allocators for now while we build up core's memory capabilities
    constexpr const std::size_t maxNumPhysicalDevices = 8;
    std::vector<VkPhysicalDevice> physicalDevices{ maxNumPhysicalDevices };

    enumeratePhysicalDevicesResult = vkEnumeratePhysicalDevices(
        vulkanInstance,
        &numPhysicalDevices,
        &physicalDevices[0]
    );

    if(enumeratePhysicalDevicesResult != VK_SUCCESS) {
        assert(false && "[vulkan]: could not enumerate physical devices");
    }

    // get physical device properties with the VkPhysicalDevice handles
    std::vector<VkPhysicalDeviceProperties> physicalDeviceProperties{ maxNumPhysicalDevices };
    std::vector<VkPhysicalDeviceFeatures> physicalDeviceFeatures{ maxNumPhysicalDevices };
    std::vector<VkPhysicalDeviceMemoryProperties> physicalDeviceMemoryProperties{ maxNumPhysicalDevices };
    for(std::size_t i = 0; i < numPhysicalDevices; ++i) {
        vkGetPhysicalDeviceProperties(
            physicalDevices[i],
            &physicalDeviceProperties[i]
        );
        vkGetPhysicalDeviceFeatures(
            physicalDevices[i],
            &physicalDeviceFeatures[i]
        );
        vkGetPhysicalDeviceMemoryProperties(
            physicalDevices[i],
            &physicalDeviceMemoryProperties[i]
        );

        // queues for workloads distributed across the device
        constexpr std::size_t maxNumFamilyProperties{ 32 };
        std::vector<VkQueueFamilyProperties> queueFamilyProperties{ maxNumFamilyProperties };
        core::u32 numQueueFamilyProperties{ 0 };
        vkGetPhysicalDeviceQueueFamilyProperties(
            physicalDevices[i],
            &numQueueFamilyProperties,
            nullptr
        );

        vkGetPhysicalDeviceQueueFamilyProperties(
            physicalDevices[i],
            &numQueueFamilyProperties,
            &queueFamilyProperties[0]
        );

        std::cout << "there are " << numQueueFamilyProperties << " queues on the physical device\n";

        VkDeviceQueueCreateInfo deviceQueueCreateInfo {
            VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            nullptr,
            0,
            0,
            queueFamilyProperties[0].queueCount,
            nullptr
        };

        // we can differentiate between optional and required
        // features of the physical device by our call to createInfo
        VkPhysicalDeviceFeatures requiredFeatures{};
        requiredFeatures.tessellationShader = VK_TRUE;

        VkDeviceCreateInfo createInfo {
            VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            nullptr,
            0,
            1,
            &deviceQueueCreateInfo,
            0,
            nullptr,
            numExtensions,
            ppExtensionNames,
            &requiredFeatures
        };

        VkDevice device{};
        VkResult res = vkCreateDevice(
            physicalDevices[i],
            &createInfo,
            nullptr,
            &device
        );

        if(res != VK_SUCCESS) {
            // todo: handle this
            //assert(false && "vulkan: could not create logical device");
        }
    }

    // SFML Windowing Context
    //sf::Window window(sf::VideoMode({800u,600u}), "Hi Chris\n");
    //while(window.isOpen()) {
    //    // poll events
    //    while(const std::optional<sf::Event> event = window.pollEvent())
    //    {
    //        // close event
    //        if(event->is<sf::Event::Closed>()) {
    //            window.close();
    //        }
    //    }
    //
    //    window.display();
    //}

    // ArenaAllocator Use: Subsystem-level Memory Management
    {
        // top level: reserve 4 MB from the OS
        core::memory::BaseAllocator baseAllocator(4 * 1024 * 1024);

        // subsystem call: create a memory region with the right size from the shared base allocator
        core::memory::MemoryRegion region = baseAllocator.reserve(1024 * 1024);
        gfx::vulkan::PhysicalDeviceEnumerator(vulkanInstance, region);
    }

    return 0;
}
