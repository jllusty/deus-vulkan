#pragma once

#include "core/memory/stack_allocator.hpp"
#include "core/log/logging.hpp"
#include "gfx/vulkan/config.hpp"

namespace gfx::vulkan {

class GpuContext {
    core::memory::StackAllocator allocator;

    const Configurator& config;
    core::log::Logger& log;

    std::span<VkDevice> devices{};
    std::span<VkBuffer> buffers{};

public:
    GpuContext(core::memory::Region region, core::log::Logger& log, const Configurator& config)
        : allocator(region), log(log), config(config)
    {}

    // todo: device queue creation parameters
    // todo: create more than one device
    std::span<const VkDevice> createDevices(const VkPhysicalDevice physicalDevice, std::size_t count = 1) {
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

        devices = allocator.allocate<VkDevice>(count);

        VkDevice logicalDevice{};
        VkResult result = vkCreateDevice(
            physicalDevice,
            &logicalDeviceCreateInfo,
            nullptr,
            devices.data()
        );

        if(result != VK_SUCCESS) {
            logError("failed to create a logical device");
            return {};
        }

        return devices;
    }

    bool destroyDevices() noexcept {
        for(std::size_t i = 0; i < devices.size(); ++i) {
            // wait until the device is idle
            VkResult result = vkDeviceWaitIdle(devices[i]);

            if(result != VK_SUCCESS) {
                logError("could not wait until logical device was idle for deletion");
                return false;
            }

            vkDestroyDevice(
                devices[i],
                nullptr
            );
        }

        devices = {};
        logInfo("destroyed all logical devices");
        return true;
    }

private:
    // log convenience
    template<typename... Args>
    void logError(const char* msg, Args... args) {
        log.error("vulkan/context", msg, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void logDebug(const char* msg, Args... args) {
        log.debug("vulkan/context", msg, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void logInfo(const char* msg, Args... args) {
        log.info("vulkan/context", msg, std::forward<Args>(args)...);
    }

};

}
