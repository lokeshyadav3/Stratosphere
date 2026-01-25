#pragma once
#include <vulkan/vulkan.h>
// Per-frame resources (one slot per in-flight frame)
struct FrameContext
{
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkSemaphore imageAcquiredSemaphore = VK_NULL_HANDLE;
    VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
    VkFence inFlightFence = VK_NULL_HANDLE;
    uint32_t frameIndex = 0;
};