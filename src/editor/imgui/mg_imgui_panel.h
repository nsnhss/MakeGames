#pragma once
#include "imgui.h"

namespace mg {
namespace editor {

// Panel Chrome 容器：替代 ImGui::Begin，为面板添加自定义 header bar
// 内部仍调用 ImGui::Begin，docking/拖拽/折叠行为完全不变
// title:       面板标题（如 "Scene Outliner"）
// icon:        FontAwesome 图标（如 ICON_FA_LIST）
// p_open:      关闭按钮状态指针（传 nullptr 则不显示关闭按钮）
// extra_flags: 额外的 ImGuiWindowFlags
// 返回 true 表示面板打开且可见（同 ImGui::Begin）
bool MG_BeginPanel(const char* title, const char* icon, bool* p_open = nullptr,
                   ImGuiWindowFlags extra_flags = 0);
void MG_EndPanel();

} // namespace editor
} // namespace mg
