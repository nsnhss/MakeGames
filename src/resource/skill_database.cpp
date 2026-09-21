#include "skill_database.h"
#include <algorithm>

namespace mg {

std::optional<SkillDatabase> SkillDatabase::FromJson(const nlohmann::json& j) {
    SkillDatabase db;
    if (!j.is_array()) return db;
    for (const auto& entry : j) {
        if (!entry.is_object()) continue;
        SkillDef def;
        if (entry.contains("id")) def.id = entry["id"].get<std::string>();
        if (entry.contains("name")) def.name = entry["name"].get<std::string>();
        if (entry.contains("mp_cost")) def.mp_cost = entry["mp_cost"].get<int>();
        if (entry.contains("damage")) def.damage = entry["damage"].get<int>();
        if (entry.contains("description")) def.description = entry["description"].get<std::string>();
        if (!def.id.empty()) db.skills[def.id] = std::move(def);
    }
    return db;
}

const SkillDatabase::SkillDef* SkillDatabase::Find(const std::string& id) const {
    auto it = skills.find(id);
    return (it != skills.end()) ? &it->second : nullptr;
}

nlohmann::json SkillDatabase::ToJson() const {
    nlohmann::json arr = nlohmann::json::array();
    std::vector<std::string> ids;
    ids.reserve(skills.size());
    for (const auto& [id, _] : skills) ids.push_back(id);
    std::sort(ids.begin(), ids.end());
    for (const auto& id : ids) {
        const auto& def = skills.at(id);
        nlohmann::json entry;
        entry["id"] = def.id;
        entry["name"] = def.name;
        entry["mp_cost"] = def.mp_cost;
        entry["damage"] = def.damage;
        entry["description"] = def.description;
        arr.push_back(entry);
    }
    return arr;
}

} // namespace mg
