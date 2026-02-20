#pragma once
/*
  LocalAvoidanceSystem.h
  ----------------------
  Purpose:
    - Adjust velocities to prevent overlap using local separation, based on neighbors
      found via SpatialIndexSystem (hash grid, cellSize = neighbor radius R).

  Requirements:
    - Components present in stores: "Position", "Velocity", "Radius", "AvoidanceParams".
        - Optional component: "Separation" (extra desired spacing beyond radii).
    - SpatialIndexSystem must have run earlier in the frame (grid built).
    - Steering should have already produced a "preferred" velocity, stored in Velocity.
      This system keeps final speeds close to that magnitude.

  Suggested order per frame:
    CommandSystem -> SteeringSystem -> SpatialIndexSystem -> LocalAvoidanceSystem -> MovementSystem
*/

#include "ECS/SystemFormat.h"
#include "ECS/Components.h"
#include "ECS/ArchetypeStore.h"

// The grid index system for neighbor queries
#include "systems/SpatialIndexSystem.h"

#include <algorithm>
#include <cmath>

class LocalAvoidanceSystem : public Engine::ECS::SystemBase
{
public:
    LocalAvoidanceSystem(const SpatialIndexSystem *grid = nullptr)
        : m_grid(grid)
    {
        // Require the data we adjust/read
        setRequiredNames({"Position", "Velocity", "Radius", "AvoidanceParams"});
        setExcludedNames({"Disabled", "Dead"});
    }

    const char *name() const override { return "LocalAvoidanceSystem"; }

    void buildMasks(Engine::ECS::ComponentRegistry &registry) override
    {
        Engine::ECS::SystemBase::buildMasks(registry);
        m_velocityId = registry.ensureId("Velocity");
    }

    void setGrid(const SpatialIndexSystem *grid) { m_grid = grid; }

