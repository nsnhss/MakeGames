#include "mg_imgui_backend.h"
#include "mg_imgui_theme.h"
#include "core/debug_log.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_vulkan.h"
#include "renderer/renderer_context.h"
#include "renderer/swapchain.h"
#include "core/log.h"
#include <imgui.h>
#include <SDL.h>

namespace mg::gameui {

void MG_ImGuiVulkan_Init(const MG_ImGuiVulkanConfig& cfg) {
    EDITOR_LOG("MG_ImGuiVulkan_Init enter");
    IMGUI_CHECKVERSION();
    EDITOR_LOG("MG_ImGuiVulkan_Init creating ImGui context...");
    ImGui::CreateContext();
    EDITOR_LOG("MG_ImGuiVulkan_Init ImGui context created");
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard
                    | ImGuiConfigFlags_NavEnableGamepad
                    | ImGuiConfigFlags_DockingEnable;
    io.IniFilename = "imgui.ini";
    if (cfg.enable_viewports)
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    float scale = cfg.content_scale > 0.0f
        ? cfg.content_scale
        : ImGui_ImplSDL2_GetContentScaleForDisplay(0);

    ImGui::GetStyle().ScaleAllSizes(scale);
    ImGui::GetStyle().FontScaleDpi = scale;

    EDITOR_LOG("MG_ImGuiVulkan_Init applying theme and fonts...");
    MG_ApplyTheme_UE5Dark();
    MG_LoadFonts(scale);
    EDITOR_LOG("MG_ImGuiVulkan_Init theme/fonts done, init SDL2/Vulkan backends...");

    ImGui_ImplSDL2_InitForVulkan(cfg.window);

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance       = cfg.instance;
    init_info.PhysicalDevice = cfg.physical_device;
    init_info.Device         = cfg.device;
    init_info.QueueFamily    = cfg.queue_family;
    init_info.Queue          = cfg.queue;
    init_info.PipelineCache  = cfg.pipeline_cache;
    init_info.DescriptorPool = cfg.descriptor_pool;
    init_info.MinImageCount  = cfg.min_image_count;
    init_info.ImageCount     = cfg.image_count;
    init_info.Allocator      = cfg.allocator;
    init_info.PipelineInfoMain.RenderPass  = cfg.render_pass;
    init_info.PipelineInfoMain.Subpass     = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.CheckVkResultFn = cfg.check_vk_result;
    EDITOR_LOG("MG_ImGuiVulkan_Init calling ImGui_ImplVulkan_Init...");
    ImGui_ImplVulkan_Init(&init_info);
    EDITOR_LOG("MG_ImGuiVulkan_Init exit");
}

void MG_ImGuiVulkan_Shutdown() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

void MG_ImGuiVulkan_NewFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

// ── 应用层共享样板（TD-1）───────────────────────────────────────────

bool MG_VulkanImGui_Init(SDL_Window* window,
                         renderer::RendererContext& renderer,
                         renderer::SwapChain& swapchain,
                         VkAllocationCallbacks* allocator,
                         bool enable_imgui_viewports) {
    if (!window) return false;
    if (!renderer.Initialize(window)) {
        MG_LOG_ERROR("Failed to initialize Vulkan renderer.");
        return false;
    }
    if (!swapchain.Initialize(window, &renderer, allocator)) {
        MG_LOG_ERROR("Failed to initialize SwapChain.");
        return false;
    }

    MG_ImGuiVulkanConfig cfg;
    cfg.window           = window;
    cfg.instance         = renderer.GetInstance();
    cfg.physical_device  = renderer.GetPhysicalDevice();
    cfg.device           = renderer.GetDevice();
    cfg.queue_family     = renderer.GetQueueFamily();
    cfg.queue            = renderer.GetQueue();
    cfg.pipeline_cache   = renderer.GetPipelineCache();
    cfg.descriptor_pool  = renderer.GetDescriptorPool();
    cfg.min_image_count  = (int)swapchain.GetImageCount();
    cfg.image_count      = (int)swapchain.GetImageCount();
    cfg.render_pass      = swapchain.GetRenderPass();
    cfg.allocator        = allocator;
    cfg.enable_viewports = enable_imgui_viewports;
    cfg.check_vk_result  = renderer::RendererContext::CheckVkResult;
    MG_ImGuiVulkan_Init(cfg);
    return true;
}

bool MG_VulkanImGui_BeginFrame(SDL_Window* window, renderer::SwapChain& swapchain) {
    bool resized = false;
    if (swapchain.NeedsRebuild()) {
        // 最小化时尺寸为 0：跳过本次重建，下一帧重试
        int w = 0, h = 0;
        SDL_GetWindowSize(window, &w, &h);
        if (w > 0 && h > 0) {
            ImGui_ImplVulkan_SetMinImageCount((int)swapchain.GetImageCount());
            swapchain.Resize(w, h);
            resized = true;
        }
    }
    MG_ImGuiVulkan_NewFrame();
    return resized;
}

void MG_VulkanImGui_RenderFrame(renderer::SwapChain& swapchain, VkCommandBuffer cmd,
                                ImDrawData* draw_data, const ImVec4& clear_color) {
    VkClearValue clear_value = {};
    clear_value.color.float32[0] = clear_color.x * clear_color.w;
    clear_value.color.float32[1] = clear_color.y * clear_color.w;
    clear_value.color.float32[2] = clear_color.z * clear_color.w;
    clear_value.color.float32[3] = clear_color.w;
    swapchain.BeginRenderPass(cmd, &clear_value);
    ImGui_ImplVulkan_RenderDrawData(draw_data, cmd);
    swapchain.EndRenderPass(cmd);
}

void MG_VulkanImGui_ProcessEvent(SDL_Window* window, SDL_Event* event,
                                 renderer::SwapChain& swapchain) {
    ImGui_ImplSDL2_ProcessEvent(event);
    if (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_RESIZED
        && event->window.windowID == SDL_GetWindowID(window)) {
        swapchain.RequestRebuild();
    }
}

void MG_VulkanImGui_Shutdown(renderer::RendererContext& renderer,
                             renderer::SwapChain& swapchain) {
    MG_ImGuiVulkan_Shutdown();
    swapchain.Destroy();
    renderer.Shutdown();
}

} // namespace mg::gameui
