#pragma once
#include <cstdint>

namespace Engine::smodel
{
#pragma pack(push, 1)

    // ============================================================
    // Skin Record (V4)
    // ============================================================
    // A skin defines a set of joints (node indices) and inverse bind matrices.
    // Vertex JOINTS indices are indices into this skin's joint list.
    struct SModelSkinRecord
    {
        uint32_t nameStrOffset; // 0 = empty

        uint32_t jointCount;
        uint32_t firstJointNodeIndex;    // index into skinJointNodeIndices[] (uint32 node indices)
        uint32_t firstInverseBindMatrix; // index into skinInverseBindMatrices[] (mat4 array, 1 matrix = 16 floats)
    };

#pragma pack(pop)

    static_assert(sizeof(SModelSkinRecord) == 16, "SModelSkinRecord size mismatch");

} // namespace Engine::smodel
