#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include "vk_mem_alloc.h"

#include "core/log/logging.hpp"
#include "gfx/vulkan/config.hpp"

namespace gfx::vulkan {

struct BufferHandle {
    std::size_t id{ 0 };
};

struct Buffer {
    VkBuffer buffer{};
    VmaAllocation allocation{};
    VmaAllocationInfo allocationInfo{};
    std::size_t size{ 0 };
};

struct ImageHandle {
    std::size_t id{ 0 };
};

struct Image {
    VkImage        image{};        // The actual Vulkan image handle
    VmaAllocation  allocation{};   // Memory allocation (same idea as Buffer)
    VkImageView    view{};         // How shaders see the image

    VkFormat       format{};       // e.g. VK_FORMAT_R16_UNORM, R32_SFLOAT, etc.
    VkExtent3D     extent{};       // { width, height, 1 } for a 2D image
    uint32_t       mipLevels{1};   // 1 if you don't use mipmapping
    uint32_t       arrayLayers{1}; // 1 for a single heightmap

    VkImageLayout  currentLayout{VK_IMAGE_LAYOUT_UNDEFINED}; // Track transitions
};

// lacking a fitting name
class ResourceManager {
    const Configurator& config;
    const VmaAllocator& allocator;
    const VkDevice device;
    core::log::Logger& log;

    std::vector<Buffer> buffers{};
    std::vector<Image> images{};

public:
    ResourceManager(const Configurator& config, const VmaAllocator& allocator, const VkDevice& device, core::log::Logger& log)
        : config(config), allocator(allocator), device(device), log(log)
    {}

    ~ResourceManager() {
        destroyBuffers();
        destroyImages();
    }

    std::optional<Buffer> getBuffer(BufferHandle handle) const noexcept {
        std::optional<Buffer> result{};
        if(handle.id >= buffers.size()) {
            logError("attempt to fetch buffer with array index (%lu) when only (%lu) buffers exist", handle.id, buffers.size());
            return result;
        }
        result.emplace(buffers[handle.id]);
        return result;
    }

    std::optional<Image> getImage(ImageHandle handle) const noexcept {
        std::optional<Image> result{};
        if(handle.id >= images.size()) {
            logError("attempt to fetch image with array index (%lu) when only (%lu) buffers exist", handle.id, buffers.size());
            return result;
        }
        result.emplace(images[handle.id]);
        return result;
    }

    // creates a device local buffer (not host visible, needs staging upload)
    std::optional<BufferHandle> createDeviceLocalVertexBuffer(std::size_t sizeBytes) noexcept {
        const VkBufferCreateInfo bufferCreateInfo {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .size = sizeBytes,
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr
        };
        VmaAllocationCreateInfo allocCreateInfo {
            .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        };
        return createBuffer(sizeBytes, bufferCreateInfo, allocCreateInfo);
    }

    std::optional<BufferHandle> createMappedVertexBuffer(std::size_t sizeBytes) noexcept {
        const VkBufferCreateInfo bufferCreateInfo {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .size = sizeBytes,
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr
        };
        VmaAllocationCreateInfo allocCreateInfo {
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        };
        return createBuffer(sizeBytes, bufferCreateInfo, allocCreateInfo);
    }

    // creates a host-visible mapped buffer: used for staging an upload
    std::optional<BufferHandle> createStagingBuffer(core::u32 sizeBytes) noexcept {
        const VkBufferCreateInfo bufferCreateInfo {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .size = sizeBytes,
            .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr
        };
        VmaAllocationCreateInfo allocCreateInfo {
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        };
        return createBuffer(sizeBytes, bufferCreateInfo, allocCreateInfo);
    }

    std::optional<ImageHandle> createImage(core::u32 width, core::u32 height, core::u32 depth) noexcept {
        std::optional<ImageHandle> resultImage{};

        VkExtent3D extent{ width, height, 1 };
        VkImageCreateInfo createInfo {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = VK_FORMAT_R16_SINT,
            .extent = VkExtent3D{ width, height, 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };

        VkImage vulkanImage{};
        VmaAllocationCreateInfo allocInfo {
            .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        };
        VmaAllocation allocation{};
        VkResult result = vmaCreateImage(allocator, &createInfo, &allocInfo, &vulkanImage, &allocation, nullptr);
        if(result != VK_SUCCESS) {
            logError("could not create image");
            return resultImage;
        }

        // though an image view is a derived object, we return it from the resource manager
        VkImageSubresourceRange subRange {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        };
        VkImageViewCreateInfo viewCreateInfo {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = vulkanImage,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = VK_FORMAT_R16_SINT,
            .subresourceRange = subRange
        };
        VkImageView imageView{};
        result = vkCreateImageView(
            device,
            &viewCreateInfo,
            nullptr,
            &imageView
        );
        if(result != VK_SUCCESS) {
            logError("could not create image view");
            return resultImage;
        }

        Image image {
            .image = vulkanImage,
            .allocation = allocation,
            .view = imageView
        };
        ImageHandle handle {
            .id = images.size()
        };
        images.push_back(image);

        resultImage.emplace(handle);
        logInfo("created a new image (%lu) and view", handle.id);
        return resultImage;
    }

private:
    std::optional<BufferHandle> createBuffer(std::size_t sizeBytes, const VkBufferCreateInfo& bufferCreateInfo,
        const VmaAllocationCreateInfo& allocCreateInfo) noexcept
    {
        std::optional<BufferHandle> handle{};

        VkBuffer buffer{};
        VmaAllocation allocation{};
        VmaAllocationInfo allocationInfo{};
        VkResult result = vmaCreateBuffer(allocator, &bufferCreateInfo, &allocCreateInfo, &buffer, &allocation,
            &allocationInfo);

        if(result != VK_SUCCESS) {
            logError("buffer creation failed\n");
            return std::nullopt;
        }

        handle.emplace(buffers.size());
        buffers.push_back({
            .buffer = buffer,
            .allocation = allocation,
            .allocationInfo = allocationInfo,
            .size = sizeBytes
        });

        logInfo("created a new buffer (%lu)", *handle);
        return handle;
    }
    void destroyBuffers() noexcept {
        for(std::size_t i = 0; i < buffers.size(); ++i) {
            vmaDestroyBuffer(allocator, buffers[i].buffer, buffers[i].allocation);
        }

        buffers.resize(0);
        logInfo("destroyed all buffers");
    }
    void destroyImages() noexcept {
        // destroy image views first
        for(std::size_t i = 0; i < images.size(); ++i) {
            vkDestroyImageView(device, images[i].view, nullptr);
        }

        for(std::size_t i = 0; i < images.size(); ++i) {
            vmaDestroyImage(allocator, images[i].image, images[i].allocation);
        }

        images.resize(0);
        logInfo("destroyed all images");
    }
    // log convenience
    template<typename... Args>
    void logError(const char* msg, Args... args) const noexcept {
        log.error("vulkan/resource-manager", msg, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void logDebug(const char* msg, Args... args) const noexcept {
        log.debug("vulkan/resource-manager", msg, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void logInfo(const char* msg, Args... args) const noexcept {
        log.info("vulkan/resource-manager", msg, std::forward<Args>(args)...);
    }
};

}
