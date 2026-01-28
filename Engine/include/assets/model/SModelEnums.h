#pragma once
#include <cstdint>

namespace Engine::smodel
{
    // ============================================================
    // Mesh / Vertex Layout Enums
    // ============================================================

    // Index buffer storage format for a mesh.
    // - U16 : 16-bit indices (smaller, faster to load)
    // - U32 : 32-bit indices (needed for very large meshes)
    enum class IndexType : uint32_t
    {
        U16 = 0,
        U32 = 1
    };

    // Bitmask describing which vertex attributes exist in the vertex buffer.
    // This helps the runtime know how to interpret the vertex bytes.
    enum VertexLayoutFlags : uint32_t
    {
        VTX_POS = (1u << 0),     // POSITION (vec3). Always required.
        VTX_NORMAL = (1u << 1),  // NORMAL   (vec3). Needed for lighting.
        VTX_UV0 = (1u << 2),     // TEXCOORD0(vec2). Needed for textures.
        VTX_TANGENT = (1u << 3), // TANGENT  (vec4). Needed for normal maps.

        // Skinning (V4)
        VTX_JOINTS = (1u << 4),  // JOINTS0 (u16x4)
        VTX_WEIGHTS = (1u << 5), // WEIGHTS0 (f32x4)
    };

    // ============================================================
    // Texture / Image Enums
    // ============================================================

    // Color space determines the VkFormat during upload.
    // - SRGB   : baseColor/emissive (color textures)
    // - Linear : normals/metalrough/occlusion (data textures)
    enum class TextureColorSpace : uint32_t
    {
        Linear = 0,
        SRGB = 1
    };

    // Image encoding describes how the texture bytes are stored in the blob.
    // Phase 1: embed PNG/JPG bytes (compressed) and decode at runtime.
    enum class ImageEncoding : uint32_t
    {
        PNG = 0,
        JPG = 1,
        RAW = 2 // optional future (raw RGBA8 stored directly)
    };

    // ============================================================
    // Sampler Enums
    // ============================================================

    // Wrapping mode for UV coordinates outside [0..1]
    enum class WrapMode : uint32_t
    {
        Repeat = 0,
        Clamp = 1,
        Mirror = 2
    };

    // Texture filter mode
    enum class FilterMode : uint32_t
    {
        Nearest = 0,
        Linear = 1
    };

    // Mipmap sampling mode
    enum class MipMode : uint32_t
    {
        None = 0, // no mipmaps
        Nearest = 1,
        Linear = 2
    };

    // ============================================================
    // Material Enums
    // ============================================================

    // glTF alpha mode. This affects culling, sorting, discard, blending later.
    enum class AlphaMode : uint32_t
    {
        Opaque = 0, // solid
        Mask = 1,   // cutout (alpha test)
        Blend = 2   // transparent
    };

} // namespace