// launcher_misc_pages.cpp — 日志页（只读+编辑）+ 设置页。D5 自 launcher_window.cpp 拆出，逐位不变。
#include "launcher_window.h"
#include "launcher_ui_kit.h"
#include "launcher_path_utils.h"
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "src/gameui/mg_imgui_theme.h"
#include "src/editor/imgui/mg_imgui_icons.h"

#include <cfloat>

namespace mg {
namespace launcher {

// ───────────────────────────────────────────────
// 日志页（只读 + 编辑模式）
// ───────────────────────────────────────────────
void LauncherWindow::DrawDevLogPage() {
    PageHeader("DEV LOG", "记录每日开发进度");

    if (!log_panel_.IsEditing()) {
        // ========== 只读模式 ==========
        const std::string& text = log_panel_.GetText();
        float btn_area_h = Fh() * 2.8f;
        ImGui::BeginChild("log_history", ImVec2(0, -btn_area_h), true);
        if (!text.empty()) {
            ImGui::TextWrapped("%s", text.c_str());
        } else {
            ImGui::TextDisabled("暂无内容，点击右下角按钮开始记录");
        }
        if (!text.empty() && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();

        ImGui::Dummy(ImVec2(0, Em(0.5f)));

        // 底部只有修改按钮
        float space = ImGui::GetContentRegionAvail().x;
        float btn_w = Em(10.0f);
        if (space > btn_w) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + space - btn_w);
        if (BtnSecondary(ICON_FA_CLIPBOARD " 修改日志", ImVec2(btn_w, Fh() * 1.1f))) {
            log_panel_.BeginEdit();
        }
    } else {
        // ========== 编辑模式 ==========
        ImVec4 accent   = Accent();

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, Em(0.25f));
        const char* buf = log_panel_.GetEditBuffer();

        // InputTextMultiline 搭配 backend IME callback，实现中文 inline composition。
        // CallbackAlways 确保每帧都能检测 composition 状态变化（开始/更新/确认/取消）。
        ImGui::InputTextMultiline("##edit_log", (char*)buf, log_panel_.GetEditBufferSize(),
            ImVec2(-FLT_MIN, -Fh() * 3.5f), ImGuiInputTextFlags_CallbackAlways, ImGui_ImplSDL2_InputTextCallback);
        ImGui::PopStyleVar();

        // 在 InputTextMultiline 之后立即渲染拼音下划线并同步 IME 候选框位置。
        // 必须在同一 widget 渲染周期内调用（依赖 GetItemRectMin/GetItemID）。
        ImGui_ImplSDL2_RenderTextInputIME(buf, ImGui::GetColorU32(accent));

        // Ctrl+S 保存
        if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
            log_panel_.EndEdit(true);
        }

        ImGui::Dummy(ImVec2(0, Em(0.5f)));
        float btns = Em(8.0f) + Em(0.5f) + Em(8.0f);
        float space = ImGui::GetContentRegionAvail().x;
        if (space > btns) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + space - btns);

        if (BtnSecondary(ICON_FA_TIMES " 取消", ImVec2(Em(8.0f), Fh() * 1.1f))) {
            log_panel_.EndEdit(false);
        }
        ImGui::SameLine();
        if (BtnPrimary(ICON_FA_CHECK " 完成", ImVec2(Em(8.0f), Fh() * 1.1f))) {
            log_panel_.EndEdit(true);
        }
    }
}

// ───────────────────────────────────────────────
// 设置页
// ───────────────────────────────────────────────
void LauncherWindow::DrawSettingsPage() {
    PageHeader("SETTINGS", "偏好设置");

    // 主题选择
    ImGui::AlignTextToFramePadding();
    ImGui::Text("主题:");
    ImGui::SameLine();
    ImGui::SetCursorPosX(Em(6.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, Em(0.25f));
    ImGui::PushItemWidth(Em(10.0f));
    int prev_theme = theme_index_;
    if (ImGui::Combo("##theme", &theme_index_, "暗色\0亮色\0")) {
        if (theme_index_ != prev_theme) {
            if (theme_index_ == 0) mg::gameui::MG_ApplyTheme_UE5Dark();
            else mg::gameui::MG_ApplyTheme_UE5Light();
        }
    }
    ImGui::PopItemWidth();
    ImGui::PopStyleVar();

    ImGui::Dummy(ImVec2(0, Em(1.2f)));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, Em(1.2f)));

    // 项目根目录
    ImGui::Text("项目根目录:");
    ImGui::Dummy(ImVec2(0, Em(0.4f)));
    std::string default_root = GetDefaultProjectsRoot();
    ImGui::TextDisabled("%s", default_root.c_str());
    ImGui::Dummy(ImVec2(0, Em(0.8f)));
    if (BtnSecondary("浏览...", ImVec2(Em(8.0f), Fh() * 1.1f))) {
        std::string picked = BrowseFolderDialog("选择默认项目根目录", default_root.c_str());
        if (!picked.empty()) {
            SaveDefaultProjectsRoot(picked);
        }
    }
}

} // namespace launcher
} // namespace mg
