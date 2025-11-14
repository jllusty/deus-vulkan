// main.cpp
#include <vulkan/vulkan.h>
#include <iostream>

int main() {
    // check that we can talk to the Vulkan loader
    uint32_t extensionCount = 0;
    VkResult result = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    if (result != VK_SUCCESS) {

        std::cerr << "Stop! You violated the law! Failed to enumerate Vulkan instance extensions. VkResult = "
                  << result << std::endl;
        return 1;
    }

    std::cout << "Vulkan do be working!" << std::endl;
    std::cout << "Available instance extensions: " << extensionCount << std::endl;

    return 0;
}
