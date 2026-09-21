#include "mg_imgui_extensions.h"
#include "mg_imgui_icons.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_sdl2.h"
#include <cmath>
#include <string>
#include <cstring>
#include <unordered_map>

namespace mg {
namespace editor {

// ── 内部辅助 ─────────────────────────────────────

static float Fh() { return ImGui::GetFrameHeight(); }
static float Fs() { return ImGui::GetFontSize(); }
static float Em(float n) { return Fs() * n; }

static ImU32 AccentColor() {
    return ImGui::GetColorU32(ImGuiCol_CheckMark);
}

// ── 光晕绘制辅助 ─────────────────────────────────
// 在矩形周围绘制 subtle 的外发光，用于选中/激活状态
void MG_DrawOuterGlow(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 color, float rounding, float spread)
{
    ImVec4 c = ImColor(color).Value;
    if (c.w <= 0.01f) return;
    // 分两层：外层更淡更宽，内层更浓更窄
    float layers[][2] = { {spread * 1.2f, 0.08f}, {spread * 0.6f, 0.18f} };
    for (auto& layer : layers) {
        float offset = layer[0];
        float alpha = layer[1];
        ImU32 glow = ImGui::GetColorU32(ImVec4(c.x, c.y, c.z, c.w * alpha));
        dl->AddRectFilled(
            ImVec2(min.x - offset, min.y - offset),
            ImVec2(max.x + offset, max.y + offset),
            glow, rounding + offset * 0.5f);
    }
}

// ── 颜色过渡动画系统 ─────────────────────────────
// ImGui 默认 hover/active 是瞬间切换，这里用每帧插值实现平滑过渡

struct ColorTransitionState {
    ImVec4 current;
    bool initialized = false;
};

static std::unordered_map<ImGuiID, ColorTransitionState> s_color_transitions;

static ImVec4 MG_LerpColor(const ImVec4& a, const ImVec4& b, float t)
{
    return ImVec4(
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
        a.w + (b.w - a.w) * t
    );
}

// 根据目标颜色和速度，返回当前帧的过渡颜色
// speed: 每秒变化的比率（例如 8.0f 表示约 125ms 完成过渡）
ImVec4 MG_GetAnimatedColor(ImGuiID id, const ImVec4& target, float speed)
{
    auto& state = s_color_transitions[id];
    if (!state.initialized) {
        state.current = target;
        state.initialized = true;
        return target;
    }
    float dt = ImGui::GetIO().DeltaTime;
    float t = 1.0f - std::exp(-speed * dt); // 指数平滑，更自然
    state.current = MG_LerpColor(state.current, target, t);
    return state.current;
}

// Asset Card 网格布局状态
static int s_asset_card_columns = 0;
static float s_asset_card_size = 72.0f;
static float s_asset_card_spacing = 8.0f;
static int s_asset_card_idx = 0;

// 在 Property 行中统一处理标签宽度和对齐
static void PropertyRowBegin(const char* label, float label_width_ratio)
{
    ImGui::PushID(label);
    float avail_w = ImGui::GetContentRegionAvail().x;
    float label_w = avail_w * label_width_ratio;
    float line_h = Fh();

    ImGui::AlignTextToFramePadding();
    ImVec2 label_pos = ImGui::GetCursorScreenPos();
    ImGui::TextDisabled("%s", label);

    // 绘制标签右对齐的虚线引导（UE5 风格）
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 text_size = ImGui::CalcTextSize(label);
    float dot_start = label_pos.x + text_size.x + Em(0.3f);
    float dot_end = label_pos.x + label_w - Em(0.3f);
    if (dot_end > dot_start + 4.0f) {
        ImU32 dot_col = ImGui::GetColorU32(ImGuiCol_TextDisabled, 0.25f);
        for (float x = dot_start; x < dot_end; x += 4.0f) {
            dl->AddRectFilled(ImVec2(x, label_pos.y + line_h * 0.5f - 0.5f),
                              ImVec2(x + 2.0f, label_pos.y + line_h * 0.5f + 0.5f), dot_col);
        }
    }

    ImGui::SameLine();
    ImGui::SetCursorPosX(label_w);
    ImGui::PushItemWidth(avail_w - label_w);
}

static void PropertyRowEnd()
{
    ImGui::PopItemWidth();
    ImGui::PopID();
}

// ── Property Grid 实现 ───────────────────────────

bool MG_PropertyLabel(const char* label, float label_width_ratio)
{
    float avail_w = ImGui::GetContentRegionAvail().x;
    float label_w = avail_w * label_width_ratio;
    ImGui::AlignTextToFramePadding();
    ImVec2 label_pos = ImGui::GetCursorScreenPos();
    ImGui::TextDisabled("%s", label);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 text_size = ImGui::CalcTextSize(label);
    float line_h = Fh();
    float dot_start = label_pos.x + text_size.x + Em(0.3f);
    float dot_end = label_pos.x + label_w - Em(0.3f);
    if (dot_end > dot_start + 4.0f) {
        ImU32 dot_col = ImGui::GetColorU32(ImGuiCol_TextDisabled, 0.25f);
        for (float x = dot_start; x < dot_end; x += 4.0f) {
            dl->AddRectFilled(ImVec2(x, label_pos.y + line_h * 0.5f - 0.5f),
                              ImVec2(x + 2.0f, label_pos.y + line_h * 0.5f + 0.5f), dot_col);
        }
    }
    return false;
}

bool MG_PropertyInputText(const char* label, char* buf, size_t buf_size, float label_width_ratio)
{
    PropertyRowBegin(label, label_width_ratio);
    bool changed = ImGui::InputText("##v", buf, buf_size,
        ImGuiInputTextFlags_CallbackAlways,
        ImGui_ImplSDL2_InputTextCallback);
    ImGui_ImplSDL2_RenderTextInputIME(buf, ImGui::GetColorU32(ImGuiCol_CheckMark));
    PropertyRowEnd();
    return changed;
}

bool MG_PropertyInputFloat(const char* label, float* v, float step, const char* fmt, float label_width_ratio)
{
    PropertyRowBegin(label, label_width_ratio);
    bool changed = ImGui::InputFloat("##v", v, step, 0.0f, fmt);
    PropertyRowEnd();
    return changed;
}

bool MG_PropertyInputFloat2(const char* label, float v[2], const char* fmt, float label_width_ratio)
{
    PropertyRowBegin(label, label_width_ratio);
    bool changed = ImGui::InputFloat2("##v", v, fmt);
    PropertyRowEnd();
    return changed;
}

bool MG_PropertyInputFloat3(const char* label, float v[3], const char* fmt, float label_width_ratio)
{
    PropertyRowBegin(label, label_width_ratio);
    bool changed = ImGui::InputFloat3("##v", v, fmt);
    PropertyRowEnd();
    return changed;
}

bool MG_PropertyInputFloat4(const char* label, float v[4], const char* fmt, float label_width_ratio)
{
    PropertyRowBegin(label, label_width_ratio);
    bool changed = ImGui::InputFloat4("##v", v, fmt);
    PropertyRowEnd();
    return changed;
}

bool MG_PropertyInputInt(const char* label, int* v, int step, float label_width_ratio)
{
    PropertyRowBegin(label, label_width_ratio);
    bool changed = ImGui::InputInt("##v", v, step);
    PropertyRowEnd();
    return changed;
}

bool MG_PropertyColorEdit3(const char* label, float col[3], float label_width_ratio)
{
    PropertyRowBegin(label, label_width_ratio);
    bool changed = ImGui::ColorEdit3("##v", col, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoSidePreview);
    PropertyRowEnd();
    return changed;
}

bool MG_PropertyColorEdit4(const char* label, float col[4], float label_width_ratio)
{
    PropertyRowBegin(label, label_width_ratio);
    bool changed = ImGui::ColorEdit4("##v", col, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoSidePreview);
    PropertyRowEnd();
    return changed;
}

bool MG_PropertyCheckbox(const char* label, bool* v, float label_width_ratio)
{
    PropertyRowBegin(label, label_width_ratio);
    bool changed = ImGui::Checkbox("##v", v);
    PropertyRowEnd();
    return changed;
}

bool MG_PropertySliderFloat(const char* label, float* v, float v_min, float v_max, const char* fmt, float label_width_ratio)
{
    PropertyRowBegin(label, label_width_ratio);
    bool changed = ImGui::SliderFloat("##v", v, v_min, v_max, fmt);
    PropertyRowEnd();
    return changed;
}

bool MG_PropertyCombo(const char* label, int* current_item, const char* const items[], int items_count, float label_width_ratio)
{
    PropertyRowBegin(label, label_width_ratio);
    bool changed = ImGui::Combo("##v", current_item, items, items_count);
    PropertyRowEnd();
    return changed;
}

// ── Asset Card 实现 ──────────────────────────────

bool MG_AssetCard(const char* name, const char* icon, bool selected, float size)
{
    // 网格布局：处理 SameLine
    if (s_asset_card_columns > 0 && s_asset_card_idx > 0 && (s_asset_card_idx % s_asset_card_columns) != 0) {
        ImGui::SameLine(0, s_asset_card_spacing);
    }
    if (s_asset_card_columns > 0) s_asset_card_idx++;

    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = size;
    float h = size + Fs() * 1.8f; // 图标区 + 文字区
    ImVec2 min = cursor;
    ImVec2 max = ImVec2(cursor.x + w, cursor.y + h);

    // 计算 hover 状态用于颜色过渡
    ImGuiID id = ImGui::GetID(name);
    ImGui::SetCursorScreenPos(min);
    ImGui::InvisibleButton(name, ImVec2(w, h));
    bool hovered = ImGui::IsItemHovered();
    bool held = ImGui::IsItemActive();

    // 目标颜色
    ImVec4 bg_target = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
    ImVec4 border_target = ImGui::GetStyleColorVec4(ImGuiCol_Border);
    if (selected) {
        bg_target = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
        border_target = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
    } else if (held) {
        bg_target = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
    } else if (hovered) {
        bg_target = ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered);
        border_target = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
    }

