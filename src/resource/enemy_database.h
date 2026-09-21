#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include "third_party/json.hpp"

namespace mg {

struct EnemyDatabase {
    struct EnemyDef {
        std::string id;
        std::string name;
        int hp = 0;
        int atk = 0;
        int def = 0;
        int gold = 0;
        int exp = 0;
        std::string sprite;
    };

    std::unordered_map<std::string, EnemyDef> enemies;

    static std::optional<EnemyDatabase> FromJson(const nlohmann::json& j);
    nlohmann::json ToJson() const;
    const EnemyDef* Find(const std::string& id) const;
    bool IsEmpty() const { return enemies.empty(); }
};

} // namespace mg
