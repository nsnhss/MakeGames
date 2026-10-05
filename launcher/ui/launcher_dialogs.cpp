// launcher_dialogs.cpp — 新建项目向导 + 重新定位项目对话框。D5 自 launcher_window.cpp 拆出，逐位不变。
#include "launcher_window.h"
#include "launcher_ui_kit.h"
#include "launcher_path_utils.h"
#include "imgui.h"
#include "src/editor/imgui/mg_imgui_icons.h"
#include "src/editor/imgui/mg_imgui_extensions.h"

#include <cstring>

namespace mg {
namespace launcher {

// ───────────────────────────────────────────────
// 新建项目向导
// ───────────────────────────────────────────────
void LauncherWindow::DrawNewProjectWizard() {
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

} // namespace launcher
} // namespace mg
