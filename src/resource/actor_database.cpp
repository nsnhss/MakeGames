#include "actor_database.h"
#include <algorithm>
#include <cmath>

namespace mg {

std::optional<ActorDatabase> ActorDatabase::FromJson(const nlohmann::json& j) {
    ActorDatabase db;
    if (!j.is_array()) return db;
    for (const auto& entry : j) {
        if (!entry.is_object()) continue;
        ActorDef def;
        if (entry.contains("id")) def.id = entry["id"].get<std::string>();
        if (entry.contains("name")) def.name = entry["name"].get<std::string>();
        if (entry.contains("hp")) def.hp = entry["hp"].get<int>();
        if (entry.contains("mp")) def.mp = entry["mp"].get<int>();
        if (entry.contains("atk")) def.atk = entry["atk"].get<int>();
        if (entry.contains("def")) def.def = entry["def"].get<int>();
        if (entry.contains("sprite")) def.sprite = entry["sprite"].get<std::string>();
        // 成长曲线字段（缺省回退：peak = base，即平坦曲线，向后兼容旧格式）
        if (entry.contains("max_level")) def.max_level = entry["max_level"].get<int>();
        if (entry.contains("hp_peak")) def.hp_peak = entry["hp_peak"].get<int>(); else def.hp_peak = def.hp;
        if (entry.contains("mp_peak")) def.mp_peak = entry["mp_peak"].get<int>(); else def.mp_peak = def.mp;
        if (entry.contains("atk_peak")) def.atk_peak = entry["atk_peak"].get<int>(); else def.atk_peak = def.atk;
        if (entry.contains("def_peak")) def.def_peak = entry["def_peak"].get<int>(); else def.def_peak = def.def;
        if (entry.contains("growth_exponent")) def.growth_exponent = entry["growth_exponent"].get<float>();
        if (def.max_level < 1) def.max_level = 1;
        if (!def.id.empty()) db.actors[def.id] = std::move(def);
    }
    return db;
}

const ActorDatabase::ActorDef* ActorDatabase::Find(const std::string& id) const {
    auto it = actors.find(id);
    return (it != actors.end()) ? &it->second : nullptr;
}

nlohmann::json ActorDatabase::ToJson() const {
    nlohmann::json arr = nlohmann::json::array();
    std::vector<std::string> ids;
    ids.reserve(actors.size());
    for (const auto& [id, _] : actors) ids.push_back(id);
    std::sort(ids.begin(), ids.end());
    for (const auto& id : ids) {
        const auto& def = actors.at(id);
        nlohmann::json entry;
        entry["id"] = def.id;
        entry["name"] = def.name;
        entry["hp"] = def.hp;
        entry["mp"] = def.mp;
        entry["atk"] = def.atk;
        entry["def"] = def.def;
        entry["sprite"] = def.sprite;
        entry["max_level"] = def.max_level;
        entry["hp_peak"] = def.hp_peak;
        entry["mp_peak"] = def.mp_peak;
        entry["atk_peak"] = def.atk_peak;
        entry["def_peak"] = def.def_peak;
        entry["growth_exponent"] = def.growth_exponent;
        arr.push_back(entry);
    }
    return arr;
}

int EvalStatCurve(int base, int peak, float exponent, int level, int max_level) {
    if (max_level <= 1) return base;
    if (level < 1) level = 1;
    if (level > max_level) level = max_level;
    if (exponent < 0.05f) exponent = 0.05f;
    if (exponent > 10.0f) exponent = 10.0f;
    float t = (float)(level - 1) / (float)(max_level - 1);   // [0,1]
    float shaped = std::pow(t, exponent);
    return (int)std::lround((float)base + (float)(peak - base) * shaped);
}

ActorBattleStats EvalActorBattleStats(const ActorDatabase::ActorDef& def, int level) {
    return {
        EvalStatCurve(def.hp,  def.hp_peak,  def.growth_exponent, level, def.max_level),
        EvalStatCurve(def.mp,  def.mp_peak,  def.growth_exponent, level, def.max_level),
        EvalStatCurve(def.atk, def.atk_peak, def.growth_exponent, level, def.max_level),
        EvalStatCurve(def.def, def.def_peak, def.growth_exponent, level, def.max_level),
    };
}

} // namespace mg
