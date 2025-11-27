#include "Engine/VulkanContext.h"
#include "Engine/Window.h"
#include <GLFW/glfw3.h> // for glfwCreateWindowSurface
#include <iostream>
#include <vector>
#include <stdexcept>

namespace Engine
{

    VulkanContext::VulkanContext(Window &window) : m_Window(window)
    {
        // constructor does not init the instance automatically; explicit Init() call pattern can be used
        Init();
    }

    VulkanContext::~VulkanContext()
    {
        Shutdown();
    }

    void VulkanContext::Init()
    {
        createInstance();
        createSurface();
        // pick physical device, create logical device, queues...
    }

    void VulkanContext::Shutdown()
    {
        if (m_Surface != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
            m_Surface = VK_NULL_HANDLE;
        }
        if (m_Instance != VK_NULL_HANDLE)
        {
            vkDestroyInstance(m_Instance, nullptr);
            m_Instance = VK_NULL_HANDLE;
        }
    }

    void VulkanContext::createInstance()
    {
        uint32_t extCount = 0;
        const char **glfwExts = glfwGetRequiredInstanceExtensions(&extCount);
        std::vector<const char *> extensions(glfwExts, glfwExts + extCount);

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "MyEngine";
        appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
        appInfo.pEngineName = "MyEngine";
        appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        ci.pApplicationInfo = &appInfo;
        ci.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        ci.ppEnabledExtensionNames = extensions.data();

        VkResult res = vkCreateInstance(&ci, nullptr, &m_Instance);
        if (res != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Vulkan instance");
        }
    }

    void VulkanContext::createSurface()
    {
        void *w = m_Window.GetWindowPointer();
        if (!w)
        {
            throw std::runtime_error("VulkanContext::createSurface - window handle is null");
        }
        GLFWwindow *window = reinterpret_cast<GLFWwindow *>(w);
        VkResult result = glfwCreateWindowSurface(m_Instance, window, nullptr, &m_Surface);
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create window surface via GLFW");
        }
        std::cout << "Vulkan surface created successfully." << std::endl;
    }
}