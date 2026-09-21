#include "app/application.h"
#include "core/log.h"
#include "renderer/renderer_context.h"
#include "renderer/swapchain.h"
#include "renderer/shader.h"
#include "renderer/buffer.h"
#include "renderer/pipeline.h"
#include "renderer/descriptor.h"
#include "renderer/texture.h"
#include "resource/resource_manager.h"
#include "resource/data_manager.h"
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
#include "scene/systems/animator_system.h"
#include "scene/systems/tween_animation_system.h"
#include "scene/systems/tile_animation.h"
#include "third_party/json.hpp"

#include <SDL.h>
#include <SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <fstream>
#include <filesystem>
#include <memory>
#include <unordered_map>
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

// 从文件加载 SPIR-V（game.exe 不继承 editor 的 LoadSpvFile）
static std::vector<uint32_t> LoadSpvFile(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto size = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<char> buffer(static_cast<size_t>(size));
    if (!f.read(buffer.data(), size)) return {};
    std::vector<uint32_t> data(size / 4);
    std::memcpy(data.data(), buffer.data(), data.size() * 4);
    return data;
}

class GameApplication : public Application {
public:
    GameApplication() : Application(Desc{
        .title = "MakeGames Runtime",
        .width = 1280,
        .height = 720,
        .maximized = false,
        .resizable = true,
        .high_dpi = true,
        .sdl_flags = SDL_WINDOW_VULKAN
    }) {}

    ~GameApplication() override = default;

protected:
    void OnInit() override;
    void OnShutdown() override;
    void BeginFrame() override;
    void EndFrame() override;
    void OnUpdate(float dt) override;
    void OnRender() override;
    bool OnProcessEvent(SDL_Event* event) override;

private:
    bool LoadProjectAndScene();
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
    scene::AnimatorSystem animator_system_;
    scene::TweenAnimationSystem tween_animation_system_;

    std::unique_ptr<scene::Scene> scene_;
    uint32_t viewport_w_ = 1280;
    uint32_t viewport_h_ = 720;
    bool tilemap_dirty_ = true;

    renderer::Buffer tilemap_vb_;
    uint32_t tilemap_vertex_count_ = 0;

    struct TilemapBatch {
        VkDescriptorSet ds = VK_NULL_HANDLE;
        uint32_t first_vertex = 0;
        uint32_t vertex_count = 0;
    };
    std::vector<TilemapBatch> tilemap_batches_;

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

    if (!renderer_.Initialize(window)) {
        MG_LOG_ERROR("GameApplication: Failed to initialize Vulkan renderer");
        return;
    }
    if (!swapchain_.Initialize(window, &renderer_, allocator_)) {
        MG_LOG_ERROR("GameApplication: Failed to initialize SwapChain");
        return;
    }

    viewport_w_ = swapchain_.GetExtent().width;
    viewport_h_ = swapchain_.GetExtent().height;

    ResourceManager::Get().Initialize(&renderer_, allocator_);
    AudioManager::Get().Init();

    // Load shaders
    auto vert_spv = LoadSpvFile("assets/shaders/sprite.vert.spv");
    auto frag_spv = LoadSpvFile("assets/shaders/sprite.frag.spv");
    if (!vert_spv.empty()) sprite_vert_.Create(renderer_.GetDevice(), vert_spv.data(), vert_spv.size() * sizeof(uint32_t), allocator_);
    if (!frag_spv.empty()) sprite_frag_.Create(renderer_.GetDevice(), frag_spv.data(), frag_spv.size() * sizeof(uint32_t), allocator_);

    // UBO layout & set
    if (sprite_vert_.IsValid() && sprite_frag_.IsValid()) {
        VkDescriptorSetLayoutBinding binding = {};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        sprite_ubo_layout_.Create(renderer_.GetDevice(), &binding, 1, allocator_);
        sprite_ubo_set_.Allocate(renderer_.GetDevice(), renderer_.GetDescriptorPool(), sprite_ubo_layout_.Get());
    }

    sprite_ubo_.Create(&renderer_, sizeof(float) * 16, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true, allocator_);
    if (sprite_ubo_set_.IsValid()) sprite_ubo_set_.WriteBuffer(0, sprite_ubo_.Get(), sizeof(float) * 16);

