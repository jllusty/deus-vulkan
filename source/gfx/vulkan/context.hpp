// context.hpp: gpu runtime context: manage devices, buffers, images, queues
#pragma once

#include "core/log/logging.hpp"
#include "gfx/vulkan/config.hpp"
#include <vulkan/vulkan_core.h>

namespace gfx::vulkan {

struct LogicalDeviceHandle {
    std::size_t id{ 0 };
};

struct BufferHandle {
    std::size_t id{ 0 };
};

class GpuContext {
    const Configurator& config;
    core::log::Logger& log;

    // logical device handles - index into devices
    std::vector<LogicalDeviceHandle> deviceHandles{};
    std::vector<VkDevice> devices{};
    // buffer handles - index into buffers
    std::vector<BufferHandle> bufferHandles{};
    std::vector<VkBuffer> buffers{};

    std::vector<std::string> extensionNames{};

public:
    GpuContext(core::log::Logger& log, const Configurator& config)
        : log(log), config(config)
    {}

    // todo: device queue creation parameters
    std::optional<const LogicalDeviceHandle> createDevice(const PhysicalDeviceHandle physicalDeviceHandle) {
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
                logInfo("config instance has VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME enabled, adding VK_KHR_portability_subset to device extension create info");
                extensionNames.push_back("VK_KHR_portability_subset");
            }
        }

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

        VkDevice device{};

        std::optional<const VkPhysicalDevice> physicalDeviceOpt = config.getVulkanPhysicalDevice(physicalDeviceHandle);
        if(!physicalDeviceOpt.has_value()) {
            logError("no valid vulkan physical devices to create a logical device with");
            return std::nullopt;
        }
        const VkPhysicalDevice physicalDevice = *physicalDeviceOpt;
        VkResult result = vkCreateDevice(
            physicalDevice,
            &logicalDeviceCreateInfo,
            nullptr,
            &device
        );

        if(result != VK_SUCCESS) {
            logError("failed to create a logical device");
            return std::nullopt;
        }

        deviceHandles.push_back({.id = devices.size()});
        devices.push_back(device);
        logInfo("created a logical device (%lu)", deviceHandles.back().id);
        return deviceHandles.back();
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

    void destroyBuffers() noexcept {
        for(std::size_t i = 0; i < buffers.size(); ++i) {
            vkDestroyBuffer(
                devices[i],
                buffers[i],
                nullptr
            );
        }

        buffers = {};
        logInfo("destroyed all buffers");
    }

    std::optional<const BufferHandle> createBuffer(LogicalDeviceHandle deviceHandle, core::u32 sizeBytes) noexcept {
        const VkBufferCreateInfo bufferCreateInfo {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .size = sizeBytes,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr
        };

        const VkDevice device = devices.at(deviceHandle.id);

        VkBuffer buffer{};
        VkResult result = vkCreateBuffer(
            device,
            &bufferCreateInfo,
            nullptr,
            &buffer
        );

        if(result != VK_SUCCESS) {
            log.error("main", "buffer creation failed\n");
        }

        bufferHandles.push_back({.id = buffers.size()});
        buffers.push_back(buffer);

        logInfo("created a new buffer (%lu) on logical device (%lu)", bufferHandles.back().id, deviceHandle.id);

        return bufferHandles.back();
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
