#pragma once
#include "third_party/json.hpp"
#include "scene/world.h"
#include "scene/action_commands.h"
#include "resource/shop_database.h"
#include "resource/actor_database.h"
#include "resource/enemy_database.h"
#include "resource/skill_database.h"
#include "scene/components/event_trigger.h"
#include "core/math/vec2.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <utility>

namespace mg {

class ScriptEngine; // 前置声明破 scene↔script 头文件环（cpp 侧再 include）

namespace scene {

// 单个事件的执行上下文
struct EventContext {
    EntityID entity = kInvalidEntity;  // 来源事件实体
    size_t page_index = 0;             // 来源页索引
    size_t action_index = 0;           // 下一个要执行的动作索引
    float wait_timer = 0.0f;           // Wait 剩余时间
    bool waiting = false;              // 是否在等待中
    bool finished = false;             // 是否已结束
    bool message_pending = false;      // 是否在等待消息关闭
    bool choices_pending = false;      // 是否在等待选项选择
    int choice_count = 0;              // 选项数量
    int selected_choice = -1;          // 用户选择 (-1 = 未选)

    // MoveEntity 路径移动状态
    bool move_pending = false;
    EntityID move_target = kInvalidEntity;
    std::vector<math::Vec2> move_path;
    float move_speed = 60.0f;
    size_t move_index = 0;

    // Loop 循环状态
    bool loop_active = false;
    size_t loop_start = 0;
    int loop_count = 0;
    int loop_current = 0;
};

class EventSystem {
public:
    explicit EventSystem(World* world);

    void Update(float dt);

    // ---- 触发接口 ----
    void TriggerOnStart();
    void TriggerOnInteract(EntityID entity);
    void TriggerOnEnter(EntityID entity);
    void TriggerOnExit(EntityID entity);
    void TriggerOnTouch(EntityID self, EntityID other);
    void TriggerOnCollision(EntityID self, EntityID other);
    void TriggerOnVariableChange(const std::string& name);

    // ---- 全局状态 ----
    void SetVariable(const std::string& name, int value);
    void SetVariable(const std::string& name, float value);
    void SetVariable(const std::string& name, bool value);
    int GetVariableInt(const std::string& name, int default_val = 0) const;
    float GetVariableFloat(const std::string& name, float default_val = 0.0f) const;
    bool GetVariableBool(const std::string& name, bool default_val = false) const;

    void SetSwitch(const std::string& name, bool value);
    bool GetSwitch(const std::string& name, bool default_val = false) const;

    void SetSelfSwitch(EntityID entity, char sw, bool value);
    bool GetSelfSwitch(EntityID entity, char sw, bool default_val = false) const;

    // ---- 消息交互（供 UI 系统查询）----
    bool HasPendingMessage() const { return has_pending_message_; }
    const std::string& GetPendingMessage() const { return pending_message_; }
    const std::string& GetPendingMessageName() const { return pending_message_name_; }
    void AcknowledgeMessage(); // UI 调用此函数表示消息已关闭，继续执行

    bool IsDialogActive() const { return dialog_active_; }

    // ---- ShowChoices（选项分支）----
    bool IsChoiceActive() const { return choice_active_; }
    const std::vector<std::string>& GetChoices() const { return choices_; }
    const std::string& GetChoiceLabel() const { return choice_label_; }
    void AcknowledgeChoice(int index);

    // ---- Toast（浮动轻提示）----
    struct ToastMessage {
        std::string text;
        float timer = 2.0f;       // 剩余显示时间
        math::Vec2 world_pos;     // 浮动位置（实体头顶）
    };
    const std::vector<ToastMessage>& GetToasts() const { return toasts_; }

    // ---- HUD Overlay（Play Mode UI）----
    struct HUDOverlay {
        std::string name;
        std::string text;
        bool visible = false;
    };
    const std::unordered_map<std::string, HUDOverlay>& GetHUDOverlays() const { return hud_overlays_; }

    // ---- 库存查询（AddItem/RemoveItem 操作的是 item_* 前缀的 int_variables_）----
    std::vector<std::pair<std::string, int>> GetInventoryItems() const;

    // ---- 场景切换（TransferPlayer 跨场景）----
    struct GlobalStateSnapshot {
        std::unordered_map<std::string, int>    int_vars;
        std::unordered_map<std::string, float>  float_vars;
        std::unordered_map<std::string, bool>   bool_vars;
        std::unordered_map<std::string, bool>   switches;
        std::unordered_map<std::string, bool>   self_switches;
    };
    bool HasPendingSceneTransition() const { return !pending_scene_transition_.empty(); }
    std::string ConsumePendingSceneTransition() { return std::exchange(pending_scene_transition_, {}); }
    int GetPendingSpawnX() const { return pending_spawn_x_; }
    int GetPendingSpawnY() const { return pending_spawn_y_; }
    GlobalStateSnapshot TakeGlobalStateSnapshot() const;
    void RestoreGlobalState(const GlobalStateSnapshot& snap);

