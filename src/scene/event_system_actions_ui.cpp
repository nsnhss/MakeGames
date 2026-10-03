// event_system_actions_ui.cpp — 对话框/Toast/选项/HUD 动作（从 event_system.cpp 拆分，D1）
#include "scene/event_system.h"
#include "scene/components/transform.h"
#include <sstream>

namespace mg {
namespace scene {

void EventSystem::ExecDialogActions(EventContext& ctx, const Action& action) {
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
        default:
            break;
    }
}

} // namespace scene
} // namespace mg
