#pragma once
#include <cstdint>

namespace Engine
{
    struct MeshHandle
    {
        uint64_t id = 0;
        uint32_t generation = 0;
        bool isValid() const { return id != 0; }
    };

    struct TextureHandle
    {
        uint64_t id = 0;
        uint32_t generation = 0;
        bool isValid() const { return id != 0; }
    };

    struct MaterialHandle
    {
        uint64_t id = 0;
        uint32_t generation = 0;
        bool isValid() const { return id != 0; }
    };

    struct ModelHandle
    {
        uint64_t id = 0;
        uint32_t generation = 0;
        bool isValid() const { return id != 0; }
    };
}
