cmake_minimum_required(VERSION 4.1)

project(Deus-Vulkan
    DESCRIPTION "Dues-Vulkan: Vulkan Rendering Projects"
    LANGUAGES C CXX
)

# minimum C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan REQUIRED)
find_package(glfw3 3.3 REQUIRED CONFIG)
message(STATUS "GLFW3 CMake config dir: ${glfw3_DIR}")

add_subdirectory(external/VulkanMemoryAllocator)

add_subdirectory(external/glm)

add_executable(VulkanApp
    source/engine/main.cpp
)

add_executable(ChunkBuilder
    source/tools/dem_chunk_builder/main.cpp
)

# Shader Compilation
# compiles explicit list of ./asserts/shaders/* into build/assets/shaders/*
# Find glslangValidator
find_program(GLSLANG_VALIDATOR glslangValidator HINTS
  "/opt/homebrew/bin"
)
if (NOT GLSLANG_VALIDATOR)
    message(FATAL_ERROR "glslangValidator not found.")
endif()
# set shader src/bin directories, make binary directory
set(SHADER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders")
set(SHADER_BIN_DIR "${CMAKE_BINARY_DIR}/assets/shaders")
file(MAKE_DIRECTORY "${SHADER_BIN_DIR}")

file(GLOB SHADER_SOURCES
    "${SHADER_SRC_DIR}/triangle.vert"
    "${SHADER_SRC_DIR}/triangle.frag"
)
if (SHADER_SOURCES STREQUAL "")
    message(WARNING "No shaders found in: ${SHADER_SRC_DIR}")
endif()

set(SHADER_SPV_OUTPUTS "")
foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(FILE_NAME ${SHADER} NAME)
    set(SPV "${SHADER_BIN_DIR}/${FILE_NAME}.spv")

    add_custom_command(
    OUTPUT "${SPV}"
    COMMAND "${GLSLANG_VALIDATOR}"
            -V
            --target-env vulkan1.0
            "${SHADER}"
            -o "${SPV}"
    DEPENDS "${SHADER}"
    COMMENT "Compiling shader ${FILE_NAME} -> ${FILE_NAME}.spv"
    VERBATIM
  )

    list(APPEND SHADER_SPV_OUTPUTS "${SPV}")
endforeach()
add_custom_target(shaders ALL DEPENDS ${SHADER_SPV_OUTPUTS})
add_dependencies(VulkanApp shaders)

# box vulkan memory allocator into a library
add_library(VMAImpl
    STATIC source/thirdparty_impl/VulkanMemoryAllocator/vma_impl.cpp
)

# set root for includes at source/
target_include_directories(VulkanApp
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source"
)

target_include_directories(VMAImpl
    PRIVATE external/VulkanMemoryAllocator/include
)

target_include_directories(ChunkBuilder PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/source"
)

target_link_libraries(VMAImpl
    PUBLIC Vulkan::Vulkan
    PRIVATE GPUOpen::VulkanMemoryAllocator
)

target_link_libraries(VulkanApp
    PRIVATE Vulkan::Vulkan GPUOpen::VulkanMemoryAllocator VMAImpl glfw glm::glm
)

# silence hundreds of warnings from vulkan memory allocator
target_compile_options(VMAImpl PRIVATE -Wno-everything)