    // 颜色过渡
    ImVec4 bg_col = MG_GetAnimatedColor(id, bg_target, 12.0f);
    ImVec4 border_col = MG_GetAnimatedColor(ImGui::GetID((std::string(name) + "_border").c_str()), border_target, 12.0f);

    // 外发光（选中或悬停时）
    if (selected || hovered) {
        ImU32 glow_col = ImGui::GetColorU32(border_col);
        MG_DrawOuterGlow(dl, min, max, glow_col, Em(0.2f), 3.0f);
    }

    // 背景
    dl->AddRectFilled(min, max, ImGui::GetColorU32(bg_col), Em(0.2f));
    // 边框
    float border_thickness = selected ? 2.0f : (hovered ? 1.5f : 1.0f);
    dl->AddRect(min, max, ImGui::GetColorU32(border_col), Em(0.2f), 0, border_thickness);

    // 图标（居中）
    ImVec2 icon_sz = ImGui::CalcTextSize(icon);
    float icon_y = cursor.y + (size - icon_sz.y) * 0.5f;
    dl->AddText(ImVec2(cursor.x + (w - icon_sz.x) * 0.5f, icon_y),
                ImGui::GetColorU32(ImGuiCol_TextDisabled), icon);

    // 文件名（底部，居中，截断）
    float text_y = cursor.y + size + Em(0.15f);
    ImVec2 name_sz = ImGui::CalcTextSize(name);
    float max_text_w = w - Em(0.4f);
    if (name_sz.x > max_text_w) {
        int len = (int)std::strlen(name);
        std::string display = name;
        while (len > 3 && ImGui::CalcTextSize((display + "...").c_str()).x > max_text_w) {
            display.pop_back();
            len--;
        }
        display += "...";
        name_sz = ImGui::CalcTextSize(display.c_str());
        dl->AddText(ImVec2(cursor.x + (w - name_sz.x) * 0.5f, text_y),
                    ImGui::GetColorU32(ImGuiCol_Text), display.c_str());
    } else {
        dl->AddText(ImVec2(cursor.x + (w - name_sz.x) * 0.5f, text_y),
                    ImGui::GetColorU32(ImGuiCol_Text), name);
    }

