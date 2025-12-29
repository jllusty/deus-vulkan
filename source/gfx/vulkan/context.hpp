// context.hpp: gpu runtime context: manage devices, buffers, images, queues
#pragma once

#include "core/log/logging.hpp"
#include "gfx/geometry/grid_mesh.hpp"
#include "gfx/vulkan/config.hpp"
#include "gfx/vulkan/device.hpp"
#include "gfx/vulkan/resources.hpp"
#include "gfx/vulkan/command.hpp"
#include "gfx/vulkan/shader.hpp"
#include "gfx/vulkan/swapchain.hpp"

#include <vulkan/vulkan_core.h>

#include <vk_mem_alloc.h>

namespace gfx::vulkan {

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
    PhysicalDeviceHandle physicalDeviceHandle;
    Device device;
    Allocator allocator;
    ResourceManager manager;
    Commander cmd;
    SwapchainManager swapchain;
    std::vector<VkSemaphore> submit;

public:
    GpuContext(PhysicalDeviceHandle physicalDeviceHandle, core::log::Logger& log, const Configurator& config)
        : log(log), config(config), physicalDeviceHandle(physicalDeviceHandle),
        device(log,config,physicalDeviceHandle),
        allocator({
            .physicalDevice = *config.getVulkanPhysicalDevice(physicalDeviceHandle),
            .device = device.get(),
            .instance       = *config.getVulkanInstance(),
            // note: from what I can tell, MoltenVK breaks a call to vkGetBufferMemoryRequirements2KHR
            // so we have to force it not to use that procedure. We set vma_impl.cpp with
            // VMA_DEDICATED_ALLOCATION = 0 to avoid that path during buffer allocation
            .vulkanApiVersion = VK_API_VERSION_1_0
        }, log),
        manager(config,allocator.get(),device.get(),log),
        cmd(log, config, device.get(), manager),
        swapchain(log,config,physicalDeviceHandle,device.get())
    {}

    ~GpuContext() {}

    void AcquireSubmitPresent() noexcept {
        cmd.awaitAndResetFrameFence();

        // this needs to be cached only once (even between recreating the swapchain), as it is not
        // the "signal" of the semaphore
        // as such, it is perfectly safe to copy from our swapchain manager
        VkSemaphore acquire = swapchain.getAcquireSemaphore();

        uint32_t imageIndex = swapchain.acquireImage();
        VkSemaphore submit = swapchain.getSubmitSemaphore(imageIndex);
        logInfo("acquired swapchain index %d",imageIndex);

        // this call should be moved into swapchain manager
        VkClearColorValue clearColorValue {
            .float32 = { 1.f, 0.f, 0.0f, 1.0f }
        };
        VkClearValue clearValue {
            .color = clearColorValue,
        };
        cmd.begin();
        cmd.beginRenderPass(
            swapchain.getRenderPass(),
            swapchain.getFramebuffers()[imageIndex],
            swapchain.getExtent(),
            clearValue
        );
        cmd.endRenderPass();

        cmd.submitSwapchain(acquire, submit);
        cmd.presentSwapchain(submit, swapchain.get(), imageIndex);
    }

    bool AcquireSwapchain(VkSurfaceKHR surface) {
        return swapchain.createSwapchain(0,surface);
    }

    bool RecreateSwapchain(VkSurfaceKHR surface) {
        return swapchain.recreateSwapchain(0,surface);
    }

    // fill an image with heightmap data
    template<size_t N>
    void CmdBuffers(const std::array<int16_t, N>& heightData, core::i32 heightResolution, const gfx::geometry::GridMesh& gridMesh) {
        // create image to store heightmap
        std::optional<const ImageHandle> imageHandle = manager.createImage(heightResolution,heightResolution,1);
        // image staging buffer
        std::optional<const BufferHandle> bufferToImgHandle = manager.createStagingBuffer(heightResolution * heightResolution * sizeof(int16_t));

        // fill image staging buffer
        bool heightFillResult = fillMemoryMappedBuffer(*bufferToImgHandle, heightData.data(), heightResolution * heightResolution);

        // note: how can we minimize total submits?
        // make heightmap image writeable
        cmd.awaitAndResetFrameFence();
        cmd.begin();
        cmd.makeWriteable(*imageHandle);
        cmd.submit();
        // fill image from staging buffer
        cmd.awaitAndResetFrameFence();
        cmd.begin();
        cmd.copy(*bufferToImgHandle, *imageHandle, heightResolution, heightResolution);
        cmd.submit();
        // make heightmap image readable from a shader
        cmd.awaitAndResetFrameFence();
        cmd.begin();
        cmd.makeReadable(*imageHandle);
        cmd.submit();

        // create buffers to hold grid mesh vertex data
        std::optional<const BufferHandle> bufferHandleGridX = manager.createDeviceLocalVertexBuffer(gridMesh.vertexCount * sizeof(core::u16));
        std::optional<const BufferHandle> bufferHandleGridZ = manager.createDeviceLocalVertexBuffer(gridMesh.vertexCount * sizeof(core::u16));
        // staging buffer for uploads
        std::optional<const BufferHandle> bufferHandleSrc = manager.createStagingBuffer(gridMesh.vertexCount * sizeof(core::u16));

        // fill staging buffer, copy to X and Z vertex buffers storing our gridmesh
        bool fillResult = fillMemoryMappedBuffer(*bufferHandleSrc, gridMesh.vertexBufferX.data(), gridMesh.vertexCount);
        cmd.awaitAndResetFrameFence();
        cmd.begin();
        cmd.copy(*bufferHandleSrc, *bufferHandleGridX);
        cmd.submit();

        fillResult = fillMemoryMappedBuffer(*bufferHandleSrc, gridMesh.vertexBufferZ.data(), gridMesh.vertexCount);
        cmd.awaitAndResetFrameFence();
        cmd.begin();
        cmd.copy(*bufferHandleSrc, *bufferHandleGridZ);
        cmd.submit();
    }

    void Shaders() {
        Shader frag(log, device.get(), "triangle.frag.spv");
        Shader vert(log, device.get(), "triangle.vert.spv");
    }

private:
    // fill host-visible buffer
    template<typename T>
    bool fillMemoryMappedBuffer(BufferHandle bufferHandle, T* data, std::size_t count) noexcept {
        std::optional<Buffer> buffer = manager.getBuffer(bufferHandle);
        void* pAlloc = buffer->allocationInfo.pMappedData;

        std::memcpy(pAlloc, data, count);

        logInfo("filled buffer (%lu) with (%lu) bytes", bufferHandle.id, count);
        return true;
    }

    // log convenience
    template<typename... Args>
    void logError(const char* msg, Args... args) const noexcept {
        log.error("gfx/vulkan/context", msg, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void logDebug(const char* msg, Args... args) const noexcept {
        log.debug("gfx/vulkan/context", msg, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void logInfo(const char* msg, Args... args) const noexcept {
        log.info("gfx/vulkan/context", msg, std::forward<Args>(args)...);
    }

};

}
