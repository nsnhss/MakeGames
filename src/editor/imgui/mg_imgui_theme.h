#pragma once
#include "imgui.h"

namespace mg {
namespace editor {

// 应用 UE5 风格暗色主题（完整配色 + 尺寸）
void MG_ApplyTheme_UE5Dark();

// 应用 UE5 风格亮色主题
void MG_ApplyTheme_UE5Light();

// 加载并合并中文字体 + FontAwesome 图标字体
// 返回主字体指针（可直接用于 ImGui::PushFont）
// scale: DPI 缩放比例
ImFont* MG_LoadFonts(float scale);

} // namespace editor
} // namespace mg