    void update(Engine::ECS::ECSContext &ecs, float dt) override
    {
        if (!m_grid)
            return;
        if (dt <= 0.0f)
            return;

        auto clamp = [](float v, float a, float b)
        { return std::max(a, std::min(v, b)); };
        auto length2 = [](float x, float z)
        { return x * x + z * z; };
        auto length = [&](float x, float z)
        { return std::sqrt(length2(x, z)); };
        auto lerp = [](float a, float b, float t)
        { return a + (b - a) * t; };

        if (m_queryId == Engine::ECS::QueryManager::InvalidQuery)
        {
            // Avoidance should run each frame for moving entities.
            Engine::ECS::ComponentMask dirty;
            dirty.set(m_velocityId);
            m_queryId = ecs.queries.createDirtyQuery(required(), excluded(), dirty, ecs.stores);
        }

        const auto &q = ecs.queries.get(m_queryId);
        for (uint32_t archetypeId : q.matchingArchetypeIds)
        {
            auto *storePtr = ecs.stores.get(archetypeId);
            if (!storePtr)
                continue;
            auto &store = *storePtr;

            auto dirtyRows = ecs.queries.consumeDirtyRows(m_queryId, archetypeId);
            if (dirtyRows.empty())
                continue;

            auto &positions = const_cast<std::vector<Engine::ECS::Position> &>(store.positions());
            auto &velocities = const_cast<std::vector<Engine::ECS::Velocity> &>(store.velocities());
            auto &radii = const_cast<std::vector<Engine::ECS::Radius> &>(store.radii());
            auto &params = const_cast<std::vector<Engine::ECS::AvoidanceParams> &>(store.avoidanceParams());
            const bool hasSep = store.hasSeparation();
            auto *sepsPtr = hasSep ? &const_cast<std::vector<Engine::ECS::Separation> &>(store.separations()) : nullptr;
            const uint32_t n = store.size();
            for (uint32_t row : dirtyRows)
            {
                if (row >= n)
                    continue;

                auto &p = positions[row];
                auto &v = velocities[row];
                const auto &r = radii[row];
                const auto &ap = params[row];
                const float sepSelf = sepsPtr ? (*sepsPtr)[row].value : 0.0f;

                // Accumulate separation correction from neighbors in 3x3 cells
                float corrX = 0.0f, corrZ = 0.0f;

                m_grid->forNeighbors(p.x, p.z, [&](uint32_t nStoreId, uint32_t nRow)
                                     {
                    // Skip self
                    if (nStoreId == archetypeId && nRow == row) return;

                    const auto* nStore = ecs.stores.get(nStoreId);
                    if (!nStore) return;
                    if (!nStore->hasPosition()) return;
                    if (!nStore->hasRadius()) return;
                    const bool nHasSep = nStore->hasSeparation();

                    const auto& np = nStore->positions()[nRow];
                    const auto& nr = nStore->radii()[nRow];
                    const float sepOther = (nHasSep ? nStore->separations()[nRow].value : 0.0f);

                    // 2D separation in gameplay ground plane (X/Z). Y is height.
                    float dx = p.x - np.x;
                    float dz = p.z - np.z;

                    const float dist2 = dx*dx + dz*dz;

                    float dist = (dist2 > 1e-12f) ? std::sqrt(dist2) : 0.0f;

                    const float desiredSeparation = sepSelf + sepOther;
                    const float desiredDist = (r.r + nr.r) + desiredSeparation;

                    // Overlap weight: strong when overlapping
                    float wOverlap = 0.0f;
                    if (dist < desiredDist && dist > 1e-6f)
                        wOverlap = (desiredDist - dist) / desiredDist;

                    // Only react within desiredDist (no extra neighborRadius).
                    float w = wOverlap;
                    if (w <= 0.0f) return;

                    // Normalize away vector
                    if (dist > 1e-6f) { dx /= dist; dz /= dist; } else { dx = 0.0f; dz = 0.0f; }

                    corrX += dx * w;
                    corrZ += dz * w; });

                // Combine correction with preferred velocity (from Steering)
                const float vPrefX = v.x;
                const float vPrefZ = v.z;
                const float prefSpeed = length(vPrefX, vPrefZ);

                // Apply strength
                float vRawX = vPrefX + ap.strength * corrX;
                float vRawZ = vPrefZ + ap.strength * corrZ;

                // Clamp speed to preferred magnitude (keeps MoveSpeed implicit)
                const float rawSpeed = length(vRawX, vRawZ);
                if (prefSpeed > 1e-6f && rawSpeed > prefSpeed)
                {
                    const float s = prefSpeed / rawSpeed;
                    vRawX *= s;
                    vRawZ *= s;
                }

                // Acceleration clamp relative to vPref
                float dvX = vRawX - vPrefX;
                float dvZ = vRawZ - vPrefZ;
                const float dvMag = length(dvX, dvZ);
                const float maxDv = ap.maxAccel * dt;
                if (dvMag > maxDv && dvMag > 1e-6f)
                {
                    const float s = maxDv / dvMag;
                    dvX *= s;
                    dvZ *= s;
                }

                const float vNewX = vPrefX + dvX;
                const float vNewZ = vPrefZ + dvZ;

                // Smooth the change to reduce jitter
                const float t = clamp(ap.blend, 0.0f, 1.0f);
                v.x = lerp(vPrefX, vNewX, t);
                v.z = lerp(vPrefZ, vNewZ, t);
                // Leave v.y unchanged (height axis)

                // Only propagate/keep active if still moving or velocity actually changed.
                const float dv1 = std::fabs(v.x - vPrefX) + std::fabs(v.z - vPrefZ);
                const float speed1 = std::fabs(v.x) + std::fabs(v.y) + std::fabs(v.z);
                if (dv1 > 1e-6f || speed1 > 1e-6f)
                    ecs.markDirty(m_velocityId, archetypeId, row);
            }
        }
    }

private:
    const SpatialIndexSystem *m_grid = nullptr; // not owned
    uint32_t m_velocityId = Engine::ECS::ComponentRegistry::InvalidID;
    Engine::ECS::QueryId m_queryId = Engine::ECS::QueryManager::InvalidQuery;
};