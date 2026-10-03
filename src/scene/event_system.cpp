#include "scene/event_system.h"
#include "scene/constants.h"
#include "scene/components/transform.h"
#include "scene/components/movement.h"
#include "resource/shop_database.h"
#include "resource/enemy_database.h"
#include <SDL.h>
#include <cassert>
#include <cmath>
#include <algorithm>

namespace mg {
namespace scene {

EventSystem::EventSystem(World* world) : world_(world) {
    // GameVariables 的 int 变量变更通知 → 事件触发器（与原 SetVariable 手动调用一致）
    variables_.SetOnVariableChange([this](const std::string& name) { TriggerOnVariableChange(name); });
}

void EventSystem::Update(float dt) {
    // 场景切换挂起时冻结所有执行
    if (!pending_scene_transition_.empty())
        return;

    if (!on_start_triggered_) {
        TriggerOnStart();
        on_start_triggered_ = true;
    }

    // 每帧轮询触发器：OnUpdate / OnInput / 区域重叠（OnEnter/OnExit/OnTouch/OnCollision）
    // 在执行上下文之前轮询，保证本帧触发本帧执行
    PollOnUpdateTriggers();
    PollOnInputTriggers();
    PollAreaTriggers();

    // 执行所有活跃上下文
    for (size_t i = 0; i < active_contexts_.size(); ) {
        ExecuteContext(active_contexts_[i], dt);
        if (active_contexts_[i].finished) {
            active_contexts_.erase(active_contexts_.begin() + i);
        } else {
            ++i;
        }
    }

    // 推进 toast timer
    for (auto& t : toasts_) t.timer -= dt;
    toasts_.erase(std::remove_if(toasts_.begin(), toasts_.end(),
        [](auto& t) { return t.timer <= 0; }), toasts_.end());

    // 更新战斗状态机
    if (battle_.IsActive()) {
        battle_.Update(dt, variables_);
    }
}

// ---- 触发接口 ----

void EventSystem::TriggerOnStart() {
    // 标记已触发：防止手动调用后 Update 首帧再触发一次（修复 OnStart 双重触发）
    on_start_triggered_ = true;
    std::vector<EntityID> entities;
    world_->Query<EventTrigger>(entities);
    for (EntityID e : entities) {
        EvaluatePagesForTrigger(EventPage::TriggerType::OnStart, e);
    }
}

void EventSystem::TriggerOnInteract(EntityID entity) {
    EvaluatePagesForTrigger(EventPage::TriggerType::OnInteract, entity);
}

void EventSystem::TriggerOnEnter(EntityID entity) {
    EvaluatePagesForTrigger(EventPage::TriggerType::OnEnter, entity);
}

void EventSystem::TriggerOnExit(EntityID entity) {
    EvaluatePagesForTrigger(EventPage::TriggerType::OnExit, entity);
}

void EventSystem::TriggerOnTouch(EntityID self, EntityID other) {
    EvaluatePagesForTrigger(EventPage::TriggerType::OnTouch, self, other);
}

void EventSystem::TriggerOnCollision(EntityID self, EntityID other) {
    EvaluatePagesForTrigger(EventPage::TriggerType::OnCollision, self, other);
}

void EventSystem::TriggerOnVariableChange(const std::string& name) {
    std::vector<EntityID> entities;
    world_->Query<EventTrigger>(entities);
    for (EntityID e : entities) {
        auto* trigger = world_->GetComponent<EventTrigger>(e);
        if (!trigger) continue;
        for (size_t i = 0; i < trigger->pages.size(); ++i) {
            const auto& page = trigger->pages[i];
            if (page.trigger == EventPage::TriggerType::OnVariableChange) {
                if (page.condition.type == EventPage::Condition::Type::Variable &&
                    page.condition.name == name) {
                    if (CheckPageCondition(page, e)) {
                        EventContext ctx;
                        ctx.entity = e;
                        ctx.page_index = i;
                        active_contexts_.push_back(ctx);
                    }
                }
            }
        }
    }
}

// ---- 评估逻辑 ----

void EventSystem::EvaluatePagesForTrigger(EventPage::TriggerType trigger, EntityID entity, EntityID other) {
    auto* trigger_comp = world_->GetComponent<EventTrigger>(entity);
    if (!trigger_comp) return;

    for (size_t i = 0; i < trigger_comp->pages.size(); ++i) {
        const auto& page = trigger_comp->pages[i];
        if (page.trigger != trigger) continue;
        if (page.run_once && page.has_run) continue;
        if (HasActiveContextFor(entity, i)) continue; // 同页仍在执行（如等待消息关闭），不重复入队
        if (!CheckPageCondition(page, entity)) continue;

        // 找到匹配页，创建执行上下文
        EventContext ctx;
        ctx.entity = entity;
        ctx.page_index = i;
        active_contexts_.push_back(ctx);

        // run_once 页触发后标记为已运行（对所有触发类型生效）
        if (page.run_once) {
            trigger_comp->pages[i].has_run = true;
        }

        // 只执行第一个匹配的页（RPG Maker 风格）
        break;
    }
}

bool EventSystem::HasActiveContextFor(EntityID entity, size_t page_index) const {
    for (const auto& c : active_contexts_) {
        if (c.entity == entity && c.page_index == page_index) return true;
    }
    return false;
}

// ---- 每帧轮询触发器 ----

void EventSystem::PollOnUpdateTriggers() {
    std::vector<EntityID> entities;
    world_->Query<EventTrigger>(entities);
    for (EntityID e : entities) {
        EvaluatePagesForTrigger(EventPage::TriggerType::OnUpdate, e);
    }
}

void EventSystem::PollOnInputTriggers() {
    const uint8_t* keys = SDL_GetKeyboardState(nullptr);
    if (!keys) return;

    std::vector<EntityID> entities;
    world_->Query<EventTrigger>(entities);

    // OnInput 页复用 condition.name 存 SDL 按键名（如 "Space" / "Return" / "A"）
    // 第一遍：收集本帧所有被引用按键的当前状态（同名按键只查一次）
    std::unordered_map<std::string, bool> cur;
    for (EntityID e : entities) {
        auto* trig = world_->GetComponent<EventTrigger>(e);
        if (!trig) continue;
        for (const auto& page : trig->pages) {
            if (page.trigger != EventPage::TriggerType::OnInput) continue;
            const std::string& key = page.condition.name;
            if (key.empty() || cur.contains(key)) continue;
            SDL_Scancode sc = SDL_GetScancodeFromName(key.c_str());
            if (sc == SDL_SCANCODE_UNKNOWN) continue;
            cur[key] = keys[sc] != 0;
        }
    }

    // 第二遍：按下沿（上帧未按、本帧按下）触发
    for (EntityID e : entities) {
        auto* trig = world_->GetComponent<EventTrigger>(e);
        if (!trig) continue;
        for (size_t i = 0; i < trig->pages.size(); ++i) {
            auto& page = trig->pages[i];
            if (page.trigger != EventPage::TriggerType::OnInput) continue;
            auto it = cur.find(page.condition.name);
            if (it == cur.end() || !it->second) continue;
            auto prev = input_key_prev_.find(it->first);
            if (prev != input_key_prev_.end() && prev->second) continue; // 持续按住不重复触发
            if (page.run_once && page.has_run) continue;
            if (HasActiveContextFor(e, i)) continue;

            EventContext ctx;
            ctx.entity = e;
            ctx.page_index = i;
            active_contexts_.push_back(ctx);
            if (page.run_once) page.has_run = true;
            break; // 每实体每帧最多触发一个 OnInput 页
        }
    }

    input_key_prev_ = std::move(cur);
}

void EventSystem::PollAreaTriggers() {
    // 找玩家（第一个带 Movement + Transform 的实体，与编辑器交互键检测约定一致）
    std::vector<EntityID> players;
    world_->Query<Movement>(players);
    EntityID player = kInvalidEntity;
    math::Vec2 player_pos;
    for (EntityID e : players) {
        if (auto* t = world_->GetComponent<Transform>(e)) {
            player = e;
            player_pos = t->position;
            break;
        }
    }
    if (player == kInvalidEntity) {
        overlapping_events_.clear();
        return;
    }

    // scene/constants.h：与 play mode 碰撞半宽一致；事件点半径 = 32px tile 半格
    constexpr float PLAYER_HALF = kPlayerCollisionHalfExtent;
    constexpr float EVENT_HALF  = kEventTouchHalfExtent;

    std::vector<EntityID> events;
    world_->Query<EventTrigger>(events);
    std::unordered_set<EntityID> cur;
    for (EntityID e : events) {
        if (e == player) continue;
        auto* t = world_->GetComponent<Transform>(e);
        if (!t) continue;
        float dx = std::fabs(t->position.x - player_pos.x);
        float dy = std::fabs(t->position.y - player_pos.y);
        if (dx < PLAYER_HALF + EVENT_HALF && dy < PLAYER_HALF + EVENT_HALF)
            cur.insert(e);
    }

    // 差分：新进入 → OnEnter + OnCollision + OnTouch（进入帧也算接触）；
    // 持续重叠 → OnTouch；新离开 → OnExit
    for (EntityID e : cur) {
        if (overlapping_events_.contains(e)) {
            TriggerOnTouch(e, player);
        } else {
            TriggerOnEnter(e);
            TriggerOnCollision(e, player);
            TriggerOnTouch(e, player);
        }
    }
    for (EntityID e : overlapping_events_) {
        if (!cur.contains(e)) TriggerOnExit(e);
    }
    overlapping_events_ = std::move(cur);
}

bool EventSystem::CheckPageCondition(const EventPage& page, EntityID entity) {
    if (page.condition.type == EventPage::Condition::Type::None) return true;
    return EvaluateCondition(page.condition, entity);
}

bool EventSystem::EvaluateCondition(const EventPage::Condition& cond, EntityID entity) {
    switch (cond.type) {
        case EventPage::Condition::Type::Variable: {
            int val = GetVariableInt(cond.name, 0);
            switch (cond.compare_op) {
                case 0: return val == cond.int_value;
                case 1: return val != cond.int_value;
                case 2: return val > cond.int_value;
                case 3: return val < cond.int_value;
                case 4: return val >= cond.int_value;
                case 5: return val <= cond.int_value;
            }
            return false;
        }
        case EventPage::Condition::Type::Switch: {
            bool val = GetSwitch(cond.name, false);
            return val == cond.bool_value;
        }
        case EventPage::Condition::Type::SelfSwitch: {
            bool val = GetSelfSwitch(entity, cond.name.empty() ? 'A' : cond.name[0], false);
            return val == cond.bool_value;
        }
        default:
            return true;
    }
}

// ---- 上下文执行 ----

void EventSystem::ExecuteContext(EventContext& ctx, float dt) {
    if (ctx.finished) return;
    if (ctx.waiting) {
        ctx.wait_timer -= dt;
        if (ctx.wait_timer <= 0.0f) {
            ctx.waiting = false;
            ++ctx.action_index;
        } else {
            return; // 还在等待中
        }
    }
    if (ctx.message_pending) {
        return; // 等待消息关闭
    }
    if (ctx.choices_pending) {
        return; // 等待选项选择
    }
    if (shop_.IsPending()) {
        return; // 等待商店关闭
    }
    if (battle_.IsActive()) {
        return; // 等待战斗结束
    }
    if (ctx.move_pending) {
        auto* t = world_->GetComponent<Transform>(ctx.move_target);
        if (!t || ctx.move_index >= ctx.move_path.size()) {
            ctx.move_pending = false;
        } else {
            math::Vec2 target = ctx.move_path[ctx.move_index];
            math::Vec2 dir = target - t->position;
            float dist = ctx.move_speed * dt;
            float len = dir.Length();
            if (len < 1e-6f) {
                // 重合点直接跳过，避免除零/卡住
                t->position = target;
                ctx.move_index++;
                if (ctx.move_index >= ctx.move_path.size()) {
                    ctx.move_pending = false;
                    ++ctx.action_index;
                }
            } else if (len <= dist) {
                t->position = target;
                ctx.move_index++;
                if (ctx.move_index >= ctx.move_path.size()) {
                    ctx.move_pending = false;
                    ++ctx.action_index; // 移动完成，推进
                }
            } else {
                dir = dir.Normalized();
                t->position.x += dir.x * dist;
                t->position.y += dir.y * dist;
            }
        }
        return; // 阻塞其他动作
    }

    auto* trigger = world_->GetComponent<EventTrigger>(ctx.entity);
    if (!trigger || ctx.page_index >= trigger->pages.size()) {
        ctx.finished = true;
        return;
    }

    const auto& page = trigger->pages[ctx.page_index];
    if (ctx.action_index >= page.actions.size()) {
        if (ctx.loop_active) {
            ctx.loop_current++;
            if (ctx.loop_count < 0 || ctx.loop_current < ctx.loop_count) {
                ctx.action_index = ctx.loop_start + 1;
                return;
            }
            ctx.loop_active = false;
        }
        ctx.finished = true;
        return;
    }

    // 执行当前动作
    ExecuteAction(ctx, page.actions[ctx.action_index]);

    // 如果不是 wait/message/move/shop/battle，自动推进到下一个动作
    if (!ctx.waiting && !ctx.message_pending && !ctx.choices_pending && !ctx.move_pending && !ctx.finished && !shop_.IsPending() && !battle_.IsActive()) {
        ++ctx.action_index;
        // 非循环页执行完最后一个动作当帧即结束，避免空转一帧
        // （否则 OnUpdate 等每帧轮询触发器会被 HasActiveContextFor 去重挡掉，隔帧才能再触发）
        if (ctx.action_index >= page.actions.size() && !ctx.loop_active) ctx.finished = true;
    }
}

// ---- 动作执行调度（分域实现见 event_system_actions_*.cpp）----

void EventSystem::ExecuteAction(EventContext& ctx, const Action& action) {
    switch (action.cmd) {
        case ActionCmd::ShowMessage:
        case ActionCmd::ShowToast:
        case ActionCmd::ShowChoices:
        case ActionCmd::ShowUI:
        case ActionCmd::HideUI:
            ExecDialogActions(ctx, action);
            break;
        case ActionCmd::TransferPlayer:
        case ActionCmd::MoveEntity:
        case ActionCmd::PlayAnimation:
            ExecWorldActions(ctx, action);
            break;
        case ActionCmd::PlaySound:
        case ActionCmd::PlayBGM:
        case ActionCmd::StopBGM:
        case ActionCmd::PlayBGS:
        case ActionCmd::StopBGS:
        case ActionCmd::PlayME:
            ExecAudioActions(action);
            break;
        case ActionCmd::Wait:
        case ActionCmd::If:
        case ActionCmd::Else:
        case ActionCmd::EndIf:
        case ActionCmd::Loop:
        case ActionCmd::BreakLoop:
        case ActionCmd::CallScript:
            ExecFlowActions(ctx, action);
            break;
        case ActionCmd::SetVariable: {
            variables_.SetInt(action.target, action.int_value);
            break;
        }
        case ActionCmd::SetSwitch: {
            variables_.SetSwitch(action.target, action.bool_value);
            break;
        }
        case ActionCmd::SetSelfSwitch: {
            char sw = action.param.empty() ? 'A' : action.param[0];
            SetSelfSwitch(ctx.entity, sw, action.bool_value);
            break;
        }
        case ActionCmd::AddItem: {
            variables_.AddItem(action.target, action.int_value);
            break;
        }
        case ActionCmd::RemoveItem: {
            variables_.RemoveItem(action.target, action.int_value);
            break;
        }
        case ActionCmd::ShopProcessing: {
            if (shop_.Database() && shop_.Database()->Find(action.target))
                shop_.Open(action.target);
            break;
        }
        case ActionCmd::BattleProcessing: {
            if (battle_.GetEnemyDatabase() && battle_.GetEnemyDatabase()->Find(action.target))
                StartBattle(action.target, action.int_value > 0 ? action.int_value : 1, action.param);
            break;
        }
        default:
            break;
    }
}

// ---- 消息 / 选项确认 ----

void EventSystem::AcknowledgeMessage() {
    has_pending_message_ = false;
    dialog_active_ = false;
    pending_message_.clear();
    pending_message_name_.clear();

    // 找到正在等待消息的上下文并推进
    for (auto& ctx : active_contexts_) {
        if (ctx.message_pending) {
            ctx.message_pending = false;
            ++ctx.action_index;
            break; // 一次只处理一个消息
        }
    }
}

void EventSystem::AcknowledgeChoice(int index) {
    choice_active_ = false;
    dialog_active_ = false;
    choices_.clear();
    choice_label_.clear();

    // 找到正在等待选择的上下文并推进
    for (auto& ctx : active_contexts_) {
        if (ctx.choices_pending) {
            ctx.choices_pending = false;
            ctx.selected_choice = index;
            variables_.SetIntSilent("_choice", index); // 不触发 OnVariableChange（与原行为一致）
            ++ctx.action_index;
            break;
        }
    }
}

// ---- 自开关（cache + 组件同步）----

void EventSystem::SetSelfSwitch(EntityID entity, char sw, bool value) {
    variables_.SetSelfSwitch(entity, sw, value);

    // 同步到组件
    auto* trigger = world_->GetComponent<EventTrigger>(entity);
    if (trigger) {
        trigger->SetSelfSwitch(sw, value);
    }
}

bool EventSystem::GetSelfSwitch(EntityID entity, char sw, bool default_val) const {
    if (const bool* v = variables_.FindSelfSwitch(entity, sw)) return *v;
    // fallback: 读取组件中的值（场景切换后新 EventSystem cache 为空）
    auto* trigger = world_->GetComponent<EventTrigger>(entity);
    if (trigger) return trigger->GetSelfSwitch(sw);
    return default_val;
}

} // namespace scene
} // namespace mg
