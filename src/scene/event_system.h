#pragma once
#include "third_party/json.hpp"
#include "scene/world.h"
#include "scene/action_commands.h"
#include "scene/components/event_trigger.h"
#include "scene/game_variables.h"
#include "scene/battle_session.h"
#include "scene/shop_session.h"
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

    // ---- 全局状态（转发 GameVariables；通知语义不变）----
    void SetVariable(const std::string& name, int value)   { variables_.SetInt(name, value); }
    void SetVariable(const std::string& name, float value) { variables_.SetFloat(name, value); }
    void SetVariable(const std::string& name, bool value)  { variables_.SetBool(name, value); }
    int GetVariableInt(const std::string& name, int default_val = 0) const { return variables_.GetInt(name, default_val); }
    float GetVariableFloat(const std::string& name, float default_val = 0.0f) const { return variables_.GetFloat(name, default_val); }
    bool GetVariableBool(const std::string& name, bool default_val = false) const { return variables_.GetBool(name, default_val); }

    void SetSwitch(const std::string& name, bool value) { variables_.SetSwitch(name, value); }
    bool GetSwitch(const std::string& name, bool default_val = false) const { return variables_.GetSwitch(name, default_val); }

    // 自开关：cache 存 GameVariables，同时同步 EventTrigger 组件（cpp 实现）
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

    // ---- 库存查询（AddItem/RemoveItem 操作的是 item_* 前缀的 int 变量）----
    std::vector<std::pair<std::string, int>> GetInventoryItems() const { return variables_.GetInventoryItems(); }

    // ---- 场景切换（TransferPlayer 跨场景）----
    using GlobalStateSnapshot = GameVariables::Snapshot;
    bool HasPendingSceneTransition() const { return !pending_scene_transition_.empty(); }
    std::string ConsumePendingSceneTransition() { return std::exchange(pending_scene_transition_, {}); }
    int GetPendingSpawnX() const { return pending_spawn_x_; }
    int GetPendingSpawnY() const { return pending_spawn_y_; }
    GlobalStateSnapshot TakeGlobalStateSnapshot() const { return variables_.TakeSnapshot(); }
    void RestoreGlobalState(const GlobalStateSnapshot& snap) { variables_.Restore(snap); }

    // 存档序列化
    nlohmann::json SaveState() const { return variables_.SaveToJson(); }
    void LoadState(const nlohmann::json& j) { variables_.LoadFromJson(j); }

    // ---- 商店系统（转发 ShopSession）----
    void SetShopDatabase(const ShopDatabase* db) { shop_.SetDatabase(db); }
    bool IsShopActive() const { return shop_.IsPending(); }
    const std::string& GetPendingShopId() const { return shop_.PendingShopId(); }
    void OpenShop(const std::string& shop_id) { shop_.Open(shop_id); }
    void AcknowledgeShop() {} // 保留钩子：当前商店通过 CloseShop 结束，无需额外确认标记
    void CloseShop() { shop_.Close(); }
    bool BuyItem(const std::string& item_id, int price) { return shop_.Buy(item_id, price, variables_); }

    // ---- 战斗系统（转发 BattleSession）----
    using BattleState = BattleSession::State;
    using BattleCombatant = BattleSession::Combatant;
    void SetActorDatabase(const ActorDatabase* db) { battle_.SetActorDatabase(db); }
    void SetEnemyDatabase(const EnemyDatabase* db) { battle_.SetEnemyDatabase(db); }
    void SetSkillDatabase(const SkillDatabase* db) { battle_.SetSkillDatabase(db); }
    void SetScriptEngine(ScriptEngine* engine) { script_engine_ = engine; }
    bool IsBattleActive() const { return battle_.IsActive(); }
    BattleState GetBattleState() const { return battle_.GetState(); }
    const BattleCombatant& GetBattlePlayer() const { return battle_.Player(); }
    const std::vector<BattleCombatant>& GetBattleEnemies() const { return battle_.Enemies(); }
    int GetBattleTarget() const { return battle_.Target(); }
    void SetBattleTarget(int idx) { battle_.SetTarget(idx); }
    const BattleCombatant& GetCurrentTarget() const {
        static BattleCombatant empty;
        int t = battle_.Target();
        const auto& enemies = battle_.Enemies();
        if (t >= 0 && t < (int)enemies.size()) return enemies[t];
        return empty;
    }
    const std::string& GetBattleLog() const { return battle_.Log(); }
    void StartBattle(const std::string& enemy_id, int count, const std::string& player_actor_id) {
        battle_.Start(enemy_id, count, player_actor_id, variables_);
    }
    void EndBattle(int result) { battle_.End(result, variables_); }
    void PlayerAttack() { battle_.PlayerAttack(); }
    void PlayerUseSkill(const std::string& skill_id) { battle_.PlayerUseSkill(skill_id); }
    void PlayerUseItem() { battle_.PlayerUseItem(variables_); }
    void PlayerTryEscape() { battle_.PlayerTryEscape(variables_); }
    void UpdateBattle(float dt) { battle_.Update(dt, variables_); }

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

    // 动作执行（分域实现于 event_system_actions_*.cpp，D1）
    void ExecuteAction(EventContext& ctx, const Action& action);
    void ExecDialogActions(EventContext& ctx, const Action& action); // ShowMessage/ShowToast/ShowChoices/ShowUI/HideUI
    void ExecWorldActions(EventContext& ctx, const Action& action);  // TransferPlayer/MoveEntity/PlayAnimation
    void ExecAudioActions(const Action& action);                     // PlaySound/PlayBGM/StopBGM/PlayBGS/StopBGS/PlayME
    void ExecFlowActions(EventContext& ctx, const Action& action);   // Wait/If/Else/EndIf/Loop/BreakLoop/CallScript

    // If/Else/EndIf 辅助（event_system_actions_flow.cpp）
    bool EvaluateActionCondition(const Action& action);
    int SkipToEndIf(const std::vector<Action>& actions, int start);

    // 子对象（D1）：变量/库存、战斗、商店
    GameVariables variables_;
    BattleSession battle_;
    ShopSession shop_;
    ScriptEngine* script_engine_ = nullptr;

    World* world_ = nullptr;
    std::vector<EventContext> active_contexts_;

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
