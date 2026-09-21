#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include "third_party/json.hpp"

namespace mg {

struct SkillDatabase {
    struct SkillDef {
        std::string id;
        std::string name;
        int mp_cost = 0;
        int damage = 0;
        std::string description;
    };

    std::unordered_map<std::string, SkillDef> skills;

    static std::optional<SkillDatabase> FromJson(const nlohmann::json& j);
    nlohmann::json ToJson() const;
    const SkillDef* Find(const std::string& id) const;
    bool IsEmpty() const { return skills.empty(); }
};

} // namespace mg
