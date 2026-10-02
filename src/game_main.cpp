// game_main.cpp — 独立游戏运行时（game.exe）。
// 布局约定（与导出对齐）：cwd = 导出根目录，project.json 在根，场景在 maps/，数据库在 data/。
// Play Mode 编排复用 scene::GameRuntime（与编辑器同源），覆盖层 UI 复用 gameui::（playmode_ui）。
#include "app/application.h"
#include "core/log.h"
#include "renderer/renderer_context.h"
#include "renderer/swapchain.h"
#include "renderer/shader.h"
#include "renderer/buffer.h"
#include "renderer/pipeline.h"
#include "renderer/descriptor.h"
#include "renderer/sprite_pipeline_setup.h"
#include "renderer/texture.h"
#include "resource/resource_manager.h"
#include "resource/data_manager.h"
#include "resource/game_databases.h"
#include "resource/vfs.h"
#include "resource/schema/project_schema.h"
#include "audio/audio_manager.h"
#include "scene/scene.h"
#include "scene/scene_serializer.h"
#include "scene/world.h"
#include "scene/components/transform.h"
#include "scene/components/camera.h"
#include "scene/components/sprite.h"
#include "scene/components/tilemap.h"
#include "scene/systems/sprite_render_system.h"
#include "scene/systems/sprite_vertex_layout.h"
#include "scene/systems/tilemap_render_system.h"
#include "scene/systems/camera_system.h"
#include "scene/systems/tile_animation.h"
#include "scene/systems/tilemap_mesh_builder.h"
#include "scene/game_runtime.h"
#include "gameui/playmode_ui.h"
#include "gameui/mg_imgui_backend.h"
#include "third_party/json.hpp"

#include <SDL.h>
#include <SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include <fstream>
#include <filesystem>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>
#include <cstring>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace mg {
namespace game {

namespace fs = std::filesystem;

// 从 VFS 加载 SPIR-V（pak 命中读包，否则落盘）
static std::vector<uint32_t> LoadSpvFile(const char* path) {
    std::vector<uint8_t> bytes;
    if (!vfs::ReadFile(path, bytes)) return {};
    std::vector<uint32_t> data(bytes.size() / 4);
    std::memcpy(data.data(), bytes.data(), data.size() * 4);
    return data;
}

// 挂载 data.pak（若存在）；MakeDesc 与 OnInit 都可能在首位调用，幂等
static void EnsurePakMounted() {
    if (!vfs::IsPakMounted() && fs::exists("data.pak")) vfs::MountPak("data.pak");
}

// 窗口参数：尽量从根目录 project.json 读 manifest（名称 / 分辨率），失败用默认值
static Application::Desc MakeDesc() {
    static std::string title = "MakeGames Runtime"; // Desc::title 是裸指针，需要静态存储
    Application::Desc d;
    d.title = "MakeGames Runtime";
    d.width = kDefaultDisplayWidth;
    d.height = kDefaultDisplayHeight;
    d.maximized = false;
    d.resizable = true;
    d.high_dpi = true;
    d.sdl_flags = SDL_WINDOW_VULKAN;
    EnsurePakMounted(); // project.json 可能在 pak 里
    if (auto m = DataManager::Get().Load<ProjectManifest>("project.json")) {
        if (!m->name.empty()) title = m->name;
        if (m->display_width > 0) d.width = m->display_width;
        if (m->display_height > 0) d.height = m->display_height;
    }
    d.title = title.c_str();
    return d;
}

class GameApplication : public Application, public scene::ISceneHost {
public:
    GameApplication() : Application(MakeDesc()) {}
    ~GameApplication() override = default;

