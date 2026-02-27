#include "Engine/PerformanceMonitor.h"
#include "Engine/VulkanContext.h"
#include "Engine/Renderer.h"
#include "Engine/Window.h"

#include <imgui.h>
#include <algorithm>
#include <numeric>
#include <atomic>
#include <cmath>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <dxgi1_4.h>
#include <psapi.h>
#endif

namespace Engine
{
    // -----------------------------------------------------------------------
    // Helpers (Windows)
    // -----------------------------------------------------------------------
#ifdef _WIN32
    static uint64_t fileTimeToU64(const FILETIME& ft)
    {
        return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    }
#endif

    // -----------------------------------------------------------------------
    // Global atomic draw call counter
    // -----------------------------------------------------------------------
    static std::atomic<uint32_t> g_drawCallCount{0};

    void DrawCallCounter::increment(uint32_t count)
    {
        g_drawCallCount.fetch_add(count, std::memory_order_relaxed);
    }

    void DrawCallCounter::reset()
    {
        g_drawCallCount.store(0, std::memory_order_relaxed);
    }

    uint32_t DrawCallCounter::get()
    {
        return g_drawCallCount.load(std::memory_order_relaxed);
    }

    // -----------------------------------------------------------------------
    // Ctor / Dtor
    // -----------------------------------------------------------------------
    PerformanceMonitor::PerformanceMonitor()
        : m_frameStart(Clock::now())
        , m_lastFrameEnd(Clock::now())
    {
    }

    PerformanceMonitor::~PerformanceMonitor()
    {
        cleanup();
    }

    // -----------------------------------------------------------------------
    // Init / Cleanup
    // -----------------------------------------------------------------------
    void PerformanceMonitor::init(VulkanContext* ctx, Renderer* renderer, Window* window)
    {
        m_ctx = ctx;
        m_renderer = renderer;
        m_window = window;
        m_initialized = true;
        m_frameTimeHistory.clear();

        querySystemInfo(); // GPU name, total VRAM, DXGI adapter, initial CPU times
    }

    void PerformanceMonitor::cleanup()
    {
        m_initialized = false;
        m_frameTimeHistory.clear();

#ifdef _WIN32
        if (m_dxgiAdapter)
        {
            m_dxgiAdapter->Release();
            m_dxgiAdapter = nullptr;
        }
#endif
    }

    // -----------------------------------------------------------------------
    // One-time system info query (called from init)
    // -----------------------------------------------------------------------
    void PerformanceMonitor::querySystemInfo()
    {
        if (!m_ctx) return;

        // --- GPU name from Vulkan ---
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(m_ctx->GetPhysicalDevice(), &props);
        m_gpuName = props.deviceName;

        // --- Total VRAM from Vulkan memory properties (always available) ---
        VkPhysicalDeviceMemoryProperties memProps{};
        vkGetPhysicalDeviceMemoryProperties(m_ctx->GetPhysicalDevice(), &memProps);
        VkDeviceSize totalDeviceLocal = 0;
        for (uint32_t i = 0; i < memProps.memoryHeapCount; i++)
        {
            if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
            {
                totalDeviceLocal += memProps.memoryHeaps[i].size;
            }
        }
        m_vramTotalMB = static_cast<float>(totalDeviceLocal) / (1024.0f * 1024.0f);

#ifdef _WIN32
        // --- Find matching DXGI adapter by vendorID + deviceID ---
        IDXGIFactory4* factory = nullptr;
        HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory4), reinterpret_cast<void**>(&factory));
        if (SUCCEEDED(hr) && factory)
        {
            IDXGIAdapter1* adapter1 = nullptr;
            for (UINT i = 0; factory->EnumAdapters1(i, &adapter1) != DXGI_ERROR_NOT_FOUND; ++i)
            {
                DXGI_ADAPTER_DESC1 desc{};
                adapter1->GetDesc1(&desc);

                if (desc.VendorId == props.vendorID && desc.DeviceId == props.deviceID)
                {
                    // Upgrade to IDXGIAdapter3 for QueryVideoMemoryInfo
                    adapter1->QueryInterface(__uuidof(IDXGIAdapter3),
                                             reinterpret_cast<void**>(&m_dxgiAdapter));
                    // Also grab total VRAM from DXGI (more accurate for dedicated memory)
                    m_vramTotalMB = static_cast<float>(desc.DedicatedVideoMemory) / (1024.0f * 1024.0f);
                    adapter1->Release();
                    break;
                }
                adapter1->Release();
            }
            factory->Release();
        }

        // --- Seed CPU-time tracking ---
        FILETIME idle, kernel, user;
        if (GetSystemTimes(&idle, &kernel, &user))
        {
            m_prevIdleTime   = fileTimeToU64(idle);
            m_prevKernelTime = fileTimeToU64(kernel);
            m_prevUserTime   = fileTimeToU64(user);
        }
