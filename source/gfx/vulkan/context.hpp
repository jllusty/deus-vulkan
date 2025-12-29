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
    // graphics pipeline (move into an owner class)
    VkPipelineLayout graphicsPipelineLayout { VK_NULL_HANDLE };
    VkPipeline graphicsPipeline { VK_NULL_HANDLE };

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

    void CreateGraphicsPipeline() noexcept {
        Shader vert(log, device.get(), "triangle.vert.spv");
        Shader frag(log, device.get(), "triangle.frag.spv");

        // vertex -> frag
        VkPipelineShaderStageCreateInfo stages[2] = {
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = vert.get(),
                .pName = "main",
                .pSpecializationInfo = nullptr
            },
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = frag.get(),
                .pName = "main",
                .pSpecializationInfo = nullptr
            }
        };

        VkPipelineVertexInputStateCreateInfo vertexInput {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .vertexBindingDescriptionCount = 0,
            .pVertexBindingDescriptions = nullptr,
            .vertexAttributeDescriptionCount = 0,
            .pVertexAttributeDescriptions = nullptr
        };

        VkPipelineInputAssemblyStateCreateInfo inputAssembly {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = 0
        };

        // scissor and viewport are dynamic - do not specify pointers
        VkPipelineViewportStateCreateInfo viewportState {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .viewportCount = 1,
            .pViewports = nullptr,
            .scissorCount = 1,
            .pScissors = nullptr
        };

        VkPipelineRasterizationStateCreateInfo rasterizer {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .depthClampEnable = 0,
            .rasterizerDiscardEnable = 0,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_NONE,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .depthBiasEnable = 0,
            .depthBiasConstantFactor = 0.f,
            .depthBiasClamp = 0.f,
            .depthBiasSlopeFactor = 0.f,
            .lineWidth = 1.f
        };

        VkPipelineMultisampleStateCreateInfo multisample {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable = 0,
            .minSampleShading = 0.f,
            .pSampleMask = nullptr,
            .alphaToCoverageEnable = 0,
            .alphaToOneEnable = 0
        };

        VkPipelineColorBlendAttachmentState colorBlendAttachment {
            .blendEnable = 0,
            .srcColorBlendFactor = VK_BLEND_FACTOR_ZERO,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
            .colorBlendOp = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            .alphaBlendOp = VK_BLEND_OP_ADD,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT |
                VK_COLOR_COMPONENT_A_BIT
        };

        VkPipelineColorBlendStateCreateInfo colorBlend {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .logicOpEnable = 0,
            .logicOp = VK_LOGIC_OP_CLEAR,
            .attachmentCount = 1,
            .pAttachments = &colorBlendAttachment,
            .blendConstants = {0.f, 0.f, 0.f, 0.f}
        };

        VkDynamicState dynamics[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamicState {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .dynamicStateCount = 2,
            .pDynamicStates = dynamics
        };

        VkPipelineLayoutCreateInfo layoutInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .setLayoutCount = 0,
            .pSetLayouts = nullptr,
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = nullptr
        };

        VkResult result = vkCreatePipelineLayout(
            device.get(),
            &layoutInfo,
            nullptr,
            &graphicsPipelineLayout
        );
        if(result != VK_SUCCESS) {
            logError("could not create graphics pipeline layout");
            graphicsPipelineLayout = VK_NULL_HANDLE;
            return;
        }

        VkGraphicsPipelineCreateInfo pipelineInfo {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stageCount = 2,
            .pStages = stages,
            .pVertexInputState = &vertexInput,
            .pInputAssemblyState = &inputAssembly,
            .pTessellationState = nullptr,
            .pViewportState = &viewportState,
            .pRasterizationState = &rasterizer,
            .pMultisampleState = &multisample,
            .pDepthStencilState = nullptr,
            .pColorBlendState = &colorBlend,
            .pDynamicState = &dynamicState,
            .layout = graphicsPipelineLayout,
            .renderPass = swapchain.getRenderPass(),
            .subpass = 0,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = 0
        };

        // create pipeline
        result = vkCreateGraphicsPipelines(
            device.get(),
            VK_NULL_HANDLE,
            1,
            &pipelineInfo,
            nullptr,
            &graphicsPipeline
        );
        if(result != VK_SUCCESS) {
            logError("could not create graphics pipeline");
            graphicsPipeline = VK_NULL_HANDLE;
        }
    }

    void DestroyGraphicsPipeline() noexcept {
        vkDeviceWaitIdle(device.get());
        // destroy pipeline + layout
        if(graphicsPipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(
                device.get(),
                graphicsPipeline,
                nullptr
            );
        }
        if(graphicsPipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(
                device.get(),
                graphicsPipelineLayout,
                nullptr
            );
        }
    }

    void AcquireSubmitPresent() noexcept {
        cmd.awaitAndResetFrameFence();

        // this needs to be cached only once (even between recreating the swapchain), as it is not
        // the "signal" of the semaphore
        // as such, it is perfectly safe to copy from our swapchain manager
        VkSemaphore acquire = swapchain.getAcquireSemaphore();

        // call vkAcquireNextImage, set submit semaphore to the corresponding index
        uint32_t imageIndex = swapchain.acquireImage();
        VkSemaphore submit = swapchain.getSubmitSemaphore(imageIndex);
        logInfo("acquired swapchain index %d",imageIndex);

        // set viewport + scissor
        VkExtent2D extent = swapchain.getExtent();
        VkViewport viewport {
            .x = 0,
            .y = 0,
            .width = static_cast<float>(extent.width),
            .height = static_cast<float>(extent.height),
            .minDepth = 0.f,
            .maxDepth = 1.f
        };
        VkRect2D scissor {
            .offset = {.x = 0, .y = 0},
            .extent = extent
        };

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

        cmd.bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
        cmd.setViewportAndScissor(viewport, scissor);
        cmd.draw();

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
