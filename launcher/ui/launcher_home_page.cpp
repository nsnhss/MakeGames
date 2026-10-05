// launcher_home_page.cpp — 主页（仪表盘 / 欢迎页）。D5 自 launcher_window.cpp 拆出，逐位不变。
#include "launcher_window.h"
#include "launcher_ui_kit.h"
#include "launcher_path_utils.h"
#include "imgui.h"
#include "src/editor/imgui/mg_imgui_icons.h"
#include "src/editor/imgui/mg_imgui_extensions.h"

#include <filesystem>
#include <cstring>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace mg {
namespace launcher {

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
#ifdef _WIN32
        namespace fs = std::filesystem;
        fs::path docs_dir = fs::current_path() / ".." / "docs";
        std::string url = docs_dir.string();
        std::replace(url.begin(), url.end(), '\\', '/');

        int len_url = MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, nullptr, 0);
        std::wstring w_url(len_url, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, &w_url[0], len_url);

        ShellExecuteW(NULL, L"open", w_url.c_str(), NULL, NULL, SW_SHOW);
#endif
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

} // namespace launcher
} // namespace mg
