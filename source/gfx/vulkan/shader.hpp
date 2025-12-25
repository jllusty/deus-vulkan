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
    std::vector<char> source{};

public:
    Shader(core::log::Logger& log, VkDevice device, std::string filename)
        : log(log), device(device)
    {
        filepath = SHADER_BIN_DIR + "/" + filename;
        readShaderSource(filepath);

        VkShaderModuleCreateInfo createInfo{
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .codeSize = source.size(),
            .pCode = reinterpret_cast<const uint32_t*>(source.data())
        };

        VkResult result = vkCreateShaderModule(
            device,
            &createInfo,
            nullptr,
            &module
        );

        if(result != VK_SUCCESS) {
            module = VK_NULL_HANDLE;
            log.error("gfx/vulkan/shader","failed to create a vulkan shader module from source at '%s'", filepath.c_str());
        }
        else {
            log.info("gfx/vulkan/shader","created a vulkan shader module from source at '%s'", filepath.c_str());
        }
    }

    ~Shader() {
        if(module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(
                device,
                module,
                nullptr
            );
            log.info("gfx/vulkan/shader","destroyed the vulkan shader module from source at '%s'", filepath.c_str());
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

        size_t size = (size_t)ifs.tellg();
        source.resize(size);
        ifs.seekg(0);
        ifs.read(source.data(), size);
    }
};

};