    // Pipeline with swapchain render pass
    if (sprite_vert_.IsValid() && sprite_frag_.IsValid() && swapchain_.GetRenderPass() != VK_NULL_HANDLE && sprite_ubo_layout_.IsValid()) {
        auto bindings = scene::GetSpriteVertexBindings();
        auto attrs = scene::GetSpriteVertexAttributes();

        VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkDescriptorSetLayout layouts[2] = { sprite_ubo_layout_.Get(), ResourceManager::Get().GetSamplerLayout() };

        renderer::Pipeline::Config cfg = {};
        cfg.render_pass = swapchain_.GetRenderPass();
        cfg.vert = &sprite_vert_;
        cfg.frag = &sprite_frag_;
        cfg.vertex_bindings = bindings.data();
        cfg.vertex_binding_count = (uint32_t)bindings.size();
        cfg.vertex_attributes = attrs.data();
        cfg.vertex_attribute_count = (uint32_t)attrs.size();
        cfg.dynamic_states = dynamic_states;
        cfg.dynamic_state_count = 2;
        cfg.viewport_width = (float)viewport_w_;
        cfg.viewport_height = (float)viewport_h_;
        cfg.cull_mode = VK_CULL_MODE_NONE;
        cfg.descriptor_set_layouts = layouts;
        cfg.descriptor_set_layout_count = 2;
        // sprite.frag 声明了 ViewMode push constant（编辑器视口用）；运行时恒为 Lit（0），但 layout 必须包含该 range
        VkPushConstantRange pc_range = {};
        pc_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        pc_range.offset = 0;
        pc_range.size = sizeof(int);
        cfg.push_constant_ranges = &pc_range;
        cfg.push_constant_range_count = 1;
        sprite_pipeline_.Create(&renderer_, cfg, allocator_);
    }

    // Quad VB
    struct Vertex { float x, y, u, v; };
    Vertex quad[6] = {
        {-16.0f, -16.0f, 0.0f, 0.0f}, { 16.0f, -16.0f, 1.0f, 0.0f}, { 16.0f,  16.0f, 1.0f, 1.0f},
        {-16.0f, -16.0f, 0.0f, 0.0f}, { 16.0f,  16.0f, 1.0f, 1.0f}, {-16.0f,  16.0f, 0.0f, 1.0f},
    };
    if (sprite_vb_.Create(&renderer_, sizeof(quad), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, true, allocator_)) {
        sprite_vb_.Upload(quad, sizeof(quad));
    }

    // Instance buffers
    constexpr VkDeviceSize kMaxSpriteInstances = 4096;
    constexpr VkDeviceSize kInstanceStride = sizeof(float) * 20;
    sprite_instance_vb_.Create(&renderer_, kMaxSpriteInstances * kInstanceStride, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, true, allocator_);
    tilemap_dummy_instance_vb_.Create(&renderer_, kInstanceStride, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, true, allocator_);
    float identity[20] = {0,0, 0,0, 1,1, 1,1, 1,1,1,1, 0,0,1,1, 0.5f,0.5f, 0,0};
    tilemap_dummy_instance_vb_.Upload(identity, sizeof(identity));

    LoadProjectAndScene();
}

