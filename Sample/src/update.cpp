#include "update.h"

namespace Sample
{
    void SystemRunner::Initialize(Engine::ECS::ComponentRegistry &registry)
    {
        if (m_initialized)
            return;

        // Ensure common IDs exist up-front (also used by scenario spawner selection).
        (void)registry.ensureId("Selected");

        m_command.buildMasks(registry);
        m_steering.buildMasks(registry);
        m_navGridBuilder.buildMasks(registry);
        m_pathfinding.buildMasks(registry);
        m_movement.buildMasks(registry);
        m_characterAnim.buildMasks(registry);
        m_renderModel.buildMasks(registry);

        // Initialize NavGrid (cover map area)
        m_navGrid.rebuild(2.0f, -400.0f, -400.0f, 400.0f, 400.0f);

        m_initialized = true;
    }

    void SystemRunner::Update(Engine::ECS::ECSContext &ecs, float dtSeconds)
    {
        if (!m_initialized)
            Initialize(ecs.components);

        if (dtSeconds <= 0.0f)
            return;

        // 1. Input
        m_command.update(ecs.stores, dtSeconds);

        // 2. NavGrid (Rebuild grid from static obstacles)
        // Optimization: Could check dirty flag, but for now run every frame
        m_navGridBuilder.update(ecs.stores, dtSeconds);

        // 3. Pathfinding (Plan paths for units with invalid/new targets)
        m_pathfinding.update(ecs.stores, dtSeconds);

        // 4. Steering (Follow waypoints, update facing)
        m_steering.update(ecs.stores, dtSeconds);

        // 5. Movement integration
        m_movement.update(ecs.stores, dtSeconds);
        
        // 6. Animation selection
        m_characterAnim.update(ecs.stores, dtSeconds);
        
        // 7. Render
        m_renderModel.update(ecs.stores, dtSeconds);
    }

    void SystemRunner::SetAssetManager(Engine::AssetManager *assets)
    {
        m_characterAnim.setAssetManager(assets);
        m_renderModel.setAssetManager(assets);
    }

    void SystemRunner::SetRenderer(Engine::Renderer *renderer)
    {
        m_renderModel.setRenderer(renderer);
    }

    void SystemRunner::SetCamera(Engine::Camera *camera)
    {
        m_renderModel.setCamera(camera);
    }

    void SystemRunner::SetGlobalMoveTarget(float x, float y, float z)
    {
        m_command.SetGlobalMoveTarget(x, y, z);
    }
}
