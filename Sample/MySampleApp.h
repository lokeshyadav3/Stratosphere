#pragma once

#include "Engine/Application.h"
#include "Engine/Camera.h"
#include "assets/Handles.h"

#include "update.h"

#include <glm/glm.hpp>

#include <memory>

namespace Engine
{
    class AssetManager;
    class SModelRenderPassModule;
}

class MySampleApp : public Engine::Application
{
public:
    MySampleApp();
    ~MySampleApp() override;

    void Close() override;
    void OnUpdate(Engine::TimeStep ts) override;
    void OnRender() override;

private:
    void setupECSFromPrefabs();
    void OnEvent(const std::string &name);

private:
    struct RTSCameraController
    {
        glm::vec3 pos{0.0f, 70.0f, 70.0f};
        float yawDeg = -45.0f;
        float pitchDeg = -50.0f;

        float basePanSpeed = 0.0020f;
        float zoomSpeed = 5.0f;
        float minHeight = 15.0f;
        float maxHeight = 200.0f;
    };

    std::unique_ptr<Engine::AssetManager> m_assets;

    std::shared_ptr<Engine::SModelRenderPassModule> m_smodelPass;
    RTSCameraController m_rtsCam;
    glm::vec2 m_lastMouse{0.0f, 0.0f};
    bool m_isPanning = false;
    float m_scrollDelta = 0.0f;
    Engine::Camera m_camera;

    Sample::SystemRunner m_systems;

    Engine::ModelHandle m_testModel{};
};
