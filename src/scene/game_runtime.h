#pragma once
#include "scene/play_mode_controller.h"
#include "scene/event_system.h"
#include "scene/systems/animator_system.h"
#include "scene/systems/tween_animation_system.h"
#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace mg {

class GameDatabases;

namespace scene {

class Scene;

// 场景来源抽象：editor 委托 SceneSession（加载后附带编辑副作用），
// game.exe 自持 Scene 裸加载。
class ISceneHost {
public:
    virtual ~ISceneHost() = default;
    virtual Scene* GetActiveScene() = 0;
    virtual bool LoadScene(const std::string& path) = 0; // 替换当前场景，成功返回 true
};

// Play Mode 运行时编排（editor 与 game.exe 共用）：
// 移动/碰撞、相机跟随、Animator/Tween、EventSystem、Fade 场景切换、
// 标题画面请求（StartNewGame/Continue）、暂停切换、存档/读档。
// 状态（PlayState/Screen/Fade/存档请求）持有在外部注入的 PlayModeController 中，
// 保证 editor 门面（EditorContext）与 runtime 操作同一份状态。
class GameRuntime {
public:
    GameRuntime(ISceneHost* host, GameDatabases* databases, PlayModeController& play_mode,
                std::string project_root,
                std::function<std::pair<uint32_t, uint32_t>()> viewport_size_fn);

    // EventSystem 重建通知（editor 用于同步 EditorContext::SetEventSystem）
    void SetEventSystemChangedCallback(std::function<void(EventSystem*)> cb) {
        on_event_system_changed_ = std::move(cb);
    }

    PlayModeController& PlayMode() { return play_mode_; }
    EventSystem* GetEventSystem() const { return event_system_.get(); }
    const std::string& GetProjectRoot() const { return project_root_; }

    void EnterPlayMode(); // 建 EventSystem + TriggerOnStart + 进标题画面
    void ExitPlayMode();  // 销毁 EventSystem
    void Update(float dt); // 标题/存读档请求消费 + Escape 暂停 + 世界系统（内部门控）

private:
    void RebuildEventSystem(bool fire_on_start = true); // 4 数据库 + ScriptEngine 注入
    void StartNewGame();
    void ContinueGame();
    void SaveToSlot(int slot);
    void LoadFromSlot(int slot);
    void RunWorldSystems(float dt);
    void HandleInteractKey();
    void EnsurePlayer();
    std::string GetStartScenePath() const;
    std::string ResolveTransferTarget(const std::string& target) const;
    std::string MakeRelPath(const std::string& abs) const;

    ISceneHost* host_ = nullptr;
    GameDatabases* databases_ = nullptr;
    PlayModeController& play_mode_;
    std::string project_root_;
    std::function<std::pair<uint32_t, uint32_t>()> viewport_size_fn_;
    std::function<void(EventSystem*)> on_event_system_changed_;

    std::unique_ptr<EventSystem> event_system_;
    AnimatorSystem animator_system_;
    TweenAnimationSystem tween_animation_system_;
    std::string current_scene_rel_;  // 当前场景相对路径（存档用）
    bool prev_interact_key_ = false; // Return 上帧状态
    bool prev_pause_key_ = false;    // Escape 上帧状态
};

} // namespace scene
} // namespace mg
