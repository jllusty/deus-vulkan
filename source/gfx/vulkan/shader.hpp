// shader.hpp: define shader objects, initialized with precompiled SPIR-V shader source filenames
#pragma once

#include <string>
#include <fstream>

#include <vulkan/vulkan_core.h>

#include "core/log/logging.hpp"

// TODO: pass as a compilation argument
constexpr std::string SHADER_BIN_DIR{ "./build/assets/shaders" };

namespace gfx::vulkan {

class Shader {
    core::log::Logger& log;
    VkDevice device{ VK_NULL_HANDLE };

    VkShaderModule module{ VK_NULL_HANDLE };

    std::string filepath{};
    std::vector<uint32_t> source{};

public:
    Shader(core::log::Logger& log, VkDevice device, std::string filename)
        : log(log), device(device)
    {
        filepath = SHADER_BIN_DIR + "/" + filename;
        readShaderSource(filepath);

        if(source.empty()) {
            return;
        }

        VkShaderModuleCreateInfo createInfo{
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .codeSize = source.size() * sizeof(uint32_t),
            .pCode = source.data()
        };

        VkResult result = vkCreateShaderModule(
            device,
            &createInfo,
            nullptr,
            &module
        );

        if(result != VK_SUCCESS) {
            module = VK_NULL_HANDLE;
            log.error("gfx/vulkan/shader","failed to create a vulkan shader module '%s'", filepath.c_str());
            return;
        }
        log.info("gfx/vulkan/shader","created a vulkan shader module '%s'", filepath.c_str());
    }

    ~Shader() {
        if(module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(
                device,
                module,
                nullptr
            );
            log.info("gfx/vulkan/shader","destroyed the vulkan shader module '%s'", filepath.c_str());
        }
    }

    VkShaderModule get() const noexcept {
        return module;
    }

private:
    void readShaderSource(std::string filepath) noexcept {
        source.clear();
        std::ifstream ifs(filepath, std::ios::ate | std::ios::binary);
        if (!ifs) {
            log.error("gfx/vulkan/shader","could not read shader source from '%s'", filepath.c_str());
            return;
        }

        std::streamsize bytes = (size_t)ifs.tellg();
        if(bytes <= 0) {
            log.error("gfx/vulkan/shader","tellg failed or file empty '%s'", filepath.c_str());
            return;
        }
        if(bytes % 4 != 0) {
            log.error("gfx/vulkan/shader","SPIR-V bytes not a multiple of 4 '%s'", filepath.c_str());
            return;
        }
        source.resize(static_cast<std::size_t>(bytes) / 4);
        ifs.seekg(0, std::ios::beg);
        if(!ifs.read(reinterpret_cast<char*>(source.data()), bytes)) {
            log.error("gfx/vulkan/shader","failed to read %lld bytes from '%s'", static_cast<long long>(bytes), filepath.c_str());
            source.clear();
        }
    }
};

};
