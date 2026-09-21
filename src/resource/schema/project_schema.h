#pragma once
#include "third_party/json.hpp"
#include <string>
#include <optional>

namespace mg {

// 项目清单（project.json）
struct ProjectManifest {
    std::string name;
    std::string engine_version = "0.1.0";
    std::string mode = "2d_topdown";   // "2d_topdown" | "2d_sidescroller"
    std::string start_scene;           // 相对路径，如 "maps/Map001.json"
    int tile_size = 32;
    int display_width = 1280;
    int display_height = 720;

    static std::optional<ProjectManifest> FromJson(const nlohmann::json& j) {
        if (!j.is_object()) return std::nullopt;
        ProjectManifest m;
        m.name = j.value("name", "");
        m.engine_version = j.value("engine_version", "0.1.0");
        m.mode = j.value("mode", "2d_topdown");
        m.start_scene = j.value("start_scene", "");
        m.tile_size = j.value("tile_size", 32);
        if (j.contains("display")) {
            const auto& d = j["display"];
            m.display_width = d.value("width", 1280);
            m.display_height = d.value("height", 720);
        }
        if (m.name.empty()) return std::nullopt;
        return m;
    }

    nlohmann::json ToJson() const {
        nlohmann::json j;
        j["name"] = name;
        j["engine_version"] = engine_version;
        j["mode"] = mode;
        j["start_scene"] = start_scene;
        j["tile_size"] = tile_size;
        j["display"] = {
            {"width", display_width},
            {"height", display_height},
            {"fullscreen", false},
            {"vsync", true},
            {"pixel_perfect", true}
        };
        return j;
    }
};

// 地图索引条目
struct MapInfoEntry {
    int id = 0;
    std::string name;
    int parent_id = 0;
};

// 地图列表索引（maps/MapInfos.json）
struct MapInfos {
    std::vector<MapInfoEntry> entries;

    static std::optional<MapInfos> FromJson(const nlohmann::json& j) {
        if (!j.is_array()) return std::nullopt;
        MapInfos mi;
        for (const auto& item : j) {
            MapInfoEntry e;
            e.id = item.value("id", 0);
            e.name = item.value("name", "");
            e.parent_id = item.value("parent_id", 0);
            mi.entries.push_back(e);
        }
        return mi;
    }

    nlohmann::json ToJson() const {
        nlohmann::json j = nlohmann::json::array();
        for (const auto& e : entries) {
            j.push_back({{"id", e.id}, {"name", e.name}, {"parent_id", e.parent_id}});
        }
        return j;
    }
};

} // namespace mg
