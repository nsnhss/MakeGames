#pragma once
#include <string>

namespace mg {
namespace editor {

class PanelManager;
class EditorContext;

// Editor 顶部菜单栏（File / Edit / Window / Help）
class EditorMenuBar {
public:
    void Draw(EditorContext& ctx, PanelManager& panel_manager);
    void RenderPopups(EditorContext& ctx);  // 在 EndMenuBar 之后调用，渲染弹出对话框

    bool show_scene_properties_ = false;
    bool show_new_scene_ = false;
    bool new_scene_error_ = false;
    char new_scene_name_[64] = "NewScene";
    int new_scene_width_ = 20;
    int new_scene_height_ = 15;
    int new_scene_tile_size_ = 32;

    bool export_success_ = false;
    bool export_error_ = false;

    // SpriteSheet 导入向导
    bool show_spritesheet_import_ = false;
    char spritesheet_path_[260] = "";
    int spritesheet_frame_w_ = 32;
    int spritesheet_frame_h_ = 32;
    float spritesheet_fps_ = 4.0f;
    std::string spritesheet_msg_;
};

} // namespace editor
} // namespace mg
