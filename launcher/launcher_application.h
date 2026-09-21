#pragma once
#include "app/application.h"
#include "src/renderer/renderer_context.h"
#include "src/renderer/swapchain.h"
#include "imgui.h"
#include <vulkan/vulkan.h>

namespace mg {
namespace launcher {

// Launcher 专用 Application 子类
// 封装 Vulkan 初始化、ImGui 集成、帧渲染与呈现
class LauncherApplication : public Application {
public:
    LauncherApplication();
    ~LauncherApplication() override;

protected:
    void OnInit() override;
    void OnShutdown() override;
    void BeginFrame() override;
    void EndFrame() override;
    bool OnProcessEvent(SDL_Event* event) override;

private:
    VkAllocationCallbacks* allocator_ = nullptr;
    renderer::RendererContext renderer_;
    renderer::SwapChain swapchain_;

    ImVec4 clear_color_ = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
};

} // namespace launcher
} // namespace mg
