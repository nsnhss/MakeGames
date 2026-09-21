#include "scene/event_system.h"
#include "scene/components/transform.h"
#include "scene/components/movement.h"
#include "scene/components/tilemap.h"
#include "scene/components/name.h"
#include "scene/components/animator.h"
#include "audio/audio_manager.h"
#include "resource/actor_database.h"
#include <SDL.h>
#include <cassert>
#include <sstream>
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace mg {
namespace scene {

EventSystem::EventSystem(World* world) : world_(world) {}

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
    if (battle_state_ != BattleState::None) {
        UpdateBattle(dt);
    }
}

// ---- 触发接口 ----

void EventSystem::TriggerOnStart() {
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

    constexpr float PLAYER_HALF = 14.0f; // 与 play mode 碰撞半宽一致
    constexpr float EVENT_HALF  = 16.0f; // 事件点半径（32px tile 的半格）

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

    // 差分：新进入 → OnEnter + OnCollision；持续重叠 → OnTouch；新离开 → OnExit
    for (EntityID e : cur) {
        if (overlapping_events_.contains(e)) {
            TriggerOnTouch(e, player);
        } else {
            TriggerOnEnter(e);
            TriggerOnCollision(e, player);
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
    if (shop_pending_) {
        return; // 等待商店关闭
    }
    if (battle_state_ != BattleState::None) {
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
    if (!ctx.waiting && !ctx.message_pending && !ctx.choices_pending && !ctx.move_pending && !ctx.finished && !shop_pending_ && battle_state_ == BattleState::None) {
        ++ctx.action_index;
    }
}

void EventSystem::ExecuteAction(EventContext& ctx, const Action& action) {
    switch (action.cmd) {
        case ActionCmd::ShowMessage: {
            has_pending_message_ = true;
            dialog_active_ = true;
            pending_message_ = action.text;
            pending_message_name_ = action.target; // 说话人名称
            ctx.message_pending = true;
            break;
        }
        case ActionCmd::ShowToast: {
            ToastMessage t;
            t.text = action.text;
            t.timer = 2.0f;
            if (auto* transform = world_->GetComponent<Transform>(ctx.entity)) {
                t.world_pos = transform->position;
                t.world_pos.y += 24.0f; // 实体头顶上方
            }
            toasts_.push_back(t);
            break; // 不阻塞事件流
        }
        case ActionCmd::ShowChoices: {
            // 解析选项：text 字段用 "|" 分隔
            choices_.clear();
            std::stringstream ss(action.text);
            std::string part;
            while (std::getline(ss, part, '|')) {
                if (!part.empty()) choices_.push_back(part);
            }
            ctx.choices_pending = true;
            ctx.choice_count = (int)choices_.size();
            ctx.selected_choice = -1;
            choice_label_ = action.param;
            choice_active_ = true;
            dialog_active_ = true; // 阻塞移动
            break;
        }
        case ActionCmd::SetVariable: {
            SetVariable(action.target, action.int_value);
            break;
        }
        case ActionCmd::SetSwitch: {
            SetSwitch(action.target, action.bool_value);
            break;
        }
        case ActionCmd::SetSelfSwitch: {
            char sw = action.param.empty() ? 'A' : action.param[0];
            SetSelfSwitch(ctx.entity, sw, action.bool_value);
            break;
        }
        case ActionCmd::Wait: {
            ctx.waiting = true;
            ctx.wait_timer = action.float_value;
            break;
        }
        case ActionCmd::TransferPlayer: {
            if (!action.target.empty()) {
                // 跨场景传送：设置切换信号，由 EditorLayer 处理
                pending_scene_transition_ = action.target;
                pending_spawn_x_ = action.int_value;
                pending_spawn_y_ = (int)action.float_value;
                break;
            }
            // 同场景传送：移动 Player entity 到指定 tile 坐标
            std::vector<EntityID> players;
            world_->Query<Movement>(players);
            if (!players.empty()) {
                auto* t = world_->GetComponent<Transform>(players[0]);
                if (t) {
                    // 从 Tilemap 读取 tile_size
                    float tw = 32.0f, th = 32.0f;
                    std::vector<EntityID> all;
                    world_->GetAllEntities(all);
                    for (auto e : all) {
                        auto* tm = world_->GetComponent<Tilemap>(e);
                        if (tm) { tw = (float)tm->tile_width; th = (float)tm->tile_height; break; }
                    }
                    t->position.x = (float)action.int_value * tw;
                    t->position.y = (float)action.float_value * th;
                }
            }
            break;
        }
        case ActionCmd::PlayAnimation: {
            EntityID target = kInvalidEntity;
            if (!action.text.empty()) {
                std::vector<EntityID> all;
                world_->GetAllEntities(all);
                for (auto e : all) {
                    auto* name = world_->GetComponent<Name>(e);
                    if (name && name->value == action.text) { target = e; break; }
                }
            }
            if (target == kInvalidEntity) target = ctx.entity;
            if (auto* anim = world_->GetComponent<Animator>(target)) {
                anim->playing = true;
                anim->current_frame = 0;
                anim->timer = 0.0f;
            }
            break;
        }
        case ActionCmd::PlaySound: {
            AudioManager::Get().PlaySE(action.text);
            break;
        }
        case ActionCmd::PlayBGM: {
            AudioManager::Get().PlayBGM(action.text, 1.0f, action.float_value);
            break;
        }
        case ActionCmd::StopBGM: {
            AudioManager::Get().StopBGM(action.float_value);
            break;
        }
        case ActionCmd::PlayBGS: {
            AudioManager::Get().PlayBGS(action.text, 1.0f, action.float_value);
            break;
        }
        case ActionCmd::StopBGS: {
            AudioManager::Get().StopBGS(action.float_value);
            break;
        }
        case ActionCmd::PlayME: {
            AudioManager::Get().PlayME(action.text);
            break;
        }
        case ActionCmd::MoveEntity: {
            // 查找目标实体
            EntityID target = kInvalidEntity;
            if (action.target == "@self") {
                target = ctx.entity;
            } else {
                std::vector<EntityID> all;
                world_->GetAllEntities(all);
                for (auto e : all) {
                    auto* name = world_->GetComponent<Name>(e);
                    if (name && name->value == action.target) { target = e; break; }
                }
            }
            if (target == kInvalidEntity) {
                ToastMessage toast;
                toast.text = "MoveEntity: target not found";
                toast.timer = 2.0f;
                if (auto* transform = world_->GetComponent<Transform>(ctx.entity)) {
                    toast.world_pos = transform->position;
                    toast.world_pos.y += 24.0f;
                }
                toasts_.push_back(toast);
                break;
            }

            // 解析路径 "x1,y1;x2,y2;..."
            std::vector<math::Vec2> path;
            {
                const std::string& s = action.param;
                size_t start = 0;
                while (start < s.size()) {
                    size_t sep = s.find(';', start);
                    std::string pair = (sep == std::string::npos) ? s.substr(start) : s.substr(start, sep - start);
                    size_t comma = pair.find(',');
                    if (comma != std::string::npos) {
                        try {
                            float px = std::stof(pair.substr(0, comma));
                            float py = std::stof(pair.substr(comma + 1));
                            path.emplace_back(px, py);
                        } catch (...) {}
                    }
                    if (sep == std::string::npos) break;
                    start = sep + 1;
                }
            }
            if (path.empty()) {
                ToastMessage toast;
                toast.text = "MoveEntity: empty path";
                toast.timer = 2.0f;
                if (auto* transform = world_->GetComponent<Transform>(ctx.entity)) {
                    toast.world_pos = transform->position;
                    toast.world_pos.y += 24.0f;
                }
                toasts_.push_back(toast);
                break;
            }

            ctx.move_target = target;
            ctx.move_path = std::move(path);
            ctx.move_speed = action.int_value > 0 ? (float)action.int_value : 60.0f;
            ctx.move_index = 0;
            ctx.move_pending = true;
            break;
        }
        case ActionCmd::AddItem: {
            std::string key = "item_" + action.target;
            int count = action.int_value > 0 ? action.int_value : 1;
            int_variables_[key] += count;
            TriggerOnVariableChange(key);
            break;
        }
        case ActionCmd::RemoveItem: {
            std::string key = "item_" + action.target;
            int count = action.int_value > 0 ? action.int_value : 1;
            auto it = int_variables_.find(key);
            if (it != int_variables_.end()) {
                it->second = std::max(0, it->second - count);
                if (it->second == 0) int_variables_.erase(it);
            }
            TriggerOnVariableChange(key);
            break;
        }
        case ActionCmd::ShopProcessing: {
            if (shop_db_ && shop_db_->Find(action.target)) {
                shop_pending_ = true;
                pending_shop_id_ = action.target;
            }
            break;
        }
        case ActionCmd::BattleProcessing: {
            if (enemy_db_ && enemy_db_->Find(action.target)) {
                StartBattle(action.target, action.int_value > 0 ? action.int_value : 1, action.param);
            }
            break;
        }
        case ActionCmd::ShowUI: {
            HUDOverlay hud;
            hud.name = action.target;
            hud.text = action.text;
            hud.visible = true;
            hud_overlays_[hud.name] = hud;
            break;
        }
        case ActionCmd::HideUI: {
            auto it = hud_overlays_.find(action.target);
            if (it != hud_overlays_.end()) it->second.visible = false;
            break;
        }
        case ActionCmd::If: {
            bool result = EvaluateActionCondition(action);
            if (!result) {
                int target_idx = SkipToEndIf(
                    world_->GetComponent<EventTrigger>(ctx.entity)->pages[ctx.page_index].actions,
                    (int)ctx.action_index);
                ctx.action_index = (size_t)target_idx; // 跳到 Else/EndIf，下帧自动++
            }
            break;
        }
        case ActionCmd::Else: {
            // If-true 之后遇到 Else，跳过到 EndIf
            int target_idx = SkipToEndIf(
                world_->GetComponent<EventTrigger>(ctx.entity)->pages[ctx.page_index].actions,
                (int)ctx.action_index);
            ctx.action_index = (size_t)target_idx;
            break;
        }
        case ActionCmd::EndIf: {
            // 什么都不做，正常推进
            break;
        }
        case ActionCmd::Loop: {
            ctx.loop_active = true;
            ctx.loop_start = ctx.action_index;
            ctx.loop_count = action.int_value;
            ctx.loop_current = 0;
            break;
        }
        case ActionCmd::BreakLoop: {
            if (ctx.loop_active) {
                ctx.loop_active = false;
                auto* trigger = world_->GetComponent<EventTrigger>(ctx.entity);
                if (trigger && ctx.page_index < trigger->pages.size()) {
                    ctx.action_index = trigger->pages[ctx.page_index].actions.size();
                }
            }
            break;
        }
        case ActionCmd::CallScript: {
            if (script_engine_) {
                script_engine_->SetContext(world_, this);
                script_engine_->DoFile(action.text);
            }
            break;
        }
        default:
            break;
    }
}

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
            int_variables_["_choice"] = index;
            ++ctx.action_index;
            break;
        }
    }
}

bool EventSystem::EvaluateActionCondition(const Action& action) {
    if (action.param == "choice") {
        return GetVariableInt("_choice", -1) == (int)action.float_value;
    }
    if (action.param == "switch") {
        return GetSwitch(action.target) == action.bool_value;
    }
    if (action.param == "variable") {
        int val = GetVariableInt(action.target, 0);
        int cmp = (int)action.float_value;
        switch (action.int_value) {
            case 0: return val == cmp;
            case 1: return val != cmp;
            case 2: return val > cmp;
            case 3: return val < cmp;
            case 4: return val >= cmp;
            case 5: return val <= cmp;
        }
        return false;
    }
    return false;
}

int EventSystem::SkipToEndIf(const std::vector<Action>& actions, int start) {
    int depth = 1;
    for (int i = start + 1; i < (int)actions.size(); ++i) {
        if (actions[i].cmd == ActionCmd::If) depth++;
        else if (actions[i].cmd == ActionCmd::EndIf) {
            depth--;
            if (depth == 0) return i;
        }
        else if (actions[i].cmd == ActionCmd::Else && depth == 1) return i;
    }
    return (int)actions.size();
}

// ---- 全局状态 ----

void EventSystem::SetVariable(const std::string& name, int value) {
    int_variables_[name] = value;
    TriggerOnVariableChange(name);
}

void EventSystem::SetVariable(const std::string& name, float value) {
    float_variables_[name] = value;
}

void EventSystem::SetVariable(const std::string& name, bool value) {
    bool_variables_[name] = value;
}

int EventSystem::GetVariableInt(const std::string& name, int default_val) const {
    auto it = int_variables_.find(name);
    return (it != int_variables_.end()) ? it->second : default_val;
}

float EventSystem::GetVariableFloat(const std::string& name, float default_val) const {
    auto it = float_variables_.find(name);
    return (it != float_variables_.end()) ? it->second : default_val;
}

bool EventSystem::GetVariableBool(const std::string& name, bool default_val) const {
    auto it = bool_variables_.find(name);
    return (it != bool_variables_.end()) ? it->second : default_val;
}

void EventSystem::SetSwitch(const std::string& name, bool value) {
    switches_[name] = value;
}

bool EventSystem::GetSwitch(const std::string& name, bool default_val) const {
    auto it = switches_.find(name);
    return (it != switches_.end()) ? it->second : default_val;
}

void EventSystem::SetSelfSwitch(EntityID entity, char sw, bool value) {
    std::string key = std::to_string(entity) + "_" + sw;
    self_switches_[key] = value;

    // 同步到组件
    auto* trigger = world_->GetComponent<EventTrigger>(entity);
    if (trigger) {
        trigger->SetSelfSwitch(sw, value);
    }
}

bool EventSystem::GetSelfSwitch(EntityID entity, char sw, bool default_val) const {
    std::string key = std::to_string(entity) + "_" + sw;
    auto it = self_switches_.find(key);
    if (it != self_switches_.end()) return it->second;
    // fallback: 读取组件中的值（场景切换后新 EventSystem cache 为空）
    auto* trigger = world_->GetComponent<EventTrigger>(entity);
    if (trigger) return trigger->GetSelfSwitch(sw);
    return default_val;
}

std::vector<std::pair<std::string, int>> EventSystem::GetInventoryItems() const {
    std::vector<std::pair<std::string, int>> items;
    for (const auto& [key, value] : int_variables_) {
        if (key.rfind("item_", 0) == 0 && value > 0) {
            items.emplace_back(key.substr(5), value);
        }
    }
    return items;
}

EventSystem::GlobalStateSnapshot EventSystem::TakeGlobalStateSnapshot() const {
    GlobalStateSnapshot snap;
    snap.int_vars       = int_variables_;
    snap.float_vars     = float_variables_;
    snap.bool_vars      = bool_variables_;
    snap.switches       = switches_;
    snap.self_switches  = self_switches_;
    return snap;
}

void EventSystem::RestoreGlobalState(const GlobalStateSnapshot& snap) {
    int_variables_   = snap.int_vars;
    float_variables_ = snap.float_vars;
    bool_variables_  = snap.bool_vars;
    switches_        = snap.switches;
    self_switches_   = snap.self_switches;
}

nlohmann::json EventSystem::SaveState() const {
    nlohmann::json j;
    auto& g = j["globals"];
    g["int_vars"]       = int_variables_;
    g["float_vars"]     = float_variables_;
    g["bool_vars"]      = bool_variables_;
    g["switches"]       = switches_;
    g["self_switches"]  = self_switches_;
    return j;
}

void EventSystem::LoadState(const nlohmann::json& j) {
    if (!j.contains("globals")) return;
    const auto& g = j["globals"];
    if (g.contains("int_vars"))       int_variables_    = g["int_vars"].get<std::unordered_map<std::string, int>>();
    if (g.contains("float_vars"))     float_variables_  = g["float_vars"].get<std::unordered_map<std::string, float>>();
    if (g.contains("bool_vars"))      bool_variables_   = g["bool_vars"].get<std::unordered_map<std::string, bool>>();
    if (g.contains("switches"))       switches_         = g["switches"].get<std::unordered_map<std::string, bool>>();
    if (g.contains("self_switches"))  self_switches_    = g["self_switches"].get<std::unordered_map<std::string, bool>>();
}

} // namespace scene
} // namespace mg
