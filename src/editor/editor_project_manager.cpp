#include "editor/editor_project_manager.h"
#include "editor/debug_log.h"
#include "resource/data_manager.h"
#include "third_party/json.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace mg {
namespace editor {

bool EditorProjectManager::NewProject(const std::string& name,
                                      const std::string& path,
                                      const std::string& mode) {
    namespace fs = std::filesystem;
    try {
        fs::create_directories(path + "/assets/sprites");
        fs::create_directories(path + "/assets/tilesets");
        fs::create_directories(path + "/assets/fonts");
        fs::create_directories(path + "/data");
        fs::create_directories(path + "/maps");
        fs::create_directories(path + "/scenes");
        fs::create_directories(path + "/prefabs");
        fs::create_directories(path + "/.cache");

        ProjectManifest manifest;
        manifest.name = name;
        manifest.mode = mode;
        manifest.start_scene = "maps/Map001.json";
        manifest.tile_size = 32;
        DataManager::Get().Save(path + "/project.json", manifest);

        // MapInfos.json 由 Editor 首次 OpenProject 时通过 RebuildMapInfos 生成
        return true;
    } catch (const std::exception& e) {
        EDITOR_LOG("EditorProjectManager::NewProject failed: " << e.what());
        return false;
    }
}

void EditorProjectManager::RebuildMapInfos() {
    EDITOR_LOG("EditorProjectManager::RebuildMapInfos enter");
    if (project_path_.empty()) {
        EDITOR_LOG("EditorProjectManager::RebuildMapInfos empty project path, returning");
        return;
    }
    namespace fs = std::filesystem;
    MapInfos infos;
    int next_id = 1;
    std::vector<std::string> map_files;
    std::string maps_dir = project_path_ + "/maps";
    if (!fs::exists(maps_dir) || !fs::is_directory(maps_dir)) {
        EDITOR_LOG("EditorProjectManager::RebuildMapInfos maps/ not found, saving empty MapInfos");
        DataManager::Get().Save(project_path_ + "/maps/MapInfos.json", infos);
        return;
    }
    for (const auto& entry : fs::directory_iterator(maps_dir)) {
        if (!entry.is_regular_file()) continue;
        std::string name = entry.path().filename().string();
        if (name == "MapInfos.json") continue;
        if (entry.path().extension() != ".json") continue;
        map_files.push_back(name);
    }
    std::sort(map_files.begin(), map_files.end(), [](const std::string& a, const std::string& b) {
        // 提取数字前缀用于自然排序（Map2.json < Map10.json）
        auto extract_num = [](const std::string& s) -> int {
            size_t start = 0;
            while (start < s.size() && !std::isdigit((unsigned char)s[start])) ++start;
            size_t end = start;
            while (end < s.size() && std::isdigit((unsigned char)s[end])) ++end;
            if (start < end) {
                try {
                    return std::stoi(s.substr(start, end - start));
                } catch (...) {
                    return 0;
                }
            }
            return 0;
        };
        int na = extract_num(a);
        int nb = extract_num(b);
        if (na != nb) return na < nb;
        return a < b;
    });
    for (const auto& name : map_files) {
        MapInfoEntry e;
        e.id = next_id++;
        e.name = name;
        infos.entries.push_back(e);
    }
    DataManager::Get().Save(project_path_ + "/maps/MapInfos.json", infos);
    EDITOR_LOG("EditorProjectManager::RebuildMapInfos exit");
}

MapInfos EditorProjectManager::LoadMapInfos() const {
    EDITOR_LOG("EditorProjectManager::LoadMapInfos enter");
    if (project_path_.empty()) {
        EDITOR_LOG("EditorProjectManager::LoadMapInfos empty project path, returning empty");
        return {};
    }
    auto mi = DataManager::Get().Load<MapInfos>(project_path_ + "/maps/MapInfos.json");
    EDITOR_LOG("EditorProjectManager::LoadMapInfos exit entries=" << (mi ? mi->entries.size() : 0));
    return mi ? *mi : MapInfos{};
}

namespace {

std::string GetRecentProjectsPath() {
    std::string dir;
    const char* appdata = std::getenv("APPDATA");
    if (appdata && appdata[0] != '\0') {
        dir = std::string(appdata) + "/MakeGames";
    } else {
        // 兜底：exe 所在目录下的 MakeGames 子目录，避免写到不可预期的 cwd
#ifdef _WIN32
        wchar_t wexe_path[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, wexe_path, MAX_PATH);
        char exe_path[MAX_PATH] = {};
        WideCharToMultiByte(CP_UTF8, 0, wexe_path, -1, exe_path, MAX_PATH, nullptr, nullptr);
        char* last_slash = strrchr(exe_path, '\\');
        if (last_slash) *(last_slash + 1) = '\0';
        dir = std::string(exe_path) + "MakeGames";
#else
        dir = "MakeGames";
#endif
    }
    std::filesystem::create_directories(dir);
    std::replace(dir.begin(), dir.end(), '\\', '/');
    return dir + "/recent_projects.json";
}

} // namespace

std::vector<std::string> EditorProjectManager::GetRecentProjects() const {
    auto raw = DataManager::Get().LoadJsonRaw(GetRecentProjectsPath());
    if (!raw || !raw->is_array()) return {};
    std::vector<std::string> result;
    for (const auto& item : *raw) {
        if (item.is_string()) result.push_back(item.get<std::string>());
    }
    return result;
}

void EditorProjectManager::AddRecentProject(const std::string& path) {
    if (path.empty()) return;
    auto recent = GetRecentProjects();
    // 去重并提到最前
    recent.erase(std::remove(recent.begin(), recent.end(), path), recent.end());
    recent.insert(recent.begin(), path);
    if (recent.size() > 5) recent.resize(5);

    nlohmann::json j = nlohmann::json::array();
    for (const auto& p : recent) j.push_back(p);
    DataManager::Get().Save(GetRecentProjectsPath(), j);
}

} // namespace editor
} // namespace mg
