#pragma once
#include "third_party/json.hpp"
#include <string>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include <fstream>

namespace mg {

// 通用 JSON 数据文件管理器
// 职责：文件 I/O + 内存缓存 + 热重载检测
// SceneSerializer 继续负责 ECS World <-> JSON 转换，不替代
class DataManager {
public:
    static DataManager& Get();

    // 读取 JSON 文件（自动缓存）
    template<typename Schema>
    std::shared_ptr<const Schema> Load(const std::string& path) {
        try {
            auto it = cache_.find(path);
            if (it != cache_.end()) {
                if (std::filesystem::exists(path)) {
                    auto mtime = std::filesystem::last_write_time(path);
                    if (it->second.mtime == mtime) {
                        return std::static_pointer_cast<const Schema>(it->second.data);
                    }
                }
            }
            std::ifstream f(path);
            if (!f.is_open()) return nullptr;
            nlohmann::json j;
            f >> j;
            auto ptr = std::make_shared<Schema>();
            auto opt = Schema::FromJson(j);
            if (!opt) return nullptr;
            *ptr = *opt;
            CacheEntry entry;
            entry.path = path;
            if (std::filesystem::exists(path)) {
                entry.mtime = std::filesystem::last_write_time(path);
            }
            entry.data = ptr;
            cache_[path] = std::move(entry);
            return ptr;
        } catch (...) {
            return nullptr;
        }
    }

    // 直接读取原始 json（不缓存到模板缓存，用于 SceneSerializer 等）
    std::optional<nlohmann::json> LoadJsonRaw(const std::string& path);

    // 写入 JSON 文件（同时更新缓存）
    template<typename Schema>
    bool Save(const std::string& path, const Schema& data) {
        try {
            std::filesystem::create_directories(std::filesystem::path(path).parent_path());
            nlohmann::json j;
            if constexpr (requires { &Schema::ToJson; }) {
                j = data.ToJson();
            } else {
                j = data;
            }
            std::ofstream f(path);
            f << j.dump(4);

            auto ptr = std::make_shared<Schema>(data);
            CacheEntry entry;
            entry.path = path;
            if (std::filesystem::exists(path)) {
                entry.mtime = std::filesystem::last_write_time(path);
            }
            entry.data = ptr;
            cache_[path] = std::move(entry);
            return true;
        } catch (...) {
            return false;
        }
    }

    // 检测文件变更 → 返回变更的文件路径列表
    std::vector<std::string> CheckForChanges();

    // 手动清除缓存
    void Invalidate(const std::string& path);
    void InvalidateAll();

private:
    struct CacheEntry {
        std::string path;
        std::filesystem::file_time_type mtime;
        std::shared_ptr<const void> data;
    };
    std::unordered_map<std::string, CacheEntry> cache_;
};

} // namespace mg
