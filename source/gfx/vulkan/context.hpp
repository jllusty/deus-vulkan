// context.hpp: gpu runtime context: manage devices, buffers, images, queues
#pragma once

#include "core/log/logging.hpp"
#include "gfx/vulkan/config.hpp"
#include "gfx/vulkan/resources.hpp"

#include <vulkan/vulkan_core.h>

#include <vk_mem_alloc.h>

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
        log.error("gfx/vulkan/device","destroyed logical device");
    }

    const VkDevice& get() const noexcept {
        return device;
    }
};

// RAII Owner for VmaAllocator
class Allocator {
    VmaAllocatorCreateInfo info{};
    VmaAllocator allocator{};

    core::log::Logger& log;

public:
    // no default c'tor
    Allocator() = delete;
    // no copying
    Allocator(Allocator& other) = delete;
    // no moving
    Allocator(Allocator&& other) = delete;

    Allocator(const VmaAllocatorCreateInfo& info, core::log::Logger& log)
        : log(log) {
        VkResult res = vmaCreateAllocator(&info, &allocator);
        if(res != VK_SUCCESS) {
            // todo: log, or maybe scream
            allocator = VK_NULL_HANDLE;
        }
        log.info("gfx/vulkan/allocator","created an allocator");
    }

    ~Allocator() {
        if(allocator != VK_NULL_HANDLE) {
            vmaDestroyAllocator(allocator);
        }
        log.info("gfx/vulkan/allocator","destroyed an allocator");
    }

    const VmaAllocator& get() const noexcept {
        return allocator;
    }
};


class GpuContext {
    std::vector<BufferHandle> bufferHandles{};

    core::log::Logger& log;
    const Configurator& config;
    Device device;
    Allocator allocator;
    ResourceManager manager;

public:
    GpuContext(PhysicalDeviceHandle physicalDeviceHandle, core::log::Logger& log, const Configurator& config)
        : log(log), config(config), device(log,config,physicalDeviceHandle),
        allocator({
                .physicalDevice = *config.getVulkanPhysicalDevice(physicalDeviceHandle),
                .device = device.get(),
                .instance       = *config.getVulkanInstance(),
                // note: from what I can tell, MoltenVK breaks a call to vkGetBufferMemoryRequirements2KHR
                // so we have to force it not to use that procedure. We set vma_impl.cpp with
                // VMA_DEDICATED_ALLOCATION = 0 to avoid that path during buffer allocation
                .vulkanApiVersion = VK_API_VERSION_1_0
            }, log), manager(config,allocator.get(),log)
    {}

    ~GpuContext() {}

    template<size_t N>
    void CmdBuffers(const std::array<int16_t, N>& heightData) {
        // create image to store heightmap
        std::optional<const ImageHandle> imageHandle = manager.createImage(N,N,1);

        // create buffer to hold grid mesh vertex data
        std::optional<const BufferHandle> bufferHandleSrc = manager.createMappedVertexBuffer(N);
        std::optional<const BufferHandle> bufferHandleDst = manager.createDeviceLocalVertexBuffer(N);

        bool success = manager.fillMemoryMappedBuffer(*bufferHandleSrc, heightData.data(), sizeof(heightData));

        auto physicalDevice = *config.getBestPhysicalDevice();
        auto queueFamilyProps = config.getQueueFamilyProperties(physicalDevice);

        VkDevice vulkanDevice = device.get();
        VkQueue pQueue{};
        vkGetDeviceQueue(
            vulkanDevice,
            0,
            0,
            &pQueue
        );

        if(pQueue == nullptr) {
            logError("failed to fetch a device queue");
            return;
        }

        const VkCommandPoolCreateInfo cmdPoolCreateInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = 0
        };

        VkCommandPool cmdPool{};
        VkResult result = vkCreateCommandPool(
            vulkanDevice,
            &cmdPoolCreateInfo,
            nullptr,
            &cmdPool
        );

        if(result != VK_SUCCESS) {
            logError("could not create a command pool");
            return;
        }

        const VkCommandBufferAllocateInfo cmdBufferAllocInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = cmdPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        VkCommandBuffer cmdBuffer{};
        result = vkAllocateCommandBuffers(
            vulkanDevice,
            &cmdBufferAllocInfo,
            &cmdBuffer
        );

        if(result != VK_SUCCESS) {
            logError("could not allocate a command pool");
            return;
        }

        VkCommandBufferBeginInfo cmdBufferBeginInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = 0,
            .pInheritanceInfo = nullptr
        };

        result = vkBeginCommandBuffer(
            cmdBuffer,
            &cmdBufferBeginInfo
        );

        if(result != VK_SUCCESS) {
            logError("could not begin a command buffer");
            return;
        }

        Buffer bufferSrc = *manager.getBuffer(*bufferHandleSrc);
        Buffer bufferDst = *manager.getBuffer(*bufferHandleDst);

        const VkBufferCopy bufferCpy {
            .srcOffset = 0,// vertexBufferSrc.allocationInfo.offset,
            .dstOffset = 0,//vertexBufferDst.allocationInfo.offset,
            .size = bufferSrc.size
        };

        // copy from buffer 1 to 2
        vkCmdCopyBuffer(
            cmdBuffer,
            bufferSrc.buffer,
            bufferDst.buffer,
            1,
            &bufferCpy
        );

        result = vkEndCommandBuffer(cmdBuffer);

        if(result != VK_SUCCESS) {
            logError("could not end command buffer");
            return;
        }

        const VkSubmitInfo submitInfo {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = nullptr,
            .pWaitDstStageMask = nullptr,
            .commandBufferCount = 1,
            .pCommandBuffers = &cmdBuffer,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores = nullptr
        };

        result = vkQueueSubmit(
            pQueue,
            1,
            &submitInfo,
            VK_NULL_HANDLE
        );

        if(result != VK_SUCCESS) {
            logError("could not submit queue");
            return;
        }

        result = vkQueueWaitIdle(pQueue);

        // free command buffers
        vkFreeCommandBuffers(
            vulkanDevice,
            cmdPool,
            1,
            &cmdBuffer
        );
        logInfo("freed command buffers");

        // destroy command pools
        vkDestroyCommandPool(
            vulkanDevice,
            cmdPool,
            nullptr
        );
        logInfo("destroyed command pools");
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
