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

add_executable(VulkanApp
    source/main.cpp
)

target_link_libraries(VulkanApp PRIVATE Vulkan::Vulkan)