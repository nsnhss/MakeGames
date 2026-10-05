// launcher_path_utils.cpp — Launcher 路径/平台助手（D5 自 launcher_window.cpp 拆出，逐位不变）。
#include "launcher_path_utils.h"
#include "imgui.h"
#include "third_party/json.hpp"

#include <filesystem>
#include <fstream>
#include <cstring>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

namespace mg {
namespace launcher {

std::string GetDefaultProjectsRoot() {
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

void SaveDefaultProjectsRoot(const std::string& path) {
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

std::string BrowseFolderDialog(const char* title_utf8, const char* default_path) {
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

std::string SanitizeProjectName(const std::string& name) {
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

std::string MakeUniquePath(const std::string& base_path) {
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

std::string GetNextProjectName(const std::string& parent_dir) {
    for (int i = 1; i < 999; ++i) {
        std::string name = "Project" + std::to_string(i);
        if (!std::filesystem::exists(parent_dir + "/" + name)) {
            return name;
        }
    }
    return "Project999";
}

std::string EllipsisPath(const std::string& path, float max_w) {
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

} // namespace launcher
} // namespace mg
