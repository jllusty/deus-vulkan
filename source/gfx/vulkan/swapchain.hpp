#pragma once

#include <vulkan/vulkan_core.h>

#include "core/log/logging.hpp"
#include "gfx/vulkan/config.hpp"

#include <vector>

namespace gfx::vulkan {

struct SwapchainSupport {
    VkSurfaceCapabilitiesKHR caps{};
    std::vector<VkSurfaceFormatKHR> formats{};
    std::vector<VkPresentModeKHR> modes{};
};

// manages render target on a single physical device
class SwapchainManager {
    core::log::Logger& log;
    const Configurator& config;
    const PhysicalDeviceHandle physicalDeviceHandle;
    const VkPhysicalDevice physicalDevice;
    const VkDevice vulkanDevice;
    // currently acquiring/presenting from
    VkSwapchainKHR active{};
    VkExtent2D extent{};
    VkFormat format{};
    std::vector<VkImage> images{};
    std::vector<VkImageView> views{};
    VkRenderPass renderPass{};
    std::vector<VkFramebuffer> framebuffers{};
    // semaphores for swapchain image availability and submission
    // note: does not need to be recreated when recreating a swapchain
    // note: need as many acquire semaphores as we do in-flight frames
    VkSemaphore acquire{};
    std::vector<VkSemaphore> submit{};

public:
    SwapchainManager(core::log::Logger& log, const Configurator& config, PhysicalDeviceHandle handle, VkDevice vulkanDevice)
        : log(log), config(config), physicalDeviceHandle(handle), physicalDevice(*config.getVulkanPhysicalDevice(handle)),
          vulkanDevice(vulkanDevice)
    {}

    ~SwapchainManager() {
        vkDeviceWaitIdle(vulkanDevice);
        destroySemaphores();
        destroyFramebuffers();
        destroyRenderPass();
        destroySwapchainImageViews();
        destroySwapchain();
    }