    return ImGui::IsItemClicked();
}

void MG_AssetCardGridBegin(int columns, float card_size, float spacing)
{
    s_asset_card_columns = columns;
    s_asset_card_size = card_size;
    s_asset_card_spacing = spacing;
    s_asset_card_idx = 0;
}

void MG_AssetCardGridEnd()
{
    s_asset_card_columns = 0;
}

// ── Toolbar 按钮 ─────────────────────────────────

bool MG_ToolbarButton(const char* icon, const char* tooltip, bool active, ImVec2 size)
{
    if (size.y <= 0) {
        size.y = Fh() * 1.35f;
    }
    if (size.x == 0) {
        // 文字按钮：自动宽度
        float text_w = ImGui::CalcTextSize(icon).x;
        size.x = text_w + ImGui::GetStyle().FramePadding.x * 2.0f;
    } else if (size.x < 0) {
        // 负值/未指定：正方形（图标按钮）
        size.x = size.y;
    }

    ImGuiID id = ImGui::GetID(icon);
    ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
    ImVec4 btn_col = ImGui::GetStyleColorVec4(ImGuiCol_Button);
    ImVec4 btn_hov = ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered);
    ImVec4 btn_act = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);

    ImVec4 target = btn_col;
    if (active) target = accent;

    // 先推入过渡后的背景色，再让 Button 处理 hover/active
    ImVec4 animated = MG_GetAnimatedColor(id, target, 10.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(animated));

    bool clicked = ImGui::Button(icon, size);
    bool hovered = ImGui::IsItemHovered();
    bool held = ImGui::IsItemActive();

    // 激活状态外发光
    if (active) {
        ImVec2 btn_min = ImGui::GetItemRectMin();
        ImVec2 btn_max = ImGui::GetItemRectMax();
        MG_DrawOuterGlow(ImGui::GetWindowDrawList(), btn_min, btn_max,
                         ImGui::GetColorU32(accent), ImGui::GetStyle().FrameRounding, 2.5f);
    }

    // 更新目标颜色（Button 内部可能已处理 hover/active，这里覆盖以保证过渡）
    if (active) {
        target = accent;
    } else if (held) {
        target = btn_act;
    } else if (hovered) {
        target = btn_hov;
    } else {
        target = btn_col;
    }
    // 重新触发动画（用同一 id 更新目标）
    MG_GetAnimatedColor(id, target, 10.0f);

    ImGui::PopStyleColor();

    if (tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::SetTooltip("%s", tooltip);
    }
    return clicked;
}

