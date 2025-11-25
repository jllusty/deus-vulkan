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
#include "gfx/vulkan/context.hpp"

int main()
{
    // ArenaAllocator Use: Subsystem-level Memory Management
    // top level: reserve 4 MB from the OS
    core::memory::BaseAllocator baseAllocator(4 * 1024 * 1024);

    // subsystem call: create a memory region with the right size from the shared base allocator
    core::memory::Region regionLog = baseAllocator.reserve(1024 * 1024);
    core::memory::Region regionVulkanConfig = baseAllocator.reserve(1024 * 1024);
    core::memory::Region regionVulkanContext = baseAllocator.reserve(1024 * 1024);

    // Logging
    int val = 3, uval = -3;
    core::log::Logger log(regionLog);

    // Vulkan Configurator
    gfx::vulkan::InstanceRequest instanceRequest {
        .requiredLayerNames = {},
        .requiredExtensionNames = {},
        .optionalLayerNames = {},
        .optionalExtensionNames = {},
    };
    std::optional<gfx::vulkan::Configurator> optConfig = gfx::vulkan::Configurator::create(regionVulkanConfig, instanceRequest, log);
    if(!optConfig.has_value()) {
        log.error("main","could not configurate vulkan");
        return -1;
    }
    const gfx::vulkan::Configurator& config = *optConfig;

    // pick a physical device
    std::optional<const VkPhysicalDevice> bestPhysicalDevice = config.getBestPhysicalDevice();
    if(!bestPhysicalDevice.has_value()) {
        log.error("main", "could not select a physical device");
        return -1;
    }
    const VkPhysicalDevice physicalDevice = *bestPhysicalDevice;

    log.info("main","I chose physical device %d", reinterpret_cast<std::size_t>(physicalDevice));

    // create gpu context
    gfx::vulkan::GpuContext context {
        regionVulkanContext,
        log,
        config
    };
    std::span<const VkDevice> devices = context.createDevices(physicalDevice);

    // destruction order: devices -> instance
    bool destroyDevices = context.destroyDevices();

    return 0;
}
