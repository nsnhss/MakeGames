#include "scene/game_variables.h"
#include <algorithm>

namespace mg {
namespace scene {

void GameVariables::SetInt(const std::string& name, int value) {
    int_variables_[name] = value;
    Notify(name);
}

void GameVariables::SetIntSilent(const std::string& name, int value) {
    int_variables_[name] = value;
}

int GameVariables::GetInt(const std::string& name, int default_val) const {
    auto it = int_variables_.find(name);
    return (it != int_variables_.end()) ? it->second : default_val;
}

void GameVariables::SetFloat(const std::string& name, float value) {
    float_variables_[name] = value;
}

float GameVariables::GetFloat(const std::string& name, float default_val) const {
    auto it = float_variables_.find(name);
    return (it != float_variables_.end()) ? it->second : default_val;
}

void GameVariables::SetBool(const std::string& name, bool value) {
    bool_variables_[name] = value;
}

bool GameVariables::GetBool(const std::string& name, bool default_val) const {
    auto it = bool_variables_.find(name);
    return (it != bool_variables_.end()) ? it->second : default_val;
}

void GameVariables::SetSwitch(const std::string& name, bool value) {
    switches_[name] = value;
}

bool GameVariables::GetSwitch(const std::string& name, bool default_val) const {
    auto it = switches_.find(name);
    return (it != switches_.end()) ? it->second : default_val;
}

void GameVariables::SetSelfSwitch(EntityID entity, char sw, bool value) {
    self_switches_[SelfSwitchKey(entity, sw)] = value;
}

const bool* GameVariables::FindSelfSwitch(EntityID entity, char sw) const {
    auto it = self_switches_.find(SelfSwitchKey(entity, sw));
    return (it != self_switches_.end()) ? &it->second : nullptr;
}

void GameVariables::AddItem(const std::string& item_id, int count) {
    std::string key = "item_" + item_id;
    if (count <= 0) count = 1;
    int_variables_[key] += count;
    Notify(key);
}

void GameVariables::RemoveItem(const std::string& item_id, int count) {
    std::string key = "item_" + item_id;
    if (count <= 0) count = 1;
    auto it = int_variables_.find(key);
    if (it != int_variables_.end()) {
        it->second = std::max(0, it->second - count);
        if (it->second == 0) int_variables_.erase(it);
    }
    Notify(key); // 与原行为一致：即使 key 不存在也触发
}

std::vector<std::pair<std::string, int>> GameVariables::GetInventoryItems() const {
    std::vector<std::pair<std::string, int>> items;
    for (const auto& [key, value] : int_variables_) {
        if (key.rfind("item_", 0) == 0 && value > 0) {
            items.emplace_back(key.substr(5), value);
        }
    }
    return items;
}

std::string GameVariables::FindFirstInventoryKey() const {
    for (const auto& [k, v] : int_variables_) {
        if (k.rfind("item_", 0) == 0 && v > 0) return k;
    }
    return {};
}

void GameVariables::DecrementItemRaw(const std::string& key) {
    auto it = int_variables_.find(key);
    if (it != int_variables_.end()) --it->second;
}

GameVariables::Snapshot GameVariables::TakeSnapshot() const {
    Snapshot snap;
    snap.int_vars      = int_variables_;
    snap.float_vars    = float_variables_;
    snap.bool_vars     = bool_variables_;
    snap.switches      = switches_;
    snap.self_switches = self_switches_;
    return snap;
}

void GameVariables::Restore(const Snapshot& snap) {
    int_variables_   = snap.int_vars;
    float_variables_ = snap.float_vars;
    bool_variables_  = snap.bool_vars;
    switches_        = snap.switches;
    self_switches_   = snap.self_switches;
}

nlohmann::json GameVariables::SaveToJson() const {
    nlohmann::json j;
    auto& g = j["globals"];
    g["int_vars"]      = int_variables_;
    g["float_vars"]    = float_variables_;
    g["bool_vars"]     = bool_variables_;
    g["switches"]      = switches_;
    g["self_switches"] = self_switches_;
    return j;
}

void GameVariables::LoadFromJson(const nlohmann::json& j) {
    if (!j.contains("globals")) return;
    const auto& g = j["globals"];
    if (g.contains("int_vars"))      int_variables_   = g["int_vars"].get<std::unordered_map<std::string, int>>();
    if (g.contains("float_vars"))    float_variables_ = g["float_vars"].get<std::unordered_map<std::string, float>>();
    if (g.contains("bool_vars"))     bool_variables_  = g["bool_vars"].get<std::unordered_map<std::string, bool>>();
    if (g.contains("switches"))      switches_        = g["switches"].get<std::unordered_map<std::string, bool>>();
    if (g.contains("self_switches")) self_switches_   = g["self_switches"].get<std::unordered_map<std::string, bool>>();
}

} // namespace scene
} // namespace mg
