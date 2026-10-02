#pragma once
#include "app/application.h"
#include "renderer/renderer_context.h"
#include "renderer/swapchain.h"
#include "renderer/shader.h"
#include "renderer/buffer.h"
#include "renderer/pipeline.h"
#include "renderer/descriptor.h"
#include "resource/resource_manager.h"
#include "imgui.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include "scene/systems/sprite_render_system.h"
#include "scene/systems/tilemap_render_system.h"
#include "scene/systems/camera_system.h"
#include "scene/systems/animator_system.h"
#include "scene/systems/tilemap_mesh_builder.h"

namespace mg {
namespace editor {

class EditorContext;

class EditorApplication : public Application {
public:
    explicit EditorApplication(const std::string& project_path, EditorContext& ctx);
    ~EditorApplication() override;

    renderer::RendererContext* GetRenderer() { return &renderer_; }
    renderer::SwapChain* GetSwapChain() { return &swapchain_; }

protected:
    void OnInit() override;
    void OnShutdown() override;
    void BeginFrame() override;
    void EndFrame() override;
    bool OnProcessEvent(SDL_Event* event) override;

private:
    EditorContext& ctx_;

    // Viewport offscreen render target
    void CreateViewportRenderTarget();
    void DestroyViewportRenderTarget();
    void DestroyViewportRenderPass();

    // 重建 Tilemap VB
    void RebuildTilemapVB();

    // 从文件加载 SPIR-V
    static std::vector<uint32_t> LoadSpvFile(const char* path);

    std::string project_path_;
    VkAllocationCallbacks* allocator_ = nullptr;
    renderer::RendererContext renderer_;
    renderer::SwapChain swapchain_;

    ImVec4 clear_color_ = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);

    // ---- Viewport offscreen render target ----
    VkRenderPass viewport_rp_ = VK_NULL_HANDLE;
    VkImage viewport_image_ = VK_NULL_HANDLE;
    VkDeviceMemory viewport_image_memory_ = VK_NULL_HANDLE;
    VkImageView viewport_image_view_ = VK_NULL_HANDLE;
    VkFramebuffer viewport_fb_ = VK_NULL_HANDLE;
    VkDescriptorSet viewport_imgui_ds_ = VK_NULL_HANDLE;
    uint32_t viewport_w_ = 0;
    uint32_t viewport_h_ = 0;

    // ---- Sprite 渲染管线 ----
    renderer::Shader sprite_vert_;
    renderer::Shader sprite_frag_;
    renderer::Pipeline sprite_pipeline_;
    renderer::Pipeline sprite_pipeline_wireframe_;  // ViewMode::Wireframe 变体（poly_mode=LINE）
    renderer::Buffer sprite_vb_;
    renderer::Buffer sprite_instance_vb_;
    renderer::Buffer sprite_ubo_;
    renderer::DescriptorSetLayout sprite_ubo_layout_;
    renderer::DescriptorSet sprite_ubo_set_;

    // ---- Tilemap 渲染 ----
    renderer::Buffer tilemap_vb_;
    renderer::Buffer tilemap_dummy_instance_vb_;
    uint32_t tilemap_vertex_count_ = 0;
    std::vector<scene::TilemapBatch> tilemap_batches_;
    // 瓦片动画时钟：帧哈希变化时重建 VB
    float tile_anim_time_ = 0.0f;
    uint32_t tile_anim_hash_ = 0;

    // ---- ECS Render Systems ----
    scene::SpriteRenderSystem sprite_system_;
    scene::SpriteRenderSystem::Config sprite_cfg_;
    scene::TilemapRenderSystem tilemap_system_;
    scene::TilemapRenderSystem::Config tilemap_cfg_;
    scene::TilemapRenderSystem::VBState tilemap_vb_state_;
};



} // namespace editor
} // namespace mg
