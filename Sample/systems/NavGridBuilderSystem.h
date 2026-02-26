#pragma once
/*
  NavGridBuilderSystem
  --------------------
  Purpose:
    - Scans all entities with Obstacle + ObstacleRadius components.
    - Marks their blocked cells in the NavGrid.
    - Runs ONCE at init (or on demand), not every frame ideally.
*/

#include "ECS/SystemFormat.h"
#include "NavGrid.h"

class NavGridBuilderSystem : public Engine::ECS::SystemBase
{
public:
    NavGridBuilderSystem(NavGrid *grid)
        : m_grid(grid)
    {
        setRequiredNames({"Position", "Obstacle", "ObstacleRadius"});
        setExcludedNames({"Disabled", "Dead"});
    }

    const char *name() const override { return "NavGridBuilderSystem"; }

    // Call this if obstacles change, or just check a dirty flag
    void update(Engine::ECS::ECSContext &ecs, float dt) override
    {
        if (!m_grid)
            return;

        // Reset grid: mark all as walkable
        std::fill(m_grid->blocked.begin(), m_grid->blocked.end(), 0);

        for (const auto &ptr : ecs.stores.stores())
        {
            if (!ptr)
                continue;
            auto &store = *ptr;

            if (!store.signature().containsAll(required()))
                continue;
            if (!store.signature().containsNone(excluded()))
                continue;

            auto &positions = store.positions();
            auto &radii = store.obstacleRadii();
            const uint32_t n = store.size();

            for (uint32_t i = 0; i < n; ++i)
            {
                // Inflate by 1.0m beyond physical radius for safe clearance on 2m grid
                m_grid->markObstacle(positions[i].x, positions[i].z, radii[i].r + 1.0f);
            }
        }
    }

private:
    NavGrid *m_grid = nullptr;
};
