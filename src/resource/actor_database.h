#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include "third_party/json.hpp"

namespace mg {

struct ActorDatabase {
    struct ActorDef {
        std::string id;
        std::string name;
        int hp = 0;   // Lv1 基础值（成长曲线起点）
        int mp = 0;
        int atk = 0;
        int def = 0;
        std::string sprite;

        // 成长曲线：value(level) = base + (peak-base) * pow(t, exponent)
        // t = (level-1)/(max_level-1)；exponent 1=线性、<1 早熟、>1 晚成
        int max_level = 99;
        int hp_peak = 0;
        int mp_peak = 0;
        int atk_peak = 0;
        int def_peak = 0;
        float growth_exponent = 1.0f;
    };

    std::unordered_map<std::string, ActorDef> actors;

    static std::optional<ActorDatabase> FromJson(const nlohmann::json& j);
    nlohmann::json ToJson() const;
    const ActorDef* Find(const std::string& id) const;
    bool IsEmpty() const { return actors.empty(); }
};

// 成长曲线求值：level 钳制 [1, max_level]；max_level<=1 时返回 base。
// exponent 内部钳制 [0.05, 10]，防 pow 奇异。
int EvalStatCurve(int base, int peak, float exponent, int level, int max_level);

// 战斗用四参数：按等级同时求 hp/mp/atk/def（纯逻辑，供 EventSystem::StartBattle 与单测）
struct ActorBattleStats { int hp, mp, atk, def; };
ActorBattleStats EvalActorBattleStats(const ActorDatabase::ActorDef& def, int level);

} // namespace mg
