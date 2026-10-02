#include "scene/game_runtime.h"
#include "scene/constants.h"
#include "scene/scene.h"
#include "scene/world.h"
#include "scene/components/transform.h"
#include "scene/components/movement.h"
#include "scene/components/name.h"
#include "scene/components/camera.h"
#include "scene/components/camera_follow.h"
#include "scene/components/sprite.h"
#include "scene/components/tilemap.h"
#include "scene/components/event_trigger.h"
#include "scene/systems/collision_merge.h"
#include "resource/game_databases.h"
#include "resource/data_manager.h"
#include "resource/schema/project_schema.h"
#include "audio/audio_manager.h"
#include "script/script_engine.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <vector>

namespace mg {
namespace scene {

namespace fs = std::filesystem;

GameRuntime::GameRuntime(ISceneHost* host, GameDatabases* databases, PlayModeController& play_mode,
                         std::string project_root,
                         std::function<std::pair<uint32_t, uint32_t>()> viewport_size_fn)
    : host_(host), databases_(databases), play_mode_(play_mode),
      project_root_(std::move(project_root)), viewport_size_fn_(std::move(viewport_size_fn)) {
}

void GameRuntime::EnterPlayMode() {
    RebuildEventSystem();
    play_mode_.SetScreen(PlayModeController::Screen::Title);
}

void GameRuntime::ExitPlayMode() {
    event_system_.reset();
    if (on_event_system_changed_) on_event_system_changed_(nullptr);
}

void GameRuntime::RebuildEventSystem(bool fire_on_start) {
    auto* scene = host_->GetActiveScene();
    if (!scene || !scene->GetWorld()) return;
    event_system_ = std::make_unique<EventSystem>(scene->GetWorld());
    event_system_->SetShopDatabase(&databases_->shop);
    event_system_->SetActorDatabase(&databases_->actor);
    event_system_->SetEnemyDatabase(&databases_->enemy);
    event_system_->SetSkillDatabase(&databases_->skill); // 修：原 Fade 场景切换分支漏注此项
    event_system_->SetScriptEngine(&ScriptEngine::Get());
    if (fire_on_start) event_system_->TriggerOnStart();
    if (on_event_system_changed_) on_event_system_changed_(event_system_.get());
}

// ------------------------------------------------------------------
// 每帧驱动
// ------------------------------------------------------------------
void GameRuntime::Update(float dt) {
    // 标题画面请求
    if (play_mode_.ConsumeStartNewGameRequest()) StartNewGame();
    if (play_mode_.ConsumeContinueGameRequest()) ContinueGame();

    // 存读档请求（不限于 Pause 状态）
    int save_slot = play_mode_.ConsumeSaveRequest();
    if (save_slot > 0) SaveToSlot(save_slot);
    int load_slot = play_mode_.ConsumeLoadRequest();
    if (load_slot > 0) LoadFromSlot(load_slot);

    // Escape 暂停/恢复（SDL 边沿检测；原实现分散在 EditorLayer 与 ViewportPanel 两处，已去重）
    const uint8_t* keys = SDL_GetKeyboardState(nullptr);
    bool esc = keys && keys[SDL_SCANCODE_ESCAPE];
    if (esc && !prev_pause_key_ &&
        (play_mode_.IsPlaying() || play_mode_.GetState() == PlayModeController::State::Paused)) {
        play_mode_.SetState(play_mode_.IsPaused() ? PlayModeController::State::Playing
                                                  : PlayModeController::State::Paused);
    }
    prev_pause_key_ = esc;

    // 世界系统：仅 Playing + Game 画面
    if (play_mode_.IsPlaying() && play_mode_.GetScreen() == PlayModeController::Screen::Game) {
        RunWorldSystems(dt);
        HandleInteractKey();
    }
}

// ------------------------------------------------------------------
// 标题画面请求
// ------------------------------------------------------------------
void GameRuntime::StartNewGame() {
    std::string path = GetStartScenePath();
    if (path.empty()) return;
    if (!host_->LoadScene(path)) return;
    current_scene_rel_ = MakeRelPath(path);
    EnsurePlayer();
    play_mode_.SetScreen(PlayModeController::Screen::Game);
    RebuildEventSystem();
}

void GameRuntime::ContinueGame() {
    int latest_slot = 0;
    std::string latest_time;
    for (int i = 1; i <= 3; ++i) {
        auto info = play_mode_.ReadSaveSlotInfo(project_root_, i);
        if (info.exists && info.last_save_time > latest_time) {
            latest_slot = i;
            latest_time = info.last_save_time;
        }
    }
    if (latest_slot > 0) LoadFromSlot(latest_slot);
    else StartNewGame(); // 无存档时回退到 Start New Game
}

std::string GameRuntime::GetStartScenePath() const {
    auto manifest = DataManager::Get().Load<ProjectManifest>(project_root_ + "/project.json");
    if (manifest && !manifest->start_scene.empty())
        return project_root_ + "/" + manifest->start_scene;
    // 回退：查找 maps/ 下第一个非 MapInfos.json 的 JSON
    std::string maps_dir = project_root_ + "/maps";
    if (fs::exists(maps_dir)) {
        for (const auto& entry : fs::directory_iterator(maps_dir)) {
            if (!entry.is_regular_file()) continue;
            std::string name = entry.path().filename().string();
            if (name == "MapInfos.json") continue;
            if (entry.path().extension() == ".json") return entry.path().string();
        }
    }
    return {};
}

// ------------------------------------------------------------------
// 存档 / 读档
// ------------------------------------------------------------------
void GameRuntime::SaveToSlot(int slot) {
    auto* scene = host_->GetActiveScene();
    if (!scene || !scene->GetWorld() || !event_system_) return;
    auto* world = scene->GetWorld();

    std::vector<EntityID> players;
    world->Query<Movement>(players);
    float px = 0.0f, py = 0.0f;
    if (!players.empty()) {
        if (auto* pt = world->GetComponent<Transform>(players[0])) {
            px = pt->position.x;
            py = pt->position.y;
        }
    }
    nlohmann::json save;
    save["scene"] = current_scene_rel_;
    save["player_x"] = px;
    save["player_y"] = py;
    save["globals"] = event_system_->SaveState()["globals"];
    save["bgm"] = AudioManager::Get().GetCurrentBGMPath();
    std::time_t now = std::time(nullptr);
    std::tm* tm = std::localtime(&now);
    char time_buf[64] = {};
    if (tm) std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%S", tm);
    save["last_save_time"] = time_buf;
    fs::create_directories(project_root_ + "/saves");
    std::string save_path = project_root_ + "/" + PlayModeController::GetSaveFilePath(slot);
    std::ofstream f(save_path);
    if (f) f << save.dump(4);
}

void GameRuntime::LoadFromSlot(int slot) {
    std::string save_path = project_root_ + "/" + PlayModeController::GetSaveFilePath(slot);
    if (!fs::exists(save_path)) return;
    std::ifstream f(save_path);
    nlohmann::json load;
    if (!(f >> load)) return;
    std::string scene_rel = load.value("scene", "");
    if (scene_rel.empty()) return;
    if (!host_->LoadScene(project_root_ + "/" + scene_rel)) return;
    current_scene_rel_ = scene_rel;

    auto* scene = host_->GetActiveScene();
    if (!scene || !scene->GetWorld()) return;
    auto* world = scene->GetWorld();
    EnsurePlayer();
    std::vector<EntityID> players;
    world->Query<Movement>(players);
    if (!players.empty()) {
        if (auto* pt = world->GetComponent<Transform>(players[0])) {
            pt->position.x = load.value("player_x", 0.0f);
            pt->position.y = load.value("player_y", 0.0f);
        }
    }
    RebuildEventSystem(/*fire_on_start=*/false);
    if (event_system_ && load.contains("globals")) event_system_->LoadState(load);
    // 恢复存档时记录的 BGM（字段缺失 = 旧存档，不动）
    if (load.contains("bgm")) {
        std::string bgm = load.value("bgm", "");
        if (!bgm.empty()) AudioManager::Get().PlayBGM(bgm);
        else AudioManager::Get().StopBGM();
    }
    if (event_system_) event_system_->TriggerOnStart();
    play_mode_.SetScreen(PlayModeController::Screen::Game);
    if (!play_mode_.IsPlaying()) play_mode_.SetState(PlayModeController::State::Playing);
}

// ------------------------------------------------------------------
// 辅助
// ------------------------------------------------------------------
void GameRuntime::EnsurePlayer() {
    auto* scene = host_->GetActiveScene();
    if (!scene || !scene->GetWorld()) return;
    auto* world = scene->GetWorld();
    std::vector<EntityID> players;
    world->Query<Movement>(players);
    if (!players.empty()) return;
    EntityID player = world->CreateEntity();
    world->AddComponent<Transform>(player);
    world->AddComponent<Name>(player, "Player");
    world->AddComponent<Movement>(player);
    world->AddComponent<Sprite>(player);
    world->AddComponent<CameraFollowTarget>(player);
}

std::string GameRuntime::ResolveTransferTarget(const std::string& target) const {
    if (target.find('/') != std::string::npos || target.find('\\') != std::string::npos) {
        // 旧数据兼容：target 存的是相对路径
        return project_root_ + "/" + target;
    }
    // target 存的是 MapInfos 中的 name（如 Village.json）
    auto mi = DataManager::Get().Load<MapInfos>(project_root_ + "/maps/MapInfos.json");
    if (mi) {
        for (const auto& e : mi->entries) {
            if (e.name == target) return project_root_ + "/maps/" + e.name;
        }
    }
    return {};
}

std::string GameRuntime::MakeRelPath(const std::string& abs) const {
    std::string prefix = project_root_ + "/";
    if (abs.find(prefix) == 0) return abs.substr(prefix.size());
    return abs;
}

// ------------------------------------------------------------------
// 世界系统（移动/碰撞 → 相机跟随 → 动画 → 事件 → Fade 场景切换）
// ------------------------------------------------------------------
void GameRuntime::RunWorldSystems(float dt) {
    auto* scene = host_->GetActiveScene();
    if (!scene || !scene->GetWorld()) return;
    auto* world = scene->GetWorld();

    // 1. MovementSystem + AABB collision
    {
        if (event_system_ && (event_system_->IsDialogActive() || event_system_->IsShopActive() || event_system_->IsBattleActive()))
            return; // 对话框 / 商店 / 战斗阻塞移动

        const uint8_t* keys = SDL_GetKeyboardState(nullptr);
        std::vector<EntityID> move_entities;
        world->Query<Transform, Movement>(move_entities);

        // Collect collision rects from all Tilemap layers.
        // A tile is solid when it blocks all directions (Pass_All).
        // 实心格子经 MergeLayerCollisionRects 合并为更大矩形，减少碰撞测试次数。
        std::vector<CollisionRect> collision_rects;
        std::vector<EntityID> all_entities;
        world->GetAllEntities(all_entities);
        for (auto e : all_entities) {
            auto* tm = world->GetComponent<Tilemap>(e);
            if (!tm) continue;
            auto* t = world->GetComponent<Transform>(e);
            float ox = t ? t->position.x : 0.0f;
            float oy = t ? t->position.y : 0.0f;
            float sx = t ? t->scale.x : 1.0f;
            float sy = t ? t->scale.y : 1.0f;
            for (auto& layer : tm->layers) {
                auto merged = MergeLayerCollisionRects(
                    layer, (float)tm->tile_width, (float)tm->tile_height, ox, oy, sx, sy);
                collision_rects.insert(collision_rects.end(), merged.begin(), merged.end());
            }
        }

        // Compute map bounds from the first visible Tilemap layer for clamping players.
        float map_min_x = 0.0f, map_min_y = 0.0f, map_max_x = 0.0f, map_max_y = 0.0f;
        bool has_map_bounds = false;
        for (auto e : all_entities) {
            auto* tm = world->GetComponent<Tilemap>(e);
            if (!tm || tm->layers.empty()) continue;
            auto* t = world->GetComponent<Transform>(e);
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

        constexpr float HALF = kPlayerCollisionHalfExtent; // 略小于半格 16，避免卡缝

        for (auto entity : move_entities) {
            auto* t = world->GetComponent<Transform>(entity);
            auto* m = world->GetComponent<Movement>(entity);
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
    std::vector<EntityID> cameras;
    world->Query<Camera, Transform>(cameras);

    // Find default follow target (first entity with CameraFollowTarget) for cameras without explicit target.
    EntityID default_follow = kInvalidEntity;
    std::vector<EntityID> follow_targets;
    world->Query<CameraFollowTarget>(follow_targets);
    if (!follow_targets.empty()) default_follow = follow_targets[0];

    for (auto e : cameras) {
        auto* cam = world->GetComponent<Camera>(e);
        auto* cam_t = world->GetComponent<Transform>(e);
        if (!cam || !cam_t) continue;
        EntityID target_id = cam->follow_target;
        if (target_id == kInvalidEntity || target_id == 0) target_id = default_follow;
        if (target_id == kInvalidEntity || target_id == 0) continue;
        if (!world->IsValid(target_id)) continue;
        auto* target_t = world->GetComponent<Transform>(target_id);
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
        {
            // 运行时改为查找第一个 Tilemap 实体（editor 原用 TilesetEditor 选中态）
            Tilemap* tm = nullptr;
            std::vector<EntityID> all;
            world->GetAllEntities(all);
            for (auto te : all) {
                tm = world->GetComponent<Tilemap>(te);
                if (tm) break;
            }
            if (tm && !tm->layers.empty()) {
                float map_w = tm->layers[0].width  * tm->tile_width;
                float map_h = tm->layers[0].height * tm->tile_height;
                auto [vp_w, vp_h] = viewport_size_fn_ ? viewport_size_fn_() : std::pair<uint32_t, uint32_t>{0, 0};
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
    if (play_mode_.GetFadeState() == PlayModeController::Fade::Out) {
        play_mode_.SetFadeAlpha(play_mode_.GetFadeAlpha() + dt * 3.0f);
        if (play_mode_.GetFadeAlpha() >= 1.0f) {
            play_mode_.SetFadeAlpha(1.0f);
            if (event_system_ && event_system_->HasPendingSceneTransition()) {
                auto state_snap = event_system_->TakeGlobalStateSnapshot();
                std::string target_scene = event_system_->ConsumePendingSceneTransition();
                int spawn_x = event_system_->GetPendingSpawnX();
                int spawn_y = event_system_->GetPendingSpawnY();

                std::string full_path = ResolveTransferTarget(target_scene);

                if (!full_path.empty() && host_->LoadScene(full_path)) {
                    current_scene_rel_ = MakeRelPath(full_path);
                    auto* new_scene = host_->GetActiveScene();
                    if (new_scene && new_scene->GetWorld()) {
                        auto* new_world = new_scene->GetWorld();

                        EnsurePlayer();
                        std::vector<EntityID> players;
                        new_world->Query<Movement>(players);
                        EntityID player = players.empty() ? kInvalidEntity : players[0];

                        auto* pt = player != kInvalidEntity ? new_world->GetComponent<Transform>(player) : nullptr;
                        if (pt) {
                            float tw = 32.0f, th = 32.0f;
                            std::vector<EntityID> all;
                            new_world->GetAllEntities(all);
                            for (auto e : all) {
                                auto* tm = new_world->GetComponent<Tilemap>(e);
                                if (tm) { tw = (float)tm->tile_width; th = (float)tm->tile_height; break; }
                            }
                            pt->position.x = (float)spawn_x * tw;
                            pt->position.y = (float)spawn_y * th;
                        }

                        RebuildEventSystem(/*fire_on_start=*/false);
                        if (event_system_) {
                            event_system_->RestoreGlobalState(state_snap);
                            event_system_->TriggerOnStart();
                        }
                    }
                }
            }
            play_mode_.SetFadeState(PlayModeController::Fade::In);
        }
        return;
    }

    // FadeIn：亮屏 → 恢复正常
    if (play_mode_.GetFadeState() == PlayModeController::Fade::In) {
        play_mode_.SetFadeAlpha(play_mode_.GetFadeAlpha() - dt * 3.0f);
        if (play_mode_.GetFadeAlpha() <= 0.0f) {
            play_mode_.SetFadeAlpha(0.0f);
            play_mode_.SetFadeState(PlayModeController::Fade::None);
        }
        return;
    }

    // 检测到切换请求，启动 FadeOut
    if (event_system_ && event_system_->HasPendingSceneTransition()) {
        play_mode_.SetFadeState(PlayModeController::Fade::Out);
        return;
    }
}

// ------------------------------------------------------------------
// 交互键检测：Enter 键触发前方事件（SDL 边沿检测）
// ------------------------------------------------------------------
void GameRuntime::HandleInteractKey() {
    const uint8_t* keys = SDL_GetKeyboardState(nullptr);
    bool enter_down = keys && (keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_KP_ENTER]);
    bool pressed = enter_down && !prev_interact_key_;
    prev_interact_key_ = enter_down;
    if (!pressed) return;
    if (event_system_ && (event_system_->IsDialogActive() || event_system_->IsShopActive() || event_system_->IsBattleActive()))
        return; // 对话框 / 商店 / 战斗期间不触发交互

    auto* scene = host_->GetActiveScene();
    if (!scene || !scene->GetWorld()) return;
    auto* world = scene->GetWorld();

    // 查找玩家实体（有 Movement 的第一个）
    std::vector<EntityID> players;
    world->Query<Movement>(players);
    if (players.empty()) return;

    auto* player_t = world->GetComponent<Transform>(players[0]);
    if (!player_t) return;

    // 查找 48px 内最近的 EventTrigger
    EntityID nearest = 0;
    float nearest_dist = 48.0f;
    std::vector<EntityID> events;
    world->Query<EventTrigger>(events);
    for (auto e : events) {
        auto* t = world->GetComponent<Transform>(e);
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

} // namespace scene
} // namespace mg