    // 存档序列化
    nlohmann::json SaveState() const;
    void LoadState(const nlohmann::json& j);

    // ---- 商店系统 ----
    void SetShopDatabase(const ShopDatabase* db) { shop_db_ = db; }
    bool IsShopActive() const { return shop_pending_; }
    const std::string& GetPendingShopId() const { return pending_shop_id_; }
    void OpenShop(const std::string& shop_id);
    void AcknowledgeShop();
    void CloseShop();
    bool BuyItem(const std::string& item_id, int price);

    // ---- 战斗系统 ----
    enum class BattleState { None, Intro, PlayerTurn, EnemyTurn, Victory, Defeat };
    struct BattleCombatant {
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
    void SetScriptEngine(ScriptEngine* engine) { script_engine_ = engine; }
    bool IsBattleActive() const { return battle_state_ != BattleState::None; }
    BattleState GetBattleState() const { return battle_state_; }
    const BattleCombatant& GetBattlePlayer() const { return battle_player_; }
    const std::vector<BattleCombatant>& GetBattleEnemies() const { return enemies_; }
    int GetBattleTarget() const { return battle_target_; }
    void SetBattleTarget(int idx) { battle_target_ = idx; }
    const BattleCombatant& GetCurrentTarget() const {
        static BattleCombatant empty;
        if (battle_target_ >= 0 && battle_target_ < (int)enemies_.size()) return enemies_[battle_target_];
        return empty;
    }
    const std::string& GetBattleLog() const { return battle_log_; }
    void StartBattle(const std::string& enemy_id, int count, const std::string& player_actor_id);
    void EndBattle(int result);
    void PlayerAttack();
    void PlayerUseSkill(const std::string& skill_id);
    void PlayerUseItem();
    void PlayerTryEscape();
    void UpdateBattle(float dt);

    // ---- 状态查询 ----
    bool IsBusy() const { return !active_contexts_.empty(); }

private:
    void EvaluatePagesForTrigger(EventPage::TriggerType trigger, EntityID entity, EntityID other = kInvalidEntity);
    bool HasActiveContextFor(EntityID entity, size_t page_index) const;
    // 每帧轮询触发器（Update 内调用）
    void PollOnUpdateTriggers();  // OnUpdate：每帧评估条件
    void PollOnInputTriggers();   // OnInput：按键按下沿（condition.name = SDL 按键名）
    void PollAreaTriggers();      // OnEnter/OnExit/OnTouch/OnCollision：玩家与事件点区域重叠差分
    void ExecuteContext(EventContext& ctx, float dt);
    bool EvaluateCondition(const EventPage::Condition& cond, EntityID entity);
    bool CheckPageCondition(const EventPage& page, EntityID entity);

    // 动作执行
    void ExecuteAction(EventContext& ctx, const Action& action);

    // If/Else/EndIf 辅助
    bool EvaluateActionCondition(const Action& action);
    int SkipToEndIf(const std::vector<Action>& actions, int start);

    World* world_ = nullptr;
    std::vector<EventContext> active_contexts_;

    // 全局状态
    std::unordered_map<std::string, int> int_variables_;
    std::unordered_map<std::string, float> float_variables_;
    std::unordered_map<std::string, bool> bool_variables_;
    std::unordered_map<std::string, bool> switches_;
    std::unordered_map<std::string, bool> self_switches_;

    // 商店状态
    const ShopDatabase* shop_db_ = nullptr;
    bool shop_pending_ = false;
    std::string pending_shop_id_;

    // 战斗状态
    const ActorDatabase* actor_db_ = nullptr;
    const EnemyDatabase* enemy_db_ = nullptr;
    const SkillDatabase* skill_db_ = nullptr;
    ScriptEngine* script_engine_ = nullptr;
    BattleState battle_state_ = BattleState::None;
    BattleCombatant battle_player_;
    std::vector<BattleCombatant> enemies_;
    int battle_target_ = 0;
    std::string battle_log_;
    float battle_timer_ = 0.0f;

    // 消息状态
    bool has_pending_message_ = false;
    std::string pending_message_;
    std::string pending_message_name_;
    bool dialog_active_ = false;          // 对话框是否阻塞移动

    // ShowChoices 状态
    bool choice_active_ = false;
    std::vector<std::string> choices_;
    std::string choice_label_;

    // Toast 浮动提示
    std::vector<ToastMessage> toasts_;

    // HUD Overlay
    std::unordered_map<std::string, HUDOverlay> hud_overlays_;

    // 场景切换信号
    std::string pending_scene_transition_;
    int pending_spawn_x_ = 0;
    int pending_spawn_y_ = 0;

    bool on_start_triggered_ = false;

    // 区域触发状态：本帧与玩家重叠的事件实体集合（差分产 OnEnter/OnExit）
    std::unordered_set<EntityID> overlapping_events_;
    // OnInput 按键上帧状态（按键名 → 是否按下），用于按下沿检测
    std::unordered_map<std::string, bool> input_key_prev_;
};

} // namespace scene
} // namespace mg
