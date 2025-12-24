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

add_executable(VulkanApp
    source/engine/main.cpp
)

add_executable(ChunkBuilder
    source/tools/dem_chunk_builder/main.cpp
)

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
    PRIVATE Vulkan::Vulkan GPUOpen::VulkanMemoryAllocator VMAImpl glfw
)

# silence hundreds of warnings from vulkan memory allocator
target_compile_options(VMAImpl PRIVATE -Wno-everything)