    // happens on application startup / window creation as well as resizing
    // returns true if the new swapchain is valid
    bool createSwapchain(uint32_t queueGraphicsFamily, VkSurfaceKHR surface) noexcept {
        std::optional<SwapchainSupport> optSupport = getSwapchainSupport(queueGraphicsFamily, surface);
        if(!optSupport.has_value()) {
            logError("could not acquire swapchain: no support for presenting");
            return false;
        }
        SwapchainSupport support = *optSupport;

        // set extent
        extent = support.caps.currentExtent;
        // set format
        // todo: check if the formats we are requesting exist in the support struct
        VkFormat format = VK_FORMAT_B8G8R8A8_SRGB;
        VkColorSpaceKHR colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

        // create swapchain (move to separate function)
        VkSwapchainCreateInfoKHR createInfo{
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .surface = surface,
            .minImageCount = support.caps.minImageCount,
            .imageFormat = format,
            .imageColorSpace = colorSpace,
            .imageExtent = support.caps.currentExtent,
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .preTransform = support.caps.currentTransform,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode = presentMode,
            .clipped = VK_TRUE,
            .oldSwapchain = VK_NULL_HANDLE
        };

        VkResult result = vkCreateSwapchainKHR(
            vulkanDevice,
            &createInfo,
            nullptr,
            &active
        );

        if(result != VK_SUCCESS) {
            logError("renderer could not create a swapchain");
            return false;
        }

        // get image references
        uint32_t imgCount{ 0 };
        vkGetSwapchainImagesKHR(vulkanDevice, active, &imgCount, nullptr);
        images.resize(imgCount);
        vkGetSwapchainImagesKHR(vulkanDevice, active, &imgCount, images.data());
        logInfo("created a swapchain with (%lu) images", images.size());

        // create image views
        for(VkImage img: images) {
            VkComponentMapping components{};
            VkImageSubresourceRange subresourceRange {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            };
            VkImageViewCreateInfo createInfo{
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .image = img,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = format,
                .components = components,
                .subresourceRange = subresourceRange
            };
            VkImageView imageView{VK_NULL_HANDLE};
            VkResult result = vkCreateImageView(
                vulkanDevice,
                &createInfo,
                nullptr,
                &imageView
            );
            if(result != VK_SUCCESS) {
                logError("could not create image view for swapchain image");
                continue;
            }
            views.push_back(imageView);
        }
        logInfo("created %lu swapchain image views", views.size());

        // create renderpass
        VkAttachmentDescription color{
            .flags = 0,
            .format = format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        };
        VkAttachmentReference colorReference {
            .attachment = 0,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        };
        VkSubpassDescription subpass {
            .flags = 0,
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .inputAttachmentCount = 0,
            .pInputAttachments = nullptr,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorReference,
            .pResolveAttachments = nullptr,
            .pDepthStencilAttachment = nullptr,
            .preserveAttachmentCount = 0,
            .pPreserveAttachments = nullptr
        };
        VkSubpassDependency subpassDep {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = VK_ACCESS_NONE,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = 0
        };
        VkRenderPassCreateInfo renderPassCreateInfo {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .attachmentCount = 1,
            .pAttachments = &color,
            .subpassCount = 1,
            .pSubpasses = &subpass,
            .dependencyCount = 1,
            .pDependencies = &subpassDep
        };
        result = vkCreateRenderPass(
            vulkanDevice,
            &renderPassCreateInfo,
            nullptr,
            &renderPass
        );
        if(result != VK_SUCCESS) {
            logError("failed to create render pass");
            renderPass = VK_NULL_HANDLE;
            return false;
        }

        // create framebuffers
        for(VkImageView view : views) {
            VkFramebufferCreateInfo framebufferCreateInfo {
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .renderPass = renderPass,
                .attachmentCount = 1,
                .pAttachments = &view,
                .width = extent.width,
                .height = extent.height,
                .layers = 1
            };
            VkFramebuffer framebuffer{VK_NULL_HANDLE};
            VkResult result = vkCreateFramebuffer(
                vulkanDevice,
                &framebufferCreateInfo,
                nullptr,
                &framebuffer
            );
            if(result != VK_SUCCESS) {
                logError("could not create framebuffer from image view");
                return false;
            }
            framebuffers.push_back(framebuffer);
        }

        // create image acquire semaphore for use with vkAcquire
        VkSemaphoreCreateInfo semaphoreCreateInfo {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0
        };
        result = vkCreateSemaphore(
            vulkanDevice,
            &semaphoreCreateInfo,
            nullptr,
            &acquire
        );
        if(result != VK_SUCCESS) {
            logError("could not create swapchain image acquire semaphore");
            return false;
        }

        // create image submission semaphores for use with submit + present
        submit.reserve(imgCount);
        for(std::size_t i = 0; i < imgCount; ++i) {
            VkSemaphore sub{VK_NULL_HANDLE};
            VkSemaphoreCreateInfo semaphoreCreateInfo {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0
            };
            result = vkCreateSemaphore(
                vulkanDevice,
                &semaphoreCreateInfo,
                nullptr,
                &sub
            );
            if(result != VK_SUCCESS) {
                logError("could not create swapchain image submit semaphore");
                return false;
            }
            submit.push_back(sub);
        }

        return true;
    }

    bool recreateSwapchain(uint32_t queueFamilyIndex, VkSurfaceKHR surface) noexcept {
        // do not recreate image available semaphore
        // frames may be in-flight when we want to cleanup our resources
        vkDeviceWaitIdle(vulkanDevice);
        destroyFramebuffers();
        destroyRenderPass();
        destroySwapchainImageViews();
        destroySwapchain();
        // create new swapchain
        return createSwapchain(queueFamilyIndex, surface);
    }

    VkRenderPass getRenderPass() const noexcept {
        return renderPass;
    }

    std::vector<VkFramebuffer> getFramebuffers() const noexcept {
        return framebuffers;
    }

    VkExtent2D getExtent() const noexcept {
        return extent;
    }

