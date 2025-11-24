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

    // query device properties, memory, and queues
    std::span<const VkPhysicalDevice> physicalDevices = config.enumeratePhysicalDevices();
    std::span<const VkPhysicalDeviceProperties> deviceProps = config.enumeratePhysicalDeviceProperties();
    std::span<const VkPhysicalDeviceMemoryProperties> deviceMemoryProps = config.enumeratePhysicalDeviceMemoryProperties();
    std::span<const VkQueueFamilyProperties> queueFamilyProps = config.enumerateQueueFamilyProperties();

    // pick a physical device
    std::optional<const VkPhysicalDevice> bestPhysicalDevice = config.getBestPhysicalDevice();
    if(!bestPhysicalDevice.has_value()) {
        pLogger->error("[main]: could not select a physical device\n");
        return -1;
    }
    const VkPhysicalDevice physicalDevice = *bestPhysicalDevice;

    // device-level extensions
    std::span<const VkExtensionProperties> deviceExtensions = config.getAvailableDeviceExtensionProperties(physicalDevice);

    // create devices
    std::span<const VkDevice> devices = config.createLogicalDevices(physicalDevice);

    // destruction order: devices -> instance
    bool destroyDevices = config.destroyLogicalDevices();
    bool destroyInstance = config.destroyInstance();

    /*
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

/*     // destory vulkan instance
    logger.log(core::log::debug("[main]: destroying vulkan instance..."));
    vkDestroyInstance(
        vulkanInstance,
        nullptr
    );
    logger.log(core::log::debug("[main]: vulkan instance destroyed"));
    */

    return 0;
}