    // ---- ISceneHost（GameRuntime 场景来源：裸加载，无编辑器副作用）----
    scene::Scene* GetActiveScene() override { return scene_.get(); }
    bool LoadScene(const std::string& path) override {
        auto raw = DataManager::Get().LoadJsonRaw(path);
        if (!raw) {
            MG_LOG_ERROR("GameApplication: Failed to load scene: " << path);
            return false;
        }
        auto s = scene::SceneSerializer::Deserialize(*raw);
        if (!s) {
            MG_LOG_ERROR("GameApplication: Failed to deserialize scene: " << path);
            return false;
        }
        s->Initialize();
        scene_ = std::move(s);
        tilemap_dirty_ = true;
        return true;
    }

protected:
    void OnInit() override;
    void OnShutdown() override;
    void BeginFrame() override;
    void EndFrame() override;
    void OnUpdate(float dt) override;
    void OnRender() override;
    bool OnProcessEvent(SDL_Event* event) override;

private:
    bool LoadProjectData();   // manifest + 数据库 + tilesets（不加载场景；场景由 GameRuntime 驱动）
    void DrawPlayUI();        // 全窗口 Play Mode 覆盖层（gameui::）
    std::pair<math::Vec2, float> GetCameraPosZoom() const; // Toast 世界→屏幕换算用
    void RebuildTilemapVB();
    void UpdateCameraUBO();

    renderer::RendererContext renderer_;
    renderer::SwapChain swapchain_;
    VkAllocationCallbacks* allocator_ = nullptr;

    renderer::Shader sprite_vert_;
    renderer::Shader sprite_frag_;
    renderer::DescriptorSetLayout sprite_ubo_layout_;
    renderer::DescriptorSet sprite_ubo_set_;
    renderer::Buffer sprite_ubo_;
    renderer::Pipeline sprite_pipeline_;
    renderer::Buffer sprite_vb_;
    renderer::Buffer sprite_instance_vb_;
    renderer::Buffer tilemap_dummy_instance_vb_;

    scene::SpriteRenderSystem sprite_system_;
    scene::TilemapRenderSystem tilemap_system_;

    // Play Mode 编排（Animator/Tween/EventSystem 由 GameRuntime 持有）
    GameDatabases databases_;
    scene::PlayModeController play_mode_;
    std::unique_ptr<scene::GameRuntime> runtime_;
    std::string game_title_ = "MakeGames";
    gameui::PauseSub pause_sub_ = gameui::PauseSub::None;

    std::unique_ptr<scene::Scene> scene_;
    uint32_t viewport_w_ = kDefaultDisplayWidth;
    uint32_t viewport_h_ = kDefaultDisplayHeight;
    bool tilemap_dirty_ = true;

    renderer::Buffer tilemap_vb_;
    uint32_t tilemap_vertex_count_ = 0;

    std::vector<scene::TilemapBatch> tilemap_batches_;

    nlohmann::json tilesets_json_;
    // 瓦片动画：tileset 名 → 动画表；帧哈希变化时重建 tilemap VB
    std::unordered_map<std::string, std::vector<scene::TileAnimDef>> tile_anims_;
    float tile_anim_time_ = 0.0f;
    uint32_t tile_anim_hash_ = 0;

