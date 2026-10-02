#pragma once
#include "app/layer.h"
#include "editor_app.h"
#include "editor_context.h"
#include "scene/game_runtime.h"
#include "scene/systems/tween_animation_system.h"
#include <string>
#include <memory>

namespace mg {
namespace editor {

// 承载 Editor 具体 UI 逻辑的 Layer
// Play Mode 编排（移动/碰撞/事件/存读档/场景切换）已下沉 scene::GameRuntime，
// 本层只保留编辑器专属：Edit↔Play 切换的快照/面板显隐、Edit 模式 Tween 预览。
class EditorLayer : public Layer, public scene::ISceneHost {
public:
    explicit EditorLayer(const std::string& project_path, EditorContext& ctx);

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate(float dt) override;
    void OnRender() override;

    // ISceneHost（GameRuntime 场景来源，委托 SceneSession 并附带编辑副作用）
    scene::Scene* GetActiveScene() override { return ctx_.GetActiveScene(); }
    bool LoadScene(const std::string& path) override;

private:
    EditorApp editor_app_;
    EditorContext& ctx_;
    std::string project_path_;
    EditorContext::PlayState last_play_state_ = EditorContext::PlayState::Edit;
    scene::GameRuntime runtime_;
    scene::TweenAnimationSystem tween_animation_system_; // Edit 模式选中实体预览用
    bool tile_palette_was_open_ = false;
    bool project_loaded_ = false;
};

} // namespace editor
} // namespace mg
