#pragma once
#include "src/editor/panel.h"
#include "../imgui/mg_imgui_icons.h"

namespace mg {
namespace editor {

class SelectionManager;
class SceneSession;

// 属性详情面板（Inspector / Details Panel）
// 根据 EditorContext 的选区动态显示属性
class DetailsPanel : public IPanel {
public:
    DetailsPanel(SelectionManager& sel, SceneSession& session) : selection_(sel), session_(session) {}
    const char* GetName() const override { return "Scene Explorer"; }
    const char* GetIcon() const override { return ICON_FA_SLIDERS_H; }
    bool IsPopup() const override { return true; }
    void Draw() override;

private:
    SelectionManager& selection_;
    SceneSession& session_;
};

} // namespace editor
} // namespace mg
