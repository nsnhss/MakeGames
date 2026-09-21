#pragma once
#include "src/editor/panel.h"

namespace mg {
namespace editor {

class SelectionManager;
class SceneSession;
class EditorProjectManager;

// 事件编辑器面板：可视化编辑 EventTrigger 组件的触发条件和动作序列
class EventEditorPanel : public IPanel {
public:
    EventEditorPanel(SelectionManager& sel, SceneSession& session, EditorProjectManager& project)
        : selection_(sel), session_(session), project_(project) {}
    const char* GetName() const override { return "Event Editor"; }
    const char* GetIcon() const override;
    bool IsPopup() const override { return true; }
    void Draw() override;

private:
    SelectionManager& selection_;
    SceneSession& session_;
    EditorProjectManager& project_;
    int active_page_ = 0;
};

} // namespace editor
} // namespace mg
