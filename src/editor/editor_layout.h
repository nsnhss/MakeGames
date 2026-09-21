#pragma once
#include "imgui.h"

namespace mg {
namespace editor {

// Editor DockSpace 默认布局管理器
// 负责首次启动时构建 UE5/Godot 风格的面板停靠布局
class EditorLayout {
public:
    void Initialize();
    void BuildDefaultLayout(ImGuiID dockspace_id, const ImVec2& size);
};

} // namespace editor
} // namespace mg
