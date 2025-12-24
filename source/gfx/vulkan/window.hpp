// window.hpp: wrap a glorious multimedia library
#pragma once

#include "core/types.hpp"
#include "core/log/logging.hpp"

#include <vulkan/vulkan_core.h>

#include <GLFW/glfw3.h>
#define GLFW_INCLUDE_NONE

namespace gfx::vulkan {

// RAII wrap the GLFW Window
class Window {
    core::log::Logger& log;

    GLFWwindow* pWindow{nullptr};
    uint32_t extensionsCount{ 0 };
    std::vector<std::string> extensionNames{};
    bool glfwInitialized{ false };

public:
    Window(core::log::Logger& log, core::u32 width, core::u32 height)
        : log(log)
    {
        int result = glfwInit();
        if(result != GLFW_TRUE) {
            log.error("gfx/vulkan/window","failed to initialize GLFW");
            return;
        }
        glfwInitialized = true;
        log.info("gfx/vulkan/window","initialized GLFW");

        // indicate OpenGL is not used
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        // Get required instance extensions from GLFW
        const char** pExtensionNames = glfwGetRequiredInstanceExtensions(&extensionsCount);
        extensionNames.reserve(extensionsCount);
        for(std::size_t i = 0; i < extensionsCount; ++i) {
            extensionNames.emplace_back(pExtensionNames[i]);
        }

        pWindow = glfwCreateWindow(width, height, "VulkanApp", nullptr, nullptr);
        if(pWindow == nullptr) {
            log.error("gfx/vulkan/window","glfwCreateWindow failed");
            return;
        }
        log.info("gfx/vulkan/window","created GLFW window");
    }

    ~Window() {
        if(pWindow != nullptr) {
            glfwDestroyWindow(pWindow);
            log.info("gfx/vulkan/window","destroyed GLFW window");
        }
        if(glfwInitialized) {
            glfwTerminate();
            log.info("gfx/vulkan/window","terminated GLFW");
        }
    }

    const std::vector<std::string> getRequiredExtensions() const noexcept {
        return extensionNames;
    }

    GLFWwindow* get() const noexcept {
        return pWindow;
    }
};

// RAII wrap the Vulkan Surface
class Surface {
    core::log::Logger& log;
    VkInstance instance{ VK_NULL_HANDLE};

    VkSurfaceKHR surface{ VK_NULL_HANDLE };

public:
    Surface(core::log::Logger& log, Window window, VkInstance instance)
        : log(log), instance(instance)
    {
        VkResult result = glfwCreateWindowSurface(
            instance,
            window.get(),
            nullptr,
            &surface
        );
        if(result != VK_SUCCESS) {
            log.error("gfx/vulkan/surface","failed to create a Vulkan surface");
        }
    }
    ~Surface() {
        vkDestroySurfaceKHR(
            instance,
            surface,
            nullptr
        );
        log.info("gfx/vulkan/surface","destroyed a Vulkan Surface");
    }

    VkSurfaceKHR get() const noexcept {
        return surface;
    }
};

}
