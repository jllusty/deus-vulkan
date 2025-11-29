// context.hpp: gpu runtime context: manage devices, buffers, images, queues
#pragma once

#include "core/memory/stack_allocator.hpp"
#include "core/log/logging.hpp"
#include "gfx/vulkan/config.hpp"
#include <vulkan/vulkan_core.h>

namespace gfx::vulkan {

class GpuContext {
    core::memory::StackAllocator allocator;

    const Configurator& config;
    core::log::Logger& log;

    std::span<VkDevice> devices{};
    std::span<VkBuffer> buffers{};

    // todo: better way to store the names of extensions we enable (same as configurator)
    // these should be querable for each device
    std::span<char*> extensionNames{};
    std::span<char> extensionNamesData{};

public:
    GpuContext(core::memory::Region region, core::log::Logger& log, const Configurator& config)
        : allocator(region), log(log), config(config)
    {}

    // todo: device queue creation parameters
    // todo: create more than one device
    std::span<const VkDevice> createDevices(const PhysicalDeviceHandle physicalDeviceHandle, std::size_t count = 1) {
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

        // todo: lldb prints like 256 addresses when I tell it "fr v" of the propNames
        // query configurator to see if portability was set
        std::span<const std::string> propNames = config.getEnabledExtensionNames();
        core::u32 numExtensions{ 0 };
        for(const std::string& props : propNames) {
            if(strcmp(props.c_str(), VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0) {
                ++numExtensions;
            }
        }
        extensionNames = allocator.allocate<char*>(numExtensions);
        extensionNamesData = allocator.allocate<char>(numExtensions * VK_MAX_EXTENSION_NAME_SIZE);
        std::size_t ptrIndex{ 0 };
        std::size_t writeIndex{ 0 };
        for(const std::string& props : propNames) {
            if(strcmp(props.c_str(), VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0) {
                extensionNames[ptrIndex] = extensionNamesData.data() + writeIndex;
                strcpy(
                    extensionNames[ptrIndex],
                    "VK_KHR_portability_subset" // todo: add to constants?
                );
                logInfo("config instance has VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME enabled, adding VK_KHR_portability_subset to device extension create info");
            }
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
            .enabledExtensionCount = static_cast<core::u32>(extensionNames.size()),
            .ppEnabledExtensionNames = extensionNames.data(),
            .pEnabledFeatures = nullptr
        };

        devices = allocator.allocate<VkDevice>(count);

        std::optional<const VkPhysicalDevice> physicalDeviceOpt = config.getVulkanPhysicalDevice(physicalDeviceHandle);
        if(!physicalDeviceOpt.has_value()) {
            logError("no valid vulkan physical devices to create a logical device with");
            return {};
        }
        const VkPhysicalDevice physicalDevice = *physicalDeviceOpt;
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

        logInfo("created a logical device");
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

    const VkBuffer* createBuffer(core::u32 sizeBytes) noexcept {
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

        buffers = allocator.allocate<VkBuffer>(1);
        VkResult result = vkCreateBuffer(
            devices.front(),
            &bufferCreateInfo,
            nullptr,
            buffers.data()
        );

        if(result != VK_SUCCESS) {
            log.error("main", "buffer creation failed\n");
        }

        return buffers.data();
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
