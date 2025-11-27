#pragma once
#include <vulkan/vulkan.h>

namespace Engine
{

    class Window;

    class VulkanContext
    {
    public:
        VulkanContext(Window &window);
        ~VulkanContext();

        void Init();
        void Shutdown();

    private:
        void createInstance();
        void createSurface();

    private:
        Window &m_Window;
        VkInstance m_Instance = VK_NULL_HANDLE;
        VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
        // More: VkPhysicalDevice, VkDevice, queues, swapchain, etc.
    };

} // namespace Engine