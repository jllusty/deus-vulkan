// engine/main.cpp: runtime main for deus-vulkan

#include <sys/wait.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include "core/log/logging.hpp"
#include "gfx/vulkan/window.hpp"

#include "engine/world/chunk.hpp"
#include "gfx/geometry/grid_mesh.hpp"

#include "engine/world/chonker.hpp"

#include "gfx/vulkan/constants.hpp"
#include "gfx/vulkan/config.hpp"
#include "gfx/vulkan/context.hpp"

int main()
{
    // Logging
    core::log::Logger log{};

    // GLFW Window
    gfx::vulkan::Window window(log, 800, 600);

    // Mesh Generator
    gfx::geometry::GridMesh gridMesh = gfx::geometry::MeshGenerator::createGridMesh(engine::world::CHUNK_RESOLUTION);

    // Chunking System: Chonker
    using namespace engine::world;
    constexpr const std::size_t capacity = 64;
    Chonker chonker(capacity);
    float2 playerPosition{ 152.f, 300.f };
    Chunk playerChunk = worldPositionXZToChunk(playerPosition);
    chonker.request(playerChunk);
    // ChunkData* pChunkData = chonker.fetch(playerChunk);

    // Vulkan Configurator
    // start with validation extensions, add what our windowing requires
    std::vector<std::string> requiredExtensionNames {
       VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, // dependency for validation
       VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
    };
    for(std::string name : window.getRequiredExtensions()) {
        requiredExtensionNames.push_back(name);
    }
    gfx::vulkan::InstanceRequest instanceRequest {
        .requiredLayerNames = { gfx::vulkan::VK_LAYER_KHRONOS_VALIDATION_NAME },
        .requiredExtensionNames = requiredExtensionNames,
        .optionalLayerNames = { "VK_LAYER_KHRONOS_shader_object", "VK_LAYER_LUNARG_api_dump" },
        .optionalExtensionNames = {},
    };
    std::optional<gfx::vulkan::Configurator> optConfig = gfx::vulkan::Configurator::create(instanceRequest, log);
    if(!optConfig.has_value()) {
        log.error("main","could not configurate vulkan");
        return -1;
    }
    const gfx::vulkan::Configurator& config = *optConfig;

    // generate surface
    gfx::vulkan::Surface surface(log, window, *config.getVulkanInstance());

    // pick a physical device
    std::optional<const gfx::vulkan::PhysicalDeviceHandle> bestPhysicalDevice = config.getBestPhysicalDevice();
    if(!bestPhysicalDevice.has_value()) {
        log.error("main", "could not select a physical device");
        return -1;
    }
    const gfx::vulkan::PhysicalDeviceHandle physicalDevice = *bestPhysicalDevice;

    // create gpu context
    gfx::vulkan::GpuContext context {
        physicalDevice,
        log,
        config
    };

    while(chonker.getStatus(playerChunk) != ChunkStatus::Loaded) {
        log.info("main","waiting before loading heightmap into GPU...");
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // copy read heads into a device-local vertex buffer
    context.CmdBuffers(chonker.fetch(playerChunk)->heights, CHUNK_RESOLUTION, gridMesh);

    // instance destroyed on config dropping out of scope
    context.Shaders();

    // acquire swapchain
    context.AcquireSwapchain(surface.get());

    while(!glfwWindowShouldClose(window.get())) {
        glfwPollEvents();

        // todo: on resize: reacquire swapchain
        context.AcquireSubmitPresent();
    }

    return 0;
}