    float clear_color_[4] = {0.1f, 0.1f, 0.12f, 1.0f};
};

void GameApplication::OnInit() {
    SDL_Window* window = GetWindow();
    if (!window) return;

    EnsurePakMounted(); // MakeDesc 已挂载则幂等跳过

    // renderer + swapchain + ImGui（含主题/中文字体）一条龙
    if (!gameui::MG_VulkanImGui_Init(window, renderer_, swapchain_, allocator_,
                                     /*enable_imgui_viewports=*/false)) {
        MG_LOG_ERROR("GameApplication: Failed to initialize Vulkan + ImGui");
        return;
    }
    // 独立运行时不写 imgui.ini，避免污染导出目录
    ImGui::GetIO().IniFilename = nullptr;
    // Play Mode 键盘留给游戏逻辑（对话 Enter 推进等），关闭 ImGui 导航
    ImGui::GetIO().ConfigFlags &= ~(ImGuiConfigFlags_NavEnableKeyboard |
                                    ImGuiConfigFlags_NavEnableGamepad);

    viewport_w_ = swapchain_.GetExtent().width;
    viewport_h_ = swapchain_.GetExtent().height;

    ResourceManager::Get().Initialize(&renderer_, allocator_);
    ResourceManager::Get().SetProjectPath("."); // 相对路径以导出根（cwd）解析
    AudioManager::Get().Init();

    // Load shaders
    auto vert_spv = LoadSpvFile("assets/shaders/sprite.vert.spv");
    auto frag_spv = LoadSpvFile("assets/shaders/sprite.frag.spv");
    if (!vert_spv.empty()) sprite_vert_.Create(renderer_.GetDevice(), vert_spv.data(), vert_spv.size() * sizeof(uint32_t), allocator_);
    if (!frag_spv.empty()) sprite_frag_.Create(renderer_.GetDevice(), frag_spv.data(), frag_spv.size() * sizeof(uint32_t), allocator_);

    // UBO layout & set
    if (sprite_vert_.IsValid() && sprite_frag_.IsValid()) {
        renderer::CreateSpriteUBOLayoutAndSet(renderer_, allocator_, sprite_ubo_layout_, sprite_ubo_set_);
    }

    renderer::CreateSpriteUBOBuffer(renderer_, allocator_, sprite_ubo_set_, sprite_ubo_);

    // Pipeline with swapchain render pass
    if (sprite_vert_.IsValid() && sprite_frag_.IsValid() && swapchain_.GetRenderPass() != VK_NULL_HANDLE && sprite_ubo_layout_.IsValid()) {
        renderer::CreateSpritePipeline(renderer_, allocator_, swapchain_.GetRenderPass(), sprite_vert_, sprite_frag_,
                                       sprite_ubo_layout_, ResourceManager::Get().GetSamplerLayout(),
                                       (float)viewport_w_, (float)viewport_h_,
                                       VK_POLYGON_MODE_FILL, sprite_pipeline_);
    }

    // Quad VB
    renderer::CreateSpriteQuadVB(renderer_, allocator_, sprite_vb_);

    // Instance buffers
    renderer::CreateSpriteInstanceBuffers(renderer_, allocator_, sprite_instance_vb_, tilemap_dummy_instance_vb_);

    if (!LoadProjectData()) {
        MG_LOG_ERROR("GameApplication: project data load failed (running without project?)");
    }

    // Play Mode 运行时：与编辑器同源。启动即进入标题画面。
    runtime_ = std::make_unique<scene::GameRuntime>(
        this, &databases_, play_mode_, ".",
        [this]() { return std::make_pair(viewport_w_, viewport_h_); });
    play_mode_.SetState(scene::PlayModeController::State::Playing);
    runtime_->EnterPlayMode();
}

bool GameApplication::LoadProjectData() {
    if (!fs::exists("project.json")) {
        MG_LOG_ERROR("GameApplication: project.json not found (cwd must be the export root)");
        return false;
    }
    auto manifest = DataManager::Get().Load<ProjectManifest>("project.json");
    if (!manifest) {
        MG_LOG_ERROR("GameApplication: Failed to load project.json");
        return false;
    }
    if (!manifest->name.empty()) {
        game_title_ = manifest->name;
        SDL_SetWindowTitle(GetWindow(), game_title_.c_str());
    }

    // 数据库（物品/角色/技能/敌人/商店）
    databases_.LoadFromDirectory("data");

    // Load tilesets（纹理预载 + 瓦片动画表）
    auto tilesets_raw = DataManager::Get().LoadJsonRaw("data/tilesets.json");
    if (tilesets_raw && tilesets_raw->is_array()) {
        tilesets_json_ = *tilesets_raw;
        for (auto& item : tilesets_json_) {
            std::string name = item.value("name", "");
            std::string img = item.value("image_path", "");
            if (!name.empty() && !img.empty()) {
                ResourceManager::Get().LoadTexture(img);
            }
            if (!name.empty() && item.contains("animations") && item["animations"].is_array()) {
                auto& anims = tile_anims_[name];
                for (const auto& aj : item["animations"]) {
                    scene::TileAnimDef a;
                    a.tile_index = aj.value("tile_index", 0);
                    a.fps = aj.value("fps", 4.0f);
                    if (aj.contains("frames")) a.frames = aj["frames"].get<std::vector<int>>();
                    anims.push_back(std::move(a));
                }
            }
        }
    }

    return true;
}

void GameApplication::OnShutdown() {
    vkDeviceWaitIdle(renderer_.GetDevice());
    runtime_.reset();
    scene_.reset();
    ResourceManager::Get().Shutdown();
    AudioManager::Get().Shutdown();
    tilemap_vb_.Destroy();
    tilemap_dummy_instance_vb_.Destroy();
    sprite_vb_.Destroy();
    sprite_instance_vb_.Destroy();
    sprite_ubo_.Destroy();
    sprite_ubo_set_ = {};
    sprite_ubo_layout_.Destroy();
    sprite_pipeline_.Destroy();
    sprite_vert_.Destroy();
    sprite_frag_.Destroy();
    gameui::MG_VulkanImGui_Shutdown(renderer_, swapchain_);
#ifdef _WIN32
    Sleep(500);
#endif
}

void GameApplication::BeginFrame() {
    gameui::MG_VulkanImGui_BeginFrame(GetWindow(), swapchain_);
    viewport_w_ = swapchain_.GetExtent().width;
    viewport_h_ = swapchain_.GetExtent().height;
}

void GameApplication::OnUpdate(float dt) {
    // Play Mode 编排：标题请求/暂停/移动/碰撞/相机/动画/事件/Fade/存读档
    if (runtime_) runtime_->Update(dt);
    AudioManager::Get().Update();

    // 瓦片动画时钟：帧哈希变化 → 重建 tilemap VB
    if (!tile_anims_.empty()) {
        tile_anim_time_ += dt;
        uint32_t h = 0;
        for (const auto& [name, anims] : tile_anims_) {
            h = h * 131u + scene::AnimFrameHash(anims, tile_anim_time_);
        }
        if (h != tile_anim_hash_) {
            tile_anim_hash_ = h;
            tilemap_dirty_ = true;
        }
    }

    DrawPlayUI();
}

// 全窗口 Play Mode 覆盖层（与编辑器 Viewport 内嵌版本共用 gameui:: 实现）
void GameApplication::DrawPlayUI() {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)viewport_w_, (float)viewport_h_));
    ImGui::Begin("##play_ui", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing |
                 ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoNav);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    using State = scene::PlayModeController::State;
    using Screen = scene::PlayModeController::Screen;
    const State state = play_mode_.GetState();
    const bool in_game = (state == State::Playing && play_mode_.GetScreen() == Screen::Game);

    gameui::PlayUIContext pui;
    pui.es = runtime_ ? runtime_->GetEventSystem() : nullptr;
    pui.dbs = &databases_;
    pui.play_mode = &play_mode_;
    pui.dl = dl;
    pui.view_min = ImVec2(0, 0);
    pui.view_max = ImVec2((float)viewport_w_, (float)viewport_h_);
    pui.fade_alpha = play_mode_.GetFadeAlpha();
    if (!game_title_.empty()) pui.game_title = game_title_.c_str();
    pui.project_root = ".";
    pui.on_exit = [this]() { Close(); };
    pui.exit_label = "Quit";
    auto [cam_pos, cam_zoom] = GetCameraPosZoom();
    pui.cam_pos = cam_pos;
    pui.cam_zoom = cam_zoom;

    gameui::DrawFadeOverlay(pui);

    if (state == State::Playing && play_mode_.GetScreen() == Screen::Title)
        gameui::DrawTitleScreen(pui);

    if (state == State::Paused) gameui::DrawPauseMenu(pui, pause_sub_);
    else pause_sub_ = gameui::PauseSub::None;

    if (in_game) {
        gameui::DrawToasts(pui);
        gameui::DrawDialog(pui, ImGui::GetMousePos());
        gameui::DrawChoices(pui);
        gameui::DrawShopOverlay(pui);
        gameui::DrawBattleOverlay(pui);
        gameui::DrawHUDOverlays(pui);
    }

    ImGui::End();
}

