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
find_package(SFML 3.0 REQUIRED COMPONENTS Window)

add_subdirectory(external/VulkanMemoryAllocator)

add_executable(VulkanApp
    source/engine/main.cpp
)

add_executable(ChunkBuilder
    source/tools/dem_chunk_builder/main.cpp
)

# box vulkan memory allocator into a library
add_library(VMAImpl OBJECT source/gfx/vulkan/vma_impl.cpp)
target_include_directories(VMAImpl
    PRIVATE external/VulkanMemoryAllocator/include
)

# set root for includes at source/
target_include_directories(VulkanApp PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/source"
)

target_include_directories(ChunkBuilder PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/source"
)

target_link_libraries(VMAImpl PRIVATE Vulkan::Vulkan)
target_link_libraries(VMAImpl PRIVATE GPUOpen::VulkanMemoryAllocator)

target_link_libraries(VulkanApp PRIVATE Vulkan::Vulkan)
target_link_libraries(VulkanApp PRIVATE GPUOpen::VulkanMemoryAllocator)
target_link_libraries(VulkanApp PRIVATE VMAImpl)
target_link_libraries(VulkanApp PRIVATE SFML::Window)

# silence hundreds of warnings from vulkan memory allocator
target_compile_options(VMAImpl PRIVATE -Wno-everything)
