#pragma once
#include "Engine/Renderer.h"
#include "Engine/Pipeline.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace Engine
{

    class MeshRenderPassModule : public RenderPassModule
    {
    public:
        struct MeshBinding
        {
            VkBuffer vertexBuffer = VK_NULL_HANDLE;
            VkDeviceSize vertexOffset = 0;
            VkBuffer indexBuffer = VK_NULL_HANDLE;
            VkDeviceSize indexOffset = 0;
            uint32_t indexCount = 0;
            VkIndexType indexType = VK_INDEX_TYPE_UINT32;
        };

        MeshRenderPassModule() = default;
        ~MeshRenderPassModule() override;

        void setMesh(const MeshBinding &binding);

        void onCreate(VulkanContext &ctx, VkRenderPass pass, const std::vector<VkFramebuffer> &fbs) override;
        void record(FrameContext &frameCtx, VkCommandBuffer cmd) override;
        void onResize(VulkanContext &ctx, VkExtent2D newExtent) override;
        void onDestroy(VulkanContext &ctx) override;
        void setEnabled(bool en) { m_enabled = en; }

    private:
        void destroyResources();
        void createPipeline(VulkanContext &ctx, VkRenderPass pass);

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkExtent2D m_extent{};
        Pipeline m_pipeline;
        VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
        MeshBinding m_binding;
        bool m_enabled = false;
    };

} // namespace Engine