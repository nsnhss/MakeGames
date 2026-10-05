// launcher_ui_kit.cpp — Launcher 共享 ImGui 控件助手（D5 自 launcher_window.cpp 拆出，逐位不变）。
#include "launcher_ui_kit.h"

namespace mg {
namespace launcher {

float Fh() { return ImGui::GetFrameHeight(); }
float Fs() { return ImGui::GetFontSize(); }
float Em(float n) { return Fs() * n; }

ImVec4 Accent() { return ImGui::GetStyleColorVec4(ImGuiCol_CheckMark); }

// 绘制卡片阴影（两层：近层较浓、远层较淡）
void DrawCardShadow(ImDrawList* dl, ImVec2 min, ImVec2 max, float rounding)
{
    dl->AddRectFilled(
        ImVec2(min.x + 1.0f, min.y + 2.0f),
        ImVec2(max.x + 1.0f, max.y + 3.0f),
        ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.10f)), rounding);
    dl->AddRectFilled(
        ImVec2(min.x + 2.0f, min.y + 4.0f),
        ImVec2(max.x + 2.0f, max.y + 5.0f),
        ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.05f)), rounding);
}

// ───────────────────────────────────────────────
// 页面标题头（大标题 + 副标题 + 底部分隔线）
// ───────────────────────────────────────────────
void PageHeader(const char* title, const char* subtitle) {
    ImVec4 text_col = ImGui::GetStyleColorVec4(ImGuiCol_Text);
    ImVec4 text_dis = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
    ImVec4 accent   = Accent();
    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2 pos      = ImGui::GetCursorScreenPos();
    float w         = ImGui::GetContentRegionAvail().x;

    // 标题
    dl->AddText(pos, ImGui::GetColorU32(text_col), title);
    ImGui::Dummy(ImVec2(0, Fs() * 1.35f));

    // 副标题
    dl->AddText(ImVec2(pos.x, pos.y + Fs() * 1.15f),
        ImGui::GetColorU32(text_dis), subtitle);

    // 底部强调色短线（作为视觉锚点）
    float line_y = pos.y + Fs() * 2.1f + Em(0.3f);
    dl->AddLine(
        ImVec2(pos.x, line_y),
        ImVec2(pos.x + Em(3.5f), line_y),
        ImGui::GetColorU32(accent), 2.0f);

    // 细灰线
    dl->AddLine(
        ImVec2(pos.x + Em(3.5f) + Em(0.4f), line_y),
        ImVec2(pos.x + w, line_y),
        ImGui::GetColorU32(ImVec4(text_col.x, text_col.y, text_col.z, 0.06f)), 1.0f);

    ImGui::Dummy(ImVec2(0, Em(1.8f)));
}

// 根据主题返回缩略图占位色
ImU32 ThumbPlaceholderColor() {
    ImVec4 frame = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
    float lum = frame.x * 0.299f + frame.y * 0.587f + frame.z * 0.114f;
    if (lum > 0.5f) {
        // Light 主题：比 FrameBg 稍深一点的灰色
        return ImGui::GetColorU32(ImVec4(frame.x - 0.06f, frame.y - 0.06f, frame.z - 0.06f, 1.0f));
    }
    // Dark 主题：深色
    return ImGui::GetColorU32(ImVec4(0.12f, 0.12f, 0.14f, 1.0f));
}

