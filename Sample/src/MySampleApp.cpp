#include "MySampleApp.h"

#include "Engine/SModelRenderPassModule.h"
#include "Engine/VulkanContext.h"
#include "Engine/Window.h"

#include "ECS/Prefab.h"
#include "ECS/PrefabSpawner.h"
#include "ECS/ECSContext.h"

#include "ScenarioSpawner.h"
#include "VerifyLoadSModel.h"

#include "assets/AssetManager.h"

#include <filesystem>
#include <iostream>
#include <sstream>

MySampleApp::MySampleApp() : Engine::Application()
{
    m_assets = std::make_unique<Engine::AssetManager>(
        GetVulkanContext().GetDevice(),
        GetVulkanContext().GetPhysicalDevice(),
        GetVulkanContext().GetGraphicsQueue(),
        GetVulkanContext().GetGraphicsQueueFamilyIndex());

    // Configure projection once; we keep it in sync every frame too.
    auto &win = GetWindow();
    const float aspect = static_cast<float>(win.GetWidth()) / static_cast<float>(win.GetHeight());
    m_camera.SetPerspective(glm::radians(60.0f), aspect, 0.1f, 200.0f);

    // RTS camera initialization (fixed yaw/pitch; position drives the view)
    m_rtsCam.pos = {0.0f, 70.0f, 70.0f};
    m_rtsCam.yawDeg = -45.0f;
    m_rtsCam.pitchDeg = -50.0f;
    m_camera.SetPosition(m_rtsCam.pos);
    m_camera.SetRotation(m_rtsCam.yawDeg, m_rtsCam.pitchDeg);

    // Seed mouse position so the first frame doesn't produce a huge delta.
    double mx = 0.0, my = 0.0;
    win.GetCursorPosition(mx, my);
    m_lastMouse = {static_cast<float>(mx), static_cast<float>(my)};

    // Load and validate a sample model.
    m_testModel = Sample::VerifyLoadSModel(*m_assets, "assets/Knight/Knight.smodel");

    // Render the loaded .smodel as a single object.
    if (m_testModel.isValid())
    {
        m_smodelPass = std::make_shared<Engine::SModelRenderPassModule>();
        m_smodelPass->setAssets(m_assets.get());
        m_smodelPass->setModel(m_testModel);
        m_smodelPass->setCamera(&m_camera);
        GetRenderer().registerPass(m_smodelPass);
    }

    // Allow gameplay systems to resolve RenderMesh handles to loaded assets.
    m_systems.SetAssetManager(m_assets.get());

    setupECSFromPrefabs();

    // Systems can be initialized after prefabs are registered.
    m_systems.Initialize(GetECS().components);

    // Hook engine window events into our handler.
    SetEventCallback([this](const std::string &e)
                     { this->OnEvent(e); });
}

MySampleApp::~MySampleApp() = default;

void MySampleApp::Close()
{
    vkDeviceWaitIdle(GetVulkanContext().GetDevice());

    if (m_assets)
    {
        m_assets->release(m_testModel);
        m_assets->garbageCollect();
    }

    m_smodelPass.reset();

    Engine::Application::Close();
}

