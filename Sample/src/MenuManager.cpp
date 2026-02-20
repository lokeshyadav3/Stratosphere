#include "MenuManager.h"

#include <imgui.h>
#include <chrono>

namespace
{
    // Helpers to get time delta in seconds for fading
    static double getTimeSeconds()
    {
        using namespace std::chrono;
        static auto start = high_resolution_clock::now();
        auto now = high_resolution_clock::now();
        return duration<double>(now - start).count();
    }
}

MenuManager::MenuManager()
{
    m_selected = 0;
    m_show = true;
    m_timeSinceShown = 0.0f;
    m_alpha = 1.0f;
    m_hasSaveFile = false;
}

void MenuManager::SetTextureLoader(TextureLoaderFn loader)
{
    m_loader = loader;
    if (m_loader)
    {
        // Attempt to load (non-fatal if missing)
        try
        {
            m_background = nullptr; // Don't load background (as per your requirement)

            m_tex[0] = m_loader("assets/raw/newgame.png");
            (void)m_tex[0];

            m_tex[1] = m_loader("assets/raw/continuegame.png");
            (void)m_tex[1];

            m_tex[2] = m_loader("assets/raw/exit.png");
            (void)m_tex[2];
        }
        catch (const std::exception &e)
        {
            // Ignore loader errors; continue with text-only buttons
            m_background = nullptr;
            m_tex = {nullptr, nullptr, nullptr};
        }
        catch (...)
        {
            // Ignore loader errors; continue with text-only buttons
            m_background = nullptr;
            m_tex = {nullptr, nullptr, nullptr};
        }
    }
}

void MenuManager::OnImGuiFrame()
{
    // Can happen during shutdown if the app tears down ImGui mid-frame.
    if (ImGui::GetCurrentContext() == nullptr)
        return;

    if (!m_show && !m_fadingToGame)
        return;

    // Update fade animation
    static double lastTime = getTimeSeconds();
    double nowTime = getTimeSeconds();
    float dt = static_cast<float>(nowTime - lastTime);
    lastTime = nowTime;

    m_timeSinceShown += dt;

    if (m_show)
    {
        // Menu fading in
        m_alpha = std::min(1.0f, m_timeSinceShown / m_fadeDuration);
    }
    else if (m_fadingToGame)
    {
        // Game world fading in after "New Game" clicked
        m_gameAlpha = std::min(1.0f, m_timeSinceShown / m_fadeDuration);

        if (m_gameAlpha >= 1.0f)
        {
            m_fadingToGame = false; // Fade complete
        }
    }
    else
    {
        // Menu fading out
        m_alpha = 1.0f - std::min(1.0f, m_timeSinceShown / m_fadeDuration);
        if (m_alpha <= 0.0f)
        {
            // Fully hidden
            return;
        }
    }

    // Setup a centered, fullscreen invisible window for the menu (no titlebar)
    ImGuiIO &io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0.0f));
    ImGui::Begin("##MainMenuFullscreen", nullptr, flags);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    // Draw menu background (if texture available)--replaced with black for simplicity
    // Main menu: dark background. Pause menu: greyish-white background.
    if (m_mode == Mode::MainMenu)
    {
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(0, 0), io.DisplaySize, ImGui::GetColorU32(ImVec4(0.5, 0.5, 0.5, 1.0f)));
    }
    else
    {
        // Light grey with fade alpha.
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(0, 0), io.DisplaySize, ImGui::GetColorU32(ImVec4(0.50f, 0.50f, 0.50f, m_alpha)));
    }

    // Capture keyboard & mouse for menu navigation (so underlying app doesn't react)
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::BeginChild("MenuButtonsRegion", ImVec2(0, 0), false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration);

    // Place the buttons centered on screen
    const float buttonWidth = 300.0f;
    const float buttonHeight = 72.0f;

    constexpr float kSpacingY = 12.0f;
    const float totalHeight = buttonHeight * 3.0f + kSpacingY * 2.0f;
    const ImVec2 winSize = ImGui::GetWindowSize();
    const float startX = (winSize.x - buttonWidth) * 0.5f;
    const float startY = (winSize.y - totalHeight) * 0.5f;

    // Keyboard handling
    handleInput();

    // Draw each button (image if we have it)
    const char *labels[3] = {"New Game", "Continue", "Exit"};
    for (int i = 0; i < 3; ++i)
    {
        ImGui::SetCursorPos(ImVec2(startX, startY + static_cast<float>(i) * (buttonHeight + kSpacingY)));

        ImGui::PushID(i);
        ImVec4 tint = ImVec4(1, 1, 1, m_alpha);
        ImVec4 bgTint = (m_selected == i) ? ImVec4(0.2f, 0.45f, 0.8f, m_alpha) : ImVec4(0, 0, 0, 0.0f);

        bool clicked = false;
        // If this is the Continue button and no save exists, treat as disabled
        bool enabled = true;
        if (i == 1 && m_mode == Mode::MainMenu && !m_hasSaveFile)
            enabled = false;
        if (m_tex[i])
        {
            // Image button (if texture available)
            ImGui::PushStyleColor(ImGuiCol_Button, bgTint);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
            if (ImGui::ImageButton(m_tex[i], ImVec2(buttonWidth, buttonHeight), ImVec2(0, 0), ImVec2(1, 1), 0, ImVec4(0, 0, 0, 0), tint))
                clicked = true;
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
        }
        else
        {
            // Text button fallback
            ImGui::PushStyleColor(ImGuiCol_Button, bgTint);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
            if (ImGui::Button(labels[i], ImVec2(buttonWidth, buttonHeight)))
                clicked = true;
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
        }

        if (clicked)
        {
            if (!enabled)
            {
                // ignore clicks when disabled
            }
            else
            {
                if (i == 0)
                    m_result = Result::NewGame;
                else if (i == 1)
                    m_result = Result::ContinueGame;
                else
                    m_result = Result::Exit;
            }
        }

        // keyboard activation
        if (m_selected == i && ImGui::IsKeyPressed(ImGuiKey_Enter))
        {
            if (enabled)
            {
                if (i == 0)
                    m_result = Result::NewGame;
                else if (i == 1)
                    m_result = Result::ContinueGame;
                else
                    m_result = Result::Exit;
            }
        }

        ImGui::PopID();
    }

    ImGui::EndChild();

    // If hiding and fully transparent, we won't render next frame
    ImGui::End();
}

void MenuManager::handleInput()
{
    if (ImGui::GetCurrentContext() == nullptr)
        return;

    ImGuiIO &io = ImGui::GetIO();
    // Arrow navigation
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) || ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
    {
        m_selected = (m_selected + 3 - 1) % 3;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_RightArrow))
    {
        m_selected = (m_selected + 1) % 3;
    }
    // Mouse hovering will update selection
    ImGuiIO &iio = ImGui::GetIO();
    // (leave hover selection to ImGui itself if needed)
}