std::pair<math::Vec2, float> GameApplication::GetCameraPosZoom() const {
    if (scene_ && scene_->GetWorld()) {
        auto* world = scene_->GetWorld();
        std::vector<scene::EntityID> all;
        world->GetAllEntities(all);
        for (auto e : all) {
            auto* cam = world->GetComponent<scene::Camera>(e);
            if (cam && cam->is_primary) {
                auto* t = world->GetComponent<scene::Transform>(e);
                return { t ? t->position : math::Vec2{0, 0}, cam->ortho_size };
            }
        }
    }
    return { math::Vec2{0, 0}, (float)viewport_h_ * 0.5f };
}

void GameApplication::UpdateCameraUBO() {
    if (!scene_ || !scene_->GetWorld()) return;
    auto* world = scene_->GetWorld();

    uint32_t cam_entity = 0;
    std::vector<scene::EntityID> all;
    world->GetAllEntities(all);
    for (auto e : all) {
        auto* cam = world->GetComponent<scene::Camera>(e);
        if (cam && cam->is_primary) { cam_entity = e; break; }
    }

    float vp[16];
    if (cam_entity != 0 && world->IsValid(cam_entity)) {
        auto* cam = world->GetComponent<scene::Camera>(cam_entity);
        auto* trans = world->GetComponent<scene::Transform>(cam_entity);
        if (cam && trans) {
            scene::CameraSystem::ComputeViewProj(vp, (int)viewport_w_, (int)viewport_h_, *cam, trans->position);
            sprite_ubo_.Upload(vp, sizeof(vp));
            return;
        }
    }

    // 没有主摄像机时退回到默认正交投影
    scene::Camera default_cam;
    default_cam.ortho_size = (float)viewport_h_ * 0.5f;
    scene::CameraSystem::ComputeViewProj(vp, (int)viewport_w_, (int)viewport_h_, default_cam, mg::math::Vec2{});
    sprite_ubo_.Upload(vp, sizeof(vp));
}

