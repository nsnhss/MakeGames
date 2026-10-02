// launcher_window.cpp
// LauncherWindow — 启动器主窗口实现。
// 负责项目管理（创建/打开/导入）、最近项目列表展示、日志面板，
// 以及启动 Editor 进行游戏编辑或运行已发布游戏。
// 使用 ImGui 绘制 UI，IMGUI_DOCK 布局模式（顶部工具栏 + 左侧导航 + 右侧内容）。
#include "launcher_window.h"
#include "pending_editor.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_sdl2.h"
#include "src/gameui/mg_imgui_theme.h"
#include "src/editor/imgui/mg_imgui_extensions.h"

#include <filesystem>
#include <fstream>
#include <cstring>
#include <cctype>
#include <vector>
#include <algorithm>

#include "src/editor/imgui/mg_imgui_icons.h"
#include "third_party/json.hpp"

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <imm.h>
#endif

// ── 平台辅助函数 ──────────────────────────────────

static std::string GetDefaultProjectsRoot() {
    // 先尝试读取 launcher/settings.json 中的自定义根目录
    if (std::filesystem::exists("launcher/settings.json")) {
        try {
            std::ifstream f("launcher/settings.json");
            nlohmann::json j;
            f >> j;
            if (j.contains("default_projects_root")) {
                std::string root = j["default_projects_root"];
                if (!root.empty()) return root;
            }
        } catch (...) {}
    }
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_PERSONAL, nullptr, 0, path))) {
        std::string result = std::string(path) + "\\MakeGames Projects";
        std::replace(result.begin(), result.end(), '\\', '/');
        return result;
    }
#endif
    // 兜底：当前目录
    return "projects";
}

static void SaveDefaultProjectsRoot(const std::string& path) {
    nlohmann::json j;
    if (std::filesystem::exists("launcher/settings.json")) {
        try {
            std::ifstream f("launcher/settings.json");
            f >> j;
        } catch (...) {}
    }
    j["default_projects_root"] = path;
    std::filesystem::create_directories("launcher");
    std::ofstream f("launcher/settings.json");
    f << j.dump(4);
}

static std::string BrowseFolderDialog(const char* title_utf8, const char* default_path) {
#ifdef _WIN32
    // UTF-8 → UTF-16
    int wlen = MultiByteToWideChar(CP_UTF8, 0, title_utf8, -1, nullptr, 0);
    std::wstring wtitle;
    if (wlen > 0) {
        wtitle.resize(wlen);
        MultiByteToWideChar(CP_UTF8, 0, title_utf8, -1, &wtitle[0], wlen);
    }

    BROWSEINFOW bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.lpszTitle = wtitle.empty() ? nullptr : wtitle.c_str();
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    wchar_t display_name[MAX_PATH];
    bi.pszDisplayName = display_name;

    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        wchar_t wpath[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, wpath)) {
            CoTaskMemFree(pidl);
            // UTF-16 → UTF-8
            int len = WideCharToMultiByte(CP_UTF8, 0, wpath, -1, nullptr, 0, nullptr, nullptr);
            std::string path;
            if (len > 0) {
                path.resize(len);
                WideCharToMultiByte(CP_UTF8, 0, wpath, -1, &path[0], len, nullptr, nullptr);
                path.pop_back(); // remove null terminator
            }
            std::replace(path.begin(), path.end(), '\\', '/');
            return path;
        }
        CoTaskMemFree(pidl);
    }
#endif
    return default_path ? default_path : "";
}

static std::string SanitizeProjectName(const std::string& name) {
    std::string result = name;
    // Windows 非法字符: \ / : * ? " < > |
    const char* illegal = "\\/:*?\"<>|";
    for (char& c : result) {
        if (std::strchr(illegal, c)) c = '_';
    }
    // 去掉首尾空格和点
    size_t start = result.find_first_not_of(" .");
    size_t end = result.find_last_not_of(" .");
    if (start == std::string::npos) return "Untitled";
    return result.substr(start, end - start + 1);
}

static std::string MakeUniquePath(const std::string& base_path) {
    namespace fs = std::filesystem;
    if (!fs::exists(base_path)) return base_path;
    std::string parent = fs::path(base_path).parent_path().string();
    std::string name = fs::path(base_path).filename().string();
    for (int i = 1; i < 999; ++i) {
        std::string candidate = parent + "/" + name + "_" + std::to_string(i);
        if (!fs::exists(candidate)) return candidate;
    }
    return base_path;
}

static std::string GetNextProjectName(const std::string& parent_dir) {
    for (int i = 1; i < 999; ++i) {
        std::string name = "Project" + std::to_string(i);
        if (!std::filesystem::exists(parent_dir + "/" + name)) {
            return name;
        }
    }
    return "Project999";
}

static std::string EllipsisPath(const std::string& path, float max_w) {
    ImVec2 full = ImGui::CalcTextSize(path.c_str());
    if (full.x <= max_w) return path;
    std::string ellipsis = "...";
    float ew = ImGui::CalcTextSize(ellipsis.c_str()).x;
    for (size_t i = 0; i < path.size(); ++i) {
        std::string tail = path.substr(i);
        if (ImGui::CalcTextSize(tail.c_str()).x + ew <= max_w) {
            return ellipsis + tail;
        }
    }
    return path;
}

