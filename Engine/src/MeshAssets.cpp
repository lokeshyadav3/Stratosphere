#include "assets/MeshAsset.h"
#include <cstring>

namespace Engine
{

    bool MeshAsset::upload(VkDevice device,
                           VkPhysicalDevice phys,
                           VkCommandPool commandPool,
                           VkQueue queue,
                           const MeshData &data)
    {
        // 1) Vertex: create host-visible staging buffer and fill it
        VertexBufferHandle stagingVB{};
        VkResult rv = CreateOrUpdateVertexBuffer(
            device, phys,
            data.vertexBytes.data(),
            static_cast<VkDeviceSize>(data.vertexBytes.size()),
            stagingVB);
        if (rv != VK_SUCCESS)
            return false;

        // 2) Vertex: create device-local destination and copy
        VkBuffer dstVBuffer = VK_NULL_HANDLE;
        VkDeviceMemory dstVMemory = VK_NULL_HANDLE;
        rv = CreateDeviceLocalBuffer(
            device, phys,
            static_cast<VkDeviceSize>(data.vertexBytes.size()),
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            dstVBuffer, dstVMemory);
        if (rv != VK_SUCCESS)
        {
            DestroyVertexBuffer(device, stagingVB);
            return false;
        }
        rv = CopyBuffer(device, commandPool, queue,
                        stagingVB.buffer, dstVBuffer,
                        static_cast<VkDeviceSize>(data.vertexBytes.size()));
        // Free staging after copy
        DestroyVertexBuffer(device, stagingVB);
        if (rv != VK_SUCCESS)
        {
            vkDestroyBuffer(device, dstVBuffer, nullptr);
            vkFreeMemory(device, dstVMemory, nullptr);
            return false;
        }
        // Store device-local vertex buffer
        m_vb.buffer = dstVBuffer;
        m_vb.memory = dstVMemory;

        // 3) Index: create host-visible staging buffer and fill it
        IndexBufferHandle stagingIB{};
        VkDeviceSize indexBytes = 0;
        if (data.indexFormat == 1)
        {
            indexBytes = static_cast<VkDeviceSize>(data.indices32.size() * sizeof(uint32_t));
            rv = CreateOrUpdateIndexBuffer(
                device, phys,
                data.indices32.data(),
                indexBytes,
                stagingIB);
            m_indexType = VK_INDEX_TYPE_UINT32;
            m_indexCount = data.indexCount;
        }
        else
        {
            indexBytes = static_cast<VkDeviceSize>(data.indices16.size() * sizeof(uint16_t));
            rv = CreateOrUpdateIndexBuffer(
                device, phys,
                data.indices16.data(),
                indexBytes,
                stagingIB);
            m_indexType = VK_INDEX_TYPE_UINT16;
            m_indexCount = data.indexCount;
        }
        if (rv != VK_SUCCESS)
        {
            // Clean up vertex device-local resources
            DestroyVertexBuffer(device, m_vb);
            return false;
        }

        // 4) Index: create device-local destination and copy
        VkBuffer dstIBuffer = VK_NULL_HANDLE;
        VkDeviceMemory dstIMemory = VK_NULL_HANDLE;
        rv = CreateDeviceLocalBuffer(
            device, phys,
            indexBytes,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            dstIBuffer, dstIMemory);
        if (rv != VK_SUCCESS)
        {
            DestroyIndexBuffer(device, stagingIB);
            DestroyVertexBuffer(device, m_vb);
            return false;
        }
        rv = CopyBuffer(device, commandPool, queue,
                        stagingIB.buffer, dstIBuffer,
                        indexBytes);
        // Free staging after copy
        DestroyIndexBuffer(device, stagingIB);
        if (rv != VK_SUCCESS)
        {
            vkDestroyBuffer(device, dstIBuffer, nullptr);
            vkFreeMemory(device, dstIMemory, nullptr);
            DestroyVertexBuffer(device, m_vb);
            return false;
        }
        // Store device-local index buffer
        m_ib.buffer = dstIBuffer;
        m_ib.memory = dstIMemory;

        // 5) Store vertex stride and copy AABB
        m_vertexStride = data.vertexStride;
        std::memcpy(m_aabbMin, data.aabbMin, sizeof(m_aabbMin));
        std::memcpy(m_aabbMax, data.aabbMax, sizeof(m_aabbMax));

        return true;
    }

    void MeshAsset::destroy(VkDevice device)
    {
        DestroyVertexBuffer(device, m_vb);
        DestroyIndexBuffer(device, m_ib);
        m_indexCount = 0;
    }

} // namespace Engine