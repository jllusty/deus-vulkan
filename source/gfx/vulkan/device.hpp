#pragma once

#include <vulkan/vulkan.h>

#include "core/log/logging.hpp"
#include "gfx/vulkan/config.hpp"

namespace gfx::vulkan {

// Owner for VkDevice
class Device {
    std::vector<std::string> extensionNames{};
    VkDevice device{};

    core::log::Logger& log;

public:
    Device() = delete;
    Device(core::log::Logger& log, const Configurator& config, const PhysicalDeviceHandle& physicalDeviceHandle)
        : log(log)
    {
        float priority = 1.0f;

        // todo: query directly from the configurator
        VkDeviceQueueCreateInfo deviceQueueCreateInfo {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = 0,
            .queueCount = 1,
            .pQueuePriorities = &priority
        };

        // query configurator to see if portability was set
        std::span<const std::string> propNames = config.getEnabledExtensionNames();
        for(const std::string& props : propNames) {
            if(strcmp(props.c_str(), VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0) {
                log.info("gfx/vulkan/Device", "config instance has VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME enabled, adding VK_KHR_portability_subset to device extension create info");
                // todo: magic constant
                extensionNames.push_back("VK_KHR_portability_subset");
            }
        }
        // enable creating swapchains
        extensionNames.push_back("VK_KHR_swapchain");
        log.info("gfx/vulkan/Device","enabling VK_KHR_swapchain");

        // create buffer for extension name pointers
        std::vector<const char*> extensionNamePtrs{};
        for(std::string& name : extensionNames) {
            extensionNamePtrs.push_back(name.c_str());
        }

        // todo: require features
        //VkPhysicalDeviceFeatures pEnabledFeatures;
        VkDeviceCreateInfo logicalDeviceCreateInfo {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &deviceQueueCreateInfo,
            .enabledLayerCount = 0,
            .ppEnabledLayerNames = nullptr,
            .enabledExtensionCount = static_cast<core::u32>(extensionNamePtrs.size()),
            .ppEnabledExtensionNames = extensionNamePtrs.data(),
            .pEnabledFeatures = nullptr
        };

        std::optional<const VkPhysicalDevice> physicalDeviceOpt = config.getVulkanPhysicalDevice(physicalDeviceHandle);
        if(!physicalDeviceOpt.has_value()) {
            log.error("gfx/vulkan/device","no valid vulkan physical devices to create a logical device with");
            device = VK_NULL_HANDLE;
            return;
        }
        const VkPhysicalDevice physicalDevice = *physicalDeviceOpt;
        VkResult result = vkCreateDevice(
            physicalDevice,
            &logicalDeviceCreateInfo,
            nullptr,
            &device
        );

        if(result != VK_SUCCESS) {
            log.error("gfx/vulkan/device","failed to create a logical device");
            device = VK_NULL_HANDLE;
            return;
        }

        log.info("gfx/vulkan/device","created a logical device");
    }

    ~Device() {
        // wait until the device is idle
        VkResult result = vkDeviceWaitIdle(device);

        if(result != VK_SUCCESS) {
            log.error("gfx/vulkan/device","could not wait until logical device was idle for deletion");
            // todo: defer to validation cleanup? is this recoverable?
            return;
        }

        vkDestroyDevice(
            device,
            nullptr
        );
        log.info("gfx/vulkan/device","destroyed logical device");
    }

    const VkDevice& get() const noexcept {
        return device;
    }
};

}