namespace mg {
namespace launcher {

static float Fh() { return ImGui::GetFrameHeight(); }
static float Fs() { return ImGui::GetFontSize(); }
static float Em(float n) { return Fs() * n; }

static ImVec4 Accent() { return ImGui::GetStyleColorVec4(ImGuiCol_CheckMark); }

// 绘制卡片阴影（两层：近层较浓、远层较淡）
static void DrawCardShadow(ImDrawList* dl, ImVec2 min, ImVec2 max, float rounding)
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
static void PageHeader(const char* title, const char* subtitle) {
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
static ImU32 ThumbPlaceholderColor() {
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
static bool NavItem(const char* icon, const char* label, bool selected) {
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
static bool BtnPrimary(const char* label, const ImVec2& sz = ImVec2(0,0)) {
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
static bool BtnSecondary(const char* label, const ImVec2& sz = ImVec2(0,0)) {
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

// ───────────────────────────────────────────────
// LauncherWindow 主逻辑
// ───────────────────────────────────────────────
} // namespace launcher
} // namespace mg

namespace mg { namespace launcher {

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
    ImVec4 text_dis = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
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

// ───────────────────────────────────────────────
// 主页（仪表盘 / 欢迎页）
// ───────────────────────────────────────────────
void LauncherWindow::DrawHomePage() {
    ImVec4 text_col = ImGui::GetStyleColorVec4(ImGuiCol_Text);
    ImVec4 text_dis = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    PageHeader("WELCOME", "开始你的游戏开发之旅");
    ImGui::Dummy(ImVec2(0, Em(0.8f)));

    // ========== 快捷操作卡片 ==========
    float avail_w = ImGui::GetContentRegionAvail().x;
    float spacing = Em(1.2f);
    float min_card_w = Em(10.0f);
    int card_count = (int)((avail_w + spacing) / (min_card_w + spacing));
    if (card_count < 1) card_count = 1;
    if (card_count > 3) card_count = 3;
    float card_w = (avail_w - spacing * (card_count - 1)) / card_count;
    float card_h = Em(9.0f);

    bool clicked_new = false, clicked_open = false, clicked_doc = false;
    auto DrawCard = [&](const char* icon, const char* title, const char* desc, bool& out_clicked, int idx) {
        ImVec2 cp = ImGui::GetCursorScreenPos();
        ImVec2 card_max = ImVec2(cp.x + card_w, cp.y + card_h);
        DrawCardShadow(dl, cp, card_max, Em(0.3f));

        ImGui::SetCursorScreenPos(cp);
        ImGui::InvisibleButton(title, ImVec2(card_w, card_h));
        bool hovered = ImGui::IsItemHovered();
        bool held = ImGui::IsItemActive();

        // 颜色过渡
        ImGuiID id = ImGui::GetID(title);
        ImVec4 bg_target = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
        if (held) bg_target = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
        else if (hovered) bg_target = ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered);
        ImVec4 bg_col = mg::editor::MG_GetAnimatedColor(id, bg_target, 12.0f);

        dl->AddRectFilled(cp, card_max, ImGui::GetColorU32(bg_col), Em(0.3f));

        // 悬停外发光
        if (hovered) {
            mg::editor::MG_DrawOuterGlow(dl, cp, card_max, ImGui::GetColorU32(Accent()), Em(0.3f), 3.0f);
        }

        float icon_area_h = Em(5.0f);
        dl->AddRectFilled(
            ImVec2(cp.x + Em(0.5f), cp.y + Em(0.5f)),
            ImVec2(cp.x + card_w - Em(0.5f), cp.y + icon_area_h),
            ThumbPlaceholderColor(), Em(0.2f));
        ImVec2 icon_sz = ImGui::CalcTextSize(icon);
        dl->AddText(
            ImVec2(cp.x + (card_w - icon_sz.x) * 0.5f, cp.y + (icon_area_h - Fs()) * 0.5f + Em(0.25f)),
            ImGui::GetColorU32(text_dis), icon);

        dl->AddText(
            ImVec2(cp.x + Em(0.8f), cp.y + icon_area_h + Em(0.6f)),
            ImGui::GetColorU32(text_col), title);
        dl->AddText(
            ImVec2(cp.x + Em(0.8f), cp.y + icon_area_h + Em(1.4f)),
            ImGui::GetColorU32(text_dis), desc);

        if (hovered) {
            dl->AddRect(cp, card_max, ImGui::GetColorU32(Accent()), Em(0.3f), 0, 2.5f);
        }
        if (ImGui::IsItemClicked()) out_clicked = true;
        if ((idx + 1) % card_count != 0 && idx + 1 < 3)
            ImGui::SameLine(0, spacing);
        else if (idx + 1 < 3)
            ImGui::Dummy(ImVec2(0, Em(0.8f)));
    };

    DrawCard(ICON_FA_PLUS, "新建项目", "从头开始创建游戏", clicked_new, 0);
    DrawCard(ICON_FA_FOLDER_OPEN, "打开项目", "继续已有的工作", clicked_open, 1);
    DrawCard(ICON_FA_BOOK, "快速入门", "查看文档和教程", clicked_doc, 2);
    if (clicked_new) show_create_dialog_ = true;
    if (clicked_open) current_tab_ = LauncherTab::Projects;
    if (clicked_doc) {
        namespace fs = std::filesystem;
        fs::path docs_dir = fs::current_path() / ".." / "docs";
        std::string url = docs_dir.string();
        std::replace(url.begin(), url.end(), '\\', '/');

        int len_url = MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, nullptr, 0);
        std::wstring w_url(len_url, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, &w_url[0], len_url);

        ShellExecuteW(NULL, L"open", w_url.c_str(), NULL, NULL, SW_SHOW);
    }

    ImGui::Dummy(ImVec2(0, Em(2.5f)));

    auto& list = project_manager_.GetProjects();
    if (list.empty()) {
        // ========== 无项目时：欢迎引导面板 ==========
        float panel_h = Em(16.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Em(1.5f), Em(1.5f)));
        ImGui::BeginChild("HomeWelcome", ImVec2(avail_w, panel_h), true);
        {
            ImVec2 cp = ImGui::GetCursorScreenPos();
            float cw = ImGui::GetContentRegionAvail().x;
            ImDrawList* cdl = ImGui::GetWindowDrawList();

            // 居中大图标
            ImVec2 icon_sz = ImGui::CalcTextSize(ICON_FA_CUBE);
            cdl->AddText(
                ImVec2(cp.x + (cw - icon_sz.x) * 0.5f, cp.y + Em(1.0f)),
                ImGui::GetColorU32(ImVec4(text_dis.x, text_dis.y, text_dis.z, 0.25f)),
                ICON_FA_CUBE);

            ImGui::Dummy(ImVec2(0, Em(5.0f)));
            ImGui::SetCursorPosX((cw - ImGui::CalcTextSize("还没有任何项目").x) * 0.5f);
            ImGui::TextDisabled("还没有任何项目");

            ImGui::Dummy(ImVec2(0, Em(0.8f)));
            const char* hint = "点击上方 \"新建项目\" 卡片，选择模板并开始你的第一个游戏。";
            ImGui::SetCursorPosX((cw - ImGui::CalcTextSize(hint).x) * 0.5f);
            ImGui::TextDisabled("%s", hint);

            ImGui::Dummy(ImVec2(0, Em(2.0f)));
            float btn_w = Em(10.0f);
            ImGui::SetCursorPosX((cw - btn_w) * 0.5f);
            if (BtnPrimary(ICON_FA_PLUS " 新建项目", ImVec2(btn_w, Fh() * 1.2f))) {
                show_create_dialog_ = true;
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
    } else {
        // ========== 有项目时：最近项目 + 概览 ==========
        bool side_by_side = (avail_w >= Em(26.0f));
        float left_w = side_by_side ? avail_w * 0.58f : avail_w;
        float right_w = side_by_side ? (avail_w - left_w - spacing) : avail_w;
        float panel_h = Em(14.0f);

        // 左侧：最近项目
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Em(1.0f), Em(1.0f)));
        ImGui::BeginChild("HomeRecent", ImVec2(left_w, panel_h), true);
        {
            ImDrawList* cdl = ImGui::GetWindowDrawList();
            ImGui::Text("最近项目");
            ImGui::Dummy(ImVec2(0, Em(0.8f)));

            int shown = 0;
            for (size_t i = 0; i < list.size() && shown < 5; ++i, ++shown) {
                auto& p = list[i];
                bool exists = std::filesystem::exists(p.path + "/project.json");
                float row_h = Fh() * 2.4f;
                ImVec2 row_pos = ImGui::GetCursorScreenPos();
                float row_w = ImGui::GetContentRegionAvail().x;

                ImU32 row_bg = exists
                    ? ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_FrameBg))
                    : ImGui::GetColorU32(ImVec4(0.45f, 0.15f, 0.10f, 0.35f));
                cdl->AddRectFilled(row_pos, ImVec2(row_pos.x + row_w, row_pos.y + row_h),
                    row_bg, Em(0.2f));

                // 丢失标记
                float name_x = row_pos.x + Em(0.8f);
                if (!exists) {
                    const char* warn = ICON_FA_TRIANGLE_EXCLAMATION " ";
                    cdl->AddText(ImVec2(name_x, row_pos.y + (row_h - Fs()) * 0.25f),
                        ImGui::GetColorU32(ImVec4(1.0f, 0.6f, 0.3f, 1.0f)), warn);
                    name_x += ImGui::CalcTextSize(warn).x;
                }
                cdl->AddText(
                    ImVec2(name_x, row_pos.y + (row_h - Fs()) * 0.25f),
                    exists ? ImGui::GetColorU32(text_col)
                           : ImGui::GetColorU32(ImVec4(1.0f, 0.6f, 0.3f, 0.85f)),
                    p.name.c_str());

                // 模板类型标签
                const char* mode_label = "";
                if (p.mode == "2d_topdown") mode_label = "俯视角";
                else if (p.mode == "2d_sidescroller") mode_label = "横板";
                else if (p.mode == "3d") mode_label = "3D";
                else if (p.mode == "empty") mode_label = "空白";
                if (mode_label[0]) {
                    ImVec2 ml_sz = ImGui::CalcTextSize(mode_label);
                    float tag_x = row_pos.x + row_w - ml_sz.x - Em(1.2f);
                    float tag_y = row_pos.y + (row_h - Fs()) * 0.25f;
                    cdl->AddRectFilled(
                        ImVec2(tag_x - Em(0.4f), tag_y - Em(0.1f)),
                        ImVec2(tag_x + ml_sz.x + Em(0.4f), tag_y + Fs() + Em(0.1f)),
                        exists ? ImGui::GetColorU32(Accent())
                               : ImGui::GetColorU32(ImVec4(0.8f, 0.3f, 0.2f, 1.0f)),
                        Em(0.15f));
                    cdl->AddText(ImVec2(tag_x, tag_y), ImGui::GetColorU32(ImVec4(1,1,1,1)), mode_label);
                }

                float path_max_w = row_w - Em(2.0f);
                std::string display_path = exists ? EllipsisPath(p.path, path_max_w) : "路径丢失，点击重新定位";
                ImU32 path_col = exists ? ImGui::GetColorU32(text_dis)
                                        : ImGui::GetColorU32(ImVec4(1.0f, 0.6f, 0.3f, 0.6f));
                cdl->AddText(
                    ImVec2(row_pos.x + Em(0.8f), row_pos.y + (row_h - Fs()) * 0.25f + Fs()),
                    path_col, display_path.c_str());

                ImGui::Dummy(ImVec2(row_w, row_h));
                if (ImGui::IsItemHovered()) {
                    cdl->AddRect(row_pos, ImVec2(row_pos.x + row_w, row_pos.y + row_h),
                        exists ? ImGui::GetColorU32(Accent())
                               : ImGui::GetColorU32(ImVec4(1.0f, 0.5f, 0.3f, 0.8f)),
                        Em(0.2f), 0, 1.5f);
                }
                if (ImGui::IsItemClicked()) {
                    if (!exists) {
                        relocate_index_ = i;
                        std::strncpy(relocate_path_, p.path.c_str(), sizeof(relocate_path_) - 1);
                        relocate_path_[sizeof(relocate_path_) - 1] = '\0';
                        show_relocate_dialog_ = true;
                    } else {
                        OpenProject(p.path);
                    }
                }
                ImGui::Dummy(ImVec2(0, Em(0.4f)));
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();

        if (side_by_side)
            ImGui::SameLine(0, spacing);
        else
            ImGui::Dummy(ImVec2(0, Em(0.8f)));

        // 右侧：概览
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Em(1.0f), Em(1.0f)));
        ImGui::BeginChild("HomeOverview", ImVec2(right_w, panel_h), true);
        {
            ImGui::Text("概览");
            ImGui::Dummy(ImVec2(0, Em(0.8f)));

            auto StatItem = [&](const char* label, const char* value) {
                ImGui::TextDisabled("%s", label);
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(text_col));
                ImGui::Text("%s", value);
                ImGui::PopStyleColor();
                ImGui::Dummy(ImVec2(0, Em(0.6f)));
            };

            std::string project_count = std::to_string(list.size());
            StatItem("引擎版本", "0.1.0");
            StatItem("项目总数", project_count.c_str());
            StatItem("最近活跃", list[0].name.c_str());

            ImGui::Dummy(ImVec2(0, Em(0.8f)));
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0, Em(0.6f)));
            ImGui::TextDisabled("小贴士");
            ImGui::TextWrapped("点击左侧导航栏的\"项目\"可以查看所有项目并进行管理。");
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
    }
}

