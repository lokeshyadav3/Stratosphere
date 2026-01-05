#pragma once
/*
  Entity.h
  --------
  Purpose:
    - Define a lightweight entity handle (index + generation).
    - Provide EntitiesRecord to create/destroy entities and to attach
      per-entity metadata: which archetype store and which row holds its data.

  Usage:
    - Use EntitiesRecord.create() to get a fresh Entity.
    - After creating a row in an archetype store, call EntitiesRecord.attach(entity, archetypeId, row).
    - Use EntitiesRecord.find(entity) to get quick O(1) location info for per-entity operations.
*/

#include <cstdint>
#include <vector>
#include <unordered_map>

namespace Engine::ECS
{
    // Compact entity handle with generation check to avoid stale references.
    struct Entity
    {
        uint32_t index = UINT32_MAX;
        uint32_t generation = 0;

        bool valid() const noexcept { return index != UINT32_MAX; }
    };

    // Per-entity record: which archetype store and row contain this entity's data.
    struct EntityRecord
    {
        uint32_t archetypeId = UINT32_MAX; // ID of the archetype (component signature)
        uint32_t row = UINT32_MAX;         // Row index inside the archetype store's SoA
    };

    // Central registry for creating/destroying entities and tracking their store membership.
    class EntitiesRecord
    {
    public:
        // Create a new entity handle.
        // Simply pop from the freelist if any index is free to use, otherwise use a new index and increase its generation value
        Entity create()
        {
            uint32_t idx;
            if (!m_free.empty())
            {
                idx = m_free.back();
                m_free.pop_back();
            }
            else
            {
                idx = static_cast<uint32_t>(m_generations.size());
                m_generations.emplace_back(0);
            }
            ++m_generations[idx]; // new generation marks the handle as alive
            return Entity{idx, m_generations[idx]};
        }

        // Destroy an entity: erase record and invalidate handle via generation bump.
        void destroy(Entity e)
        {
            if (!isAlive(e))
                return;
            m_records.erase(e.index);
            ++m_generations[e.index];
            m_free.push_back(e.index);
        }

        // Is the entity currently alive?
        bool isAlive(Entity e) const
        {
            return e.index < m_generations.size() && m_generations[e.index] == e.generation;
        }

        // Attach the entity to an archetype store and row.
        void attach(Entity e, uint32_t archetypeId, uint32_t row)
        {
            if (!isAlive(e))
                return;
            m_records[e.index] = EntityRecord{archetypeId, row};
        }

        // Detach the entity (remove its mapping).
        void detach(Entity e)
        {
            if (!isAlive(e))
                return;
            m_records.erase(e.index);
        }

        // Find record; returns nullptr if missing or dead.
        const EntityRecord *find(Entity e) const
        {
            if (!isAlive(e))
                return nullptr;
            auto it = m_records.find(e.index);
            return (it != m_records.end()) ? &it->second : nullptr;
        }

        EntityRecord *find(Entity e)
        {
            if (!isAlive(e))
                return nullptr;
            auto it = m_records.find(e.index);
            return (it != m_records.end()) ? &it->second : nullptr;
        }

    private:
        std::vector<uint32_t> m_generations;                  // generation per index
        std::vector<uint32_t> m_free;                         // freelist of indices
        std::unordered_map<uint32_t, EntityRecord> m_records; // index -> record
    };

} // namespace Engine::ECS