#endif
    }

    // -----------------------------------------------------------------------
    // Per-frame system metrics (VRAM used, CPU %, RAM)
    // -----------------------------------------------------------------------
    void PerformanceMonitor::updateSystemMetrics()
    {
#ifdef _WIN32
        // --- VRAM usage via DXGI ---
        if (m_dxgiAdapter)
        {
            DXGI_QUERY_VIDEO_MEMORY_INFO info{};
            HRESULT hr = m_dxgiAdapter->QueryVideoMemoryInfo(
                0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info);
            if (SUCCEEDED(hr))
            {
                m_vramUsedMB = static_cast<float>(info.CurrentUsage) / (1024.0f * 1024.0f);
            }
        }

        // --- System-wide CPU usage via GetSystemTimes ---
        {
            FILETIME idle, kernel, user;
            if (GetSystemTimes(&idle, &kernel, &user))
            {
                uint64_t curIdle   = fileTimeToU64(idle);
                uint64_t curKernel = fileTimeToU64(kernel);
                uint64_t curUser   = fileTimeToU64(user);

                uint64_t idleDiff   = curIdle   - m_prevIdleTime;
                uint64_t kernelDiff = curKernel - m_prevKernelTime;
                uint64_t userDiff   = curUser   - m_prevUserTime;
                uint64_t totalSys   = kernelDiff + userDiff; // kernel includes idle

                if (totalSys > 0)
                {
                    m_cpuUsagePercent =
                        (1.0f - static_cast<float>(idleDiff) / static_cast<float>(totalSys)) * 100.0f;
                }

                m_prevIdleTime   = curIdle;
                m_prevKernelTime = curKernel;
                m_prevUserTime   = curUser;
            }
        }

        // --- Process RAM (working set) ---
        {
            PROCESS_MEMORY_COUNTERS pmc{};
            pmc.cb = sizeof(pmc);
            if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
            {
                m_ramUsedMB = static_cast<float>(pmc.WorkingSetSize) / (1024.0f * 1024.0f);
            }
        }
#endif
    }

    // -----------------------------------------------------------------------
    // Frame begin / end
    // -----------------------------------------------------------------------
    void PerformanceMonitor::beginFrame()
    {
        m_frameStart = Clock::now();
        DrawCallCounter::reset();
    }

    void PerformanceMonitor::endFrame()
    {
        auto now = Clock::now();
        
        // Calculate frame time
        float frameTimeMs = std::chrono::duration<float, std::milli>(now - m_lastFrameEnd).count();
        m_lastFrameEnd = now;
        
        // CPU time is the time spent between beginFrame and endFrame
        m_cpuTimeMs = std::chrono::duration<float, std::milli>(now - m_frameStart).count();
        
        // Store frame time in history
        m_frameTimeHistory.push_back(frameTimeMs);
        if (m_frameTimeHistory.size() > HISTORY_SIZE)
        {
            m_frameTimeHistory.pop_front();
        }

        // Get draw call count from global counter
        m_lastFrameDrawCalls = DrawCallCounter::get();

        // Get GPU time from renderer if available
        if (m_renderer)
        {
            m_gpuTimeMs = m_renderer->getGpuTimeMs();
        }

        // Apply EMA (Exponential Moving Average) smoothing for display values
        m_smoothedFrameTimeMs = EMA_SMOOTHING_FACTOR * frameTimeMs + 
                                (1.0f - EMA_SMOOTHING_FACTOR) * m_smoothedFrameTimeMs;
        m_smoothedCpuTimeMs = EMA_SMOOTHING_FACTOR * m_cpuTimeMs + 
                              (1.0f - EMA_SMOOTHING_FACTOR) * m_smoothedCpuTimeMs;
        m_smoothedGpuTimeMs = EMA_SMOOTHING_FACTOR * m_gpuTimeMs + 
                              (1.0f - EMA_SMOOTHING_FACTOR) * m_smoothedGpuTimeMs;

        // Update FPS metrics periodically (100ms)
        m_updateTimer += frameTimeMs / 1000.0f;
        if (m_updateTimer >= UPDATE_INTERVAL)
        {
            updateMetrics();
            m_updateTimer = 0.0f;
        }

        // Update system metrics less frequently (500ms) to reduce overhead
        m_sysUpdateTimer += frameTimeMs / 1000.0f;
        if (m_sysUpdateTimer >= SYS_UPDATE_INTERVAL)
        {
            updateSystemMetrics();

            // Smooth VRAM and CPU % so the overlay doesn't jump around
            m_smoothedVramUsedMB = EMA_SMOOTHING_FACTOR * m_vramUsedMB +
                                   (1.0f - EMA_SMOOTHING_FACTOR) * m_smoothedVramUsedMB;
            m_smoothedCpuUsagePercent = EMA_SMOOTHING_FACTOR * m_cpuUsagePercent +
                                       (1.0f - EMA_SMOOTHING_FACTOR) * m_smoothedCpuUsagePercent;
            m_sysUpdateTimer = 0.0f;
        }

        m_frameTimeMs = frameTimeMs;
    }

    void PerformanceMonitor::recordDrawCall(uint32_t primitiveCount)
    {
        DrawCallCounter::increment(1);
        m_primitiveCount += primitiveCount;
    }

    void PerformanceMonitor::resetDrawCalls()
    {
        DrawCallCounter::reset();
        m_primitiveCount = 0;
    }

    void PerformanceMonitor::toggle()
    {
        m_visible = !m_visible;
    }

    void PerformanceMonitor::updateMetrics()
    {
        if (m_frameTimeHistory.empty())
            return;

        // Calculate average FPS
        float totalTime = 0.0f;
        for (float t : m_frameTimeHistory)
        {
            totalTime += t;
        }
        float avgFrameTime = totalTime / static_cast<float>(m_frameTimeHistory.size());
        m_avgFPS = (avgFrameTime > 0.0f) ? (1000.0f / avgFrameTime) : 0.0f;

        // Calculate percentile FPS
        calculatePercentileFPS();
    }

    void PerformanceMonitor::calculatePercentileFPS()
    {
        if (m_frameTimeHistory.size() < 10)
        {
            m_1percentLowFPS = m_avgFPS;
            m_01percentLowFPS = m_avgFPS;
            return;
        }

        // Copy and sort frame times (descending - longest times first = worst frames)
        std::vector<float> sortedTimes(m_frameTimeHistory.begin(), m_frameTimeHistory.end());
        std::sort(sortedTimes.begin(), sortedTimes.end(), std::greater<float>());

        // 1% low = average of worst 1% of frames
        size_t onePercentCount = std::max(static_cast<size_t>(1), sortedTimes.size() / 100);
        float sum1Percent = 0.0f;
        for (size_t i = 0; i < onePercentCount; ++i)
        {
            sum1Percent += sortedTimes[i];
        }
        float avg1PercentTime = sum1Percent / static_cast<float>(onePercentCount);
        m_1percentLowFPS = (avg1PercentTime > 0.0f) ? (1000.0f / avg1PercentTime) : 0.0f;

        // 0.1% low = the single worst frame (or average of worst 0.1%)
        size_t point1PercentCount = std::max(static_cast<size_t>(1), sortedTimes.size() / 1000);
        float sum01Percent = 0.0f;
        for (size_t i = 0; i < point1PercentCount; ++i)
        {
            sum01Percent += sortedTimes[i];
        }
        float avg01PercentTime = sum01Percent / static_cast<float>(point1PercentCount);
        m_01percentLowFPS = (avg01PercentTime > 0.0f) ? (1000.0f / avg01PercentTime) : 0.0f;
    }

    uint32_t PerformanceMonitor::getResolutionWidth() const
    {
        if (m_window)
            return m_window->GetWidth();
        return 0;
    }

    uint32_t PerformanceMonitor::getResolutionHeight() const
    {
        if (m_window)
            return m_window->GetHeight();
        return 0;
    }

    void PerformanceMonitor::renderOverlay()
    {
        if (!m_visible || !m_initialized)
            return;

        // Set up overlay window flags
        ImGuiWindowFlags windowFlags = 
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoMove;

        // Position in top-right corner with padding
        const float padding = 10.0f;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 workPos = viewport->WorkPos;
        ImVec2 workSize = viewport->WorkSize;
        ImVec2 windowPos(workPos.x + workSize.x - padding, workPos.y + padding);
        ImVec2 windowPivot(1.0f, 0.0f);
        ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, windowPivot);
        ImGui::SetNextWindowBgAlpha(0.75f);

        if (ImGui::Begin("Performance Monitor", nullptr, windowFlags))
        {
            // Title with styling
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.9f, 0.4f, 1.0f));
            ImGui::Text("Performance Monitor");
            ImGui::PopStyleColor();
            ImGui::Separator();

            // --- GPU Name ---
            if (!m_gpuName.empty())
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.85f, 1.0f, 1.0f));
                ImGui::Text("GPU: %s", m_gpuName.c_str());
                ImGui::PopStyleColor();
                ImGui::Spacing();
            }

            // --- FPS Section ---
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.4f, 1.0f));
            ImGui::Text("FPS");
            ImGui::PopStyleColor();
            
            // Color-code FPS based on performance
            ImVec4 fpsColor;
            if (m_avgFPS >= 60.0f)
                fpsColor = ImVec4(0.4f, 1.0f, 0.4f, 1.0f); // Green
            else if (m_avgFPS >= 30.0f)
                fpsColor = ImVec4(1.0f, 1.0f, 0.4f, 1.0f); // Yellow
            else
                fpsColor = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); // Red

            ImGui::PushStyleColor(ImGuiCol_Text, fpsColor);
            ImGui::Text("  Average: %.1f", m_avgFPS);
            ImGui::PopStyleColor();
            ImGui::Text("  1%% Low:  %.1f", m_1percentLowFPS);
            ImGui::Text("  0.1%% Low: %.1f", m_01percentLowFPS);

            ImGui::Spacing();

            // --- Frame Time Section ---
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.4f, 1.0f));
            ImGui::Text("Frame Time");
            ImGui::PopStyleColor();
            ImGui::Text("  Frame: %.2f ms", m_smoothedFrameTimeMs);
            ImGui::Text("  CPU:   %.2f ms", m_smoothedCpuTimeMs);
            
            if (m_gpuTimeMs > 0.0f)
            {
                ImGui::Text("  GPU:   %.2f ms", m_smoothedGpuTimeMs);
            }
            else
            {
                ImGui::TextDisabled("  GPU:   N/A");
            }

            ImGui::Spacing();

            // --- VRAM Section ---
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.4f, 1.0f));
            ImGui::Text("VRAM");
            ImGui::PopStyleColor();

            if (m_vramTotalMB > 0.0f)
            {
                float usedDisplay = (m_smoothedVramUsedMB > 0.0f) ? m_smoothedVramUsedMB : m_vramUsedMB;
                if (usedDisplay > 0.0f)
                {
                    float pct = (usedDisplay / m_vramTotalMB) * 100.0f;
                    // Color-code VRAM: green < 60%, yellow 60-85%, red > 85%
                    ImVec4 vramColor;
                    if (pct < 60.0f)
                        vramColor = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
                    else if (pct < 85.0f)
                        vramColor = ImVec4(1.0f, 1.0f, 0.4f, 1.0f);
                    else
                        vramColor = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);

                    ImGui::PushStyleColor(ImGuiCol_Text, vramColor);
                    ImGui::Text("  Used: %.0f / %.0f MB (%.0f%%)",
                                usedDisplay, m_vramTotalMB, pct);
                    ImGui::PopStyleColor();
                }
                else
                {
                    ImGui::Text("  Total: %.0f MB", m_vramTotalMB);
                    ImGui::TextDisabled("  Used:  N/A");
                }
            }
            else
            {
                ImGui::TextDisabled("  N/A");
            }

            ImGui::Spacing();

            // --- CPU Usage Section ---
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.4f, 1.0f));
            ImGui::Text("System");
            ImGui::PopStyleColor();

            {
                float cpuDisplay = (m_smoothedCpuUsagePercent > 0.0f)
                                       ? m_smoothedCpuUsagePercent
                                       : m_cpuUsagePercent;
                if (cpuDisplay > 0.0f)
                {
                    ImVec4 cpuColor;
                    if (cpuDisplay < 50.0f)
                        cpuColor = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
                    else if (cpuDisplay < 80.0f)
                        cpuColor = ImVec4(1.0f, 1.0f, 0.4f, 1.0f);
                    else
                        cpuColor = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);

                    ImGui::PushStyleColor(ImGuiCol_Text, cpuColor);
                    ImGui::Text("  CPU:  %.1f%%", cpuDisplay);
                    ImGui::PopStyleColor();
                }
                else
                {
                    ImGui::TextDisabled("  CPU:  N/A");
                }
            }

            if (m_ramUsedMB > 0.0f)
            {
                ImGui::Text("  RAM:  %.0f MB", m_ramUsedMB);
            }

            ImGui::Spacing();

            // --- Resolution & Display ---
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.4f, 1.0f));
            ImGui::Text("Display");
            ImGui::PopStyleColor();
            ImGui::Text("  Resolution: %ux%u", getResolutionWidth(), getResolutionHeight());

            ImGui::Spacing();

            // --- Draw Calls Section ---
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.4f, 1.0f));
            ImGui::Text("Rendering");
            ImGui::PopStyleColor();
            ImGui::Text("  Draw Calls: %u", m_lastFrameDrawCalls);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextDisabled("Press F1 to toggle");
        }
        ImGui::End();
    }

} // namespace Engine
