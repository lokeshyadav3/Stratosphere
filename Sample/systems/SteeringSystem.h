#pragma once
#include "ECS/SystemFormat.h"
#include "ECS/Components.h"
#include <algorithm>
#include <cmath>

class SteeringSystem : public Engine::ECS::SystemBase
{
public:
    SteeringSystem()
    {
        // Position + Velocity + MoveTarget + MoveSpeed required
        setRequiredNames({"Position", "Velocity", "MoveTarget", "MoveSpeed"});
        setExcludedNames({"Disabled", "Dead"});
    }

    const char *name() const override { return "SteeringSystem"; }

    void buildMasks(Engine::ECS::ComponentRegistry &registry) override
    {
        Engine::ECS::SystemBase::buildMasks(registry);
        m_positionId = registry.ensureId("Position");
        m_velocityId = registry.ensureId("Velocity");
        m_moveTargetId = registry.ensureId("MoveTarget");
        m_facingId = registry.ensureId("Facing");
    }

    void update(Engine::ECS::ECSContext &ecs, float dt) override
    {
        if (m_queryId == Engine::ECS::QueryManager::InvalidQuery)
        {
            // Re-steer when the target changes, and also when position changes (movement).
            Engine::ECS::ComponentMask dirty;
            dirty.set(m_positionId);
            dirty.set(m_moveTargetId);
            m_queryId = ecs.queries.createDirtyQuery(required(), excluded(), dirty, ecs.stores);
        }

        auto clamp = [](float v, float a, float b)
        { return std::max(a, std::min(v, b)); };
        auto len2 = [](float x, float z)
        { return std::sqrt(x * x + z * z); };

        // Gameplay uses meters; stop once we're within a small radius.
        const float arrivalRadius = 1.0f; // Increased from 0.25 for easier stopping
        // Snappy stop: no long slowdown phase.
        // We'll keep full speed, but clamp the final step so we hit the arrival radius cleanly.

        const auto &q = ecs.queries.get(m_queryId);
        for (uint32_t archetypeId : q.matchingArchetypeIds)
        {
            Engine::ECS::ArchetypeStore *storePtr = ecs.stores.get(archetypeId);
            if (!storePtr)
                continue;
            auto &store = *storePtr;

            auto dirtyRows = ecs.queries.consumeDirtyRows(m_queryId, archetypeId);
            if (dirtyRows.empty())
                continue;

            // Accessors: positions, velocities, moveTargets, moveSpeeds
            auto &positions = const_cast<std::vector<Engine::ECS::Position> &>(store.positions());
            auto &velocities = const_cast<std::vector<Engine::ECS::Velocity> &>(store.velocities());

            // You'll add these to ArchetypeStore once you wire MoveTarget/MoveSpeed there:
            auto &targets = const_cast<std::vector<Engine::ECS::MoveTarget> &>(store.moveTargets());
            auto &speeds = const_cast<std::vector<Engine::ECS::MoveSpeed> &>(store.moveSpeeds());

            auto *facings = store.hasFacing() ? &const_cast<std::vector<Engine::ECS::Facing> &>(store.facings()) : nullptr;
            const uint32_t n = store.size();

            for (uint32_t i : dirtyRows)
            {
                if (i >= n)
                    continue;

                auto &pos = positions[i];
                auto &vel = velocities[i];
                auto &tgt = targets[i];
                const auto &spd = speeds[i];

                if (!tgt.active)
                    continue;

                float dx = tgt.x - pos.x;
                float dz = tgt.z - pos.z;
                float dist = len2(dx, dz);

                if (dist <= arrivalRadius)
                {
                    vel.x = vel.y = vel.z = 0.0f;
                    tgt.active = 0;
                    ecs.markDirty(m_velocityId, archetypeId, i);
                    ecs.markDirty(m_moveTargetId, archetypeId, i);
                    continue;
                }

                // normalize
                if (dist > 1e-6f)
                {
                    dx /= dist;
                    dz /= dist;
                }
                else
                {
                    dx = dz = 0.0f;
                }

                // Snappy stop: constant speed, but avoid "creeping" by clamping so we reach the
                // arrival radius in this frame (then the next update will stop & clear the target).
                float desiredSpeed = spd.value;
                if (dt > 1e-6f)
                {
                    const float remaining = std::max(0.0f, dist - arrivalRadius);
                    const float maxSpeedThisFrame = remaining / dt;
                    desiredSpeed = std::min(desiredSpeed, maxSpeedThisFrame);
                }

                vel.x = dx * desiredSpeed;
                vel.z = dz * desiredSpeed;
                // Height axis is y; gameplay movement stays on the ground plane for now.
                vel.y = 0.0f;

                ecs.markDirty(m_velocityId, archetypeId, i);

                // Keep active targets updating every frame even if position doesn't change
                // (e.g., blocked movement). This avoids scanning the whole store.
                if (tgt.active)
                    ecs.markDirty(m_moveTargetId, archetypeId, i);

                // Update facing if moving
                if (facings && (vel.x != 0.0f || vel.z != 0.0f))
                {
                    (*facings)[i].yaw = std::atan2(vel.x, vel.z);
                    ecs.markDirty(m_facingId, archetypeId, i);
                }
            }
        }
    }

private:
    Engine::ECS::QueryId m_queryId = Engine::ECS::QueryManager::InvalidQuery;
    uint32_t m_positionId = Engine::ECS::ComponentRegistry::InvalidID;
    uint32_t m_velocityId = Engine::ECS::ComponentRegistry::InvalidID;
    uint32_t m_moveTargetId = Engine::ECS::ComponentRegistry::InvalidID;
    uint32_t m_facingId = Engine::ECS::ComponentRegistry::InvalidID;
};