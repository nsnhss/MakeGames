#pragma once
#include "src/editor/panel.h"
#include "scene/entity.h"
#include <array>
#include <string>

namespace mg {
namespace editor {

class SelectionManager;
class SceneSession;
class EditorProjectManager;
class CommandHistory;
class PanelManager;
class PrefabManager;
class ViewportState;

// 场景大纲面板（Scene Outliner / World Outliner）
// 显示场景对象的层级树，支持搜索过滤和选中同步
class OutlinerPanel : public IPanel {
public:
    OutlinerPanel(SelectionManager& sel, SceneSession& session, EditorProjectManager& project,
                  CommandHistory& commands, PanelManager* pm,
                  PrefabManager& prefabs, ViewportState& viewport)
        : selection_(sel), session_(session), project_(project), commands_(commands),
          panel_manager_(pm), prefabs_(prefabs), viewport_(viewport) {}
    const char* GetName() const override { return "Scene Outliner"; }
    const char* GetIcon() const override;
    void Draw() override;

private:
    SelectionManager& selection_;
    SceneSession& session_;
    EditorProjectManager& project_;
    CommandHistory& commands_;
    PanelManager* panel_manager_;
    PrefabManager& prefabs_;
    ViewportState& viewport_;
    char search_buffer_[128] = {};

    // 场景管理弹窗状态
    bool show_scene_rename_ = false;
    bool show_scene_delete_confirm_ = false;
    std::string scene_rename_old_;
    std::array<char, 128> scene_rename_new_{};
    std::string scene_delete_target_;
    std::string scene_context_target_;
    bool open_scene_context_ = false;
    char new_scene_from_outliner_[128] = {};
    int new_scene_width_ = 20;
    int new_scene_height_ = 15;
    int new_scene_tile_size_ = 32;

    // Save as Prefab 弹窗状态
    bool show_prefab_save_ = false;
    scene::EntityID prefab_source_ = 0;
    char prefab_name_[64] = {};
};

} // namespace editor
} // namespace mg
