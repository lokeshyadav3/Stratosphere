#pragma once
#include "Engine/Renderer.h"
#include "Engine/Pipeline.h"
#include "assets/AssetManager.h"
#include "assets/TextureAsset.h"
#include "Engine/Camera.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>

namespace Engine
{

    // RenderPassModule to render a cooked .smodel (ModelAsset) without node graph.
    // Draws all primitives at identity transform (or a caller-provided model matrix).
    class SModelRenderPassModule : public RenderPassModule
    {
    public:
        SModelRenderPassModule() = default;
        ~SModelRenderPassModule() override;

        void setEnabled(bool en) { m_enabled = en; }

        void setAssets(AssetManager *assets)
        {
            m_assets = assets;
            refreshModelMatrix();
        }
        void setModel(ModelHandle h)
        {
            m_model = h;
            refreshModelMatrix();
        }

        void setCamera(Camera *cam) { m_camera = cam; }

        // Per-instance world transforms for instanced drawing.
        // If not called (or count==0), the module defaults to drawing 1 instance at identity.
        void setInstances(const glm::mat4 *instanceWorlds, uint32_t count);

        // Column-major 4x4 matrix (16 floats). Defaults to identity.
        void setModelMatrix(const float *m16);

        void onCreate(VulkanContext &ctx, VkRenderPass pass, const std::vector<VkFramebuffer> &fbs) override;
        void record(FrameContext &frameCtx, VkCommandBuffer cmd) override;
        void onResize(VulkanContext &ctx, VkExtent2D newExtent) override;
        void onDestroy(VulkanContext &ctx) override;

    private:
        struct InstanceFrame
        {
            VkBuffer buffer = VK_NULL_HANDLE;
            VkDeviceMemory memory = VK_NULL_HANDLE;
            void *mapped = nullptr;
            uint32_t capacity = 0;
        };

        struct PushConstantsModel
        {
            float model[16];
            float baseColorFactor[4];
            float materialParams[4]; // x=alphaCutoff, y=alphaMode, z/w unused
        };

        struct CameraUBO
        {
            glm::mat4 view;
            glm::mat4 proj;
        };

        struct CameraFrame
        {
            VkBuffer buffer = VK_NULL_HANDLE;
            VkDeviceMemory memory = VK_NULL_HANDLE;
            VkDescriptorSet set = VK_NULL_HANDLE;
        };

        void destroyResources();
        void createPipelines(VulkanContext &ctx, VkRenderPass pass);
        bool refreshModelMatrix();
        bool createCameraResources(VulkanContext &ctx, size_t frameCount);
        void destroyCameraResources();

        bool createInstanceResources(VulkanContext &ctx, size_t frameCount);
        void destroyInstanceResources();
        bool ensureInstanceCapacity(InstanceFrame &frame, uint32_t needed);

        bool createMaterialResources(VulkanContext &ctx);
        void destroyMaterialResources();
        VkDescriptorSet getOrCreateMaterialSet(MaterialHandle h, const MaterialAsset *mat);

        VkPipelineColorBlendStateCreateInfo makeBlendState(bool enableBlend, VkPipelineColorBlendAttachmentState &outAttachment) const;

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
        VkExtent2D m_extent{};

        AssetManager *m_assets = nullptr;
        ModelHandle m_model{};
        Camera *m_camera = nullptr;

        bool m_enabled = true;

        VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
        Pipeline m_pipelineOpaque;
        Pipeline m_pipelineMask;
        Pipeline m_pipelineBlend;

        VkDescriptorSetLayout m_cameraSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool m_cameraPool = VK_NULL_HANDLE;
        std::vector<CameraFrame> m_cameraFrames;

        VkDescriptorSetLayout m_materialSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool m_materialPool = VK_NULL_HANDLE;
        std::unordered_map<uint64_t, VkDescriptorSet> m_materialSetCache;

        std::vector<InstanceFrame> m_instanceFrames;
        std::vector<glm::mat4> m_instanceWorlds;

        TextureAsset m_fallbackWhiteTexture;

        PushConstantsModel m_pc{};
    };

} // namespace Engine
