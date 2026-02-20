#include "update.h"

namespace Sample
{
    void SystemRunner::Initialize(Engine::ECS::ECSContext &ecs)
    {
        if (m_initialized)
            return;

        // Ensure queries get incrementally updated as new stores appear.
        ecs.WireQueryManager();

        auto &registry = ecs.components;

        // Ensure common IDs exist up-front (also used by scenario spawner selection).
        (void)registry.ensureId("Selected");

        m_command.buildMasks(registry);
        m_steering.buildMasks(registry);
        m_spatial.buildMasks(registry);
        m_avoidance.buildMasks(registry);
        m_movement.buildMasks(registry);
        m_characterAnim.buildMasks(registry);
        m_poseUpdate.buildMasks(registry);
        m_renderModel.buildMasks(registry);

        // Neighbor radius (meters). Tune later; matches SpatialIndexSystem doc.
        m_spatial.setCellSize(m_spatial.getCellSize());

        m_initialized = true;
    }

    void SystemRunner::Update(Engine::ECS::ECSContext &ecs, float dtSeconds)
    {
        if (!m_initialized)
            Initialize(ecs);

        if (dtSeconds <= 0.0f)
            return;

        // Suggested order per LocalAvoidanceSystem.h
        m_command.update(ecs, dtSeconds);
        m_steering.update(ecs, dtSeconds);
        // m_spatial.update(ecs.stores, dtSeconds);    // Disabled: SpatialIndexSystem
        // m_avoidance.update(ecs.stores, dtSeconds);  // Disabled: LocalAvoidanceSystem
        m_movement.update(ecs, dtSeconds);
        m_characterAnim.update(ecs, dtSeconds);
        m_poseUpdate.update(ecs, dtSeconds);
        m_renderModel.update(ecs, dtSeconds);
    }

    void SystemRunner::SetAssetManager(Engine::AssetManager *assets)
    {
        m_characterAnim.setAssetManager(assets);
        m_poseUpdate.setAssetManager(assets);
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
