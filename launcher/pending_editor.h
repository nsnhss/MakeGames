#pragma once
#include <string>

namespace mg {
namespace launcher {

// Launcher 关闭后待启动的 Editor 项目路径（由 LauncherWindow::OpenProject 设置，
// main() 在 Run() 返回、Vulkan 销毁完毕后读取并 spawn editor.exe）。
void SetPendingEditorPath(const std::string& path);
const std::string& GetPendingEditorPath();

} // namespace launcher
} // namespace mg
