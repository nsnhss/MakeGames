#pragma once
// ---------------------------------------------------------------------------
// BattleSession：回合制战斗子对象（从 EventSystem 拆出，D1）
// 职责：战斗状态机（Intro→PlayerTurn/EnemyTurn→Victory/Defeat）、
//       攻击/技能/物品/逃跑、战斗日志。不持有 World；
//       需要改全局变量的操作（胜利结算/消耗物品）经 GameVariables& 注入。
// ---------------------------------------------------------------------------
#include <string>
#include <vector>

namespace mg {

class ActorDatabase;
class EnemyDatabase;
class SkillDatabase;

namespace scene {

class GameVariables;

class BattleSession {
public:
    enum class State { None, Intro, PlayerTurn, EnemyTurn, Victory, Defeat };

    struct Combatant {
        std::string id;
        std::string name;
        int hp = 0;
        int max_hp = 0;
        int mp = 0;
        int max_mp = 0;
        int atk = 0;
        int def = 0;
    };

    void SetActorDatabase(const ActorDatabase* db) { actor_db_ = db; }
    void SetEnemyDatabase(const EnemyDatabase* db) { enemy_db_ = db; }
    void SetSkillDatabase(const SkillDatabase* db) { skill_db_ = db; }
    const EnemyDatabase* GetEnemyDatabase() const { return enemy_db_; }

    bool IsActive() const { return state_ != State::None; }
    State GetState() const { return state_; }
    const Combatant& Player() const { return player_; }
    const std::vector<Combatant>& Enemies() const { return enemies_; }
    int Target() const { return target_; }
    void SetTarget(int idx) { target_ = idx; }
    const std::string& Log() const { return log_; }

    void Start(const std::string& enemy_id, int count, const std::string& player_actor_id,
               GameVariables& vars);
    void End(int result, GameVariables& vars);
    void PlayerAttack();
    void PlayerUseSkill(const std::string& skill_id);
    void PlayerUseItem(GameVariables& vars);
    void PlayerTryEscape(GameVariables& vars);
    void Update(float dt, GameVariables& vars);

private:
    const ActorDatabase* actor_db_ = nullptr;
    const EnemyDatabase* enemy_db_ = nullptr;
    const SkillDatabase* skill_db_ = nullptr;
    State state_ = State::None;
    Combatant player_;
    std::vector<Combatant> enemies_;
    int target_ = 0;
    std::string log_;
    float timer_ = 0.0f;
};

} // namespace scene
} // namespace mg
