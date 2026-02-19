#pragma once
#include "ECS/SystemFormat.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

class CommandSystem : public Engine::ECS::SystemBase
{
public:
    CommandSystem()
    {
        // Require MoveTarget + MoveSpeed so we only command movable units.
        // Require MoveTarget + MoveSpeed + Path + Facing so we command units properly.
        setRequiredNames({"MoveTarget", "MoveSpeed", "Path", "Facing"});
        setExcludedNames({"Disabled", "Dead"});
    }

    const char *name() const override { return "CommandSystem"; }

    void buildMasks(Engine::ECS::ComponentRegistry &registry) override
    {
        Engine::ECS::SystemBase::buildMasks(registry);
        m_selectedId = registry.ensureId("Selected");
    }

    // Set the last clicked target; system will write it to entities on next update.
    void SetGlobalMoveTarget(float x, float y, float z)
    {
        m_hasPending = true;
        m_pendingX = x;
        m_pendingY = y;
        m_pendingZ = z;
    }

    void update(Engine::ECS::ArchetypeStoreManager &mgr, float /*dt*/) override
    {
        if (!m_hasPending)
            return;

        // Formation tuning in gameplay world coordinates (meters).
        // Ground plane is X/Z (Y is height).
        constexpr float spacing = 0.5f; // distance between formation slots

        constexpr float kMinWorld = -10000.0f;
        constexpr float kMaxWorld = 10000.0f;

        auto clamp = [](float v, float a, float b)
        { return std::max(a, std::min(v, b)); };

        for (const auto &ptr : mgr.stores())
        {
            if (!ptr)
                continue;
            auto &store = *ptr;
            if (!store.signature().containsAll(required()))
                continue;
            if (!store.signature().containsNone(excluded()))
                continue;

            auto &targets = const_cast<std::vector<Engine::ECS::MoveTarget> &>(store.moveTargets());
            auto &masks = store.rowMasks();
            const uint32_t n = store.size();

            // Collect selected rows first so we can distribute target offsets.
            std::vector<uint32_t> selectedRows;
            selectedRows.reserve(n);
            for (uint32_t i = 0; i < n; ++i)
            {
                if (!masks[i].matches(required(), excluded()))
                    continue;
                if (m_selectedId == Engine::ECS::ComponentRegistry::InvalidID || !masks[i].has(m_selectedId))
                    continue;
                selectedRows.push_back(i);
            }

            const uint32_t selCount = static_cast<uint32_t>(selectedRows.size());
            if (selCount == 0)
                continue;

            // Distribute selected units over a centered grid around the clicked target.
            // Example (selCount=5, side=3): offsets at (-1, -1), (0,-1), (1,-1), (-1,0), (0,0) * spacing.
            const uint32_t side = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<float>(selCount))));
            const float half = (static_cast<float>(side) - 1.0f) * 0.5f;

            for (uint32_t k = 0; k < selCount; ++k)
            {
                const uint32_t row = k / side;
                const uint32_t col = k % side;
                const float ox = (static_cast<float>(col) - half) * spacing;
                const float oz = (static_cast<float>(row) - half) * spacing;

                const uint32_t i = selectedRows[k];
                auto &paths = const_cast<std::vector<Engine::ECS::Path> &>(store.paths());
                auto &path = paths[i];
                path.valid = false; // Invalidate path so PathfindingSystem replans
                path.count = 0;
                path.current = 0;

                targets[i].x = clamp(m_pendingX + ox, kMinWorld, kMaxWorld);
                targets[i].y = m_pendingY; // height
                targets[i].z = clamp(m_pendingZ + oz, kMinWorld, kMaxWorld);
                targets[i].active = 1;
            }

            std::cout << "[CommandSystem] Selected=" << selCount
                      << " baseTarget=(" << m_pendingX << "," << m_pendingZ << ")"
                      << " gridSide=" << side << " spacing=" << spacing << "\n";
        }

        m_hasPending = false;
    }

private:
    bool m_hasPending = false;
    float m_pendingX = 0.0f, m_pendingY = 0.0f, m_pendingZ = 0.0f;
    uint32_t m_selectedId = Engine::ECS::ComponentRegistry::InvalidID;
};