// ───────────────────────────────────────────────
// 项目列表页
// ───────────────────────────────────────────────
void LauncherWindow::DrawProjectsPage() {
    ImVec4 text_col = ImGui::GetStyleColorVec4(ImGuiCol_Text);
    ImVec4 text_dis = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
    PageHeader("PROJECTS", "所有项目");

    // 搜索栏 + 操作按钮
    static char search_buf[128] = {};
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, Em(0.25f));
    ImGui::PushItemWidth(Em(18.0f));
    ImGui::InputTextWithHint("##search", ICON_FA_SEARCH " 搜索项目...", search_buf, sizeof(search_buf));
    ImGui::PopItemWidth();
    ImGui::PopStyleVar();
    ImGui::Dummy(ImVec2(0, Em(1.0f)));

    if (BtnPrimary(ICON_FA_PLUS " 新建项目", ImVec2(Em(9.0f), Fh() * 1.2f))) show_create_dialog_ = true;
    ImGui::SameLine();
    if (BtnSecondary(ICON_FA_FOLDER_OPEN " 打开已有项目", ImVec2(Em(10.0f), Fh() * 1.2f))) {
        std::string picked = BrowseFolderDialog("选择项目文件夹", GetDefaultProjectsRoot().c_str());
        if (!picked.empty()) {
            if (std::filesystem::exists(picked + "/project.json")) {
                project_manager_.LoadProjectFromDisk(picked);
            }
        }
    }
    ImGui::Dummy(ImVec2(0, Em(2.2f)));

    // 项目卡片网格
    auto& list = project_manager_.GetProjects();

    // 搜索过滤：按项目名/路径子串匹配（大小写不敏感），visible 存原始下标
    auto to_lower = [](std::string s) {
        for (auto& c : s) c = (char)tolower((unsigned char)c);
        return s;
    };
    const std::string needle = to_lower(search_buf);
    std::vector<size_t> visible;
    visible.reserve(list.size());
    for (size_t i = 0; i < list.size(); ++i) {
        if (needle.empty() ||
            to_lower(list[i].name).find(needle) != std::string::npos ||
            to_lower(list[i].path).find(needle) != std::string::npos) {
            visible.push_back(i);
        }
    }

    if (!list.empty()) {
        ImGui::Text("所有项目");
        ImGui::Dummy(ImVec2(0, Em(0.8f)));

        if (visible.empty()) {
            ImGui::TextDisabled("没有匹配 \"%s\" 的项目", search_buf);
        }

        float spacing = Em(1.2f);
        float avail_w = ImGui::GetContentRegionAvail().x;
        float max_card_w = Em(16.0f);
        int cards_per_row = (int)(avail_w / max_card_w);
        if (cards_per_row < 1) cards_per_row = 1;
        float card_w = (avail_w - spacing * (cards_per_row - 1)) / cards_per_row;
        float card_h = Em(13.0f);
        float thumb_h = Em(8.0f);

        for (size_t vi = 0; vi < visible.size(); ++vi) {
            const size_t i = visible[vi];
            auto& p = list[i];
            bool exists = std::filesystem::exists(p.path + "/project.json");

            ImVec2 card_min = ImGui::GetCursorScreenPos();
            ImVec2 card_max = ImVec2(card_min.x + card_w, card_min.y + card_h);
            DrawCardShadow(ImGui::GetWindowDrawList(), card_min, card_max, Em(0.3f));

            bool is_selected = (selected_project_index_ == (int)i);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Em(0.5f), Em(0.5f)));
            if (exists) {
                if (is_selected) {
                    // 选中状态：使用 ButtonActive 背景 + Accent 边框
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                    ImGui::PushStyleColor(ImGuiCol_Border, Accent());
                } else {
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
                }
            } else {
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.25f, 0.12f, 0.10f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.8f, 0.3f, 0.2f, 0.5f));
            }
            ImGui::BeginChild(("card"+std::to_string(i)).c_str(), ImVec2(card_w, card_h), true,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 card_pos = ImGui::GetCursorScreenPos();

            // 缩略图占位
            ImVec2 thumb_min = card_pos;
            ImVec2 thumb_max = ImVec2(card_pos.x + card_w - Em(1.0f), card_pos.y + thumb_h);
            if (exists) {
                dl->AddRectFilled(thumb_min, thumb_max, ThumbPlaceholderColor(), Em(0.2f));
                ImVec2 icon_sz = ImGui::CalcTextSize(ICON_FA_IMAGE);
                dl->AddText(
                    ImVec2(thumb_min.x + (thumb_max.x - thumb_min.x - icon_sz.x) * 0.5f,
                           thumb_min.y + (thumb_max.y - thumb_min.y - icon_sz.y) * 0.5f),
                    ImGui::GetColorU32(ImVec4(text_dis.x, text_dis.y, text_dis.z, 0.35f)), ICON_FA_IMAGE);
            } else {
                dl->AddRectFilled(thumb_min, thumb_max,
                    ImGui::GetColorU32(ImVec4(0.35f, 0.15f, 0.10f, 0.5f)), Em(0.2f));
                const char* warn_icon = ICON_FA_TRIANGLE_EXCLAMATION;
                ImVec2 icon_sz = ImGui::CalcTextSize(warn_icon);
                dl->AddText(
                    ImVec2(thumb_min.x + (thumb_max.x - thumb_min.x - icon_sz.x) * 0.5f,
                           thumb_min.y + (thumb_max.y - thumb_min.y - icon_sz.y) * 0.5f),
                    ImGui::GetColorU32(ImVec4(1.0f, 0.5f, 0.3f, 0.7f)), warn_icon);
            }

            // 模板类型标签（右上角）
            const char* mode_label = "";
            if (p.mode == "2d_topdown") mode_label = "俯视角";
            else if (p.mode == "2d_sidescroller") mode_label = "横板";
            else if (p.mode == "3d") mode_label = "3D";
            else if (p.mode == "empty") mode_label = "空白";
            if (mode_label[0]) {
                ImVec2 ml_sz = ImGui::CalcTextSize(mode_label);
                float tag_x = card_pos.x + card_w - Em(1.0f) - ml_sz.x - Em(0.6f);
                float tag_y = card_pos.y + Em(0.4f);
                dl->AddRectFilled(
                    ImVec2(tag_x - Em(0.3f), tag_y - Em(0.1f)),
                    ImVec2(tag_x + ml_sz.x + Em(0.3f), tag_y + Fs() + Em(0.1f)),
                    exists ? ImGui::GetColorU32(Accent())
                           : ImGui::GetColorU32(ImVec4(0.8f, 0.3f, 0.2f, 1.0f)),
                    Em(0.15f));
                dl->AddText(ImVec2(tag_x, tag_y), ImGui::GetColorU32(ImVec4(1,1,1,1)), mode_label);
            }

            // 项目名称、路径
            ImGui::SetCursorPosY(thumb_h + Em(0.4f));
            ImGui::Text("%s", p.name.c_str());
            if (exists) {
                float path_max_w = card_w - Em(1.0f);
                std::string display_path = EllipsisPath(p.path, path_max_w);
                ImGui::TextDisabled("%s", display_path.c_str());
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 0.8f), "路径丢失");
                if (ImGui::Button("重新定位", ImVec2(card_w - Em(1.0f), Fh() * 1.2f))) {
                    relocate_index_ = i;
                    std::strncpy(relocate_path_, p.path.c_str(), sizeof(relocate_path_) - 1);
                    relocate_path_[sizeof(relocate_path_) - 1] = '\0';
                    show_relocate_dialog_ = true;
                }
            }

            ImGui::EndChild();
            if (exists && is_selected) {
                ImGui::PopStyleColor(2);
            } else if (!exists) {
                ImGui::PopStyleColor(2);
            } else {
                ImGui::PopStyleColor();
            }
            ImGui::PopStyleVar();

            // 悬停高亮（绘制在父窗口上）
            if (ImGui::IsItemHovered()) {
                ImVec2 min = ImGui::GetItemRectMin();
                ImVec2 max = ImGui::GetItemRectMax();
                ImGui::GetWindowDrawList()->AddRect(min, max,
                    exists ? ImGui::GetColorU32(Accent())
                           : ImGui::GetColorU32(ImVec4(1.0f, 0.5f, 0.3f, 0.8f)),
                    Em(0.3f), 0, 2.5f);
            }
            if (ImGui::IsItemClicked()) {
                if (exists) {
                    selected_project_index_ = (int)i;
                    OpenProject(p.path);
                }
            }

            // 水平/垂直布局（按过滤后的可见序号排布）
            if ((vi + 1) % cards_per_row != 0 && vi + 1 < visible.size()) {
                ImGui::SameLine(0, spacing);
            } else if (vi + 1 < visible.size()) {
                ImGui::Dummy(ImVec2(0, Em(0.8f)));
            }
        }
    } else {
        // 无项目时的空状态
        float avail_h = ImGui::GetContentRegionAvail().y;
        ImVec2 cp = ImGui::GetCursorScreenPos();
        float cw = ImGui::GetContentRegionAvail().x;
        ImDrawList* cdl = ImGui::GetWindowDrawList();

        float center_y = cp.y + avail_h * 0.35f;
        ImVec2 icon_sz = ImGui::CalcTextSize(ICON_FA_FOLDER_OPEN);
        cdl->AddText(
            ImVec2(cp.x + (cw - icon_sz.x) * 0.5f, center_y),
            ImGui::GetColorU32(ImVec4(text_dis.x, text_dis.y, text_dis.z, 0.25f)),
            ICON_FA_FOLDER_OPEN);

        ImGui::Dummy(ImVec2(0, Em(6.0f)));
        const char* empty_text = "还没有任何项目";
        ImGui::SetCursorPosX((cw - ImGui::CalcTextSize(empty_text).x) * 0.5f);
        ImGui::TextDisabled("%s", empty_text);

        ImGui::Dummy(ImVec2(0, Em(0.6f)));
        const char* hint = "点击上方 \"新建项目\" 按钮创建第一个游戏。";
        ImGui::SetCursorPosX((cw - ImGui::CalcTextSize(hint).x) * 0.5f);
        ImGui::TextDisabled("%s", hint);
    }
}

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
        ImVec4 text_col = ImGui::GetStyleColorVec4(ImGuiCol_Text);
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
// 新建项目向导
// ───────────────────────────────────────────────
void LauncherWindow::DrawNewProjectWizard() {
    ImVec4 text_col = ImGui::GetStyleColorVec4(ImGuiCol_Text);
    ImVec4 accent = Accent();
    // 返回按钮（精致小按钮）
    if (BtnSecondary(ICON_FA_ARROW_LEFT " 返回", ImVec2(Em(6.0f), Fh() * 1.1f))) {
        show_create_dialog_ = false;
        new_project_name_[0] = '\0';
        new_project_path_[0] = '\0';
    }
    ImGui::Dummy(ImVec2(0, Em(0.8f)));

    PageHeader("NEW PROJECT", "创建一个新的游戏项目");
    // PageHeader 已自带分隔线，无需额外 Separator

    // 模板选择
    ImGui::Text("选择模板");
    ImGui::Dummy(ImVec2(0, Em(0.6f)));

    const char* tpls[] = {"2D 俯视角 RPG", "2D 横板平台", "3D 项目", "空白项目"};
    const char* desc[] = {
        "类似 RPG Maker\n四方向移动 / Y-sort",
        "类似平台跳跃\n重力 / 碰撞层",
        "3D 网格渲染\n透视相机（预留）",
        "最小化配置\n自行搭建"
    };
    const char* tpl_modes[] = {"2d_topdown", "2d_sidescroller", "3d", "empty"};
    float tpl_w = Em(10.0f);
    float tpl_h = Em(6.0f);

    ImVec4 child_bg = ImGui::GetStyleColorVec4(ImGuiCol_ChildBg);
    ImVec4 frame_bg = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);

    for (int i = 0; i < 4; ++i) {
        bool sel = (selected_template_ == i);
        ImVec2 tpl_min = ImGui::GetCursorScreenPos();
        ImVec2 tpl_max = ImVec2(tpl_min.x + tpl_w, tpl_min.y + tpl_h);

        // 绘制阴影
        ImDrawList* pdl = ImGui::GetWindowDrawList();
        DrawCardShadow(pdl, tpl_min, tpl_max, Em(0.25f));

        // 检测交互（用 InvisibleButton 替代 IsMouseHoveringRect，更准确）
        ImGui::SetCursorScreenPos(tpl_min);
        ImGui::InvisibleButton(("tpl_btn"+std::to_string(i)).c_str(), ImVec2(tpl_w, tpl_h));
        bool tpl_hov = ImGui::IsItemHovered();
        bool tpl_held = ImGui::IsItemActive();
        if (ImGui::IsItemClicked()) selected_template_ = i;

        // 颜色过渡
        ImGuiID tpl_id = ImGui::GetID(("tpl_bg"+std::to_string(i)).c_str());
        ImVec4 bg_target = frame_bg;
        ImVec4 border_target = ImGui::GetStyleColorVec4(ImGuiCol_Border);
        if (sel) {
            bg_target = ImVec4(accent.x, accent.y, accent.z, 0.10f);
            border_target = accent;
        } else if (tpl_held) {
            bg_target = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
        } else if (tpl_hov) {
            bg_target = ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered);
            border_target = accent;
        }
        ImVec4 bg_col = mg::editor::MG_GetAnimatedColor(tpl_id, bg_target, 12.0f);
        ImVec4 border_col = mg::editor::MG_GetAnimatedColor(ImGui::GetID(("tpl_border"+std::to_string(i)).c_str()), border_target, 12.0f);

        pdl->AddRectFilled(tpl_min, tpl_max, ImGui::GetColorU32(bg_col), Em(0.25f));
        if (sel || tpl_hov) {
            mg::editor::MG_DrawOuterGlow(pdl, tpl_min, tpl_max, ImGui::GetColorU32(border_col), Em(0.25f), 2.5f);
        }
        float border_thick = sel ? 2.0f : 1.5f;
        pdl->AddRect(tpl_min, tpl_max, ImGui::GetColorU32(border_col), Em(0.25f), 0, border_thick);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, 0); // 透明，自己画
        ImGui::PushStyleColor(ImGuiCol_Border, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Em(0.8f), Em(0.7f)));
        ImGui::BeginChild(("tpl"+std::to_string(i)).c_str(), ImVec2(tpl_w, tpl_h), false);

        ImGui::Text("%s", tpls[i]);
        ImGui::Dummy(ImVec2(0, Em(0.3f)));
        ImGui::TextDisabled("%s", desc[i]);
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);

        if ((i+1)%4 != 0 && i+1 < 4) ImGui::SameLine();
    }

    ImGui::Dummy(ImVec2(0, Em(1.8f)));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, Em(1.2f)));

    // 项目配置
    ImGui::Text("项目配置");
    ImGui::Dummy(ImVec2(0, Em(0.8f)));

    float label_w = Em(4.0f);
    float input_w = Em(28.0f);

    // 首次打开时自动填充默认名称和父目录
    if (!new_project_name_[0]) {
        std::string default_parent = GetDefaultProjectsRoot();
        std::string next_name = GetNextProjectName(default_parent);
        std::strncpy(new_project_name_, next_name.c_str(), sizeof(new_project_name_) - 1);
        new_project_name_[sizeof(new_project_name_) - 1] = '\0';
    }
    if (!new_project_path_[0]) {
        std::string default_parent = GetDefaultProjectsRoot();
        std::strncpy(new_project_path_, default_parent.c_str(), sizeof(new_project_path_) - 1);
        new_project_path_[sizeof(new_project_path_) - 1] = '\0';
    }

    ImGui::AlignTextToFramePadding();
    ImGui::Text("名称:");
    ImGui::SameLine();
    ImGui::SetCursorPosX(label_w);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, Em(0.25f));
    ImGui::PushItemWidth(input_w);
    ImGui::InputText("##name", new_project_name_, sizeof(new_project_name_));
    ImGui::PopItemWidth();
    ImGui::PopStyleVar();

    ImGui::Dummy(ImVec2(0, Em(0.4f)));

    ImGui::AlignTextToFramePadding();
    ImGui::Text("路径:");
    ImGui::SameLine();
    ImGui::SetCursorPosX(label_w);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, Em(0.25f));
    ImGui::PushItemWidth(input_w - Em(5.5f));
    ImGui::InputText("##path", new_project_path_, sizeof(new_project_path_));
    ImGui::PopItemWidth();
    ImGui::PopStyleVar();
    ImGui::SameLine();
    if (BtnSecondary("浏览...", ImVec2(Em(5.0f), 0))) {
        std::string picked = BrowseFolderDialog("选择父文件夹", new_project_path_);
        if (!picked.empty()) {
            std::strncpy(new_project_path_, picked.c_str(), sizeof(new_project_path_) - 1);
            new_project_path_[sizeof(new_project_path_) - 1] = '\0';
        }
    }

    // 显示最终将要创建的完整路径预览
    {
        std::string clean = SanitizeProjectName(new_project_name_);
        std::string parent = new_project_path_[0] ? new_project_path_ : GetDefaultProjectsRoot();
        std::string preview = parent + "/" + clean;
        ImGui::Dummy(ImVec2(0, Em(0.6f)));
        ImGui::TextDisabled("将在以下位置创建: %s", preview.c_str());
    }

    ImGui::Dummy(ImVec2(0, Em(2.5f)));

    // 底部右对齐按钮
    float btns = Em(8.0f) + Em(0.5f) + Em(8.0f);
    float space = ImGui::GetContentRegionAvail().x;
    if (space > btns) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + space - btns);

    if (BtnSecondary(ICON_FA_TIMES " 取消", ImVec2(Em(8.0f), Fh() * 1.1f))) {
        show_create_dialog_ = false;
        new_project_name_[0] = '\0';
        new_project_path_[0] = '\0';
        selected_template_ = 0;
    }
    ImGui::SameLine();
    if (BtnPrimary(ICON_FA_CHECK " 创建", ImVec2(Em(8.0f), Fh() * 1.1f))) {
        std::string clean_name = SanitizeProjectName(new_project_name_);
        std::string parent_dir = new_project_path_[0] ? new_project_path_ : GetDefaultProjectsRoot();
        std::string final_path = parent_dir + "/" + clean_name;
        final_path = MakeUniquePath(final_path);
        if (!clean_name.empty() && !final_path.empty()) {
            project_manager_.CreateProject(clean_name, final_path, tpl_modes[selected_template_]);
            show_create_dialog_ = false;
            new_project_name_[0] = '\0';
            new_project_path_[0] = '\0';
            selected_template_ = 0;
        }
    }
}

