// launcher_projects_page.cpp — 项目列表页。D5 自 launcher_window.cpp 拆出，逐位不变。
#include "launcher_window.h"
#include "launcher_ui_kit.h"
#include "launcher_path_utils.h"
#include "imgui.h"
#include "src/editor/imgui/mg_imgui_icons.h"

#include <filesystem>
#include <cstring>
#include <cctype>
#include <vector>

namespace mg {
namespace launcher {

void LauncherWindow::DrawProjectsPage() {
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

} // namespace launcher
} // namespace mg
