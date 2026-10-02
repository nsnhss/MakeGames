#include "launcher_application.h"
#include "src/core/log.h"
#include "src/gameui/mg_imgui_backend.h"
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace mg {
namespace launcher {

LauncherApplication::LauncherApplication()
    : Application([]() {
        Application::Desc d;
        d.title = "MakeGames Launcher";
        d.width = 1440;
        d.height = 900;
        d.maximized = true;
        d.resizable = true;
        d.high_dpi = true;
        d.sdl_flags = SDL_WINDOW_VULKAN;
        return d;
    }())
{
}

LauncherApplication::~LauncherApplication() = default;

void LauncherApplication::OnInit()
{
    SDL_Window* window = GetWindow();
    if (!window) return;

    if (!mg::gameui::MG_VulkanImGui_Init(window, renderer_, swapchain_, allocator_)) {
        MG_LOG_ERROR("Launcher: Failed to initialize Vulkan + ImGui.");
        return;
    }
}

void LauncherApplication::OnShutdown()
{
    // 先销毁 SDL 窗口（释放 Vulkan surface）
    DestroyWindow();
    vkDeviceWaitIdle(renderer_.GetDevice());

    mg::gameui::MG_VulkanImGui_Shutdown(renderer_, swapchain_);
}

void LauncherApplication::BeginFrame()
{
    mg::gameui::MG_VulkanImGui_BeginFrame(GetWindow(), swapchain_);
}

void LauncherApplication::EndFrame()
{
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);

    if (!is_minimized && swapchain_.AcquireNextImage()) {
        VkCommandBuffer cmd = swapchain_.BeginCommandBuffer();
        mg::gameui::MG_VulkanImGui_RenderFrame(swapchain_, cmd, draw_data, clear_color_);
        swapchain_.SubmitAndPresent(cmd);
    }
}

bool LauncherApplication::OnProcessEvent(SDL_Event* event)
{
    mg::gameui::MG_VulkanImGui_ProcessEvent(GetWindow(), event, swapchain_);
    return false;
}

} // namespace launcher
} // namespace mg
