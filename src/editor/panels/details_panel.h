#pragma once
#include "src/editor/panel.h"
#include "../imgui/mg_imgui_icons.h"
#include "scene/entity.h"
#include "third_party/json.hpp"

namespace mg {
namespace scene { class World; }
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
    // 结算挂起的连续编辑（合并为单条 UpdateEntityCommand 推入 Undo 栈）
    void FlushPendingEdit();
    // 立即型组件变更（Add/Remove Component）：before/after 快照包一条 UpdateEntityCommand
    void PushSnapshotCommand(scene::World* world, scene::EntityID entity, nlohmann::json before);

    SelectionManager& selection_;
    SceneSession& session_;

    // Details Undo（D4b）：连续帧编辑合并为单条命令
    bool edit_pending_ = false;
    scene::World* edit_world_ = nullptr;
    scene::EntityID edit_entity_ = 0;
    nlohmann::json edit_before_;
};

} // namespace editor
} // namespace mg