void MG_ToolbarSeparator()
{
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(Em(0.8f), 0));
    ImGui::SameLine();
}

// ── 带图标的 TreeNode ────────────────────────────

bool MG_TreeNodeEx(const char* label, const char* icon, ImGuiTreeNodeFlags flags)
{
    std::string display = std::string(icon) + "  " + label;
    return ImGui::TreeNodeEx(display.c_str(), flags);
}

bool MG_TreeNodeExLeaf(const char* label, const char* icon, bool selected)
{
    std::string display = std::string(icon) + "  " + label;
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (selected) flags |= ImGuiTreeNodeFlags_Selected;
    bool ret = ImGui::TreeNodeEx(display.c_str(), flags);

    // 选中时左侧 accent 竖线
    if (selected) {
        ImVec2 p = ImGui::GetItemRectMin();
        ImVec2 sz = ImGui::GetItemRectSize();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddLine(ImVec2(p.x, p.y + 1), ImVec2(p.x, p.y + sz.y - 1),
            ImGui::GetColorU32(ImGuiCol_CheckMark), 2.0f);
    }
    return ret;
}

// ── 分隔线增强 ───────────────────────────────────

void MG_SeparatorTextEx(const char* label, ImU32 color)
{
    if (color == 0) color = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::SeparatorText(label);
    ImGui::PopStyleColor();
}

// ── Section Header（圆点 + 标题 + 延伸细线）────────────────

void MG_SectionHeader(const char* label)
{
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    float y = p.y + ImGui::GetFrameHeight() * 0.5f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 accent = ImGui::GetColorU32(ImGuiCol_CheckMark);
    ImU32 line_col = ImGui::GetColorU32(ImGuiCol_Separator);

    // 左侧小圆点
    dl->AddCircleFilled(ImVec2(p.x + 4, y), 3.0f, accent);
    // 标题
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", label);
    ImGui::SameLine();
    // 右侧细线延伸到面板边缘
    float line_x = ImGui::GetCursorScreenPos().x;
    dl->AddLine(ImVec2(line_x, y), ImVec2(p.x + w, y), line_col, 1.0f);
    ImGui::Dummy(ImVec2(0, 4));
}

// ── 状态栏 ───────────────────────────────────────

void MG_StatusBarBegin(float height)
{
    if (height <= 0) height = Fh() * 1.4f;
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Em(0.6f), Em(0.3f)));
    ImGui::BeginChild("StatusBar", ImVec2(0, height), false, flags);
}

void MG_StatusBarEnd()
{
    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void MG_StatusBarText(const char* text, ImU32 color)
{
    if (color == 0) color = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    ImGui::TextColored(ImColor(color).Value, "%s", text);
    ImGui::SameLine();
}

} // namespace editor
} // namespace mg
