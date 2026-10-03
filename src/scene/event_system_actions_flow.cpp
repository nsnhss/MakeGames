// event_system_actions_flow.cpp — 流程控制/脚本动作（从 event_system.cpp 拆分，D1）
#include "scene/event_system.h"
#include "script/script_engine.h"

namespace mg {
namespace scene {

void EventSystem::ExecFlowActions(EventContext& ctx, const Action& action) {
    switch (action.cmd) {
        case ActionCmd::Wait: {
            ctx.waiting = true;
            ctx.wait_timer = action.float_value;
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

} // namespace scene
} // namespace mg
