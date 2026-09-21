#pragma once
#include "imgui.h"

namespace mg {
namespace editor {

// ── Property Grid（属性网格）─────────────────────
// 标签在左、控件在右的 UE5 风格属性行

bool MG_PropertyLabel(const char* label, float label_width_ratio = 0.35f);
bool MG_PropertyInputText(const char* label, char* buf, size_t buf_size, float label_width_ratio = 0.35f);
bool MG_PropertyInputFloat(const char* label, float* v, float step = 0.1f, const char* fmt = "%.3f", float label_width_ratio = 0.35f);
bool MG_PropertyInputFloat2(const char* label, float v[2], const char* fmt = "%.3f", float label_width_ratio = 0.35f);
bool MG_PropertyInputFloat3(const char* label, float v[3], const char* fmt = "%.3f", float label_width_ratio = 0.35f);
bool MG_PropertyInputFloat4(const char* label, float v[4], const char* fmt = "%.3f", float label_width_ratio = 0.35f);
bool MG_PropertyInputInt(const char* label, int* v, int step = 1, float label_width_ratio = 0.35f);
bool MG_PropertyColorEdit3(const char* label, float col[3], float label_width_ratio = 0.35f);
bool MG_PropertyColorEdit4(const char* label, float col[4], float label_width_ratio = 0.35f);
bool MG_PropertyCheckbox(const char* label, bool* v, float label_width_ratio = 0.35f);
bool MG_PropertySliderFloat(const char* label, float* v, float v_min, float v_max, const char* fmt = "%.3f", float label_width_ratio = 0.35f);
bool MG_PropertyCombo(const char* label, int* current_item, const char* const items[], int items_count, float label_width_ratio = 0.35f);

// ── Asset Card（资源卡片）────────────────────────
// Content Browser 用的资源卡片

bool MG_AssetCard(const char* name, const char* icon, bool selected, float size = 72.0f);
void MG_AssetCardGridBegin(int columns, float card_size = 72.0f, float spacing = 8.0f);
void MG_AssetCardGridEnd();

// ── Toolbar 按钮 ─────────────────────────────────

bool MG_ToolbarButton(const char* icon, const char* tooltip, bool active = false, ImVec2 size = ImVec2(0, 0));
void MG_ToolbarSeparator();

// ── 带图标的 TreeNode ────────────────────────────

bool MG_TreeNodeEx(const char* label, const char* icon, ImGuiTreeNodeFlags flags = 0);
bool MG_TreeNodeExLeaf(const char* label, const char* icon, bool selected = false);

// ── 分隔线增强 ───────────────────────────────────

void MG_SeparatorTextEx(const char* label, ImU32 color = 0);

// ── Section Header（圆点 + 标题 + 延伸细线）────────────────

void MG_SectionHeader(const char* label);

// ── 状态栏 ───────────────────────────────────────

void MG_StatusBarBegin(float height = 0.0f);
void MG_StatusBarEnd();
void MG_StatusBarText(const char* text, ImU32 color = 0);

// ── 颜色过渡与光晕（供 editor / launcher 共用）────

// 返回从当前值向 target 平滑过渡后的颜色（指数平滑，speed 越大过渡越快）
ImVec4 MG_GetAnimatedColor(ImGuiID id, const ImVec4& target, float speed = 10.0f);

// 在矩形外侧绘制 subtle 外发光（两层半透明渐变）
void MG_DrawOuterGlow(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 color, float rounding, float spread = 4.0f);

} // namespace editor
} // namespace mg
