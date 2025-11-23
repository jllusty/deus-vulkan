// main.cpp

#include <span>

#include <sys/wait.h>
#include <vulkan/vulkan.h>
#include <SFML/Window.hpp>
#include <vulkan/vulkan_core.h>

#include "core/types.hpp"
#include "core/memory/base_allocator.hpp"
#include "core/memory/types.hpp"
#include "core/log/logging.hpp"

#include "gfx/vulkan/config.hpp"

int main()
{
    // ArenaAllocator Use: Subsystem-level Memory Management
    // top level: reserve 4 MB from the OS
    core::memory::BaseAllocator baseAllocator(4 * 1024 * 1024);

    // subsystem call: create a memory region with the right size from the shared base allocator
    core::memory::Region regionLog = baseAllocator.reserve(1024 * 1024);
    core::memory::Region regionVulkanConfig = baseAllocator.reserve(1024 * 1024);

    // Logging
    core::log::Logger logger(regionLog);
    core::log::Logger* pLogger = &logger;

    // Vulkan Configurator
    gfx::vulkan::Configurator config(regionVulkanConfig, pLogger);

    // instance-level vulkan api
    auto availableInstanceAPI = config.getAvailableInstanceVersion();
    if(!availableInstanceAPI) {
        pLogger->log<core::log::Level::error>("[main]: could not retrieve vulkan api version");
        return -1;
    }
    core::u32 apiVersion = *availableInstanceAPI;

    logger.info(
        "[main]: fetched instance-level vulkan api version: %d.%d",
        VK_VERSION_MAJOR(apiVersion),
        VK_VERSION_MINOR(apiVersion)
    );

    // available instance-level layers and extensions
    std::span<const VkLayerProperties> layerProps = config.getAvailableInstanceLayerProperties();
    std::span<const VkExtensionProperties> extensionProps = config.getAvailableInstanceExtensionProperties();

    std::optional<const VkInstance> createdInstance = config.createInstance(
        "Vulkan Application",
        "deus-vulkan",
        apiVersion,
        {},
        { VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME }
    );
    if(!createdInstance.has_value()) {
        logger.error(
            "[main]: failed to create vulkan instance"
        );
        return -1;
    }
    const VkInstance instance = *createdInstance;

    std::span<const VkPhysicalDevice> physicalDevices = config.enumeratePhysicalDevices();

    bool destroyInstance = config.destroyInstance();

    // available device-level extensions

    /*
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
    */

    // destroy vulkan logical device(s)

    /*
    logger.log(core::log::debug("[main]: waiting until logical device is idle..."));
    result = vkDeviceWaitIdle(
        logicalDevice
    );
    logger.log(core::log::debug("[main]: logical device is idle, destroying..."));
    vkDestroyDevice(
        logicalDevice,
        nullptr
    );
    logger.log(core::log::debug("[main]: logical device destroyed"));

    // destory vulkan instance
    logger.log(core::log::debug("[main]: destroying vulkan instance..."));
    vkDestroyInstance(
        vulkanInstance,
        nullptr
    );
    logger.log(core::log::debug("[main]: vulkan instance destroyed"));
    */

    return 0;
}
