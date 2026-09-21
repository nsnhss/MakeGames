#pragma once
#include "editor/panel.h"

namespace mg {
namespace editor {

class SelectionManager;
class SceneSession;

// Animation 面板：选中实体 TweenAnimation 组件的关键帧编辑（MVP：线性插值、4 轨道）
class AnimationPanel : public IPanel {
public:
    AnimationPanel(SelectionManager& selection, SceneSession& session);
    const char* GetName() const override { return "Animation"; }
    const char* GetIcon() const override;
    void Draw() override;

private:
    SelectionManager& selection_;
    SceneSession& session_;
};

} // namespace editor
} // namespace mg
