#pragma once

#include "core/log/logging.hpp"
#include "gfx/vulkan/config.hpp"
#include "gfx/vulkan/resources.hpp"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace gfx::vulkan {

class Commander {
    core::log::Logger& log;
    const Configurator& config;
    const VkDevice vulkanDevice;
    ResourceManager& manager;

    // queue
    VkQueue queue{ VK_NULL_HANDLE };
    // command pool (should be per-thread)
    VkCommandPool pool{ VK_NULL_HANDLE };
    // command buffers
    VkCommandBuffer buffer{ VK_NULL_HANDLE };
    // fence (per frame)
    VkFence frame{ VK_NULL_HANDLE };

public:
    Commander(core::log::Logger& log, const Configurator& config, const VkDevice device, gfx::vulkan::ResourceManager& manager)
        : log(log), config(config), vulkanDevice(device), manager(manager)
    {
        // request a single queue
        vkGetDeviceQueue(
            vulkanDevice,
            0,
            0,
            &queue
        );

        if(queue == VK_NULL_HANDLE) {
            logError("failed to fetch a device queue");
        }

        // create a single command pool
        const VkCommandPoolCreateInfo cmdPoolCreateInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            // transient: command buffers allocated from the pool will be short-lived,
            // meaning that they will be reset or freed in a relatively short timeframe
            // reset command buffer: any command buffer allocated from a pool to be
            // individually reset to the initial state
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = 0
        };

        VkResult result = vkCreateCommandPool(
            vulkanDevice,
            &cmdPoolCreateInfo,
            nullptr,
            &pool
        );
        if(result != VK_SUCCESS) {
            pool = VK_NULL_HANDLE;
            logError("could not create a command pool");
        }
        logInfo("created a command pool");

