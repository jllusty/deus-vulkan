// context.hpp: gpu runtime context: manage devices, buffers, images, queues
#pragma once

#include "core/log/logging.hpp"
#include "gfx/vulkan/config.hpp"

#include <vulkan/vulkan_core.h>

#include <vk_mem_alloc.h>

namespace gfx::vulkan {

struct BufferHandle {
    std::size_t id{ 0 };
};

class GpuContext {
    struct VertexBuffer {
        VkBuffer buffer{};
        VmaAllocation allocation{};
        std::uint64_t count{ 0 };
    };

    const Configurator& config;
    core::log::Logger& log;

    VmaAllocator allocator{};

    // logical device handles - index into devices
    // std::vector<LogicalDeviceHandle> deviceHandles{};
    // todo: still expose those, but each needs integration with a specific
    //       vma allocator
    std::vector<VkDevice> devices{};
    // buffer handles - index into buffers
    std::vector<BufferHandle> bufferHandles{};
    std::vector<VertexBuffer> buffers{};

    std::vector<std::string> extensionNames{};

public:
    GpuContext(PhysicalDeviceHandle physicalDeviceHandle, core::log::Logger& log, const Configurator& config)
        : log(log), config(config)
    {
        // create a logical device for use with the vma allocator
        VmaAllocatorCreateInfo info{};
        info.instance       = *config.getVulkanInstance();
        info.physicalDevice = *config.getVulkanPhysicalDevice(physicalDeviceHandle);
        info.device         = *createDevice(physicalDeviceHandle);
        // note: from what I can tell, MoltenVK breaks a call to vkGetBufferMemoryRequirements2KHR
        // so we have to force it not to use that procedure. We set vma_impl.cpp with
        // VMA_DEDICATED_ALLOCATION = 0 to avoid the path during buffer allocation
        info.vulkanApiVersion = VK_API_VERSION_1_0; // VK_VERSION_1_0; //*config.getVulkanAPI();

        VkResult res = vmaCreateAllocator(&info, &allocator);
        if (res != VK_SUCCESS) {
            logError("vmaCreateAllocator failed");
        }
    }

    ~GpuContext() {
        // destroy buffers
        destroyBuffers();

        // destroy vma allocator
        vmaDestroyAllocator(allocator);

        // destroy devices
        destroyDevices();
    }

    // todo: device queue creation parameters
    std::optional<const BufferHandle> createBuffer(core::u32 sizeBytes) noexcept {
        const VkBufferCreateInfo bufferCreateInfo {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .size = sizeBytes,
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr
        };

        VmaAllocationCreateInfo allocInfo {};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

        VkBuffer buffer;
        VmaAllocation allocation;
        VkResult result = vmaCreateBuffer(allocator, &bufferCreateInfo, &allocInfo, &buffer, &allocation, nullptr);

        if(result != VK_SUCCESS) {
            logError("buffer creation failed\n");
            return std::nullopt;
        }

        VertexBuffer vertexBuffer {
            .buffer = buffer,
            .allocation = allocation,
            .count = sizeBytes
        };

        bufferHandles.push_back({.id = buffers.size()});
        buffers.push_back(vertexBuffer);

        logInfo("created a new buffer (%lu)", bufferHandles.back().id);

        return bufferHandles.back();
    }

private:
    // used once at instantiation
    std::optional<const VkDevice> createDevice(const PhysicalDeviceHandle physicalDeviceHandle) {
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

        // needed for vulkan memory allocator
        extensionNames.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        extensionNames.push_back(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
        extensionNames.push_back(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);

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

        devices.push_back(device);
        logInfo("created a logical device");
        return devices.back();
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
            vmaDestroyBuffer(allocator, buffers[i].buffer, buffers[i].allocation);
        }

        buffers = {};
        logInfo("destroyed all buffers");
    }



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
