#include "project_manager.h"
#include <filesystem>
#include <fstream>
#include "third_party/json.hpp"
#include "src/resource/schema/project_schema.h"

namespace mg {
namespace launcher {

using json = nlohmann::json;

// ── 默认文件内容生成器 ─────────────────────────────

static json MakeProjectJson(const std::string& name, const std::string& mode) {
    json j;
    j["name"] = name;
    j["engine_version"] = "0.1.0";
    j["mode"] = mode;
    j["start_scene"] = "maps/Map001.json";
    auto now = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", std::localtime(&now));
    j["created_at"] = buf;
    j["last_opened"] = buf;
    j["display"] = {
        {"title", name},
        {"width", 1280},
        {"height", 720},
        {"fullscreen", false},
        {"vsync", true},
        {"pixel_perfect", true}
    };
    j["tile_size"] = 32;
    j["default_font"] = "assets/fonts/default.ttf";
    return j;
}

static json MakeTopDownMap() {
    json j;
    j["id"] = 1;
    j["name"] = "初始地图";
    j["width"] = 16;
    j["height"] = 12;
    j["tile_size"] = 32;
    j["mode"] = "2d_topdown";
    j["scroll_type"] = "none";
    j["bgm"] = {{"name", ""}, {"volume", 80}, {"pitch", 100}};
    j["bgs"] = {{"name", ""}, {"volume", 80}, {"pitch", 100}};
    j["parallax"] = nullptr;

    std::vector<int> ground(16 * 12, 1);
    j["layers"] = json::array();
    j["layers"].push_back({
        {"name", "地面"},
        {"type", "tile"},
        {"tileset", "tilesets/Overworld.tileset.json"},
        {"data", ground},
        {"visible", true},
        {"opacity", 1.0}
    });
    j["layers"].push_back({
        {"name", "实体层"},
        {"type", "entity"},
        {"entities", json::array({
            {{"prefab", "prefabs/player.prefab.json"}, {"x", 8}, {"y", 6}, {"overrides", nullptr}}
        })}
    });
    j["events"] = json::array();
    j["regions"] = json::array();
    return j;
}

static json MakeSideScrollerMap() {
    json j;
    j["id"] = 1;
    j["name"] = "1-1 起点";
    j["width"] = 40;
    j["height"] = 20;
    j["tile_size"] = 32;
    j["mode"] = "2d_sidescroller";
    j["bgm"] = {{"name", ""}, {"volume", 80}, {"pitch", 100}};
    j["background_color"] = "#87CEEB";

    std::vector<int> decor(40 * 20, 0);
    std::vector<int> terrain(40 * 20, 0);
    std::vector<int> collision(40 * 20, 0);

    // 底部铺一层地面
    for (int x = 0; x < 40; ++x) {
        terrain[x + 17 * 40] = 1;
        collision[x + 17 * 40] = 1;
    }
    // 几个平台
    for (int x = 8; x < 14; ++x) {
        terrain[x + 13 * 40] = 2;
        collision[x + 13 * 40] = 2; // platform
    }
    for (int x = 22; x < 28; ++x) {
        terrain[x + 10 * 40] = 2;
        collision[x + 10 * 40] = 2;
    }

    j["layers"] = json::array();
    j["layers"].push_back({
        {"name", "背景装饰"},
        {"type", "tile"},
        {"tileset", "tilesets/Stage1_Decor.tileset.json"},
        {"data", decor},
        {"visible", true},
        {"parallax_factor", 0.3}
    });
    j["layers"].push_back({
        {"name", "地形"},
        {"type", "tile"},
        {"tileset", "tilesets/Stage1_Ground.tileset.json"},
        {"data", terrain},
        {"visible", true}
    });
    j["layers"].push_back({
        {"name", "碰撞"},
        {"type", "collision"},
        {"data", collision},
        {"collision_shapes", {{"1", "solid"}, {"2", "platform"}}}
    });
    j["layers"].push_back({
        {"name", "实体层"},
        {"type", "entity"},
        {"entities", json::array({
            {{"prefab", "prefabs/player.prefab.json"}, {"x", 3}, {"y", 15}, {"overrides", nullptr}}
        })}
    });
    j["events"] = json::array();
    j["camera_bounds"] = {{"min_x", 0}, {"max_x", 1280}, {"min_y", 0}, {"max_y", 640}};
    j["rooms"] = json::array();
    return j;
}

static json MakeEmptyDataArray() {
    return json::array();
}

static json MakeTilesetIndex() {
    return json::array();
}

static std::string GitIgnoreContent() {
    return
        "# MakeGames generated cache\n"
        ".cache/\n"
        ".import/\n"
        "\n"
        "# Editor local settings\n"
        "project.settings.json\n"
        "\n"
        "# OS files\n"
        ".DS_Store\n"
        "Thumbs.db\n"
        "desktop.ini\n";
}

static bool WriteJsonFile(const std::string& path, const json& j) {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    std::ofstream f(path);
    if (!f.is_open()) {
        fprintf(stderr, "[ProjectManager] 无法写入文件: %s\n", path.c_str());
        return false;
    }
    f << j.dump(4);
    return f.good();
}

static bool WriteTextFile(const std::string& path, const std::string& content) {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    std::ofstream f(path);
    if (!f.is_open()) {
        fprintf(stderr, "[ProjectManager] 无法写入文件: %s\n", path.c_str());
        return false;
    }
    f << content;
    return f.good();
}

// ── ProjectManager 实现 ────────────────────────────

// 从 launcher/projects.json 加载项目列表
bool ProjectManager::LoadProjects() {
    if (!std::filesystem::exists(config_path_)) {
        // 首次启动：项目列表为空
        return true;
    }

    try {
        std::ifstream file(config_path_);
        json j;
        file >> j;

        for (const auto& item : j["projects"]) {
            ProjectInfo info;
            info.name = item.value("name", "");
            info.path = item.value("path", "");
            info.engine_version = item.value("engine_version", "0.1.0");
            info.mode = item.value("mode", "empty");
            info.last_opened = item.value("last_opened", "");
            info.created_at = item.value("created_at", "");
            info.start_scene = item.value("start_scene", "");
            projects_.push_back(info);
        }
    } catch (const std::exception& e) {
        fprintf(stderr, "[ProjectManager] LoadProjects 失败: %s (%s)\n", config_path_.c_str(), e.what());
        return false;
    }
    return true;
}

// 将项目列表保存到 launcher/projects.json
void ProjectManager::SaveProjects() {
    std::filesystem::create_directories(std::filesystem::path(config_path_).parent_path());

    json j;
    j["projects"] = json::array();
    for (const auto& p : projects_) {
        json item;
        item["name"] = p.name;
        item["path"] = p.path;
        item["engine_version"] = p.engine_version;
        item["mode"] = p.mode;
        item["last_opened"] = p.last_opened;
        item["created_at"] = p.created_at;
        item["start_scene"] = p.start_scene;
        j["projects"].push_back(item);
    }

    std::ofstream file(config_path_);
    if (!file.is_open()) {
        fprintf(stderr, "[ProjectManager] 无法保存项目列表: %s\n", config_path_.c_str());
        return;
    }
    file << j.dump(4);
}

// 添加项目到内存列表并立即保存
void ProjectManager::AddProject(const ProjectInfo& info) {
    projects_.push_back(info);
    SaveProjects();
}

// 从内存列表移除项目并保存
void ProjectManager::RemoveProject(size_t index) {
    if (index < projects_.size()) {
        projects_.erase(projects_.begin() + index);
        SaveProjects();
    }
}

// 创建新项目目录结构：assets/、data/、maps/、prefabs/ 等，
// 根据模板模式生成默认地图和配置文件
bool ProjectManager::CreateProject(const std::string& name, const std::string& path, const std::string& mode) {
    try {
        namespace fs = std::filesystem;
        fs::create_directories(path + "/assets/sprites");
        fs::create_directories(path + "/assets/tilesets");
        fs::create_directories(path + "/assets/backgrounds");
        fs::create_directories(path + "/assets/animations");
        fs::create_directories(path + "/assets/audio/bgm");
        fs::create_directories(path + "/assets/audio/bgs");
        fs::create_directories(path + "/assets/audio/se");
        fs::create_directories(path + "/assets/audio/me");
        fs::create_directories(path + "/assets/fonts");
        fs::create_directories(path + "/assets/scripts");
        fs::create_directories(path + "/data");
        fs::create_directories(path + "/maps");
        fs::create_directories(path + "/scenes");
        fs::create_directories(path + "/prefabs");
        fs::create_directories(path + "/.cache/imported");
        fs::create_directories(path + "/.cache/compiled");

        // project.json（关键文件，写失败则整个创建失败）
        if (!WriteJsonFile(path + "/project.json", MakeProjectJson(name, mode)))
            return false;

        // .gitignore
        WriteTextFile(path + "/.gitignore", GitIgnoreContent());

        // data/
        WriteJsonFile(path + "/data/actors.json", MakeEmptyDataArray());
        WriteJsonFile(path + "/data/classes.json", MakeEmptyDataArray());
        WriteJsonFile(path + "/data/items.json", MakeEmptyDataArray());
        WriteJsonFile(path + "/data/skills.json", MakeEmptyDataArray());
        WriteJsonFile(path + "/data/enemies.json", MakeEmptyDataArray());
        WriteJsonFile(path + "/data/troops.json", MakeEmptyDataArray());
        WriteJsonFile(path + "/data/states.json", MakeEmptyDataArray());
        WriteJsonFile(path + "/data/animations.json", MakeEmptyDataArray());
        WriteJsonFile(path + "/data/tilesets.json", MakeTilesetIndex());
        WriteJsonFile(path + "/data/system.json", json::object());
        WriteJsonFile(path + "/data/common_events.json", MakeEmptyDataArray());

        // maps/ — 留空，由 Editor 首次打开时生成默认场景
        fs::create_directories(path + "/maps");

        // MapInfos.json — 初始为空
        WriteJsonFile(path + "/maps/MapInfos.json", json::array());

        // scenes/
        // 预留空白

        // prefabs/
        // 预留空白

        // 记录到 launcher 项目列表
        auto now = std::time(nullptr);
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", std::localtime(&now));

        ProjectInfo info;
        info.name = name;
        info.path = path;
        info.engine_version = "0.1.0";
        info.mode = mode;
        info.last_opened = buf;
        info.created_at = buf;
        info.start_scene = "maps/Map001.json";
        AddProject(info);

        return true;
    } catch (const std::exception& e) {
        fprintf(stderr, "[ProjectManager] CreateProject 失败: %s (%s)\n", path.c_str(), e.what());
        return false;
    }
}

// 从磁盘加载已有项目（读取 project.json），若已存在则更新 last_opened
bool ProjectManager::LoadProjectFromDisk(const std::string& project_path) {
    try {
        std::ifstream file(project_path + "/project.json");
        if (!file.is_open()) return false;

        nlohmann::json j;
        file >> j;

        auto manifest = mg::ProjectManifest::FromJson(j);
        if (!manifest) return false;

        ProjectInfo info;
        info.name = manifest->name;
        info.path = project_path;
        info.engine_version = manifest->engine_version;
        info.mode = manifest->mode;
        info.last_opened = j.value("last_opened", "");
        info.created_at = j.value("created_at", "");
        info.start_scene = manifest->start_scene;

        for (auto& p : projects_) {
            if (p.path == project_path) {
                auto now = std::time(nullptr);
                char buf[64];
                std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", std::localtime(&now));
                p.last_opened = buf;
                SaveProjects();
                return true;
            }
        }

        AddProject(info);
        return true;
    } catch (const std::exception& e) {
        fprintf(stderr, "[ProjectManager] LoadProjectFromDisk 失败: %s (%s)\n", project_path.c_str(), e.what());
        return false;
    }
}

// 启动时验证：清理 project.json 已不存在的项目目录
bool ProjectManager::ValidateProjects() {
    bool removed = false;
    for (auto it = projects_.begin(); it != projects_.end(); ) {
        if (!std::filesystem::exists(it->path + "/project.json")) {
            it = projects_.erase(it);
            removed = true;
        } else {
            ++it;
        }
    }
    if (removed) SaveProjects();
    return removed;
}

// 重新定位项目路径（项目被移动后手动修复路径）
bool ProjectManager::RelocateProject(size_t index, const std::string& new_path) {
    if (index >= projects_.size()) return false;
    if (!std::filesystem::exists(new_path + "/project.json")) return false;

    try {
        std::ifstream file(new_path + "/project.json");
        json j;
        file >> j;

        projects_[index].path = new_path;
        projects_[index].name = j.value("name", projects_[index].name);
        projects_[index].engine_version = j.value("engine_version", projects_[index].engine_version);
        projects_[index].mode = j.value("mode", projects_[index].mode);
        projects_[index].start_scene = j.value("start_scene", projects_[index].start_scene);

        auto now = std::time(nullptr);
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", std::localtime(&now));
        projects_[index].last_opened = buf;

        SaveProjects();
        return true;
    } catch (const std::exception& e) {
        fprintf(stderr, "[ProjectManager] RelocateProject 失败: %s (%s)\n", new_path.c_str(), e.what());
        return false;
    }
}

} // namespace launcher
} // namespace mg
