// event_system_battle.cpp — 回合制战斗（从 event_system.cpp 拆分，保持类成员语义不变）
#include "scene/event_system.h"
#include <algorithm>
#include <cstdlib>

namespace mg {
namespace scene {

void EventSystem::StartBattle(const std::string& enemy_id, int count, const std::string& player_actor_id) {
    const auto* enemy_def = enemy_db_ ? enemy_db_->Find(enemy_id) : nullptr;
    if (!enemy_def) return;

    std::string actor_id = player_actor_id;
    if (actor_id.empty()) actor_id = "Player";
    const auto* actor_def = actor_db_ ? actor_db_->Find(actor_id) : nullptr;
    if (!actor_def) return;

    // 成长曲线接入：按等级变量求战斗数值（未设等级 → Lv1 = 基础值，行为与旧版一致）
    int level = GetVariableInt("level", 1);
    ActorBattleStats stats = EvalActorBattleStats(*actor_def, level);
    battle_player_ = {actor_def->id, actor_def->name, stats.hp, stats.hp,
                      stats.mp, stats.mp, stats.atk, stats.def};
    enemies_.clear();
    int n = count > 0 ? count : 1;
    for (int i = 0; i < n; ++i) {
        std::string name = enemy_def->name;
        if (n > 1) name += " " + std::to_string(i + 1);
        enemies_.push_back({enemy_def->id, name, enemy_def->hp, enemy_def->hp,
                            0, 0, enemy_def->atk, enemy_def->def});
    }
    battle_target_ = 0;
    battle_log_ = n > 1 ? (std::to_string(n) + " " + enemy_def->name + "s appear!")
                        : ("A wild " + enemy_def->name + " appears!");
    battle_timer_ = 0.0f;
    battle_state_ = BattleState::Intro;
}

void EventSystem::EndBattle(int result) {
    if (result == 1) { // Victory
        int total_gold = 0, total_exp = 0;
        const auto* edb = enemy_db_;
        for (const auto& e : enemies_) {
            if (!edb) break;
            if (auto* def = edb->Find(e.id)) {
                total_gold += def->gold;
                total_exp += def->exp;
            }
        }
        SetVariable("gold", GetVariableInt("gold", 0) + total_gold);
        SetVariable("exp", GetVariableInt("exp", 0) + total_exp);
    }
    SetVariable("battle_result", result);
    battle_state_ = BattleState::None;
    battle_player_ = {};
    enemies_.clear();
    battle_target_ = 0;
    battle_log_.clear();
    battle_timer_ = 0.0f;
}

static bool AllEnemiesDefeated(const std::vector<EventSystem::BattleCombatant>& enemies) {
    for (const auto& e : enemies) if (e.hp > 0) return false;
    return true;
}

static int NextAliveEnemy(const std::vector<EventSystem::BattleCombatant>& enemies, int start) {
    for (int i = 0; i < (int)enemies.size(); ++i) {
        int idx = (start + i) % (int)enemies.size();
        if (enemies[idx].hp > 0) return idx;
    }
    return -1;
}

void EventSystem::PlayerAttack() {
    if (battle_state_ != BattleState::PlayerTurn) return;
    if (enemies_.empty()) return;
    if (battle_target_ < 0 || battle_target_ >= (int)enemies_.size() || enemies_[battle_target_].hp <= 0) {
        battle_target_ = NextAliveEnemy(enemies_, battle_target_);
        if (battle_target_ < 0) {
            battle_state_ = BattleState::Victory;
            battle_log_ = "All enemies defeated!";
            return;
        }
    }
    auto& target = enemies_[battle_target_];
    int damage = std::max(1, battle_player_.atk - target.def);
    target.hp = std::max(0, target.hp - damage);
    battle_log_ = battle_player_.name + " attacks " + target.name + " for " + std::to_string(damage) + " damage!";
    if (AllEnemiesDefeated(enemies_)) {
        battle_state_ = BattleState::Victory;
        battle_log_ += "\nAll enemies defeated!";
    } else if (target.hp <= 0) {
        battle_log_ += "\n" + target.name + " defeated!";
        int next = NextAliveEnemy(enemies_, battle_target_);
        if (next >= 0) battle_target_ = next;
        battle_state_ = BattleState::EnemyTurn;
        battle_timer_ = 0.0f;
    } else {
        battle_state_ = BattleState::EnemyTurn;
        battle_timer_ = 0.0f;
    }
}

void EventSystem::PlayerUseSkill(const std::string& skill_id) {
    if (battle_state_ != BattleState::PlayerTurn) return;
    if (enemies_.empty()) return;
    const auto* skill = skill_db_ ? skill_db_->Find(skill_id) : nullptr;
    if (!skill) {
        battle_log_ = "Skill not found!";
        return;
    }
    if (battle_player_.mp < skill->mp_cost) {
        battle_log_ = "Not enough MP!";
        return;
    }
    if (battle_target_ < 0 || battle_target_ >= (int)enemies_.size() || enemies_[battle_target_].hp <= 0) {
        battle_target_ = NextAliveEnemy(enemies_, battle_target_);
        if (battle_target_ < 0) {
            battle_state_ = BattleState::Victory;
            battle_log_ = "All enemies defeated!";
            return;
        }
    }
    auto& target = enemies_[battle_target_];
    battle_player_.mp = std::max(0, battle_player_.mp - skill->mp_cost);
    int damage = std::max(1, battle_player_.atk + skill->damage - target.def);
    target.hp = std::max(0, target.hp - damage);
    battle_log_ = battle_player_.name + " uses " + skill->name + " on " + target.name + " for " + std::to_string(damage) + " damage!";
    if (AllEnemiesDefeated(enemies_)) {
        battle_state_ = BattleState::Victory;
        battle_log_ += "\nAll enemies defeated!";
    } else {
        battle_state_ = BattleState::EnemyTurn;
        battle_timer_ = 0.0f;
    }
}

void EventSystem::PlayerUseItem() {
    if (battle_state_ != BattleState::PlayerTurn) return;
    // 简化物品系统：任一 item_* 数量 > 0 的物品可消耗，恢复 30 HP
    std::string item_key;
    for (auto& [k, v] : int_variables_) {
        if (k.rfind("item_", 0) == 0 && v > 0) {
            item_key = k;
            break;
        }
    }
    if (item_key.empty()) {
        battle_log_ = "No items!";
        return;
    }
    int heal = 30;
    int before = battle_player_.hp;
    battle_player_.hp = std::min(battle_player_.max_hp, battle_player_.hp + heal);
    int actual = battle_player_.hp - before;
    int_variables_[item_key]--;
    battle_log_ = battle_player_.name + " uses an item and recovers " + std::to_string(actual) + " HP!";
    battle_state_ = BattleState::EnemyTurn;
    battle_timer_ = 0.0f;
}

void EventSystem::PlayerTryEscape() {
    if (battle_state_ != BattleState::PlayerTurn) return;
    bool success = (std::rand() % 100) < 50;
    if (success) {
        battle_log_ = "Escaped successfully!";
        EndBattle(3);
    } else {
        battle_log_ = "Escape failed!";
        battle_state_ = BattleState::EnemyTurn;
        battle_timer_ = 0.0f;
    }
}

void EventSystem::UpdateBattle(float dt) {
    if (battle_state_ == BattleState::EnemyTurn) {
        battle_timer_ += dt;
        if (battle_timer_ >= 0.8f) {
            // 找下一个存活的敌人攻击
            int attacker = NextAliveEnemy(enemies_, battle_target_ + 1);
            if (attacker < 0) attacker = NextAliveEnemy(enemies_, 0);
            if (attacker >= 0) {
                const auto& enemy = enemies_[attacker];
                int damage = std::max(1, enemy.atk - battle_player_.def);
                battle_player_.hp = std::max(0, battle_player_.hp - damage);
                battle_log_ = enemy.name + " attacks " + battle_player_.name + " for " + std::to_string(damage) + " damage!";
                if (battle_player_.hp <= 0) {
                    battle_state_ = BattleState::Defeat;
                    battle_log_ += "\n" + battle_player_.name + " was defeated...";
                    return;
                }
            }
            // 敌人回合结束
            battle_state_ = BattleState::PlayerTurn;
            battle_timer_ = 0.0f;
            battle_log_ = "Your turn.";
        }
    } else if (battle_state_ == BattleState::Intro) {
        battle_timer_ += dt;
        if (battle_timer_ >= 0.5f) {
            battle_state_ = BattleState::PlayerTurn;
            battle_timer_ = 0.0f;
            battle_log_ = "Your turn.";
        }
    } else if (battle_state_ == BattleState::Victory || battle_state_ == BattleState::Defeat) {
        battle_timer_ += dt;
        if (battle_timer_ >= 1.5f) {
            EndBattle(battle_state_ == BattleState::Victory ? 1 : 2);
        }
    }
}

} // namespace scene
} // namespace mg
