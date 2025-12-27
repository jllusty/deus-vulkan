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

public:
    SwapchainManager(core::log::Logger& log, const Configurator& config, PhysicalDeviceHandle handle, VkDevice vulkanDevice)
        : log(log), config(config), physicalDeviceHandle(handle), physicalDevice(*config.getVulkanPhysicalDevice(handle)),
          vulkanDevice(vulkanDevice)
    {}

    ~SwapchainManager() {
        // todo: destroy framebuffers
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

        // todo: check if the formats we are requesting exist in the support struct
        VkFormat imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
        VkColorSpaceKHR colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

        // create swapchain (move to separate function)
        VkSwapchainCreateInfoKHR createInfo{
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .surface = surface,
            .minImageCount = support.caps.minImageCount,
            .imageFormat = imageFormat,
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
                .format = imageFormat,
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

        return true;
    }

    bool recreateSwapchain(uint32_t queueFamilyIndex, VkSurfaceKHR surface) noexcept {
        // frames may be in-flight when we want to cleanup our resources
        vkDeviceWaitIdle(vulkanDevice);
        // cleanup swapchain images
        destroySwapchainImageViews();
        destroySwapchain();
        // create new swapchain
        return createSwapchain(queueFamilyIndex, surface);
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

private:
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
