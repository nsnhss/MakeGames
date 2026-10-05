#pragma once
// launcher_path_utils.h — Launcher 路径/平台助手（D5 自 launcher_window.cpp 拆出）。
#include <string>

namespace mg {
namespace launcher {

// 默认项目根目录：优先 launcher/settings.json 的自定义值，否则 我的文档/MakeGames Projects
std::string GetDefaultProjectsRoot();
void SaveDefaultProjectsRoot(const std::string& path);

// 系统文件夹浏览对话框（UTF-8 入参/返回，'\' 归一为 '/'）；取消返回 default_path
std::string BrowseFolderDialog(const char* title_utf8, const char* default_path);

// 项目名净化：Windows 非法字符→'_'，去首尾空格/点；全空返回 "Untitled"
std::string SanitizeProjectName(const std::string& name);

// 路径已存在时追加 _1/_2... 直至唯一
std::string MakeUniquePath(const std::string& base_path);

// 在父目录下找第一个可用的 ProjectN 名称
std::string GetNextProjectName(const std::string& parent_dir);

// 路径超宽时头部省略（"...tail"）
std::string EllipsisPath(const std::string& path, float max_w);

} // namespace launcher
} // namespace mg