void MySampleApp::OnUpdate(Engine::TimeStep ts)
{
    // Keep camera projection in sync with window size.
    auto &win = GetWindow();
    const float aspect = static_cast<float>(win.GetWidth()) / static_cast<float>(win.GetHeight());
    m_camera.SetPerspective(glm::radians(60.0f), aspect, 0.1f, 200.0f);

    // Read mouse and compute per-frame delta.
    double mx = 0.0, my = 0.0;
    win.GetCursorPosition(mx, my);
    const glm::vec2 mouse{static_cast<float>(mx), static_cast<float>(my)};
    const glm::vec2 delta = mouse - m_lastMouse;
    m_lastMouse = mouse;

    // Pan (RMB drag) in ground plane.
    if (m_isPanning)
    {
        glm::vec3 forward;
        forward.x = std::cos(glm::radians(m_rtsCam.yawDeg)) * std::cos(glm::radians(m_rtsCam.pitchDeg));
        forward.y = std::sin(glm::radians(m_rtsCam.pitchDeg));
        forward.z = std::sin(glm::radians(m_rtsCam.yawDeg)) * std::cos(glm::radians(m_rtsCam.pitchDeg));
        forward = glm::normalize(forward);

        const glm::vec3 worldUp{0.0f, 1.0f, 0.0f};
        const glm::vec3 right = glm::normalize(glm::cross(forward, worldUp));

        glm::vec3 forwardXZ{forward.x, 0.0f, forward.z};
        glm::vec3 rightXZ{right.x, 0.0f, right.z};

        const float forwardLen2 = glm::dot(forwardXZ, forwardXZ);
        const float rightLen2 = glm::dot(rightXZ, rightXZ);
        if (forwardLen2 > 1e-6f)
            forwardXZ *= 1.0f / std::sqrt(forwardLen2);
        if (rightLen2 > 1e-6f)
            rightXZ *= 1.0f / std::sqrt(rightLen2);

        const float panScale = m_rtsCam.basePanSpeed * m_rtsCam.pos.y;
        // Classic RTS/map drag feel:
        // - drag right  -> camera moves left
        // - drag down   -> camera moves forward
        m_rtsCam.pos += (-rightXZ * delta.x + forwardXZ * delta.y) * panScale;
    }

    // Zoom (mouse wheel) modifies height.
    const float wheel = m_scrollDelta;
    m_scrollDelta = 0.0f;
    if (wheel != 0.0f)
    {
        m_rtsCam.pos.y -= wheel * m_rtsCam.zoomSpeed;
        m_rtsCam.pos.y = glm::clamp(m_rtsCam.pos.y, m_rtsCam.minHeight, m_rtsCam.maxHeight);
    }

    // Apply RTS state to engine camera every frame.
    m_camera.SetPosition(m_rtsCam.pos);
    m_camera.SetRotation(m_rtsCam.yawDeg, m_rtsCam.pitchDeg);

    m_systems.Update(GetECS(), ts.DeltaSeconds);
}

void MySampleApp::OnRender()
{
    // Rendering handled by Renderer/Engine.
}

void MySampleApp::setupECSFromPrefabs()
{
    auto &ecs = GetECS();

    // Load all prefab definitions from JSON copied next to executable.
    // (CMake copies Sample/entities/*.json -> <build>/Sample/entities/)
    size_t prefabCount = 0;
    try
    {
        for (const auto &entry : std::filesystem::directory_iterator("entities"))
        {
            if (!entry.is_regular_file())
                continue;
            if (entry.path().extension() != ".json")
                continue;
            const std::string path = entry.path().generic_string();
            const std::string jsonText = Engine::ECS::readFileText(path);
            if (jsonText.empty())
            {
                std::cerr << "[Prefab] Failed to read: " << path << "\n";
                continue;
            }
            Engine::ECS::Prefab p = Engine::ECS::loadPrefabFromJson(jsonText, ecs.components, ecs.archetypes, *m_assets);
            if (p.name.empty())
            {
                std::cerr << "[Prefab] Missing name in: " << path << "\n";
                continue;
            }
            ecs.prefabs.add(p);
            ++prefabCount;
            std::cout << "[Prefab] Loaded " << p.name << " from " << path << "\n";
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "[Prefab] Failed to enumerate entities/: " << e.what() << "\n";
        return;
    }

    if (prefabCount == 0)
    {
        std::cerr << "[Prefab] No prefabs loaded from entities/*.json\n";
        return;
    }

    Sample::SpawnFromScenarioFile(ecs, "Scinerio.json", /*selectSpawned=*/true);
}

void MySampleApp::OnEvent(const std::string &name)
{
    std::istringstream iss(name);
    std::string evt;
    iss >> evt;

    if (evt == "MouseButtonRightDown")
    {
        m_isPanning = true;
        auto &win = GetWindow();
        double mx = 0.0, my = 0.0;
        win.GetCursorPosition(mx, my);
        m_lastMouse = {static_cast<float>(mx), static_cast<float>(my)};
        return;
    }

    if (evt == "MouseButtonRightUp")
    {
        m_isPanning = false;
        return;
    }

    if (evt == "MouseScroll")
    {
        double xoff = 0.0, yoff = 0.0;
        iss >> xoff >> yoff;
        (void)xoff;
        m_scrollDelta += static_cast<float>(yoff);
        return;
    }

    if (name == "EscapePressed")
    {
        Close();
        return;
    }
}
