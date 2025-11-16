// main.cpp
#include <cstddef>
#include <iostream>

#include <vulkan/vulkan.h>
#include <SFML/Window.hpp>

#include "core/memory/os_memory.hpp"
#include "core/types.hpp"


int main()
{
    // Application Info: Describes the application
    static core::u32 vulkanApiVersion = VK_MAKE_API_VERSION(0,1,0,0);
    static const VkApplicationInfo applicationInfo {
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
    static const core::u32 numLayers{ 0 };
    static const char* const* ppLayerNames { nullptr };
    // Extensions: [VK_KHR_portability_enumeration]
    //
    // 1. VK_KHR_portability_enumeration: used for Mac OS X development environment.
    // -> enumerate available Vulkan Portability-compliant physical devices and groups
    // in addition to the Vulkan physical devices and groups that are enumerated by default.
    static const core::u32 numExtensions{ 1 };
    static const char* const ppExtensionNames[] { VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME };
    static const VkInstanceCreateInfo createInfo {
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
    static VkResult instanceCreationResult = vkCreateInstance(
        &createInfo,
        pHostMemoryAllocator,
        &vulkanInstance
    );

    std::cout << "[vulkan]: vkcreateInstance result: " << instanceCreationResult << "\n";

    core::u32 numPhysicalDevices{};
    static VkResult enumeratePhysicalDevicesResult = vkEnumeratePhysicalDevices(
        vulkanInstance,
        &numPhysicalDevices,
        nullptr
    );

    std::cout << "[vulkan]: physical devices result: " << enumeratePhysicalDevicesResult << "\n";
    std::cout << "[vulkan]: physical devices: " << numPhysicalDevices << "\n";

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

    // Core - OS Allocator
    core::memory::OsArenaAllocator osAllocator{};

    core::memory::OsArena mem1 = osAllocator.reserve(1);

    *mem1.pBase = std::byte{1};

    core::memory::OsArena mem2 = osAllocator.reserve(1);

    osAllocator.release(mem2);
    osAllocator.release(mem1);

    return 0;
}
