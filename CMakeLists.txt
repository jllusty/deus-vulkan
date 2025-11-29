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

add_executable(VulkanApp
    source/engine/main.cpp
)

add_executable(ChunkBuilder
    source/tools/dem_chunk_builder/main.cpp
)

# set root for includes at source/
target_include_directories(VulkanApp PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/source"
)

target_include_directories(ChunkBuilder PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/source"
)

target_link_libraries(VulkanApp PRIVATE Vulkan::Vulkan)
target_link_libraries(VulkanApp PRIVATE SFML::Window)
