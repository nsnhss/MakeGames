// launcher_window.cpp
// LauncherWindow — 启动器主窗口实现（框架部分）。
// 负责项目管理（创建/打开/导入）、最近项目列表展示、日志面板，
// 以及启动 Editor 进行游戏编辑或运行已发布游戏。
// 使用 ImGui 绘制 UI，IMGUI_DOCK 布局模式（顶部工具栏 + 左侧导航 + 右侧内容）。
// D5 拆分：共享控件 → launcher_ui_kit.*，路径助手 → launcher_path_utils.*，
//          页面/对话框 → launcher_home_page.cpp / launcher_projects_page.cpp /
//          launcher_misc_pages.cpp / launcher_dialogs.cpp。
#include "launcher_window.h"
#include "launcher_ui_kit.h"
#include "pending_editor.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "src/editor/imgui/mg_imgui_icons.h"

namespace mg {
namespace launcher {

void LauncherWindow::OpenProject(const std::string& project_path) {
    project_manager_.LoadProjectFromDisk(project_path);
    SetPendingEditorPath(project_path);   // 延迟到窗口关闭后再 spawn（见 main.cpp）
    should_quit_ = true;
}

void LauncherWindow::Initialize() {
    project_manager_.LoadProjects();
    project_manager_.ValidateProjects();
    log_panel_.Load();
}
void LauncherWindow::Shutdown() {
    // 兜底落盘：项目列表的 last_opened 等字段可能在会话中被内存修改
    project_manager_.SaveProjects();
}

void LauncherWindow::Draw() {
    ImVec2 ws = ImGui::GetIO().DisplaySize;
    float topbar_h = Fh() * 2.6f;
    float nav_w = Em(14.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::SetNextWindowPos(ImVec2(0,0));
    ImGui::SetNextWindowSize(ws);
    ImGui::Begin("LauncherRoot", nullptr,
        ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|
        ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoNavFocus);
    ImGui::PopStyleVar(2);

    // 顶部标题栏
    DrawTopBar(topbar_h);

    // 下方区域：左侧导航 + 右侧内容
    ImGui::SetCursorPos(ImVec2(0, topbar_h));

    // 左侧导航（无边框，背景在 DrawNav 里用 DrawList 画，避免 PushStyleColor 干扰主题）
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, Em(0.8f)));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, Em(0.1f)));
    ImGui::BeginChild("Nav", ImVec2(nav_w, ws.y - topbar_h), false);
    DrawNav();
    ImGui::EndChild();
    ImGui::PopStyleVar(2);

    ImGui::SameLine();

    // 右侧内容（带边框，直接用全局 ChildBg，不参与 PushStyleColor）
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Em(2.2f), Em(1.8f)));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, Em(0.5f)));
    ImGui::BeginChild("Content", ImVec2(0, ws.y - topbar_h), true, ImGuiWindowFlags_None);
    DrawContent();
    ImGui::EndChild();
    ImGui::PopStyleVar(2);

    ImGui::End();
}

