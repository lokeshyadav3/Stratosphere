#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
#include "assets/MeshFormats.h"
#include "utils/BufferUtils.h"

namespace Engine
{

    // GPU-backed mesh asset: owns device-local vertex/index buffers and metadata
    class MeshAsset
    {
    public:
        MeshAsset() = default;
        ~MeshAsset() = default;

        // Upload MeshData into device-local buffers using staging.
        // Requires a command pool and queue for the copy operations.
        bool upload(VkDevice device,
                    VkPhysicalDevice phys,
                    VkCommandPool commandPool,
                    VkQueue queue,
                    const MeshData &data);

        // Destroy GPU resources
        void destroy(VkDevice device);

        // Accessors for rendering
        VkBuffer getVertexBuffer() const { return m_vb.buffer; }
        VkBuffer getIndexBuffer() const { return m_ib.buffer; }
        uint32_t getIndexCount() const { return m_indexCount; }
        VkIndexType getIndexType() const { return m_indexType; }
        uint32_t getVertexStride() const { return m_vertexStride; }
        const float *getAABBMin() const { return m_aabbMin; }
        const float *getAABBMax() const { return m_aabbMax; }

    private:
        VertexBufferHandle m_vb{};
        IndexBufferHandle m_ib{};
        uint32_t m_indexCount = 0;
        VkIndexType m_indexType = VK_INDEX_TYPE_UINT32;
        uint32_t m_vertexStride = 0;
        float m_aabbMin[3]{};
        float m_aabbMax[3]{};
    };

} // namespace Engine