// ───────────────────────────────────────────────
// 重新定位项目对话框
// ───────────────────────────────────────────────
void LauncherWindow::DrawRelocateDialog() {
    if (show_relocate_dialog_)
        ImGui::OpenPopup("重新定位项目");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(Em(28.0f), 0), ImGuiCond_Appearing);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Em(1.2f), Em(1.2f)));
    if (ImGui::BeginPopupModal("重新定位项目", &show_relocate_dialog_,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
        auto& list = project_manager_.GetProjects();
        if (relocate_index_ < list.size()) {
            auto& p = list[relocate_index_];
            ImGui::Text("项目: %s", p.name.c_str());
            ImGui::TextDisabled("原路径: %s", p.path.c_str());
            ImGui::Dummy(ImVec2(0, Em(0.8f)));

            ImGui::AlignTextToFramePadding();
            ImGui::Text("新路径:");
            ImGui::SameLine();
            ImGui::SetCursorPosX(Em(5.0f));
            ImGui::PushItemWidth(Em(14.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, Em(0.25f));
            ImGui::InputText("##reloc_path", relocate_path_, sizeof(relocate_path_));
            ImGui::PopItemWidth();
            ImGui::PopStyleVar();
            ImGui::SameLine();
            if (BtnSecondary("浏览...", ImVec2(Em(5.0f), 0))) {
                std::string picked = BrowseFolderDialog("选择项目文件夹", relocate_path_);
                if (!picked.empty()) {
                    std::strncpy(relocate_path_, picked.c_str(), sizeof(relocate_path_) - 1);
                    relocate_path_[sizeof(relocate_path_) - 1] = '\0';
                }
            }

            ImGui::Dummy(ImVec2(0, Em(1.0f)));
            float btn_w = Em(7.0f);
            float total_w = btn_w * 2 + Em(0.8f);
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - total_w) * 0.5f);
            if (BtnPrimary("确认", ImVec2(btn_w, Fh() * 1.2f))) {
                if (project_manager_.RelocateProject(relocate_index_, relocate_path_)) {
                    show_relocate_dialog_ = false;
                    relocate_path_[0] = '\0';
                }
            }
            ImGui::SameLine(0, Em(0.8f));
            if (BtnSecondary("取消", ImVec2(btn_w, Fh() * 1.2f))) {
                show_relocate_dialog_ = false;
                relocate_path_[0] = '\0';
            }
        } else {
            show_relocate_dialog_ = false;
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
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
