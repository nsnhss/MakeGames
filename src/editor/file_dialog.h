#pragma once
#include <string>
#include <vector>

namespace mg {
namespace editor {

// Windows 原生文件对话框（阻塞式，支持多选）
// filter_name: "Images", filter_ext: "*.png;*.jpg"
std::vector<std::string> OpenFileDialogMulti(const char* filter_name, const char* filter_ext);

// Windows 原生文件夹选择对话框
std::string OpenFolderDialog(const char* title);

} // namespace editor
} // namespace mg