bool GameApplication::LoadProjectAndScene() {
    if (!fs::exists("data/project.json")) {
        MG_LOG_ERROR("GameApplication: data/project.json not found");
        return false;
    }
    auto manifest = DataManager::Get().Load<ProjectManifest>("data/project.json");
    if (!manifest) {
        MG_LOG_ERROR("GameApplication: Failed to load project.json");
        return false;
    }

    std::string scene_path = manifest->start_scene;
    if (scene_path.empty()) scene_path = "maps/Map001.json";

    // start_scene 可能是 "maps/Map001.json"，需要加上 data/ 前缀
    if (!fs::exists(scene_path) && fs::exists("data/" + scene_path)) {
        scene_path = "data/" + scene_path;
    }
    if (!fs::exists(scene_path)) {
        MG_LOG_ERROR("GameApplication: Start scene not found: " << scene_path);
        return false;
    }

    auto raw = DataManager::Get().LoadJsonRaw(scene_path);
    if (!raw) {
        MG_LOG_ERROR("GameApplication: Failed to load scene: " << scene_path);
        return false;
    }
    scene_ = scene::SceneSerializer::Deserialize(*raw);
    if (!scene_) {
        MG_LOG_ERROR("GameApplication: Failed to deserialize scene");
        return false;
    }
    scene_->Initialize();

    // Load tilesets
    auto tilesets_raw = DataManager::Get().LoadJsonRaw("data/tilesets.json");
    if (tilesets_raw && tilesets_raw->is_array()) {
        tilesets_json_ = *tilesets_raw;
        for (auto& item : tilesets_json_) {
            std::string name = item.value("name", "");
            std::string img = item.value("image_path", "");
            if (!name.empty() && !img.empty()) {
                ResourceManager::Get().LoadTexture(img);
            }
            // 解析瓦片动画表
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
    swapchain_.Destroy();
    renderer_.Shutdown();
#ifdef _WIN32
    Sleep(500);
#endif
}

void GameApplication::BeginFrame() {
    if (swapchain_.NeedsRebuild()) {
        int w = 0, h = 0;
        SDL_GetWindowSize(GetWindow(), &w, &h);
        if (w == 0 || h == 0) { w = (int)viewport_w_; h = (int)viewport_h_; }
        swapchain_.Resize(w, h);
        viewport_w_ = swapchain_.GetExtent().width;
        viewport_h_ = swapchain_.GetExtent().height;
    }
}

void GameApplication::OnUpdate(float dt) {
    if (!scene_ || !scene_->GetWorld()) return;
    auto* world = scene_->GetWorld();

    animator_system_.Update(dt, world);
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

    std::vector<scene::EntityID> all;
    world->GetAllEntities(all);

    struct TVertex { float x, y, u, v; };
    std::vector<TVertex> verts;
    tilemap_batches_.clear();

    for (auto e : all) {
        auto* tm = world->GetComponent<scene::Tilemap>(e);
        auto* t = world->GetComponent<scene::Transform>(e);
        if (!tm || tm->layers.empty()) continue;
        auto names = tm->GetTilesetNames();
        if (names.empty()) continue;
        auto& layer = tm->layers[0];
        float ox = t ? t->position.x : 0.0f;
        float oy = t ? t->position.y : 0.0f;
        float sx = t ? t->scale.x : 1.0f;
        float sy = t ? t->scale.y : 1.0f;
        int tw = tm->tile_width;
        int th = tm->tile_height;

        for (const std::string& ts_name : names) {
            const nlohmann::json* ts_item = nullptr;
            for (auto& item : tilesets_json_) {
                if (item.value("name", "") == ts_name) {
                    ts_item = &item;
                    break;
                }
            }
            if (!ts_item) continue;
            int cols = ts_item->value("columns", 1);
            int rows = ts_item->value("rows", 1);
            int start_index = ts_item->value("start_index", 0);
            std::string image_path = ts_item->value("image_path", "");
            if (cols <= 0) cols = 1;
            if (rows <= 0) rows = 1;
            int tile_count = cols * rows;
            int ts_end = start_index + tile_count;
            const auto* anims = [this, &ts_name]() -> const std::vector<scene::TileAnimDef>* {
                auto it = tile_anims_.find(ts_name);
                return it != tile_anims_.end() ? &it->second : nullptr;
            }();

            uint32_t batch_start = (uint32_t)verts.size();
            for (int y = 0; y < layer.height; ++y) {
                for (int x = 0; x < layer.width; ++x) {
                    int global_tile = layer.GetTile(x, y);
                    if (global_tile < start_index || global_tile >= ts_end) continue;
                    // 瓦片动画：命中动画的 tile 替换为当前帧
                    if (anims) global_tile = scene::ResolveAnimatedTile(*anims, global_tile, start_index, tile_anim_time_);
                    int local = global_tile - start_index;
                    int col = local % cols;
                    int row = local / cols;
                    float u0 = (float)(col * tw) / (cols * tw);
                    float u1 = (float)((col + 1) * tw) / (cols * tw);
                    float v0 = (float)(row * th) / (rows * th);
                    float v1 = (float)((row + 1) * th) / (rows * th);
                    float x0 = ox + x * tw * sx;
                    float x1 = ox + (x + 1) * tw * sx;
                    float y0 = oy + y * th * sy;
                    float y1 = oy + (y + 1) * th * sy;
                    verts.push_back({x0, y0, u0, v0});
                    verts.push_back({x1, y0, u1, v0});
                    verts.push_back({x1, y1, u1, v1});
                    verts.push_back({x0, y0, u0, v0});
                    verts.push_back({x1, y1, u1, v1});
                    verts.push_back({x0, y1, u0, v1});
                }
            }
            uint32_t batch_count = (uint32_t)verts.size() - batch_start;
            if (batch_count > 0 && !image_path.empty()) {
                VkDescriptorSet ds = ResourceManager::Get().GetOrCreateSamplerDS(image_path);
                tilemap_batches_.push_back({ds, batch_start, batch_count});
            }
        }
    }

    if (!verts.empty()) {
        size_t bytes = verts.size() * sizeof(TVertex);
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
    if (!scene_ || !scene_->GetWorld()) return;
    auto* world = scene_->GetWorld();

    UpdateCameraUBO();

    if (tilemap_dirty_) {
        RebuildTilemapVB();
        tilemap_dirty_ = false;
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

    // Render tilemap per-tileset batches
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

    swapchain_.EndRenderPass(cmd);
    swapchain_.SubmitAndPresent(cmd);
}

void GameApplication::EndFrame() {
    // Present handled in OnRender
}

bool GameApplication::OnProcessEvent(SDL_Event* event) {
    if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_ESCAPE) {
        Close();
        return true;
    }
    if (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_RESIZED
        && event->window.windowID == SDL_GetWindowID(GetWindow())) {
        swapchain_.RequestRebuild();
    }
    return false;
}

} // namespace game
} // namespace mg

int main(int, char**) {
    mg::game::GameApplication app;
    app.Run();
    return 0;
}
