#pragma once
#include <vulkan/vulkan.h>
#include "imgui.h"

struct SDL_Window;
union SDL_Event;

namespace mg {
namespace renderer { class RendererContext; class SwapChain; }
}

namespace mg::gameui {

struct MG_ImGuiVulkanConfig {
    SDL_Window*         window = nullptr;
    VkInstance          instance = VK_NULL_HANDLE;
    VkPhysicalDevice    physical_device = VK_NULL_HANDLE;
    VkDevice            device = VK_NULL_HANDLE;
    uint32_t            queue_family = 0;
    VkQueue             queue = VK_NULL_HANDLE;
    VkPipelineCache     pipeline_cache = VK_NULL_HANDLE;
    VkDescriptorPool    descriptor_pool = VK_NULL_HANDLE;
    int                 min_image_count = 2;
    int                 image_count = 2;
    VkRenderPass        render_pass = VK_NULL_HANDLE;
    VkAllocationCallbacks* allocator = nullptr;
    float               content_scale = 0.0f;   // 0 = auto-detect via ImGui_ImplSDL2_GetContentScaleForDisplay(0)
    bool                enable_viewports = false;
    void (*check_vk_result)(VkResult) = nullptr;
};

void MG_ImGuiVulkan_Init(const MG_ImGuiVulkanConfig& cfg);
void MG_ImGuiVulkan_Shutdown();
void MG_ImGuiVulkan_NewFrame();

// ── 应用层共享样板（Launcher / Editor 共用，TD-1）──────────────────────

// renderer + swapchain + ImGui 一条龙初始化（任一步失败 MG_LOG_ERROR 并返回 false）
bool MG_VulkanImGui_Init(SDL_Window* window,
                         renderer::RendererContext& renderer,
                         renderer::SwapChain& swapchain,
                         VkAllocationCallbacks* allocator,
                         bool enable_imgui_viewports = false);

// BeginFrame：swapchain 重建检查 + ImGui NewFrame。
// 返回 true = 本帧发生了 resize（调用方可重建自有的离屏渲染目标）。
bool MG_VulkanImGui_BeginFrame(SDL_Window* window, renderer::SwapChain& swapchain);

// 在已 acquire 的 cmd 上跑 swapchain render pass 并提交 ImGui 帧
// （clear_color 按预乘 alpha 写入，与既有调用点一致）。
void MG_VulkanImGui_RenderFrame(renderer::SwapChain& swapchain, VkCommandBuffer cmd,
                                ImDrawData* draw_data, const ImVec4& clear_color);

// 事件：转发 ImGui_ImplSDL2_ProcessEvent + 窗口 resize → RequestRebuild
void MG_VulkanImGui_ProcessEvent(SDL_Window* window, SDL_Event* event,
                                 renderer::SwapChain& swapchain);

// 关闭尾部：MG_ImGuiVulkan_Shutdown + swapchain.Destroy + renderer.Shutdown。
// 调用方负责先 vkDeviceWaitIdle 并清理自有资源。
void MG_VulkanImGui_Shutdown(renderer::RendererContext& renderer,
                             renderer::SwapChain& swapchain);

} // namespace mg::gameui