    // query a physical device for swapchain capabilities, formats, and present modes for a specified surface and queueFamilyIndex
    std::optional<SwapchainSupport> getSwapchainSupport(uint32_t queueFamilyIndex, VkSurfaceKHR surface) const noexcept {
        std::optional<SwapchainSupport> support{};

        VkBool32 presentOK{false};
        VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(
            physicalDevice,
            queueFamilyIndex,
            surface,
            &presentOK
        );

        if(result != VK_SUCCESS) {
            logError("could not query physical device for (%lu) presentation support", physicalDeviceHandle.id);
            return support;
        }
        if(presentOK == false) {
            logInfo("queueFamilyIndex (%lu) does not support presenting to specified surface on physical device (%lu)",
                queueFamilyIndex, physicalDeviceHandle.id);
            return support;
        }

        VkSurfaceCapabilitiesKHR caps{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &caps);

        uint32_t formatCount{0};
        std::vector<VkSurfaceFormatKHR> formats{};
        result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
        if(result != VK_SUCCESS) {
            logError("could not query physical device (%lu) for specified surface's count of supported color formats", physicalDeviceHandle.id);
            return support;
        }
        formats.resize(formatCount);
        result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());
        if(result != VK_SUCCESS) {
            logError("could not query physical device (%lu) for specified surface's supported color formats", physicalDeviceHandle.id);
            return support;
        }

        uint32_t presentCount{0};
        std::vector<VkPresentModeKHR> modes{};
        result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentCount, nullptr);
        if(result != VK_SUCCESS) {
            logError("could not query physical device (%lu) for specified surface's count of supported present modes", physicalDeviceHandle.id);
            return support;
        }
        modes.resize(presentCount);
        result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentCount, modes.data());
        if(result != VK_SUCCESS) {
            logError("could not query physical device (%lu) for specified surface's supported present modes", physicalDeviceHandle.id);
            return support;
        }
        support.emplace(caps,formats,modes);
        return support;
    }

    // signals imageAvailable, not const
    uint32_t acquireImage() noexcept {
        uint32_t imageIndex{ 0 };
        VkResult result = vkAcquireNextImageKHR(
            vulkanDevice,
            active,
            UINT64_MAX,
            acquire,
            VK_NULL_HANDLE,
            &imageIndex
        );
        if(result != VK_SUCCESS) {
            logError("could not acquire next swapchain image");
        }
        return imageIndex;
    }

    VkSwapchainKHR get() const noexcept {
        return active;
    }

    VkSemaphore getAcquireSemaphore() const noexcept  {
        return acquire;
    }

    VkSemaphore getSubmitSemaphore(uint32_t index) const noexcept {
        return submit[index];
    }

private:
    void destroySemaphores() noexcept {
        for(std::size_t i = 0; i < submit.size(); ++i) {
            vkDestroySemaphore(
                vulkanDevice,
                submit[i],
                nullptr
            );
        }
        submit.resize(0);
        vkDestroySemaphore(
            vulkanDevice,
            acquire,
            nullptr
        );
        logInfo("destroyed swapchain image acquire and submit semaphores");
    }

    void destroyFramebuffers() noexcept {
        for(VkFramebuffer framebuffer : framebuffers) {
            vkDestroyFramebuffer(
                vulkanDevice,
                framebuffer,
                nullptr
            );
        }
        std::size_t numFramebuffers = framebuffers.size();
        framebuffers.resize(0);
        logInfo("destroyed %lu framebuffers", numFramebuffers);
    }

    void destroyRenderPass() noexcept {
        if(renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(
                vulkanDevice,
                renderPass,
                nullptr
            );
            renderPass = VK_NULL_HANDLE;
            logInfo("destroyed a render pass");
        }
    }

    void destroySwapchainImageViews() noexcept {
        for(VkImageView view : views) {
            vkDestroyImageView(
                vulkanDevice,
                view,
                nullptr
            );
        }
        views.resize(0);
        logInfo("destroyed %lu swapchain image views", images.size());
    }

    void destroySwapchain() noexcept {
        vkDestroySwapchainKHR(
            vulkanDevice,
            active,
            nullptr
        );
        std::size_t numImages = images.size();
        images.resize(0);
        logInfo("destroyed a swapchain with %lu images", numImages);
    }

    // log convenience
    template<typename... Args>
    void logError(const char* msg, Args... args) const noexcept {
        log.error("gfx/vulkan/swapchain", msg, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void logDebug(const char* msg, Args... args) const noexcept {
        log.debug("gfx/vulkan/swapchain", msg, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void logInfo(const char* msg, Args... args) const noexcept {
        log.info("gfx/vulkan/swapchain", msg, std::forward<Args>(args)...);
    }
};

}