// ───────────────────────────────────────────────
// 导航项（左侧栏，图标+文字，选中高亮）
// ───────────────────────────────────────────────
bool NavItem(const char* icon, const char* label, bool selected) {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    float h = Fh() * 2.4f;
    float pad_x = Em(0.5f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec4 text_col = ImGui::GetStyleColorVec4(ImGuiCol_Text);
    ImVec4 text_dis = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
    ImVec4 accent = Accent();

    ImGui::Dummy(ImVec2(w, h));
    bool hov = ImGui::IsItemHovered();
    bool clk = ImGui::IsItemClicked();

    // 背景
    if (selected) {
        dl->AddRectFilled(
            ImVec2(pos.x + pad_x, pos.y + Em(0.08f)),
            ImVec2(pos.x + w - pad_x * 0.5f, pos.y + h - Em(0.08f)),
            ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.10f)),
            Em(0.35f));
        // 左侧竖条
        dl->AddRectFilled(
            ImVec2(pos.x + pad_x, pos.y + Em(0.08f)),
            ImVec2(pos.x + pad_x + Em(0.22f), pos.y + h - Em(0.08f)),
            ImGui::GetColorU32(accent), Em(0.1f));
    } else if (hov) {
        dl->AddRectFilled(
            ImVec2(pos.x + pad_x, pos.y + Em(0.08f)),
            ImVec2(pos.x + w - pad_x * 0.5f, pos.y + h - Em(0.08f)),
            ImGui::GetColorU32(ImVec4(text_col.x, text_col.y, text_col.z, 0.05f)),
            Em(0.35f));
    }

    // 图标圆形背景
    float icon_bg_sz = Fs() * 1.35f;
    float icon_bg_x = pos.x + pad_x + Em(0.55f);
    float icon_bg_y = pos.y + (h - icon_bg_sz) * 0.5f;
    if (selected) {
        dl->AddRectFilled(
            ImVec2(icon_bg_x, icon_bg_y),
            ImVec2(icon_bg_x + icon_bg_sz, icon_bg_y + icon_bg_sz),
            ImGui::GetColorU32(accent), Em(0.35f));
    } else {
        dl->AddRectFilled(
            ImVec2(icon_bg_x, icon_bg_y),
            ImVec2(icon_bg_x + icon_bg_sz, icon_bg_y + icon_bg_sz),
            ImGui::GetColorU32(ImVec4(text_col.x, text_col.y, text_col.z, 0.06f)), Em(0.35f));
    }

    // 文字颜色
    ImVec4 c = selected ? accent : text_dis;
    if (hov && !selected) c = text_col;

    // 图标（居中在圆形背景内）
    ImVec2 icon_sz = ImGui::CalcTextSize(icon);
    dl->AddText(
        ImVec2(icon_bg_x + (icon_bg_sz - icon_sz.x) * 0.5f,
               icon_bg_y + (icon_bg_sz - icon_sz.y) * 0.5f),
        ImGui::GetColorU32(selected ? ImVec4(1,1,1,1) : c), icon);

    // 标签
    dl->AddText(
        ImVec2(pos.x + pad_x + icon_bg_sz + Em(0.75f), pos.y + (h - Fs()) * 0.5f),
        ImGui::GetColorU32(c), label);

    return clk && !selected;
}

// ───────────────────────────────────────────────
// 主按钮（强调色背景，圆角）
// ───────────────────────────────────────────────
bool BtnPrimary(const char* label, const ImVec2& sz) {
    ImVec4 accent = Accent();
    ImVec4 hover  = ImVec4(accent.x + 0.05f, accent.y + 0.05f, accent.z + 0.05f, 1.0f);
    ImVec4 active = ImVec4(accent.x - 0.04f, accent.y - 0.04f, accent.z - 0.04f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(accent));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetColorU32(hover));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetColorU32(active));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,1,1,1));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, Em(0.3f));
    bool ret = ImGui::Button(label, sz);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
    return ret;
}

// ───────────────────────────────────────────────
// 次按钮（边框风格，圆角）
// ───────────────────────────────────────────────
bool BtnSecondary(const char* label, const ImVec2& sz) {
    ImVec4 bg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
    ImVec4 hover  = ImVec4(bg.x+0.04f, bg.y+0.04f, bg.z+0.04f, 1);
    ImVec4 active = ImVec4(bg.x-0.02f, bg.y-0.02f, bg.z-0.02f, 1);
    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(bg));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetColorU32(hover));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetColorU32(active));
    ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetColorU32(ImVec4(0.4f,0.4f,0.42f,0.35f)));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, Em(0.3f));
    bool ret = ImGui::Button(label, sz);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    return ret;
}

} // namespace launcher
} // namespace mg
