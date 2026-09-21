#pragma once
#include "../core/project_manager.h"
#include "log_panel.h"

namespace mg {
namespace launcher {

enum class LauncherTab {
    Home,      // 欢迎主页
    Projects,  // 项目列表
    DevLog,    // 开发日志
    Settings   // 设置
};

class LauncherWindow {
public:
    void Initialize();
    void Shutdown();
    void Draw();

private:
    void DrawTopBar(float height);
    void DrawNav();
    void DrawContent();

    void DrawHomePage();
    void DrawProjectsPage();
    void DrawDevLogPage();
    void DrawSettingsPage();
    void DrawNewProjectWizard();
    void DrawRelocateDialog();

    ProjectManager project_manager_;
    LogPanel log_panel_;
    LauncherTab current_tab_ = LauncherTab::Home;

    bool show_create_dialog_ = false;
    char new_project_name_[256] = {};
    char new_project_path_[512] = {};
    int selected_template_ = 0;  // 0=2d_topdown, 1=2d_sidescroller, 2=3d, 3=empty
    
    // 重新定位对话框状态
    bool show_relocate_dialog_ = false;
    size_t relocate_index_ = 0;
    char relocate_path_[512] = {};

    // 主题设置：0=UE5 暗色, 1=UE5 亮色
    int theme_index_ = 0;

    // 项目页面：当前选中的项目索引（-1 表示无）
    int selected_project_index_ = -1;

    bool should_quit_ = false;
    void OpenProject(const std::string& project_path);
public:
    bool ShouldQuit() const { return should_quit_; }
};

} // namespace launcher
} // namespace mg