        // allocate a single resettable command buffer
        const VkCommandBufferAllocateInfo cmdBufferAllocInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };
        result = vkAllocateCommandBuffers(
            vulkanDevice,
            &cmdBufferAllocInfo,
            &buffer
        );
        logInfo("allocated a command buffer");

        if(result != VK_SUCCESS) {
            buffer = VK_NULL_HANDLE;
            logError("could not allocate a command buffer");
        }

        // create fence, initialize to (SIGNALED)
        VkFenceCreateInfo fenceCreateInfo {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };
        result = vkCreateFence(
            vulkanDevice,
            &fenceCreateInfo,
            nullptr,
            &frame
        );

        if(result != VK_SUCCESS) {
            frame = VK_NULL_HANDLE;
            logError("could not create a fence");
        }
        logInfo("created a frame fence");
    }

    ~Commander() {
        // await for resettable command pool
        VkResult result = vkWaitForFences(
            vulkanDevice,
            1,
            &frame,
            VK_TRUE,
            UINT64_MAX
        );
        if(result != VK_SUCCESS) {
            logError("could not wait for fences");
        }

        // destroy the fence
        vkDestroyFence(
            vulkanDevice,
            frame,
            nullptr
        );
        logInfo("destroyed frame fence");

        // free command buffer
        vkFreeCommandBuffers(
            vulkanDevice,
            pool,
            1,
            &buffer
        );
        logInfo("freed command buffer");

        // destroy single command pool
        vkDestroyCommandPool(
            vulkanDevice,
            pool,
            nullptr
        );
        logInfo("destroyed command pool");
    }

    // delete assignment/copy/move constructors
    Commander(const Commander&) = delete;
    Commander& operator=(const Commander&) = delete;
    Commander(Commander&&) = delete;
    Commander& operator=(Commander&&) = delete;

    // awaits writable command buffer and resets the fence used in submission
    bool begin() noexcept {
        // await for resettable command pool
        VkResult result = vkWaitForFences(
            vulkanDevice,
            1,
            &frame,
            VK_TRUE,
            UINT64_MAX
        );
        if(result != VK_SUCCESS) {
            logError("could not wait for fences");
            return false;
        }

        // reset fence
        result = vkResetFences(
            vulkanDevice,
            1,
            &frame
        );
        if(result != VK_SUCCESS) {
            logError("could not reset frame fence");
            return false;
        }

        // reset buffer
        result = vkResetCommandBuffer(buffer, 0);
        if(result != VK_SUCCESS) {
            logError("could not reset command buffer");
            return false;
        }

        // begin fresh command submission
        VkCommandBufferBeginInfo cmdBufferBeginInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = 0,
            .pInheritanceInfo = nullptr
        };

        result = vkBeginCommandBuffer(
            buffer,
            &cmdBufferBeginInfo
        );

        if(result != VK_SUCCESS) {
            logError("could not begin a command buffer");
            return false;
        }

        logInfo("reset command buffer, recording");
        return true;
    }

    // ends the command buffer, and then submits to the queue
    bool submit() noexcept {
        VkResult result = vkEndCommandBuffer(buffer);
        if(result != VK_SUCCESS) {
            logError("could not end command buffer");
            return false;
        }

        const VkSubmitInfo submitInfo {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = nullptr,
            .pWaitDstStageMask = nullptr,
            .commandBufferCount = 1,
            .pCommandBuffers = &buffer,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores = nullptr
        };

        result = vkQueueSubmit(
            queue,
            1,
            &submitInfo,
            frame
        );

        // we must await before re-writing to the command buffers

        if(result != VK_SUCCESS) {
            logError("could not submit queue");
            return false;
        }

        logInfo("submitted command buffer");
        return true;
    }

    // no fences are used inside of the command wrappers
    void copy(BufferHandle bufferHandleSrc, BufferHandle bufferHandleDst) noexcept {
        Buffer bufferSrc = *manager.getBuffer(bufferHandleSrc);
        Buffer bufferDst = *manager.getBuffer(bufferHandleDst);

        const VkBufferCopy bufferCpy {
            .srcOffset = 0,// vertexBufferSrc.allocationInfo.offset,
            .dstOffset = 0,//vertexBufferDst.allocationInfo.offset,
            .size = bufferSrc.size
        };

        // copy from buffer 1 to 2
        vkCmdCopyBuffer(
            buffer,
            bufferSrc.buffer,
            bufferDst.buffer,
            1,
            &bufferCpy
        );

        logInfo("command: copy buffer (%lu) -> buffer (%lu)", bufferHandleSrc.id, bufferHandleDst.id);
    }

    void makeWriteable(ImageHandle handle) noexcept {
        Image img = *manager.getImage(handle);
        VkImageSubresourceRange subresourceRange {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        };
        VkImageMemoryBarrier barrier {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = img.currentLayout,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = img.image,
            .subresourceRange = subresourceRange
        };

        vkCmdPipelineBarrier(
            buffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier
        );

        // update layout
        manager.updateImageLayout(handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        logInfo("command: barrier image (%lu) access -> writeable", handle.id);
    }

    void makeReadable(ImageHandle handle) noexcept {
       Image img = *manager.getImage(handle);
       VkImageSubresourceRange subresourceRange {
           .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
           .baseMipLevel = 0,
           .levelCount = 1,
           .baseArrayLayer = 0,
           .layerCount = 1
       };
       VkImageMemoryBarrier barrier {
           .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
           .pNext = nullptr,
           .srcAccessMask = 0,
           .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
           .oldLayout = img.currentLayout,
           .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
           .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
           .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
           .image = img.image,
           .subresourceRange = subresourceRange
       };

       vkCmdPipelineBarrier(
           buffer,
           VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
           VK_PIPELINE_STAGE_TRANSFER_BIT,
           0,
           0,
           nullptr,
           0,
           nullptr,
           1,
           &barrier
       );

       // update layout
       manager.updateImageLayout(handle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

       logInfo("command: barrier image (%lu) access -> readable from shader", handle.id);
    }

    void copy(BufferHandle bufferHandle, ImageHandle imageHandle, core::u32 imageWidth, core::u32 imageHeight) noexcept {
        VkImageSubresourceLayers subresourceLayers {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        };

        VkBufferImageCopy region{
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = subresourceLayers,
            .imageOffset = VkOffset3D { 0, 0, 0},
            .imageExtent = VkExtent3D { imageWidth, imageHeight, 1}
        };

        vkCmdCopyBufferToImage(
            buffer,
            manager.getBuffer(bufferHandle)->buffer,
            manager.getImage(imageHandle)->image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region
        );

        logInfo("command: copy buffer (%lu) -> image (%lu)", bufferHandle.id, imageHandle.id);
    }

    void beginRenderpass() noexcept {
        VkRenderPassCreateInfo renderPassCreateInfo {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .attachmentCount = 0,
            .pAttachments = nullptr,
            .subpassCount = 0,
            .pSubpasses = nullptr,
            .dependencyCount = 0,
            .pDependencies = nullptr
        };
        VkRenderPass renderPass{};
        VkResult result = vkCreateRenderPass(
            vulkanDevice,
            &renderPassCreateInfo,
            nullptr,
            &renderPass
        );
        if(result != VK_SUCCESS) {
            logError("failed to create render pass");
            return;
        }

        VkFramebufferCreateInfo framebufferCreateInfo {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderPass = renderPass,
            .attachmentCount = 0,
            .pAttachments = nullptr,
            .width = 800,
            .height = 600,
            .layers = 1
        };
        VkFramebuffer framebuffer{VK_NULL_HANDLE};

        VkRenderPassBeginInfo beginInfo {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .pNext = nullptr,
            .renderPass = renderPass,
            .framebuffer = framebuffer,
            .renderArea = VkRect2D{.offset = VkOffset2D{ 0, 0}, .extent = VkExtent2D{ 800, 600} },
            .clearValueCount = 0,
            .pClearValues = nullptr
        };
        VkSubpassContents contents{ VK_SUBPASS_CONTENTS_INLINE };
        vkCmdBeginRenderPass(
            buffer,
            &beginInfo,
            contents
        );
    }


private:
    // log convenience
    template<typename... Args>
    void logError(const char* msg, Args... args) {
        log.error("gfx/vulkan/command", msg, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void logDebug(const char* msg, Args... args) {
        log.debug("gfx/vulkan/command", msg, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void logInfo(const char* msg, Args... args) {
        log.info("gfx/vulkan/command", msg, std::forward<Args>(args)...);
    }
};

}
