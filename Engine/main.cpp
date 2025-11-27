#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>
#include <stdexcept>
#include <cstdlib>

void glfwErrorCallback(int code, const char *description)
{
    std::cerr << "[GLFW ERROR] (" << code << "): " << description << std::endl;
}

int main()
{
    // GLFW init
    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW\n";
        return EXIT_FAILURE;
    }

    // Tell GLFW we will use Vulkan (no OpenGL context)
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    const int width = 1280;
    const int height = 720;
    GLFWwindow *window = glfwCreateWindow(width, height, "MyEngine - GLFW+Vulkan", nullptr, nullptr);
    if (!window)
    {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return EXIT_FAILURE;
    }

    // Get required instance extensions from GLFW
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    if (!glfwExtensions)
    {
        std::cerr << "Failed to get required GLFW Vulkan extensions\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    std::vector<const char *> instanceExtensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    // Optionally print them
    std::cout << "GLFW requested instance extensions:\n";
    for (const char *ext : instanceExtensions)
    {
        std::cout << "  " << ext << "\n";
    }

    // Create Vulkan instance (no validation layers here for max compatibility)
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "MyEngine";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "MyEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0; // request 1.0+, loader may support newer

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
    createInfo.ppEnabledExtensionNames = instanceExtensions.data();
    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = nullptr;

    VkInstance instance = VK_NULL_HANDLE;
    VkResult res = vkCreateInstance(&createInfo, nullptr, &instance);
    if (res != VK_SUCCESS)
    {
        std::cerr << "Failed to create Vulkan instance: error " << res << "\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }
    std::cout << "Vulkan instance created\n";

    // Create a VkSurfaceKHR from the GLFW window
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
    {
        std::cerr << "Failed to create Vulkan surface\n";
        vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }
    std::cout << "Vulkan surface created\n";

    // Main loop (no rendering yet)
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        // Here you will later do: acquire image, submit commands, present swapchain
    }

    // Cleanup
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);

    glfwDestroyWindow(window);
    glfwTerminate();

    std::cout << "Clean exit\n";
    return EXIT_SUCCESS;
}