void GameApplication::RebuildTilemapVB() {
    if (!scene_ || !scene_->GetWorld()) return;
    auto* world = scene_->GetWorld();

    std::vector<scene::TilemapVertex> verts;

    // tileset 数据源：tilesets.json（无独立 tile 尺寸，UV 由地图 tile 尺寸推导）
    scene::TilesetRenderInfo info;  // 复用 scratch（构建器在下次回调前消费完毕）
    scene::BuildTilemapMesh(world,
        [this, &info](const std::string& ts_name, int map_tw, int map_th) -> const scene::TilesetRenderInfo* {
            const nlohmann::json* ts_item = nullptr;
            for (auto& item : tilesets_json_) {
                if (item.value("name", "") == ts_name) {
                    ts_item = &item;
                    break;
                }
            }
            if (!ts_item) return nullptr;
            int cols = ts_item->value("columns", 1);
            int rows = ts_item->value("rows", 1);
            if (cols <= 0) cols = 1;
            if (rows <= 0) rows = 1;
            std::string image_path = ts_item->value("image_path", "");
            info.columns       = cols;
            info.tile_width    = map_tw;
            info.tile_height   = map_th;
            info.start_index   = ts_item->value("start_index", 0);
            info.tile_count    = cols * rows;
            info.tex_width     = (float)(cols * map_tw);
            info.tex_height    = (float)(rows * map_th);
            info.descriptor_set = image_path.empty() ? VK_NULL_HANDLE
                : ResourceManager::Get().GetOrCreateSamplerDS(image_path);
            auto it = tile_anims_.find(ts_name);
            info.animations    = it != tile_anims_.end() ? &it->second : nullptr;
            return &info;
        },
        /*respect_layer_visibility=*/false, tile_anim_time_, verts, tilemap_batches_);

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

void GameApplication::OnRender() {
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
    if (is_minimized) return;

    auto* world = (scene_ && scene_->GetWorld()) ? scene_->GetWorld() : nullptr;
    if (world) {
        UpdateCameraUBO();
        if (tilemap_dirty_) {
            RebuildTilemapVB();
            tilemap_dirty_ = false;
        }
    }

    if (!swapchain_.AcquireNextImage()) return;

    VkClearValue clear_value = {};
    clear_value.color.float32[0] = clear_color_[0] * clear_color_[3];
    clear_value.color.float32[1] = clear_color_[1] * clear_color_[3];
    clear_value.color.float32[2] = clear_color_[2] * clear_color_[3];
    clear_value.color.float32[3] = clear_color_[3];

    VkCommandBuffer cmd = swapchain_.BeginCommandBuffer();
    swapchain_.BeginRenderPass(cmd, &clear_value);

    VkViewport viewport = {};
    viewport.width = (float)viewport_w_;
    viewport.height = (float)viewport_h_;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.extent.width = viewport_w_;
    scissor.extent.height = viewport_h_;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    if (world) {
        scene::SpriteRenderSystem::Config sprite_cfg = {};
        sprite_cfg.pipeline = sprite_pipeline_.Get();
        sprite_cfg.layout = sprite_pipeline_.GetLayout();
        sprite_cfg.quad_vb = sprite_vb_.Get();
        sprite_cfg.instance_vb = &sprite_instance_vb_;
        sprite_cfg.instance_vb_size = sprite_instance_vb_.GetSize();
        sprite_cfg.ubo_set = sprite_ubo_set_.Get();
        sprite_cfg.get_texture_ds = [](void*, const std::string& path) -> VkDescriptorSet {
            return ResourceManager::Get().GetOrCreateSamplerDS(path);
        };
        sprite_cfg.tex_ctx = nullptr;
        sprite_cfg.get_atlas_rect = [](void*, const std::string& path, float out_uv_rect[4]) -> bool {
            return ResourceManager::Get().GetSpriteAtlasRect(path, out_uv_rect);
        };
        sprite_cfg.get_atlas_ds = [](void*) -> VkDescriptorSet {
            return ResourceManager::Get().GetSpriteAtlasDS();
        };
        sprite_cfg.atlas_ctx = nullptr;

        scene::TilemapRenderSystem::Config tilemap_cfg = {};
        tilemap_cfg.pipeline = sprite_pipeline_.Get();
        tilemap_cfg.layout = sprite_pipeline_.GetLayout();
        tilemap_cfg.ubo_set = sprite_ubo_set_.Get();
        tilemap_cfg.dummy_instance_vb = tilemap_dummy_instance_vb_.Get();

        // Render tilemap per-(layer, tileset) batches
        for (const auto& batch : tilemap_batches_) {
            if (batch.vertex_count == 0) continue;
            scene::TilemapRenderSystem::VBState vb;
            vb.buffer = &tilemap_vb_;
            vb.first_vertex = batch.first_vertex;
            vb.vertex_count = batch.vertex_count;
            tilemap_system_.Render(cmd, world, vb, batch.ds, tilemap_cfg);
        }

        // Render sprites
        sprite_system_.Render(cmd, world, sprite_cfg);
    }

    // Play Mode 覆盖层 UI（同一 render pass，绘制在世界之上）
    ImGui_ImplVulkan_RenderDrawData(draw_data, cmd);

    swapchain_.EndRenderPass(cmd);
    swapchain_.SubmitAndPresent(cmd);
}

void GameApplication::EndFrame() {
    // Present handled in OnRender
}

bool GameApplication::OnProcessEvent(SDL_Event* event) {
    // ESC 不再直接退出：Playing 中由 GameRuntime 切暂停菜单，退出走菜单 Quit / 窗口关闭按钮
    gameui::MG_VulkanImGui_ProcessEvent(GetWindow(), event, swapchain_);
    return false;
}

} // namespace game
} // namespace mg

int main(int, char**) {
    mg::game::GameApplication app;
    app.Run();
    return 0;
}
