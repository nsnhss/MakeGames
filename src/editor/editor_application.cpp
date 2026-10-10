// editor_application.cpp
// EditorApplication — 编辑器主应用类实现。
// 负责 Vulkan 渲染管线初始化、Viewport 渲染目标管理、SPIR-V 着色器加载、
// ImGui 与 Vulkan 对接、以及编辑器专用的 Sprite/Tilemap/Animator ECS 渲染系统。
#include "editor_application.h"
#include "editor_context.h"
#include "gameui/mg_imgui_backend.h"
#include "renderer/sprite_pipeline_setup.h"
#include "renderer/texture.h"
#include "scene/scene.h"
#include "scene/world.h"
#include "scene/components/transform.h"
#include "scene/components/sprite.h"
#include "scene/components/camera.h"
#include "scene/components/tilemap.h"
#include "scene/systems/sprite_vertex_layout.h"
#include "scene/systems/tile_animation.h"
#include "audio/audio_manager.h"
#include <stdio.h>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#endif
#include "core/debug_log.h"


#ifdef IMGUI_IMPL_VULKAN_USE_VOLK
#define VOLK_IMPLEMENTATION
#include <volk.h>
#endif
namespace mg {
namespace editor {

EditorApplication::EditorApplication(const std::string& project_path, EditorContext& ctx)
    : Application([]() {
        Application::Desc d;
        d.title = "MakeGames Editor";
        d.width = 1600;
        d.height = 900;
        d.maximized = true;
        d.resizable = true;
        d.high_dpi = true;
        d.sdl_flags = SDL_WINDOW_VULKAN;
        return d;
    }()),
      ctx_(ctx),
      project_path_(project_path)
{
    EDITOR_LOG("EditorApplication::EditorApplication enter");
}

EditorApplication::~EditorApplication()
{
    EDITOR_LOG("EditorApplication::~EditorApplication");
}

// 从文件加载 SPIR-V 着色器二进制，返回 uint32 数组。
// 自动搜索：先尝试相对路径，失败则在 Windows 下用 exe 目录反推项目根路径。
std::vector<uint32_t> EditorApplication::LoadSpvFile(const char* path)
{
    FILE* f = fopen(path, "rb");

    // 如果 cwd 打开失败，尝试从 exe 目录解析（Launcher 启动时工作目录错误）
    if (!f) {
#ifdef _WIN32
        wchar_t wexe_path[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, wexe_path, MAX_PATH);

        char exe_path[MAX_PATH] = {};
        WideCharToMultiByte(CP_UTF8, 0, wexe_path, -1, exe_path, MAX_PATH, nullptr, nullptr);

        // 找到最后一个反斜杠，截断为目录
        char* last_slash = strrchr(exe_path, '\\');
        if (last_slash) *(last_slash + 1) = '\0';

        // exe 在 cmake-build-release/editor/，上两级到项目根
        char full_path[MAX_PATH * 2];
        _snprintf(full_path, sizeof(full_path), "%s..\\..\\%s", exe_path, path);
        f = fopen(full_path, "rb");
#endif
    }

    if (!f) {
        EDITOR_LOG("[LoadSpvFile] FAILED to open: " << path);
        return {};
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint32_t> data(size / 4);
    if (data.empty()) {
        fclose(f);
        return {};
    }
    size_t read = fread(data.data(), 4, data.size(), f);
    (void)read;
    fclose(f);
    return data;
}

// Callback for SpriteRenderSystem — delegates to ResourceManager
static VkDescriptorSet SpriteGetTextureDS(void*, const std::string& path) {
    return ResourceManager::Get().GetOrCreateSamplerDS(path);
}
static bool SpriteGetAtlasRect(void*, const std::string& path, float out_uv_rect[4]) {
    return ResourceManager::Get().GetSpriteAtlasRect(path, out_uv_rect);
}
static VkDescriptorSet SpriteGetAtlasDS(void*) {
    return ResourceManager::Get().GetSpriteAtlasDS();
}

// 重建 Tilemap VB：按 tileset 分批次收集所有可见 layer 的顶点
void EditorApplication::RebuildTilemapVB()
{
    auto* scene = ctx_.GetActiveScene();
    if (!scene) return;
    auto* world = scene->GetWorld();
    if (!world) return;

    std::vector<scene::TilemapVertex> verts;
    verts.reserve(4096);

    // tileset 数据源：TilesetAsset + 真实纹理尺寸（UV 用）
    scene::TilesetRenderInfo info;  // 复用 scratch（构建器在下次回调前消费完毕）
    scene::BuildTilemapMesh(world,
        [this, &info](const std::string& ts_name, int, int) -> const scene::TilesetRenderInfo* {
            const auto* ts = ctx_.GetTileset(ts_name);
            if (!ts || !ts->texture_ptr || !ts->texture_ptr->IsValid()) return nullptr;
            info.columns       = ts->columns;
            info.tile_width    = ts->tile_width;
            info.tile_height   = ts->tile_height;
            info.start_index   = ts->start_index;
            info.tile_count    = ts->tile_count;
            info.tex_width     = (float)ts->texture_ptr->GetWidth();
            info.tex_height    = (float)ts->texture_ptr->GetHeight();
            info.descriptor_set = ResourceManager::Get().GetOrCreateSamplerDS(ts->texture_ptr);
            info.animations    = &ts->animations;
            return &info;
        },
        /*respect_layer_visibility=*/true, tile_anim_time_, verts, tilemap_batches_);

    if (!verts.empty()) {
        size_t bytes = verts.size() * sizeof(scene::TilemapVertex);
        if (!tilemap_vb_.IsValid() || tilemap_vb_.GetSize() < (VkDeviceSize)bytes) {
            tilemap_vb_.Destroy();
            tilemap_vb_.Create(&renderer_, bytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, true, allocator_);
        }
        tilemap_vb_.Upload(verts.data(), bytes);
        tilemap_vertex_count_ = (uint32_t)verts.size();
    } else {
        tilemap_vertex_count_ = 0;
    }
}


void EditorApplication::OnInit()
{
    EDITOR_LOG("EditorApplication::OnInit enter");
    SDL_Window* window = GetWindow();
    if (!window) return;

    if (!gameui::MG_VulkanImGui_Init(window, renderer_, swapchain_, allocator_,
                             /*enable_imgui_viewports=*/true)) {
        EDITOR_LOG("Failed to initialize Vulkan + ImGui");
        return;
    }
    EDITOR_LOG("EditorApplication::OnInit MG_VulkanImGui_Init done");

    // 在第一帧 NewFrame 前设置项目级 imgui.ini，确保 ImGui 正确加载项目级布局
    if (!project_path_.empty()) {
        static std::string imgui_ini_path;
        imgui_ini_path = project_path_ + "/imgui.ini";
        std::filesystem::path ini_p(imgui_ini_path);
        if (!std::filesystem::exists(ini_p.parent_path())) {
            std::filesystem::create_directories(ini_p.parent_path());
        }
        ImGui::GetIO().IniFilename = imgui_ini_path.c_str();
        EDITOR_LOG("EditorApplication::OnInit IniFilename=" << imgui_ini_path);
    }

    // ---- Create Viewport render target ----
    viewport_rt_.Create(renderer_, swapchain_, allocator_);
    ctx_.SetViewportSize(viewport_rt_.GetWidth(), viewport_rt_.GetHeight());

    // ---- Load sprite shaders ----
    auto vert_spv = LoadSpvFile("assets/shaders/sprite.vert.spv");
    auto frag_spv = LoadSpvFile("assets/shaders/sprite.frag.spv");
    if (!vert_spv.empty()) {
        sprite_vert_.Create(renderer_.GetDevice(), vert_spv.data(), vert_spv.size() * sizeof(uint32_t), allocator_);
    }
    if (!frag_spv.empty()) {
        sprite_frag_.Create(renderer_.GetDevice(), frag_spv.data(), frag_spv.size() * sizeof(uint32_t), allocator_);
    }

    // ---- Create UBO layout & set ----
    if (sprite_vert_.IsValid() && sprite_frag_.IsValid()) {
        renderer::CreateSpriteUBOLayoutAndSet(renderer_, allocator_, sprite_ubo_layout_, sprite_ubo_set_);
    }

    // ---- Create UBO buffer ----
    renderer::CreateSpriteUBOBuffer(renderer_, allocator_, sprite_ubo_set_, sprite_ubo_);


    // ---- Initialize ResourceManager (texture loading + sampler DS) ----
    ResourceManager::Get().Initialize(&renderer_, allocator_);

    // ---- Initialize AudioManager ----
    AudioManager::Get().Init();

    // ---- Create Sprite Pipeline ----
    if (sprite_vert_.IsValid() && sprite_frag_.IsValid() && viewport_rt_.GetRenderPass() != VK_NULL_HANDLE
        && sprite_ubo_layout_.IsValid()) {
        renderer::CreateSpritePipeline(renderer_, allocator_, viewport_rt_.GetRenderPass(), sprite_vert_, sprite_frag_,
                                       sprite_ubo_layout_, ResourceManager::Get().GetSamplerLayout(),
                                       (float)viewport_rt_.GetWidth(), (float)viewport_rt_.GetHeight(),
                                       VK_POLYGON_MODE_FILL, sprite_pipeline_);

        // Wireframe 变体：同配置仅多边形模式为线框（需 fillModeNonSolid 特性）
        if (renderer_.IsWireframeSupported()) {
            renderer::CreateSpritePipeline(renderer_, allocator_, viewport_rt_.GetRenderPass(), sprite_vert_, sprite_frag_,
                                           sprite_ubo_layout_, ResourceManager::Get().GetSamplerLayout(),
                                           (float)viewport_rt_.GetWidth(), (float)viewport_rt_.GetHeight(),
                                           VK_POLYGON_MODE_LINE, sprite_pipeline_wireframe_);
        }
        ctx_.GetViewport().SetWireframeSupported(renderer_.IsWireframeSupported());
    }

    // ---- Create Quad Vertex Buffer ----
    renderer::CreateSpriteQuadVB(renderer_, allocator_, sprite_vb_);

    // ---- Create Sprite Instance Buffer + Tilemap Dummy Instance Buffer ----
    // The tilemap shares the sprite pipeline, which expects instance data at
    // binding 1. Bind a zeroed buffer so no stale sprite instance data is read.
    renderer::CreateSpriteInstanceBuffers(renderer_, allocator_, sprite_instance_vb_, tilemap_dummy_instance_vb_);

    EDITOR_LOG("EditorApplication::OnInit about to load tilesets...");
    // ---- Load tilesets from project ----
    ctx_.SetTilesetRenderer(&renderer_);  // OpenProject 重载 tileset 用（此时项目路径尚未确定）
    if (!ctx_.LoadTilesetsFromProject(&renderer_)) {
        ctx_.LoadDemoTileset(&renderer_);
    }

    // Patch: EditorLayer::OnAttach creates/loads the scene before tilesets are
    // available (PushLayer -> OnAttach happens before Run -> OnInit). Fix any
    // Tilemap with an empty tileset_name and mark dirty so RebuildTilemapVB
    // runs with real texture data.
    ctx_.FixupActiveTilemapTileset();
    EDITOR_LOG("EditorApplication::OnInit exit");

    // ---- Construct ECS Render System Configs ----
    sprite_cfg_.pipeline = sprite_pipeline_.Get();
    sprite_cfg_.layout = sprite_pipeline_.GetLayout();
    sprite_cfg_.quad_vb = sprite_vb_.Get();
    sprite_cfg_.instance_vb = &sprite_instance_vb_;
    sprite_cfg_.instance_vb_size = renderer::kMaxSpriteInstances * renderer::kSpriteInstanceStride;
    sprite_cfg_.ubo_set = sprite_ubo_set_.Get();

    sprite_cfg_.get_texture_ds = SpriteGetTextureDS;
    sprite_cfg_.tex_ctx = this;
    sprite_cfg_.get_atlas_rect = SpriteGetAtlasRect;
    sprite_cfg_.get_atlas_ds = SpriteGetAtlasDS;
    sprite_cfg_.atlas_ctx = this;

    tilemap_cfg_.pipeline = sprite_pipeline_.Get();
    tilemap_cfg_.layout = sprite_pipeline_.GetLayout();
    tilemap_cfg_.ubo_set = sprite_ubo_set_.Get();
    tilemap_cfg_.dummy_instance_vb = tilemap_dummy_instance_vb_.Get();
}

void EditorApplication::OnShutdown()
{
    EDITOR_LOG("EditorApplication::OnShutdown enter");
    VkResult err = vkDeviceWaitIdle(renderer_.GetDevice());
    renderer::RendererContext::CheckVkResult(err);
    ctx_.UnloadAllTilesets();
    AudioManager::Get().Shutdown();
    ResourceManager::Get().Shutdown();

    tilemap_vb_.Destroy();
    tilemap_dummy_instance_vb_.Destroy();
    sprite_vb_.Destroy();
    sprite_instance_vb_.Destroy();
    sprite_ubo_.Destroy();
    sprite_ubo_set_ = {};  // DescriptorSet has no Destroy (freed with pool)
    sprite_ubo_layout_.Destroy();
    sprite_pipeline_.Destroy();
    sprite_pipeline_wireframe_.Destroy();
    sprite_vert_.Destroy();
    sprite_frag_.Destroy();

    viewport_rt_.Destroy();
    viewport_rt_.DestroyRenderPass();

    gameui::MG_VulkanImGui_Shutdown(renderer_, swapchain_);

    EDITOR_LOG("EditorApplication::OnShutdown exit");
}

void EditorApplication::BeginFrame()
{
    // swapchain 重建 + ImGui NewFrame；resize 后重建 Viewport 离屏渲染目标
    if (gameui::MG_VulkanImGui_BeginFrame(GetWindow(), swapchain_)) {
        viewport_rt_.Destroy();
        viewport_rt_.Create(renderer_, swapchain_, allocator_);
        ctx_.SetViewportSize(viewport_rt_.GetWidth(), viewport_rt_.GetHeight());
    }

    {
        auto& ctx = ctx_;
        auto* scene = ctx.GetActiveScene();
        std::string title = "MakeGames Editor";
        if (scene) {
            title += " - " + scene->GetName();
            if (ctx.IsSceneDirty()) title += " *";
        }
        SDL_SetWindowTitle(GetWindow(), title.c_str());
    }
}

void EditorApplication::EndFrame()
{
    static int endframe_count = 0;
    ++endframe_count;
    if (endframe_count <= 3) EDITOR_LOG("EditorApplication::EndFrame #" << endframe_count);
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);

    if (!is_minimized && swapchain_.AcquireNextImage()) {
        VkCommandBuffer cmd = swapchain_.BeginCommandBuffer();

        // --- ① Render Viewport (offscreen) ---
        if (viewport_rt_.GetFramebuffer() != VK_NULL_HANDLE && sprite_pipeline_.IsValid() && sprite_vb_.IsValid()) {
            VkClearValue vp_clear = {};
            vp_clear.color.float32[0] = 0.05f;
            vp_clear.color.float32[1] = 0.05f;
            vp_clear.color.float32[2] = 0.05f;
            vp_clear.color.float32[3] = 1.0f;

            VkRenderPassBeginInfo vp_rp = {};
            vp_rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            vp_rp.renderPass = viewport_rt_.GetRenderPass();
            vp_rp.framebuffer = viewport_rt_.GetFramebuffer();
            vp_rp.renderArea.extent.width = viewport_rt_.GetWidth();
            vp_rp.renderArea.extent.height = viewport_rt_.GetHeight();
            vp_rp.clearValueCount = 1;
            vp_rp.pClearValues = &vp_clear;

            vkCmdBeginRenderPass(cmd, &vp_rp, VK_SUBPASS_CONTENTS_INLINE);

            // ECS render systems bind their own pipeline, descriptor sets and vertex buffers below.
            // Only viewport and scissor are set up here as shared dynamic state.
            VkViewport viewport = {};
            viewport.width = (float)viewport_rt_.GetWidth();
            viewport.height = (float)viewport_rt_.GetHeight();
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(cmd, 0, 1, &viewport);

            VkRect2D scissor = {};
            scissor.extent.width = viewport_rt_.GetWidth();
            scissor.extent.height = viewport_rt_.GetHeight();
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            // Camera: compute VP matrix from primary Camera Component
            {
                auto& ctx = ctx_;
                auto* scene = ctx.GetActiveScene();
                auto* world = scene ? scene->GetWorld() : nullptr;
                uint32_t cam_entity = ctx.GetPrimaryCameraEntity();

                if (world && cam_entity != 0 && world->IsValid(cam_entity)) {
                    auto* cam_comp = world->GetComponent<scene::Camera>(cam_entity);
                    auto* cam_trans = world->GetComponent<scene::Transform>(cam_entity);
                    if (cam_comp && cam_trans) {
                        float vp[16];
                        scene::CameraSystem::ComputeViewProj(vp, viewport_rt_.GetWidth(), viewport_rt_.GetHeight(),
                            *cam_comp, cam_trans->position);
                        sprite_ubo_.Upload(vp, sizeof(vp));
                    }
                }
            }

            // ViewMode：Wireframe 换线框管线变体，Unlit 让片段着色器忽略 tint
            const auto view_mode = ctx_.GetViewMode();
            auto tm_cfg = tilemap_cfg_;
            auto sp_cfg = sprite_cfg_;
            if (view_mode == EditorContext::ViewMode::Wireframe && sprite_pipeline_wireframe_.IsValid()) {
                tm_cfg.pipeline = sprite_pipeline_wireframe_.Get();
                tm_cfg.layout = sprite_pipeline_wireframe_.GetLayout();
                sp_cfg.pipeline = sprite_pipeline_wireframe_.Get();
                sp_cfg.layout = sprite_pipeline_wireframe_.GetLayout();
            }
            const int frag_mode = (view_mode == EditorContext::ViewMode::Unlit) ? 1 : 0;
            tm_cfg.view_mode = frag_mode;
            sp_cfg.view_mode = frag_mode;

            // Tilemap: dirty → rebuild VB, then render
            // 瓦片动画时钟：任一 tileset 动画帧哈希变化 → 标脏触发重建
            {
                tile_anim_time_ += ImGui::GetIO().DeltaTime;
                uint32_t h = 0;
                for (const auto& [ts_name, ts] : ctx_.GetTilesetRegistry()) {
                    if (ts.animations.empty()) continue;
                    h = h * 131u + scene::AnimFrameHash(ts.animations, tile_anim_time_);
                }
                if (h != tile_anim_hash_) {
                    tile_anim_hash_ = h;
                    ctx_.MarkTilemapDirty();
                }
            }
            if (ctx_.IsTilemapDirty()) {
                RebuildTilemapVB();
                ctx_.ClearTilemapDirty();
            }
            // Tilemap: render per-tileset batches
            scene::World* tilemap_world = ctx_.GetActiveScene()
                ? ctx_.GetActiveScene()->GetWorld() : nullptr;
            for (const auto& batch : tilemap_batches_) {
                if (batch.vertex_count == 0) continue;
                tilemap_vb_state_.buffer = &tilemap_vb_;
                tilemap_vb_state_.first_vertex = batch.first_vertex;
                tilemap_vb_state_.vertex_count = batch.vertex_count;
                tilemap_system_.Render(cmd, tilemap_world, tilemap_vb_state_,
                    batch.ds, tm_cfg);
            }

            // Sprite: update selected_entity, then render
            // Sprite: render
            auto* sprite_scene = ctx_.GetActiveScene();
            sprite_system_.Render(cmd,
                sprite_scene ? sprite_scene->GetWorld() : nullptr,
                sp_cfg);

            vkCmdEndRenderPass(cmd);

            ctx_.SetViewportTexture((ImTextureID)(intptr_t)viewport_rt_.GetImGuiDS());
        }

        // --- ② Render ImGui (swapchain) ---
        gameui::MG_VulkanImGui_RenderFrame(swapchain_, cmd, draw_data, clear_color_);

        swapchain_.SubmitAndPresent(cmd);
    }

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

bool EditorApplication::OnProcessEvent(SDL_Event* event)
{
    // 全局快捷键：在 ImGui 处理之前拦截，确保焦点在任意位置都能触发
    if (event->type == SDL_KEYDOWN && !event->key.repeat) {
        const bool ctrl = (event->key.keysym.mod & KMOD_CTRL) != 0;
        if (ctrl && event->key.keysym.sym == SDLK_s) {
            ctx_.SaveCurrentScene();
            return true;  // 消费事件，阻止继续传递
        }
    }

    gameui::MG_VulkanImGui_ProcessEvent(GetWindow(), event, swapchain_);
    return false;
}

} // namespace editor
} // namespace mg
