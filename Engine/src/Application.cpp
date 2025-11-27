#include "Engine/Application.h"
#include "Engine/Window.h"
#include "Engine/VulkanContext.h"
#include <chrono>
#include <iostream>

namespace Engine
{

    struct Application::Impl
    {
        std::unique_ptr<Window> window;
        std::unique_ptr<VulkanContext> vkContext;
        bool running = true;
        EventCallbackFn eventCallback;
    };

    Application::Application()
        : m_Impl(std::make_unique<Impl>())
    {
        // Create window (platform-specific implementation returns a concrete Window)
        m_Impl->window = Window::Create({"Engine Window", 1280, 720});

        m_Impl->window->SetEventCallback([this](const std::string &e)
                                         { this->handleWindowEvent(e); });

        // Create Vulkan context (owns instance, surface creation using the window handle)
        m_Impl->vkContext = std::make_unique<VulkanContext>(*m_Impl->window);
    }

    Application::~Application() = default;

    void Application::Run()
    {
        using clock = std::chrono::high_resolution_clock;
        auto last = clock::now();

        while (m_Impl->running)
        {
            auto now = clock::now();
            std::chrono::duration<float> dt = now - last;
            last = now;

            TimeStep ts;
            ts.DeltaSeconds = dt.count();

            // Poll OS events via window
            m_Impl->window->OnUpdate();

            // Call user update
            OnUpdate(ts);

            // Call renderer (user or engine) to submit draws. For now just the hook:
            OnRender();

            // Optionally throttle to limit CPU spin
        }
    }

    void Application::handleWindowEvent(const std::string &name)
    {
        if (m_Impl->eventCallback)
            m_Impl->eventCallback(name);
        if (name == "WindowClose" || name == "EscapePressed")
        {
            Close();
        }
    }

    Window &Application::GetWindow() { return *m_Impl->window; }

    void Application::Close() { m_Impl->running = false; }

} // namespace Engine