#pragma once
// launcher_ui_kit.h — Launcher 共享 ImGui 控件助手（D5 自 launcher_window.cpp 拆出）。
// 全部基于当前 ImGui 样式动态取色，兼容暗/亮主题。
#include "imgui.h"

namespace mg {
namespace launcher {

float Fh();                  // 当前 FrameHeight
float Fs();                  // 当前 FontSize
float Em(float n);           // n 倍 FontSize（间距单位）
ImVec4 Accent();             // 强调色（= CheckMark 色）

// 绘制卡片阴影（两层：近层较浓、远层较淡）
void DrawCardShadow(ImDrawList* dl, ImVec2 min, ImVec2 max, float rounding);

// 页面标题头（大标题 + 副标题 + 底部分隔线）
void PageHeader(const char* title, const char* subtitle);

// 根据主题返回缩略图占位色
ImU32 ThumbPlaceholderColor();

// 导航项（左侧栏，图标+文字，选中高亮）；返回 true = 被点击且未选中
bool NavItem(const char* icon, const char* label, bool selected);

// 主按钮（强调色背景，圆角）
bool BtnPrimary(const char* label, const ImVec2& sz = ImVec2(0, 0));

// 次按钮（边框风格，圆角）
bool BtnSecondary(const char* label, const ImVec2& sz = ImVec2(0, 0));

} // namespace launcher
} // namespace mg
