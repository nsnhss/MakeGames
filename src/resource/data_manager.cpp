#include "data_manager.h"
#include "src/editor/debug_log.h"

namespace mg {

DataManager& DataManager::Get() {
    static DataManager instance;
    return instance;
}

std::optional<nlohmann::json> DataManager::LoadJsonRaw(const std::string& path) {
    EDITOR_LOG("DataManager::LoadJsonRaw path=" << path);
    std::ifstream f(path);
    if (!f.is_open()) {
        EDITOR_LOG("DataManager::LoadJsonRaw failed to open path=" << path);
        return std::nullopt;
    }
    try {
        nlohmann::json j;
        f >> j;
        return j;
    } catch (...) {
        EDITOR_LOG("DataManager::LoadJsonRaw parse exception path=" << path);
        return std::nullopt;
    }
}

std::vector<std::string> DataManager::CheckForChanges() {
    std::vector<std::string> changed;
    for (auto& [path, entry] : cache_) {
        if (!std::filesystem::exists(path)) {
            changed.push_back(path);
            continue;
        }
        auto mtime = std::filesystem::last_write_time(path);
        if (entry.mtime != mtime) {
            changed.push_back(path);
        }
    }
    return changed;
}

void DataManager::Invalidate(const std::string& path) {
    cache_.erase(path);
}

void DataManager::InvalidateAll() {
    cache_.clear();
}

} // namespace mg
