// main.cpp

#include <span>

#include <sys/wait.h>
#include <vulkan/vulkan.h>
#include <SFML/Window.hpp>
#include <vulkan/vulkan_core.h>

#include "gfx/geometry/grid_mesh.hpp"

#include "engine/world/chunk.hpp"
#include "engine/world/chunk_data.hpp"
#include "engine/world/chunk_pool.hpp"

#include "core/memory/base_allocator.hpp"
#include "core/memory/types.hpp"
#include "core/log/logging.hpp"

#include "gfx/vulkan/constants.hpp"
#include "gfx/vulkan/config.hpp"
#include "gfx/vulkan/context.hpp"

int main()
{
    // ArenaAllocator Use: Subsystem-level Memory Management
    // top level: reserve 4 MB from the OS
    core::memory::BaseAllocator baseAllocator(4 * 1024 * 1024);

    // subsystem call: create a memory region with the right size from the shared base allocator
    core::memory::Region regionLog = baseAllocator.reserve(1024 * 1024);
    core::memory::Region regionChonker = baseAllocator.reserve(1024 * 1024);
    core::memory::Region regionVulkanConfig = baseAllocator.reserve(1024 * 1024);
    core::memory::Region regionVulkanContext = baseAllocator.reserve(1024 * 1024);

    // Logging
    core::log::Logger log(regionLog);

    // Mesh Generator
    gfx::geometry::GridMesh gridMesh = gfx::geometry::MeshGenerator::createGridMesh(engine::world::CHUNK_RESOLUTION);

    // Chunking System
    using namespace engine::world;
    ChunkPool chunkPool(64);
    float2 posXZ{ .x = 134.f, .y = -63.f};
    Chunk playerChunk = worldPositionXZToChunk(posXZ);
    ChunkData* pPlayerChunk = chunkPool.load(playerChunk);
    // load in chunks around the player
    for(int dx = -1; dx <= 1; ++dx) {
        for(int dz = -1; dz <= 1; ++dz) {
            Chunk adjacentChunk = playerChunk;
            adjacentChunk.x += dx;
            adjacentChunk.z += dz;
            // if dx == 0 and dz == 0, this will do nothing since we already
            // loaded that chunk
            ChunkData* pAdjacentChunk = chunkPool.load(adjacentChunk);
        }
    }

    chunkPool.unload(playerChunk);

    // Pool Allocator
    // Vulkan Configurator
    gfx::vulkan::InstanceRequest instanceRequest {
        .requiredLayerNames = { gfx::vulkan::VK_LAYER_KHRONOS_VALIDATION_NAME },
        .requiredExtensionNames = { VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME },
        .optionalLayerNames = { "VK_LAYER_KHRONOS_shader_object", "VK_LAYER_LUNARG_api_dump" },
        .optionalExtensionNames = {},
    };
    std::optional<gfx::vulkan::Configurator> optConfig = gfx::vulkan::Configurator::create(regionVulkanConfig, instanceRequest, log);
    if(!optConfig.has_value()) {
        log.error("main","could not configurate vulkan");
        return -1;
    }
    const gfx::vulkan::Configurator& config = *optConfig;

    // pick a physical device
    std::optional<const VkPhysicalDevice> bestPhysicalDevice = config.getBestPhysicalDevice();
    if(!bestPhysicalDevice.has_value()) {
        log.error("main", "could not select a physical device");
        return -1;
    }
    const VkPhysicalDevice physicalDevice = *bestPhysicalDevice;

    // create gpu context
    gfx::vulkan::GpuContext context {
        regionVulkanContext,
        log,
        config
    };

    std::span<const VkDevice> devices = context.createDevices(physicalDevice);
    const VkBuffer* buffer = context.createBuffer(1024 * 1024);

    // destruction order: buffers -> devices -> instance
    context.destroyBuffers();
    bool destroyDevices = context.destroyDevices();

    // instance destroyed on config dropping out of scope

    return 0;
}