// ───────────────────────────────────────────────
// 顶部标题栏
// ───────────────────────────────────────────────
void LauncherWindow::DrawTopBar(float height) {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec4 text_col = ImGui::GetStyleColorVec4(ImGuiCol_Text);
    ImVec4 accent = Accent();

    // 背景（最深的板块，动态计算，兼容暗/亮主题）
    ImVec4 win_bg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
    ImVec4 bg = ImVec4(ImMax(win_bg.x - 0.05f, 0.0f), ImMax(win_bg.y - 0.05f, 0.0f), ImMax(win_bg.z - 0.03f, 0.0f), 1.0f);
    dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + height), ImGui::GetColorU32(bg));

    // 底部分割线（更细、更柔和）
    dl->AddLine(ImVec2(pos.x, pos.y + height), ImVec2(pos.x + w, pos.y + height),
        ImGui::GetColorU32(ImVec4(text_col.x, text_col.y, text_col.z, 0.08f)));

    // 左侧品牌标记 + 标题
    float logo_sz = Fs() * 0.75f;
    float logo_x = pos.x + Em(1.0f);
    float logo_y = pos.y + (height - logo_sz) * 0.5f;
    dl->AddRectFilled(
        ImVec2(logo_x, logo_y),
        ImVec2(logo_x + logo_sz, logo_y + logo_sz),
        ImGui::GetColorU32(accent), Em(0.15f));
    dl->AddText(ImVec2(logo_x + logo_sz + Em(0.55f), pos.y + (height - Fs()) * 0.45f),
        ImGui::GetColorU32(text_col), "MakeGames Launcher");

    // 右侧设置按钮（可点击，跳转设置页）
    float btn_sz = Fh() * 1.5f;
    float btn_x = pos.x + w - btn_sz - Em(0.4f);
    float btn_y = pos.y + (height - btn_sz) * 0.5f;
    ImGui::SetCursorScreenPos(ImVec2(btn_x, btn_y));
    ImGui::PushStyleColor(ImGuiCol_Button, 0);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
        ImGui::GetColorU32(ImVec4(text_col.x, text_col.y, text_col.z, 0.08f)));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
        ImGui::GetColorU32(ImVec4(text_col.x, text_col.y, text_col.z, 0.14f)));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, Em(0.25f));
    if (ImGui::Button(ICON_FA_COG, ImVec2(btn_sz, btn_sz))) {
        current_tab_ = LauncherTab::Settings;
        show_create_dialog_ = false;
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);

    ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + height));
    ImGui::Dummy(ImVec2(0, 0));
}

// ───────────────────────────────────────────────
// 左侧导航
// ───────────────────────────────────────────────
void LauncherWindow::DrawNav() {
    // 用 DrawList 画覆盖整个 Nav 区域的深色背景（不用 PushStyleColor，避免 pitfall #5）
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 win_min = ImGui::GetWindowPos();
    ImVec2 win_sz = ImGui::GetWindowSize();
    ImVec2 win_max = ImVec2(win_min.x + win_sz.x, win_min.y + win_sz.y);
    ImVec4 win_bg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
    ImVec4 nav_bg = ImVec4(win_bg.x - 0.03f, win_bg.y - 0.03f, win_bg.z - 0.03f, 1.0f);
    dl->AddRectFilled(win_min, win_max, ImGui::GetColorU32(nav_bg));

    auto pos = ImGui::GetCursorScreenPos();
    ImVec4 text_dis = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
    dl->AddText(ImVec2(pos.x + Em(0.9f), pos.y),
        ImGui::GetColorU32(text_dis), "Menu");
    ImGui::Dummy(ImVec2(0, Em(1.2f)));

    struct { const char* icon; const char* label; LauncherTab t; } items[] = {
        {ICON_FA_HOME,   "主页", LauncherTab::Home},
        {ICON_FA_FOLDER, "项目", LauncherTab::Projects},
        {ICON_FA_BOOK,   "日志", LauncherTab::DevLog},
        {ICON_FA_COG,    "设置", LauncherTab::Settings}
    };
    for (auto& it : items) {
        if (NavItem(it.icon, it.label, current_tab_ == it.t)) {
            current_tab_ = it.t;
            show_create_dialog_ = false;
        }
    }
}

void LauncherWindow::DrawContent() {
    if (show_create_dialog_) {
        DrawNewProjectWizard();
        return;
    }
    switch (current_tab_) {
        case LauncherTab::Home:     DrawHomePage();     break;
        case LauncherTab::Projects: DrawProjectsPage(); break;
        case LauncherTab::DevLog:   DrawDevLogPage();   break;
        case LauncherTab::Settings: DrawSettingsPage(); break;
    }
    DrawRelocateDialog();
}

} // namespace launcher
} // namespace mg
