// battle_session.cpp — 回合制战斗（原 event_system_battle.cpp，D1 拆为独立子对象）
#include "scene/battle_session.h"
#include "scene/game_variables.h"
#include "resource/actor_database.h"
#include "resource/enemy_database.h"
#include "resource/skill_database.h"
#include <algorithm>
#include <cstdlib>

namespace mg {
namespace scene {

void BattleSession::Start(const std::string& enemy_id, int count, const std::string& player_actor_id,
                          GameVariables& vars) {
    const auto* enemy_def = enemy_db_ ? enemy_db_->Find(enemy_id) : nullptr;
    if (!enemy_def) return;

    std::string actor_id = player_actor_id;
    if (actor_id.empty()) actor_id = "Player";
    const auto* actor_def = actor_db_ ? actor_db_->Find(actor_id) : nullptr;
    if (!actor_def) return;

    // 成长曲线接入：按等级变量求战斗数值（未设等级 → Lv1 = 基础值，行为与旧版一致）
    int level = vars.GetInt("level", 1);
    ActorBattleStats stats = EvalActorBattleStats(*actor_def, level);
    player_ = {actor_def->id, actor_def->name, stats.hp, stats.hp,
               stats.mp, stats.mp, stats.atk, stats.def};
    enemies_.clear();
    int n = count > 0 ? count : 1;
    for (int i = 0; i < n; ++i) {
        std::string name = enemy_def->name;
        if (n > 1) name += " " + std::to_string(i + 1);
        enemies_.push_back({enemy_def->id, name, enemy_def->hp, enemy_def->hp,
                            0, 0, enemy_def->atk, enemy_def->def});
    }
    target_ = 0;
    log_ = n > 1 ? (std::to_string(n) + " " + enemy_def->name + "s appear!")
                 : ("A wild " + enemy_def->name + " appears!");
    timer_ = 0.0f;
    state_ = State::Intro;
}

void BattleSession::End(int result, GameVariables& vars) {
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
        vars.SetInt("gold", vars.GetInt("gold", 0) + total_gold);
        vars.SetInt("exp", vars.GetInt("exp", 0) + total_exp);
    }
    vars.SetInt("battle_result", result);
    state_ = State::None;
    player_ = {};
    enemies_.clear();
    target_ = 0;
    log_.clear();
    timer_ = 0.0f;
}

static bool AllEnemiesDefeated(const std::vector<BattleSession::Combatant>& enemies) {
    for (const auto& e : enemies) if (e.hp > 0) return false;
    return true;
}

static int NextAliveEnemy(const std::vector<BattleSession::Combatant>& enemies, int start) {
    for (int i = 0; i < (int)enemies.size(); ++i) {
        int idx = (start + i) % (int)enemies.size();
        if (enemies[idx].hp > 0) return idx;
    }
    return -1;
}

void BattleSession::PlayerAttack() {
    if (state_ != State::PlayerTurn) return;
    if (enemies_.empty()) return;
    if (target_ < 0 || target_ >= (int)enemies_.size() || enemies_[target_].hp <= 0) {
        target_ = NextAliveEnemy(enemies_, target_);
        if (target_ < 0) {
            state_ = State::Victory;
            log_ = "All enemies defeated!";
            return;
        }
    }
    auto& target = enemies_[target_];
    int damage = std::max(1, player_.atk - target.def);
    target.hp = std::max(0, target.hp - damage);
    log_ = player_.name + " attacks " + target.name + " for " + std::to_string(damage) + " damage!";
    if (AllEnemiesDefeated(enemies_)) {
        state_ = State::Victory;
        log_ += "\nAll enemies defeated!";
    } else if (target.hp <= 0) {
        log_ += "\n" + target.name + " defeated!";
        int next = NextAliveEnemy(enemies_, target_);
        if (next >= 0) target_ = next;
        state_ = State::EnemyTurn;
        timer_ = 0.0f;
    } else {
        state_ = State::EnemyTurn;
        timer_ = 0.0f;
    }
}

void BattleSession::PlayerUseSkill(const std::string& skill_id) {
    if (state_ != State::PlayerTurn) return;
    if (enemies_.empty()) return;
    const auto* skill = skill_db_ ? skill_db_->Find(skill_id) : nullptr;
    if (!skill) {
        log_ = "Skill not found!";
        return;
    }
    if (player_.mp < skill->mp_cost) {
        log_ = "Not enough MP!";
        return;
    }
    if (target_ < 0 || target_ >= (int)enemies_.size() || enemies_[target_].hp <= 0) {
        target_ = NextAliveEnemy(enemies_, target_);
        if (target_ < 0) {
            state_ = State::Victory;
            log_ = "All enemies defeated!";
            return;
        }
    }
    auto& target = enemies_[target_];
    player_.mp = std::max(0, player_.mp - skill->mp_cost);
    int damage = std::max(1, player_.atk + skill->damage - target.def);
    target.hp = std::max(0, target.hp - damage);
    log_ = player_.name + " uses " + skill->name + " on " + target.name + " for " + std::to_string(damage) + " damage!";
    if (AllEnemiesDefeated(enemies_)) {
        state_ = State::Victory;
        log_ += "\nAll enemies defeated!";
    } else {
        state_ = State::EnemyTurn;
        timer_ = 0.0f;
    }
}

void BattleSession::PlayerUseItem(GameVariables& vars) {
    if (state_ != State::PlayerTurn) return;
    // 简化物品系统：任一 item_* 数量 > 0 的物品可消耗，恢复 30 HP
    std::string item_key = vars.FindFirstInventoryKey();
    if (item_key.empty()) {
        log_ = "No items!";
        return;
    }
    int heal = 30;
    int before = player_.hp;
    player_.hp = std::min(player_.max_hp, player_.hp + heal);
    int actual = player_.hp - before;
    vars.DecrementItemRaw(item_key);
    log_ = player_.name + " uses an item and recovers " + std::to_string(actual) + " HP!";
    state_ = State::EnemyTurn;
    timer_ = 0.0f;
}

void BattleSession::PlayerTryEscape(GameVariables& vars) {
    if (state_ != State::PlayerTurn) return;
    bool success = (std::rand() % 100) < 50;
    if (success) {
        log_ = "Escaped successfully!";
        End(3, vars);
    } else {
        log_ = "Escape failed!";
        state_ = State::EnemyTurn;
        timer_ = 0.0f;
    }
}

void BattleSession::Update(float dt, GameVariables& vars) {
    if (state_ == State::EnemyTurn) {
        timer_ += dt;
        if (timer_ >= 0.8f) {
            // 找下一个存活的敌人攻击
            int attacker = NextAliveEnemy(enemies_, target_ + 1);
            if (attacker < 0) attacker = NextAliveEnemy(enemies_, 0);
            if (attacker >= 0) {
                const auto& enemy = enemies_[attacker];
                int damage = std::max(1, enemy.atk - player_.def);
                player_.hp = std::max(0, player_.hp - damage);
                log_ = enemy.name + " attacks " + player_.name + " for " + std::to_string(damage) + " damage!";
                if (player_.hp <= 0) {
                    state_ = State::Defeat;
                    log_ += "\n" + player_.name + " was defeated...";
                    return;
                }
            }
            // 敌人回合结束
            state_ = State::PlayerTurn;
            timer_ = 0.0f;
            log_ = "Your turn.";
        }
    } else if (state_ == State::Intro) {
        timer_ += dt;
        if (timer_ >= 0.5f) {
            state_ = State::PlayerTurn;
            timer_ = 0.0f;
            log_ = "Your turn.";
        }
    } else if (state_ == State::Victory || state_ == State::Defeat) {
        timer_ += dt;
        if (timer_ >= 1.5f) {
            End(state_ == State::Victory ? 1 : 2, vars);
        }
    }
}

} // namespace scene
} // namespace mg
