#pragma once
#include "app/layer.h"
#include "editor_app.h"
#include "editor_context.h"
#include "scene/event_system.h"
#include "scene/systems/animator_system.h"
#include "scene/systems/tween_animation_system.h"
#include <string>
#include <memory>
#include <vector>

namespace mg {
namespace editor {

// 承载 Editor 具体 UI 逻辑的 Layer
class EditorLayer : public Layer {
public:
    explicit EditorLayer(const std::string& project_path, EditorContext& ctx);

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate(float dt) override;
    void OnRender() override;

private:
    void RunPlayModeSystems(float dt);
    void HandleInteractKey();

    EditorApp editor_app_;
    EditorContext& ctx_;
    std::string project_path_;
    EditorContext::PlayState last_play_state_ = EditorContext::PlayState::Edit;
    std::unique_ptr<scene::EventSystem> event_system_;
    scene::AnimatorSystem animator_system_;
    scene::TweenAnimationSystem tween_animation_system_;
    bool tile_palette_was_open_ = false;
    bool project_loaded_ = false;
};

} // namespace editor
} // namespace mg
