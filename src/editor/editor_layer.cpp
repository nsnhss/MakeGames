#include "editor_layer.h"
#include "editor_context.h"
#include "debug_log.h"
#include "panels/console_panel.h"
#include "scene/scene_serializer.h"
#include "scene/world.h"
#include "scene/components/transform.h"
#include "scene/components/movement.h"
#include "scene/components/name.h"
#include "scene/components/camera.h"
#include "scene/components/camera_follow.h"
#include "scene/components/event_trigger.h"
#include "scene/components/sprite.h"
#include "scene/components/tilemap.h"
#include "scene/components/tween_animation.h"
#include "scene/systems/collision_merge.h"
#include "audio/audio_manager.h"
#include "script/script_engine.h"
#include <SDL.h>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <algorithm>

namespace mg {
namespace editor {

EditorLayer::EditorLayer(const std::string& project_path, EditorContext& ctx)
    : Layer("EditorLayer"), ctx_(ctx), project_path_(project_path)
{
}

void EditorLayer::OnAttach()
{
    EDITOR_LOG("EditorLayer::OnAttach enter");
    editor_app_.Initialize(ctx_);
    ctx_.SetPanelManager(&editor_app_.GetPanelManager());
    // 注意：ImGui 上下文在 EditorApplication::OnInit 中创建，发生在 Run() 之后
    // 项目加载和布局恢复推迟到 OnUpdate 第一帧，确保 ImGui 已就绪
    if (project_path_.empty()) {
        ctx_.CreateDefaultScene();
    }
    EDITOR_LOG("EditorLayer::OnAttach exit");
}

void EditorLayer::OnDetach()
{
    EDITOR_LOG("EditorLayer::OnDetach enter");
    if (!project_path_.empty()) {
        editor_app_.GetPanelManager().SaveLayout(project_path_ + "/data/editor_layout.json");
    }
    EDITOR_LOG("EditorLayer::OnDetach exit");
}

void EditorLayer::OnUpdate(float dt)
{
    static int update_count = 0;
    ++update_count;
    if (update_count <= 3) EDITOR_LOG("EditorLayer::OnUpdate #" << update_count);
    auto& ctx = ctx_;

    // 第一帧：ImGui 上下文已创建，加载项目和布局
    if (!project_loaded_ && !project_path_.empty()) {
        EDITOR_LOG("EditorLayer::OnUpdate first frame project load start");
        project_loaded_ = true;
        ctx.SetProjectPath(project_path_);
        EDITOR_LOG("EditorLayer::OnUpdate calling OpenProject...");
        if (!ctx.OpenProject(project_path_)) {
            EDITOR_LOG("EditorLayer::OnUpdate OpenProject failed, creating default scene");
            ctx.CreateDefaultScene();
        } else {
            EDITOR_LOG("EditorLayer::OnUpdate OpenProject succeeded");
        }
        EDITOR_LOG("EditorLayer::OnUpdate calling LoadLayout...");
        editor_app_.GetPanelManager().LoadLayout(project_path_ + "/data/editor_layout.json");
        EDITOR_LOG("EditorLayer::OnUpdate first frame project load end");
    }

    auto state = ctx.GetPlayState();

    // ---- 标题画面请求处理（Start New Game / Continue） ----
    if (ctx.ConsumeStartNewGameRequest()) {
        std::string path = ctx.GetStartScenePath();
        if (!path.empty() && ctx.LoadScene(path)) {
            ctx.EnsurePlayer();
            ctx.SetScreenMode(EditorContext::ScreenMode::Game);
            auto* scene = ctx.GetActiveScene();
            if (scene && scene->GetWorld()) {
                event_system_ = std::make_unique<scene::EventSystem>(scene->GetWorld());
                event_system_->SetShopDatabase(&ctx.GetShopDatabase());
                event_system_->SetActorDatabase(&ctx.GetActorDatabase());
                event_system_->SetEnemyDatabase(&ctx.GetEnemyDatabase());
                event_system_->SetSkillDatabase(&ctx.GetSkillDatabase());
                event_system_->SetScriptEngine(&ScriptEngine::Get());
                event_system_->TriggerOnStart();
                ctx.SetEventSystem(event_system_.get());
            }
            ctx.TakeSnapshot();
            ctx.SetPlayState(EditorContext::PlayState::Playing);
        }
    }
    if (ctx.ConsumeContinueGameRequest()) {
        int latest_slot = 0;
        std::string latest_time;
        for (int i = 1; i <= 3; ++i) {
            auto info = ctx.ReadSaveSlotInfo(i);
            if (info.exists && info.last_save_time > latest_time) {
                latest_slot = i;
                latest_time = info.last_save_time;
            }
        }
        if (latest_slot > 0) {
            ctx.RequestLoadFromSlot(latest_slot);
        } else {
            // 无存档时回退到 Start New Game
            ctx.RequestStartNewGame();
        }
    }

    // ---- PlayState 变化检测：快照 + 面板显隐 ----
    if (state != last_play_state_) {
        if (state == EditorContext::PlayState::Playing
            && last_play_state_ == EditorContext::PlayState::Edit) {
            // Edit → Playing：保存快照，隐藏非 Viewport/Console 面板，关闭 ImGui 键盘导航
            ctx.TakeSnapshot();
            ctx.SetScreenMode(EditorContext::ScreenMode::Title);
            editor_app_.GetPanelManager().SetAllVisible(false);
            // Play Mode 自动隐藏 Tile Palette（记录状态，Stop 后恢复）
            if (auto* tp = editor_app_.GetPanelManager().FindPanel("Tile Palette")) {
                tile_palette_was_open_ = tp->IsOpen();
                if (tile_palette_was_open_) tp->SetOpen(false);
            }
            ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
            // 创建 EventSystem（如果标题画面请求尚未创建）
            if (!event_system_) {
                auto* scene = ctx.GetActiveScene();
                if (scene && scene->GetWorld()) {
                    event_system_ = std::make_unique<scene::EventSystem>(scene->GetWorld());
                    event_system_->SetShopDatabase(&ctx.GetShopDatabase());
                    event_system_->SetActorDatabase(&ctx.GetActorDatabase());
                    event_system_->SetEnemyDatabase(&ctx.GetEnemyDatabase());
                    event_system_->SetSkillDatabase(&ctx.GetSkillDatabase());
                    event_system_->SetScriptEngine(&ScriptEngine::Get());
                    event_system_->TriggerOnStart();
                    ctx.SetEventSystem(event_system_.get());
                }
            }
        }
        else if (state == EditorContext::PlayState::Edit
            && last_play_state_ != EditorContext::PlayState::Edit) {
            // Playing/Paused → Edit：恢复快照，显示面板，恢复 ImGui 键盘导航
            ctx.RestoreSnapshot();
            editor_app_.GetPanelManager().SetAllVisible(true);
            // 恢复 Play Mode 前 Tile Palette 的显示状态
            if (tile_palette_was_open_) {
                if (auto* tp = editor_app_.GetPanelManager().FindPanel("Tile Palette")) {
                    tp->SetOpen(true);
                }
                tile_palette_was_open_ = false;
            }
            ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            event_system_.reset();
            ctx.SetEventSystem(nullptr);
        }
        last_play_state_ = state;
    }

    // Escape 暂停/恢复
    if ((state == EditorContext::PlayState::Playing || state == EditorContext::PlayState::Paused)
        && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        ctx.SetPlayState(ctx.IsPaused() ? EditorContext::PlayState::Playing
                                        : EditorContext::PlayState::Paused);
    }

    // ---- Play Mode 系统更新 ----
    if (state == EditorContext::PlayState::Playing && !ctx.IsPaused()
        && ctx.GetScreenMode() == EditorContext::ScreenMode::Game) {
        RunPlayModeSystems(dt);
        HandleInteractKey();
    } else if (state == EditorContext::PlayState::Edit) {
        // 编辑模式：仅预览选中实体的 TweenAnimation（playing=true 时）
        auto* scene = ctx.GetActiveScene();
        if (scene && scene->GetWorld()
            && ctx.GetSelection().GetType() == SelectionManager::Type::Entity) {
            auto e = ctx.GetSelection().GetSelectedEntity();
            auto* anim = scene->GetWorld()->GetComponent<scene::TweenAnimation>(e);
            if (anim && anim->playing) {
                tween_animation_system_.UpdateOne(dt, scene->GetWorld(), e);
            }
        }
    }

    // ---- 存档/读档请求处理（不限于 Pause 状态） ----
    int save_slot = ctx.ConsumeSaveRequest();
    if (save_slot > 0) {
        auto* scene = ctx.GetActiveScene();
        if (scene && scene->GetWorld() && event_system_) {
            auto* world = scene->GetWorld();
            std::vector<scene::EntityID> players;
            world->Query<scene::Movement>(players);
            float px = 0.0f, py = 0.0f;
            if (!players.empty()) {
                if (auto* pt = world->GetComponent<scene::Transform>(players[0])) {
                    px = pt->position.x;
                    py = pt->position.y;
                }
            }
            nlohmann::json save;
            std::string rel = ctx.GetScenePath();
            if (!rel.empty() && rel.find(ctx.GetProjectPath()) == 0) {
                rel = rel.substr(ctx.GetProjectPath().size() + 1);
            }
            save["scene"] = rel;
            save["player_x"] = px;
            save["player_y"] = py;
            save["globals"] = event_system_->SaveState()["globals"];
            save["bgm"] = AudioManager::Get().GetCurrentBGMPath();
            std::time_t now = std::time(nullptr);
            std::tm* tm = std::localtime(&now);
            char time_buf[64] = {};
            if (tm) std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%S", tm);
            save["last_save_time"] = time_buf;
            std::filesystem::create_directories(ctx.GetProjectPath() + "/saves");
            std::string save_path = ctx.GetProjectPath() + "/" + ctx.GetSaveFilePath(save_slot);
            std::ofstream f(save_path);
            if (f) f << save.dump(4);
        }
    }
    int load_slot = ctx.ConsumeLoadRequest();
    if (load_slot > 0) {
        std::string save_path = ctx.GetProjectPath() + "/" + ctx.GetSaveFilePath(load_slot);
        if (std::filesystem::exists(save_path)) {
            std::ifstream f(save_path);
            nlohmann::json load;
            if (f >> load) {
                std::string scene_rel = load.value("scene", "");
                if (!scene_rel.empty()) {
                    if (ctx.LoadScene(ctx.GetProjectPath() + "/" + scene_rel)) {
                        auto* new_scene = ctx.GetActiveScene();
                        if (new_scene && new_scene->GetWorld()) {
                            auto* new_world = new_scene->GetWorld();
                            ctx.EnsurePlayer();
                            std::vector<scene::EntityID> players;
                            new_world->Query<scene::Movement>(players);
                            if (!players.empty()) {
                                if (auto* pt = new_world->GetComponent<scene::Transform>(players[0])) {
                                    pt->position.x = load.value("player_x", 0.0f);
                                    pt->position.y = load.value("player_y", 0.0f);
                                }
                            }
                            event_system_ = std::make_unique<scene::EventSystem>(new_world);
                            event_system_->SetShopDatabase(&ctx.GetShopDatabase());
                            event_system_->SetActorDatabase(&ctx.GetActorDatabase());
                            event_system_->SetEnemyDatabase(&ctx.GetEnemyDatabase());
                            event_system_->SetSkillDatabase(&ctx.GetSkillDatabase());
                            event_system_->SetScriptEngine(&ScriptEngine::Get());
                            if (load.contains("globals")) event_system_->LoadState(load);
                            // 恢复存档时记录的 BGM（字段缺失 = 旧存档，不动）
                            if (load.contains("bgm")) {
                                std::string bgm = load.value("bgm", "");
                                if (!bgm.empty()) AudioManager::Get().PlayBGM(bgm);
                                else AudioManager::Get().StopBGM();
                            }
                            event_system_->TriggerOnStart();
                            ctx.SetEventSystem(event_system_.get());
                            ctx.TakeSnapshot();
                            ctx.SetScreenMode(EditorContext::ScreenMode::Game);
                            ctx.SetPlayState(EditorContext::PlayState::Playing);
                        }
                    }
                }
            }
        }
    }

    // 音频 BGM 循环缓冲补充（Edit/Play 都可能播放）
    AudioManager::Get().Update();
}

void EditorLayer::OnRender()
{
    static int render_count = 0;
    ++render_count;
    if (render_count <= 3) EDITOR_LOG("EditorLayer::OnRender #" << render_count);
    editor_app_.Draw();
}

// ------------------------------------------------------------------
// Play Mode Systems
// ------------------------------------------------------------------
void EditorLayer::RunPlayModeSystems(float dt)
{
    auto& ctx = ctx_;
    auto* scene = ctx.GetActiveScene();
    if (!scene || !scene->GetWorld()) return;
    auto* world = scene->GetWorld();

    // 1. MovementSystem + AABB collision
    {
        if (event_system_ && (event_system_->IsDialogActive() || event_system_->IsShopActive() || event_system_->IsBattleActive()))
            return; // 对话框 / 商店 / 战斗阻塞移动

        const uint8_t* keys = SDL_GetKeyboardState(nullptr);
        std::vector<scene::EntityID> move_entities;
        world->Query<scene::Transform, scene::Movement>(move_entities);

        // Collect collision rects from all Tilemap layers.
        // A tile is solid when it blocks all directions (Pass_All).
        // 实心格子经 MergeLayerCollisionRects 合并为更大矩形，减少碰撞测试次数。
        std::vector<scene::CollisionRect> collision_rects;
        std::vector<scene::EntityID> all_entities;
        world->GetAllEntities(all_entities);
        for (auto e : all_entities) {
            auto* tm = world->GetComponent<scene::Tilemap>(e);
            if (!tm) continue;
            auto* t = world->GetComponent<scene::Transform>(e);
            float ox = t ? t->position.x : 0.0f;
            float oy = t ? t->position.y : 0.0f;
            float sx = t ? t->scale.x : 1.0f;
            float sy = t ? t->scale.y : 1.0f;
            for (auto& layer : tm->layers) {
                auto merged = scene::MergeLayerCollisionRects(
                    layer, (float)tm->tile_width, (float)tm->tile_height, ox, oy, sx, sy);
                collision_rects.insert(collision_rects.end(), merged.begin(), merged.end());
            }
        }

        // Compute map bounds from the first visible Tilemap layer for clamping players.
        float map_min_x = 0.0f, map_min_y = 0.0f, map_max_x = 0.0f, map_max_y = 0.0f;
        bool has_map_bounds = false;
        for (auto e : all_entities) {
            auto* tm = world->GetComponent<scene::Tilemap>(e);
            if (!tm || tm->layers.empty()) continue;
            auto* t = world->GetComponent<scene::Transform>(e);
            float ox = t ? t->position.x : 0.0f;
            float oy = t ? t->position.y : 0.0f;
            float sx = t ? t->scale.x : 1.0f;
            float sy = t ? t->scale.y : 1.0f;
            map_min_x = ox;
            map_min_y = oy;
            map_max_x = ox + tm->layers[0].width * tm->tile_width * sx;
            map_max_y = oy + tm->layers[0].height * tm->tile_height * sy;
            has_map_bounds = true;
            break;
        }

        constexpr float HALF = 14.0f; // slightly less than 16 to avoid seam sticking

        for (auto entity : move_entities) {
            auto* t = world->GetComponent<scene::Transform>(entity);
            auto* m = world->GetComponent<scene::Movement>(entity);
            if (!t || !m) continue;

            math::Vec2 input(0, 0);
            if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP])    input.y += 1;
            if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN])  input.y -= 1;
            if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT])  input.x -= 1;
            if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) input.x += 1;

            if (input.x != 0 || input.y != 0) {
                float len = std::sqrt(input.x * input.x + input.y * input.y);
                input.x /= len; input.y /= len;
            }

            float dx = input.x * m->speed * dt;
            float dy = input.y * m->speed * dt;

            float px0 = t->position.x - HALF, py0 = t->position.y - HALF;
            float px1 = t->position.x + HALF, py1 = t->position.y + HALF;

            // X axis
            bool bx = false;
            float tx0 = px0 + dx, tx1 = px1 + dx;
            for (auto& r : collision_rects)
                if (tx0 < r.x1 && tx1 > r.x0 && py0 < r.y1 && py1 > r.y0) { bx = true; break; }
            if (!bx) t->position.x += dx;

            // Y axis
            bool by = false;
            for (auto& r : collision_rects)
                if (px0 < r.x1 && px1 > r.x0 && py0 + dy < r.y1 && py1 + dy > r.y0) { by = true; break; }
            if (!by) t->position.y += dy;

            // Clamp player inside the map so it cannot walk off the edge.
            if (has_map_bounds) {
                t->position.x = std::clamp(t->position.x, map_min_x + HALF, map_max_x - HALF);
                t->position.y = std::clamp(t->position.y, map_min_y + HALF, map_max_y - HALF);
            }
        }
    }

    // 2. CameraFollowSystem: Camera entities follow their target
    std::vector<scene::EntityID> cameras;
    world->Query<scene::Camera, scene::Transform>(cameras);

    // Find default follow target (first entity with CameraFollowTarget) for cameras without explicit target.
    scene::EntityID default_follow = scene::kInvalidEntity;
    std::vector<scene::EntityID> follow_targets;
    world->Query<scene::CameraFollowTarget>(follow_targets);
    if (!follow_targets.empty()) default_follow = follow_targets[0];

    for (auto e : cameras) {
        auto* cam = world->GetComponent<scene::Camera>(e);
        auto* cam_t = world->GetComponent<scene::Transform>(e);
        if (!cam || !cam_t) continue;
        scene::EntityID target_id = cam->follow_target;
        if (target_id == scene::kInvalidEntity || target_id == 0) target_id = default_follow;
        if (target_id == scene::kInvalidEntity || target_id == 0) continue;
        if (!world->IsValid(target_id)) continue;
        auto* target_t = world->GetComponent<scene::Transform>(target_id);
        if (!target_t) continue;

        math::Vec2 target = target_t->position;
        math::Vec2 curr = cam_t->position;
        math::Vec2 delta = target - curr;

        if (cam->smooth_follow) {
            if (std::fabs(delta.x) < cam->dead_zone.x) delta.x = 0;
            if (std::fabs(delta.y) < cam->dead_zone.y) delta.y = 0;
            curr.x += delta.x * cam->follow_speed * dt;
            curr.y += delta.y * cam->follow_speed * dt;
        } else {
            curr = target;
        }
        cam_t->position = curr;

        // Clamp camera position to tilemap bounds so the viewport never shows
        // the empty area outside the map.
        auto* scene = ctx.GetActiveScene();
        if (scene) {
            auto* map_world = scene->GetWorld();
            uint32_t tm_entity = ctx.GetTilemapEntity();
            if (map_world && tm_entity != 0 && map_world->IsValid(tm_entity)) {
                auto* tm = map_world->GetComponent<scene::Tilemap>(tm_entity);
                if (tm && !tm->layers.empty()) {
                    float map_w = tm->layers[0].width  * tm->tile_width;
                    float map_h = tm->layers[0].height * tm->tile_height;
                    uint32_t vp_w = ctx.GetViewportWidth();
                    uint32_t vp_h = ctx.GetViewportHeight();
                    if (vp_w > 0 && vp_h > 0) {
                        float aspect = (float)vp_w / (float)vp_h;
                        float half_h = cam->ortho_size;
                        float half_w = half_h * aspect;
                        // Clamp camera so the viewport does not show too much outside the map,
                        // but still allow it to follow the player when the viewport is larger
                        // than the map (in which case the valid range collapses toward center).
                        float min_x = std::min(half_w, map_w - half_w);
                        float max_x = std::max(half_w, map_w - half_w);
                        float min_y = std::min(half_h, map_h - half_h);
                        float max_y = std::max(half_h, map_h - half_h);
                        cam_t->position.x = std::clamp(cam_t->position.x, min_x, max_x);
                        cam_t->position.y = std::clamp(cam_t->position.y, min_y, max_y);
                    }
                }
            }
        }
    }
    // 3. AnimatorSystem
    animator_system_.Update(dt, world);

    // 3.5 TweenAnimationSystem（关键帧补间）
    tween_animation_system_.Update(dt, world);

    // 4. EventSystem
    if (event_system_) {
        event_system_->Update(dt);
    }

    // 5. 场景切换（TransferPlayer 跨场景）

    // FadeOut：黑屏 → 执行切换
    if (ctx.GetFadeState() == EditorContext::FadeState::Out) {
        ctx.SetFadeAlpha(ctx.GetFadeAlpha() + dt * 3.0f);
        if (ctx.GetFadeAlpha() >= 1.0f) {
            ctx.SetFadeAlpha(1.0f);
            if (event_system_ && event_system_->HasPendingSceneTransition()) {
                auto state_snap = event_system_->TakeGlobalStateSnapshot();
                std::string target_scene = event_system_->ConsumePendingSceneTransition();
                int spawn_x = event_system_->GetPendingSpawnX();
                int spawn_y = event_system_->GetPendingSpawnY();

                // target_scene 存的是 MapInfos 中的 name（如 Village.json）
                std::string full_path;
                if (target_scene.find('/') != std::string::npos || target_scene.find('\\') != std::string::npos) {
                    // 旧数据兼容：target 存的是相对路径
                    full_path = ctx.GetProjectPath() + "/" + target_scene;
                } else {
                    auto mi = ctx.LoadMapInfos();
                    for (const auto& e : mi.entries) {
                        if (e.name == target_scene) {
                            full_path = ctx.GetProjectPath() + "/maps/" + e.name;
                            break;
                        }
                    }
                }

                if (!full_path.empty() && ctx.LoadScene(full_path)) {
                    ctx.SetScenePath(full_path);
                    ctx.ClearSceneDirty();
                    auto* new_scene = ctx.GetActiveScene();
                    if (new_scene && new_scene->GetWorld()) {
                        auto* new_world = new_scene->GetWorld();

                        std::vector<scene::EntityID> players;
                        new_world->Query<scene::Movement>(players);
                        scene::EntityID player = 0;
                        if (!players.empty()) {
                            player = players[0];
                        } else {
                            player = new_world->CreateEntity();
                            new_world->AddComponent<scene::Transform>(player);
                            new_world->AddComponent<scene::Name>(player, "Player");
                            new_world->AddComponent<scene::Movement>(player);
                            new_world->AddComponent<scene::Sprite>(player);
                            new_world->AddComponent<scene::CameraFollowTarget>(player);
                        }

                        auto* pt = new_world->GetComponent<scene::Transform>(player);
                        if (pt) {
                            float tw = 32.0f, th = 32.0f;
                            std::vector<scene::EntityID> all;
                            new_world->GetAllEntities(all);
                            for (auto e : all) {
                                auto* tm = new_world->GetComponent<scene::Tilemap>(e);
                                if (tm) { tw = (float)tm->tile_width; th = (float)tm->tile_height; break; }
                            }
                            pt->position.x = (float)spawn_x * tw;
                            pt->position.y = (float)spawn_y * th;
                        }

                        event_system_ = std::make_unique<scene::EventSystem>(new_world);
                        event_system_->SetShopDatabase(&ctx.GetShopDatabase());
                        event_system_->SetActorDatabase(&ctx.GetActorDatabase());
                        event_system_->SetEnemyDatabase(&ctx.GetEnemyDatabase());
                        event_system_->SetScriptEngine(&ScriptEngine::Get());
                        event_system_->RestoreGlobalState(state_snap);
                        event_system_->TriggerOnStart();
                        ctx.SetEventSystem(event_system_.get());
                        ctx.TakeSnapshot();
                    }
                }
            }
            ctx.SetFadeState(EditorContext::FadeState::In);
        }
        return;
    }

    // FadeIn：亮屏 → 恢复正常
    if (ctx.GetFadeState() == EditorContext::FadeState::In) {
        ctx.SetFadeAlpha(ctx.GetFadeAlpha() - dt * 3.0f);
        if (ctx.GetFadeAlpha() <= 0.0f) {
            ctx.SetFadeAlpha(0.0f);
            ctx.SetFadeState(EditorContext::FadeState::None);
        }
        return;
    }

    // 检测到切换请求，启动 FadeOut
    if (event_system_ && event_system_->HasPendingSceneTransition()) {
        ctx.SetFadeState(EditorContext::FadeState::Out);
        return;
    }
}

