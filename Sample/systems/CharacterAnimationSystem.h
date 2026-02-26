#pragma once

#include "ECS/SystemFormat.h"
#include "assets/AssetManager.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

// Animation clip indices for Knight model
namespace AnimClips
{
    constexpr uint32_t RUN = 28;          // Armature|Run_No_Equipments
    constexpr uint32_t RUN_NO_EQUIP = 28; // Armature|Run_No_Equipments
    constexpr uint32_t IDLE = 65;         // Armature|Stand_Idle_0
    constexpr uint32_t IDLE_1 = 66;       // Armature|Stand_Idle_1
    constexpr uint32_t WALK = 112;        // Armature|Walk
}

// CharacterAnimationSystem
// - Advances per-entity RenderAnimation time
// - Automatically switches between Idle and Run animations based on movement state
// - Checks MoveTarget.active and Velocity to determine if entity is moving
class CharacterAnimationSystem : public Engine::ECS::SystemBase
{
public:
    CharacterAnimationSystem()
    {
        setRequiredNames({"RenderModel", "RenderAnimation"});
        setExcludedNames({"Disabled", "Dead"});
    }

    const char *name() const override { return "CharacterAnimationSystem"; }

    void setAssetManager(Engine::AssetManager *assets) { m_assets = assets; }

    void buildMasks(Engine::ECS::ComponentRegistry &registry) override
    {
        Engine::ECS::SystemBase::buildMasks(registry);
        m_selectedId = registry.ensureId("Selected");
        m_renderAnimId = registry.ensureId("RenderAnimation");
        m_velocityId = registry.ensureId("Velocity");
    }

    void update(Engine::ECS::ECSContext &ecs, float dt) override
    {
        if (!m_assets)
            return;

        // Velocity threshold to consider entity as "moving"
        constexpr float kVelocityThreshold = 0.1f;
        constexpr float kVelocityThreshold2 = kVelocityThreshold * kVelocityThreshold;

        if (m_queryId == Engine::ECS::QueryManager::InvalidQuery)
        {
            // We only need to re-run animation when movement state changes or while moving.
            // MovementSystem keeps movers active by re-marking Velocity dirty each frame.
            Engine::ECS::ComponentMask dirty;
            dirty.set(m_velocityId);
            m_queryId = ecs.queries.createDirtyQuery(required(), excluded(), dirty, ecs.stores);
        }

        const auto &q = ecs.queries.get(m_queryId);
        for (uint32_t archetypeId : q.matchingArchetypeIds)
        {
            auto *ptr = ecs.stores.get(archetypeId);
            if (!ptr)
                continue;

            auto &store = *ptr;
            if (!store.signature().containsAll(required()))
                continue;
            if (!store.signature().containsNone(excluded()))
                continue;
            if (!store.hasRenderModel() || !store.hasRenderAnimation())
                continue;

            auto &renderModels = store.renderModels();
            auto &renderAnimations = store.renderAnimations();
            const uint32_t n = store.size();

            auto dirtyRows = ecs.queries.consumeDirtyRows(m_queryId, archetypeId);
            if (dirtyRows.empty())
                continue;

            // Check if this store has velocity and move target for movement detection
            const bool hasVelocity = store.hasVelocity();
            const bool hasMoveTarget = store.hasMoveTarget();
            const auto *velocities = hasVelocity ? &store.velocities() : nullptr;
            const auto *targets = hasMoveTarget ? &store.moveTargets() : nullptr;

            for (uint32_t row : dirtyRows)
            {
                if (row >= n)
                    continue;

                const Engine::ModelHandle handle = renderModels[row].handle;
                Engine::ModelAsset *asset = m_assets->getModel(handle);
                if (!asset)
                    continue;

                auto &anim = renderAnimations[row];

                if (asset->animClips.empty())
                {
                    anim.clipIndex = 0;
                    anim.timeSec = 0.0f;
                    continue;
                }

                // --- Animation State Machine ---
                // Determine if entity is moving based on Velocity ONLY
                // (MoveTarget.active check removed - velocity is the ground truth)
                bool isMoving = false;

                if (velocities)
                {
                    const auto &vel = (*velocities)[row];
                    const float speed2 = vel.x * vel.x + vel.y * vel.y + vel.z * vel.z;
                    isMoving = (speed2 > kVelocityThreshold2);
                }

                // Select appropriate animation clip
                uint32_t desiredClip = isMoving ? AnimClips::RUN : AnimClips::IDLE;

                // Clamp to valid range
                const uint32_t maxClip = static_cast<uint32_t>(asset->animClips.size() - 1);
                desiredClip = std::min(desiredClip, maxClip);

                bool changed = false;

                // If clip changed, reset animation time
                if (anim.clipIndex != desiredClip)
                {
                    anim.clipIndex = desiredClip;
                    anim.timeSec = 0.0f;
                    changed = true;
                }

                // Policy: only animate while moving; idle pose is cached and reused.
                const bool desiredPlaying = isMoving;
                if (anim.playing != desiredPlaying)
                {
                    anim.playing = desiredPlaying;
                    // When stopping, clamp to the start of the idle clip for a stable pose.
                    if (!anim.playing)
                        anim.timeSec = 0.0f;
                    changed = true;
                }

                // Always loop when playing.
                if (anim.loop != true)
                {
                    anim.loop = true;
                    changed = true;
                }

                // --- Advance Animation Time ---
                const float duration = asset->animClips[anim.clipIndex].durationSec;
                if (!anim.playing || duration <= 1e-6f)
                {
                    if (changed)
                    {
                        ecs.markDirty(m_renderAnimId, archetypeId, row);
                    }
                    continue;
                }

                const float delta = dt * anim.speed;
                if (std::abs(delta) > 1e-9f)
                {
                    anim.timeSec += delta;
                    changed = true;
                }
                if (anim.loop)
                {
                    anim.timeSec = std::fmod(anim.timeSec, duration);
                    if (anim.timeSec < 0.0f)
                        anim.timeSec += duration;
                }
                else
                {
                    if (anim.timeSec < 0.0f)
                        anim.timeSec = 0.0f;
                    if (anim.timeSec > duration)
                        anim.timeSec = duration;
                }

                if (changed)
                {
                    ecs.markDirty(m_renderAnimId, archetypeId, row);
                }
            }
        }
    }

private:
    Engine::AssetManager *m_assets = nullptr;
    uint32_t m_selectedId = Engine::ECS::ComponentRegistry::InvalidID;
    Engine::ECS::QueryId m_queryId = Engine::ECS::QueryManager::InvalidQuery;
    uint32_t m_renderAnimId = Engine::ECS::ComponentRegistry::InvalidID;
    uint32_t m_velocityId = Engine::ECS::ComponentRegistry::InvalidID;
};
