#pragma once
#include "guid.h"
#include "third_party/json.hpp"
#include <string>
#include <fstream>

namespace mg {

// 资产元数据文件（.meta.json）
struct MetaFile {
    std::string guid;
    std::string asset_path;
    nlohmann::json import_settings;

    static MetaFile LoadOrCreate(const std::string& asset_path) {
        MetaFile mf;
        mf.asset_path = asset_path;
        std::string meta_path = asset_path + ".meta.json";
        std::ifstream f(meta_path);
        if (f.is_open()) {
            try {
                nlohmann::json j;
                f >> j;
                mf.guid = j.value("guid", "");
                mf.import_settings = j.value("import_settings", nlohmann::json::object());
            } catch (...) {}
        }
        if (mf.guid.empty()) {
            mf.guid = GenerateUUIDv4();
            mf.Save();
        }
        return mf;
    }

    void Save() const {
        nlohmann::json j;
        j["guid"] = guid;
        j["import_settings"] = import_settings;
        std::ofstream f(asset_path + ".meta.json");
        f << j.dump(4);
    }
};

} // namespace mg
