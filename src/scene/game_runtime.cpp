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
#include "scene/systems/camera_follow_system.h"
#include "scene/save_system.h"
#include "resource/game_databases.h"
#include "resource/data_manager.h"
#include "resource/schema/project_schema.h"
#include "audio/audio_manager.h"
#include "script/script_engine.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
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
    movement_system_.InvalidateCollisionCache();
    current_scene_rel_ = MakeRelPath(path);
    EnsurePlayer();
    play_mode_.SetScreen(PlayModeController::Screen::Game);
    RebuildEventSystem();
}

void GameRuntime::ContinueGame() {
    int latest_slot = FindLatestSaveSlot(play_mode_, project_root_);
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
    auto save = BuildSaveGameJson(scene->GetWorld(), *event_system_, current_scene_rel_);
    WriteSaveSlot(project_root_, slot, save);
}

void GameRuntime::LoadFromSlot(int slot) {
    nlohmann::json load;
    if (!ReadSaveSlot(project_root_, slot, load)) return;
    std::string scene_rel = load.value("scene", "");
    if (scene_rel.empty()) return;
    if (!host_->LoadScene(project_root_ + "/" + scene_rel)) return;
    movement_system_.InvalidateCollisionCache();
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

    // 1. MovementSystem + AABB collision（PlayerMovementSystem，碰撞矩形带指纹缓存）
    {
        bool dialog_or_shop = event_system_ && (event_system_->IsDialogActive() || event_system_->IsShopActive());
        bool battle_active = event_system_ && event_system_->IsBattleActive();
        if (dialog_or_shop)
            return; // 对话框 / 商店：冻结全部世界系统（原行为）
        if (!battle_active) {
            movement_system_.Update(dt, world, SDL_GetKeyboardState(nullptr));
        }
        // 战斗期间仅阻塞移动，其余系统继续（修复：原实现此处 return 会连 EventSystem::Update
        // 一起冻结，UpdateBattle 不 tick → Intro→PlayerTurn 永不推进，战斗 soft-lock）
    }

    // 2. CameraFollowSystem: Camera entities follow their target
    {
        auto [vp_w, vp_h] = viewport_size_fn_ ? viewport_size_fn_() : std::pair<uint32_t, uint32_t>{0, 0};
        UpdateCameraFollowSystem(world, dt, vp_w, vp_h);
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
                    movement_system_.InvalidateCollisionCache();
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
