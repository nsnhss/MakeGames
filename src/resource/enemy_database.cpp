#include "enemy_database.h"
#include <algorithm>

namespace mg {

std::optional<EnemyDatabase> EnemyDatabase::FromJson(const nlohmann::json& j) {
    EnemyDatabase db;
    if (!j.is_array()) return db;
    for (const auto& entry : j) {
        if (!entry.is_object()) continue;
        EnemyDef def;
        if (entry.contains("id")) def.id = entry["id"].get<std::string>();
        if (entry.contains("name")) def.name = entry["name"].get<std::string>();
        if (entry.contains("hp")) def.hp = entry["hp"].get<int>();
        if (entry.contains("atk")) def.atk = entry["atk"].get<int>();
        if (entry.contains("def")) def.def = entry["def"].get<int>();
        if (entry.contains("gold")) def.gold = entry["gold"].get<int>();
        if (entry.contains("exp")) def.exp = entry["exp"].get<int>();
        if (entry.contains("sprite")) def.sprite = entry["sprite"].get<std::string>();
        if (!def.id.empty()) db.enemies[def.id] = std::move(def);
    }
    return db;
}

const EnemyDatabase::EnemyDef* EnemyDatabase::Find(const std::string& id) const {
    auto it = enemies.find(id);
    return (it != enemies.end()) ? &it->second : nullptr;
}

nlohmann::json EnemyDatabase::ToJson() const {
    nlohmann::json arr = nlohmann::json::array();
    std::vector<std::string> ids;
    ids.reserve(enemies.size());
    for (const auto& [id, _] : enemies) ids.push_back(id);
    std::sort(ids.begin(), ids.end());
    for (const auto& id : ids) {
        const auto& def = enemies.at(id);
        nlohmann::json entry;
        entry["id"] = def.id;
        entry["name"] = def.name;
        entry["hp"] = def.hp;
        entry["atk"] = def.atk;
        entry["def"] = def.def;
        entry["gold"] = def.gold;
        entry["exp"] = def.exp;
        entry["sprite"] = def.sprite;
        arr.push_back(entry);
    }
    return arr;
}

} // namespace mg