// ------------------------------------------------------------------
// 交互键检测：Enter 键触发前方事件
// ------------------------------------------------------------------
void EditorLayer::HandleInteractKey()
{
    if (!ImGui::IsKeyPressed(ImGuiKey_Enter)) return;
    if (event_system_ && (event_system_->IsDialogActive() || event_system_->IsShopActive() || event_system_->IsBattleActive())) return; // 对话框 / 商店 / 战斗期间不触发交互

    auto& ctx = ctx_;
    auto* scene = ctx.GetActiveScene();
    if (!scene || !scene->GetWorld()) return;
    auto* world = scene->GetWorld();

    // 查找玩家实体（有 Movement 的第一个）
    std::vector<scene::EntityID> players;
    world->Query<scene::Movement>(players);
    if (players.empty()) return;

    auto* player_t = world->GetComponent<scene::Transform>(players[0]);
    if (!player_t) return;

    // 查找 48px 内最近的 EventTrigger
    scene::EntityID nearest = 0;
    float nearest_dist = 48.0f;
    std::vector<scene::EntityID> events;
    world->Query<scene::EventTrigger>(events);
    for (auto e : events) {
        auto* t = world->GetComponent<scene::Transform>(e);
        if (!t) continue;
        float dx = t->position.x - player_t->position.x;
        float dy = t->position.y - player_t->position.y;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist < nearest_dist) {
            nearest_dist = dist;
            nearest = e;
        }
    }

    if (nearest != 0 && event_system_) {
        event_system_->TriggerOnInteract(nearest);
    }
}

} // namespace editor
} // namespace mg
