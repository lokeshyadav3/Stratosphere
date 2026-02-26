#pragma once
/*
  MovementSystem.h (SampleApp-side)
  ---------------------------------
  Purpose:
    - Moves entities: position += velocity * dt for any archetype store that has both Position and Velocity
      and does not contain excluded tags.

  How to customize:
    - Change required/excluded component names in the constructor to reflect the game rules.
    - Modify update() to implement your movement logic (e.g., acceleration).
*/

#include "ECS/SystemFormat.h" // IGameplaySystem, SystemBase
#include "ECS/Components.h"
#include <cmath>
class MovementSystem : public Engine::ECS::SystemBase
{
public:
    MovementSystem()
    {
        // Declare which components this system needs/excludes by name.
        // Build masks from these names in buildMasks(ComponentRegistry&).
        setRequiredNames({"Position", "Velocity"});

        // Optional excluded tags/components (define them in your registry if you use them).
        // Comment out if not used.
        setExcludedNames({"Disabled", "Dead"});
    }

    const char *name() const override { return "MovementSystem"; }

    void buildMasks(Engine::ECS::ComponentRegistry &registry) override
    {
        Engine::ECS::SystemBase::buildMasks(registry);
        m_positionId = registry.ensureId("Position");
        m_velocityId = registry.ensureId("Velocity");
    }

    // Called once after creation: registry will resolve names to IDs and build masks.
    // buildMasks is inherited from SystemBase; no override needed unless custom behavior is required.

    // Per-frame update over all matching stores.
    void update(Engine::ECS::ECSContext &ecs, float dt) override
    {
        if (m_queryId == Engine::ECS::QueryManager::InvalidQuery)
        {
            Engine::ECS::ComponentMask dirty;
            dirty.set(m_velocityId);
            m_queryId = ecs.queries.createDirtyQuery(required(), excluded(), dirty, ecs.stores);
        }

        const auto &q = ecs.queries.get(m_queryId);
        for (uint32_t archetypeId : q.matchingArchetypeIds)
        {
            Engine::ECS::ArchetypeStore *storePtr = ecs.stores.get(archetypeId);
            if (!storePtr)
                continue;
            const auto &store = *storePtr;

            auto dirtyRows = ecs.queries.consumeDirtyRows(m_queryId, archetypeId);
            if (dirtyRows.empty())
                continue;

            const uint32_t n = store.size();
            auto &positions = const_cast<std::vector<Engine::ECS::Position> &>(store.positions());
            auto &velocities = const_cast<std::vector<Engine::ECS::Velocity> &>(store.velocities());

            const bool canLogTarget = store.hasMoveTarget();
            const auto *targetsPtr = canLogTarget ? &store.moveTargets() : nullptr;

            for (uint32_t i : dirtyRows)
            {
                if (i >= n)
                    continue;

                const float velMag1 = std::fabs(velocities[i].x) + std::fabs(velocities[i].y) + std::fabs(velocities[i].z);
                if (velMag1 <= 1e-6f)
                    continue;

                const auto before = positions[i];

                positions[i].x += velocities[i].x * dt;
                positions[i].y += velocities[i].y * dt;
                positions[i].z += velocities[i].z * dt;

                ecs.markDirty(m_positionId, archetypeId, i);

                // Keep movers active: movement must run every frame while velocity is non-zero.
                ecs.markDirty(m_velocityId, archetypeId, i);

                const bool targetActive = (targetsPtr && (*targetsPtr)[i].active != 0);
                const bool moving = (std::fabs(velocities[i].x) + std::fabs(velocities[i].y) + std::fabs(velocities[i].z)) > 1e-6f;
                if (targetActive || moving)
                {
                    const auto after = positions[i];
                    const float dx = after.x - before.x;
                    const float dy = after.y - before.y;
                    const float dz = after.z - before.z;
                }
            }
        }
    }

private:
    Engine::ECS::QueryId m_queryId = Engine::ECS::QueryManager::InvalidQuery;
    uint32_t m_positionId = Engine::ECS::ComponentRegistry::InvalidID;
    uint32_t m_velocityId = Engine::ECS::ComponentRegistry::InvalidID;
};