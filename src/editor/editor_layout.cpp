// editor_layout.cpp
// EditorLayout — 编辑器布局管理器实现。
// 负责构建默认 ImGui DockSpace 布局（左侧 Outliner、中间 Viewport、底部 Console），
// 并支持保存/加载用户自定义布局配置。
#include "editor_layout.h"
#include "imgui_internal.h"

namespace mg {
namespace editor {

// 预留：可在此加载用户保存的自定义布局配置
void EditorLayout::Initialize() { /* reserved */ }

// 构建默认编辑器布局：左侧 Outliner，中间 Viewport，底部 Console
// 只在 DockBuilderGetNode 返回 nullptr 时执行一次（首次启动或重置布局）
void EditorLayout::BuildDefaultLayout(ImGuiID dockspace_id, const ImVec2& size) {
    if (ImGui::DockBuilderGetNode(dockspace_id) != nullptr)
        return;

    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, size);

    ImGuiID dock_main_id = dockspace_id;
    ImGuiID dock_left_id, dock_bottom_id;

    // 左侧 22%
    ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.22f, &dock_left_id, &dock_main_id);
    // 底部 25%（Console 区域，可隐藏）
    ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.25f, &dock_bottom_id, &dock_main_id);

    // 停靠窗口
    ImGui::DockBuilderDockWindow("Viewport",         dock_main_id);
    ImGui::DockBuilderDockWindow("Scene Outliner",   dock_left_id);
    ImGui::DockBuilderDockWindow("Console",          dock_bottom_id);

    ImGui::DockBuilderFinish(dockspace_id);
}

} // namespace editor
} // namespace mg
