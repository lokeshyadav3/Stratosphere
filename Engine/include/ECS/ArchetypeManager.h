#pragma once
/*
  ArchetypeManager.h
  ------------------
  Purpose:
    - Maintain a registry of archetypes keyed by component signature (ComponentMask).
    - Assign and look up an archetype ID for each unique signature.

  Usage:
    - uint32_t id = manager.getOrCreate(signature);
    - const Archetype* info = manager.get(id);
*/

#include <unordered_map>
#include <vector>
#include <cstdint>
#include <string>
#include "ECS/Components.h"

namespace Engine::ECS
{
    struct Archetype
    {
        uint32_t id = UINT32_MAX;
        ComponentMask signature;
    };

    class ArchetypeManager
    {
    public:
        // Returns existing ID for signature or creates a new archetype and returns its ID.
        uint32_t getOrCreate(const ComponentMask &signature)
        {
            const std::string key = signature.toKey();
            auto it = m_keyToId.find(key);
            if (it != m_keyToId.end())
                return it->second;

            const uint32_t id = static_cast<uint32_t>(m_archetypes.size());
            m_keyToId.emplace(key, id);
            m_archetypes.push_back(Archetype{id, signature});
            return id;
        }

        // Retrieve archetype info by ID.
        const Archetype *get(uint32_t id) const
        {
            return (id < m_archetypes.size()) ? &m_archetypes[id] : nullptr;
        }

    private:
        std::unordered_map<std::string, uint32_t> m_keyToId;
        std::vector<Archetype> m_archetypes;
    };

} // namespace Engine::ECS