#include "event_editor_panel.h"
#include "src/editor/selection_manager.h"
#include "src/editor/scene_session.h"
#include "src/editor/editor_project_manager.h"
#include "src/editor/imgui/mg_imgui_icons.h"
#include "src/editor/imgui/mg_imgui_extensions.h"
#include "src/editor/imgui/mg_imgui_panel.h"
#include "scene/world.h"
#include "scene/components/event_trigger.h"
#include "scene/components/name.h"
#include "scene/action_commands.h"
#include "imgui.h"
#include <algorithm>
#include <cstring>

namespace mg {
namespace editor {

using namespace mg::scene;

const char* EventEditorPanel::GetIcon() const {
    return ICON_FA_LIGHTBULB;
}

// ------------------------------------------------------------------
// 命令名称映射
// ------------------------------------------------------------------
static const char* GetActionCmdName(ActionCmd cmd) {
    switch (cmd) {
        case ActionCmd::ShowMessage:    return "Show Message";
        case ActionCmd::ShowToast:      return "Show Toast";
        case ActionCmd::ShowChoices:    return "Show Choices";
        case ActionCmd::TransferPlayer: return "Transfer Player";
        case ActionCmd::MoveEntity:     return "Move Entity";
        case ActionCmd::PlayAnimation:  return "Play Animation";
        case ActionCmd::PlaySound:      return "Play Sound";
        case ActionCmd::PlayBGM:        return "Play BGM";
        case ActionCmd::StopBGM:        return "Stop BGM";
        case ActionCmd::PlayBGS:        return "Play BGS";
        case ActionCmd::StopBGS:        return "Stop BGS";
        case ActionCmd::PlayME:         return "Play ME";
        case ActionCmd::SetVariable:    return "Set Variable";
        case ActionCmd::SetSwitch:      return "Set Switch";
        case ActionCmd::SetSelfSwitch:  return "Set Self Switch";
        case ActionCmd::AddItem:        return "Add Item";
        case ActionCmd::RemoveItem:     return "Remove Item";
        case ActionCmd::ShopProcessing: return "Shop Processing";
        case ActionCmd::BattleProcessing: return "Battle Processing";
        case ActionCmd::ShowUI:         return "Show UI";
        case ActionCmd::HideUI:         return "Hide UI";
        case ActionCmd::Wait:           return "Wait";
        case ActionCmd::If:             return "If";
        case ActionCmd::Else:           return "Else";
        case ActionCmd::EndIf:          return "End If";
        case ActionCmd::Loop:           return "Loop";
        case ActionCmd::BreakLoop:      return "Break Loop";
        case ActionCmd::CallScript:     return "Call Script";
        default: return "Unknown";
    }
}

[[maybe_unused]] static const char* GetTriggerTypeName(EventPage::TriggerType type) {
    switch (type) {
        case EventPage::TriggerType::OnStart:          return "On Start";
        case EventPage::TriggerType::OnInteract:       return "On Interact";
        case EventPage::TriggerType::OnEnter:          return "On Enter";
        case EventPage::TriggerType::OnExit:           return "On Exit";
        case EventPage::TriggerType::OnTouch:          return "On Touch";
        case EventPage::TriggerType::OnUpdate:         return "On Update";
        case EventPage::TriggerType::OnVariableChange: return "On Variable Change";
        case EventPage::TriggerType::OnInput:          return "On Input";
        case EventPage::TriggerType::OnCollision:      return "On Collision";
        default: return "Unknown";
    }
}

[[maybe_unused]] static const char* GetConditionTypeName(EventPage::Condition::Type type) {
    switch (type) {
        case EventPage::Condition::Type::None:        return "None";
        case EventPage::Condition::Type::Variable:    return "Variable";
        case EventPage::Condition::Type::Switch:      return "Switch";
        case EventPage::Condition::Type::SelfSwitch:  return "Self Switch";
        default: return "Unknown";
    }
}

// ------------------------------------------------------------------
// 1. Page Tabs
// ------------------------------------------------------------------
static void DrawPageTabs(EventTrigger* trigger, int& active_page, bool& dirty) {
    int page_count = (int)trigger->pages.size();
    if (page_count == 0) {
        trigger->pages.emplace_back();
        page_count = 1;
        dirty = true;
    }
    if (active_page >= page_count) active_page = page_count - 1;
    if (active_page < 0) active_page = 0;

    ImGui::BeginGroup();
    for (int i = 0; i < page_count; ++i) {
        if (i > 0) ImGui::SameLine();
        char label[32];
        snprintf(label, sizeof(label), "Page %d##page%d", i + 1, i);
        if (ImGui::Button(label, ImVec2(0, 0))) {
            active_page = i;
        }
        if (active_page == i) {
            ImVec2 min = ImGui::GetItemRectMin();
            ImVec2 max = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(min.x, max.y - 1), ImVec2(max.x, max.y - 1),
                ImGui::GetColorU32(ImGuiCol_ButtonActive), 2.0f);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_PLUS " Add Page")) {
        trigger->pages.emplace_back();
        active_page = (int)trigger->pages.size() - 1;
        dirty = true;
    }
    if (page_count > 1) {
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_TRASH " Remove")) {
            trigger->pages.erase(trigger->pages.begin() + active_page);
            if (active_page >= (int)trigger->pages.size()) active_page = (int)trigger->pages.size() - 1;
            dirty = true;
        }
    }
    ImGui::EndGroup();
}

// ------------------------------------------------------------------
// 2. Trigger Config
// ------------------------------------------------------------------
static void DrawTriggerConfig(EventPage& page, bool& dirty) {
    const char* trigger_names[] = {
        "On Start", "On Interact", "On Enter", "On Exit", "On Touch",
        "On Update", "On Variable Change", "On Input", "On Collision"
    };
    EventPage::TriggerType trigger_values[] = {
        EventPage::TriggerType::OnStart, EventPage::TriggerType::OnInteract,
        EventPage::TriggerType::OnEnter, EventPage::TriggerType::OnExit,
        EventPage::TriggerType::OnTouch, EventPage::TriggerType::OnUpdate,
        EventPage::TriggerType::OnVariableChange, EventPage::TriggerType::OnInput,
        EventPage::TriggerType::OnCollision
    };
    int current = 0;
    for (int i = 0; i < 9; ++i) {
        if (page.trigger == trigger_values[i]) { current = i; break; }
    }
    if (ImGui::Combo("Trigger", &current, trigger_names, IM_ARRAYSIZE(trigger_names))) {
        page.trigger = trigger_values[current];
        dirty = true;
    }
    if (ImGui::Checkbox("Run Once", &page.run_once)) {
        dirty = true;
    }

    // OnInput：condition.name 复用为 SDL 按键名
    if (page.trigger == EventPage::TriggerType::OnInput) {
        char key_buf[64];
        strncpy(key_buf, page.condition.name.c_str(), sizeof(key_buf) - 1);
        key_buf[sizeof(key_buf) - 1] = '\0';
        if (ImGui::InputText("Key", key_buf, sizeof(key_buf))) {
            page.condition.name = key_buf;
            dirty = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("SDL 按键名（如 Space / Return / A），按下沿触发（持续按住不重复触发）");
        }
    }

    // 区域触发：玩家与事件点 ±16px 区域重叠驱动
    if (page.trigger == EventPage::TriggerType::OnEnter ||
        page.trigger == EventPage::TriggerType::OnExit ||
        page.trigger == EventPage::TriggerType::OnTouch ||
        page.trigger == EventPage::TriggerType::OnCollision) {
        ImGui::TextDisabled("玩家与事件点周围 ±16px 区域重叠时触发");
    }
}

// ------------------------------------------------------------------
// 3. Condition Editor
// ------------------------------------------------------------------
static void DrawCondition(EventPage::Condition& cond, bool& dirty) {
    const char* cond_types[] = {"None", "Variable", "Switch", "Self Switch"};
    int cond_type = (int)cond.type;
    if (ImGui::Combo("Condition", &cond_type, cond_types, IM_ARRAYSIZE(cond_types))) {
        cond.type = (EventPage::Condition::Type)cond_type;
        dirty = true;
    }

    if (cond.type == EventPage::Condition::Type::Variable) {
        char buf[64];
        strncpy(buf, cond.name.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        if (ImGui::InputText("Var Name", buf, sizeof(buf))) {
            cond.name = buf;
            dirty = true;
        }
        const char* ops[] = {"==", "!=", ">", "<", ">=", "<="};
        if (ImGui::Combo("Op", &cond.compare_op, ops, IM_ARRAYSIZE(ops))) dirty = true;
        if (ImGui::InputInt("Value", &cond.int_value)) dirty = true;
    }
    else if (cond.type == EventPage::Condition::Type::Switch) {
        char buf[64];
        strncpy(buf, cond.name.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        if (ImGui::InputText("Switch Name", buf, sizeof(buf))) {
            cond.name = buf;
            dirty = true;
        }
        if (ImGui::Checkbox("Value", &cond.bool_value)) dirty = true;
    }
    else if (cond.type == EventPage::Condition::Type::SelfSwitch) {
        const char* sw_names[] = {"A", "B", "C", "D"};
        int sw = 0;
        if (!cond.name.empty()) sw = cond.name[0] - 'A';
        if (sw < 0) sw = 0;
        if (sw > 3) sw = 3;
        if (ImGui::Combo("Switch", &sw, sw_names, IM_ARRAYSIZE(sw_names))) {
            cond.name = std::string(1, (char)('A' + sw));
            dirty = true;
        }
        if (ImGui::Checkbox("Value", &cond.bool_value)) dirty = true;
    }
}

// ------------------------------------------------------------------
// 4. Action Params
// ------------------------------------------------------------------
static void DrawActionParams(EditorProjectManager& project_, Action& action, bool& dirty) {
    switch (action.cmd) {
        case ActionCmd::ShowMessage: {
            char buf[256];
            strncpy(buf, action.text.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            if (ImGui::InputTextMultiline("Text", buf, sizeof(buf), ImVec2(0, 60))) {
                action.text = buf;
                dirty = true;
            }
            break;
        }
        case ActionCmd::ShowToast: {
            char buf[256];
            strncpy(buf, action.text.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            if (ImGui::InputText("Text", buf, sizeof(buf))) {
                action.text = buf;
                dirty = true;
            }
            break;
        }
        case ActionCmd::ShowChoices: {
            char buf[256];
            strncpy(buf, action.text.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            if (ImGui::InputText("Choices", buf, sizeof(buf))) {
                action.text = buf;
                dirty = true;
            }
            if (ImGui::InputInt("Default", &action.int_value)) dirty = true;
            break;
        }
        case ActionCmd::TransferPlayer: {
            char buf[64];
            strncpy(buf, action.target.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            if (ImGui::InputText("Target", buf, sizeof(buf))) {
                action.target = buf;
                dirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("...")) {
                ImGui::OpenPopup("SelectTargetScene");
            }
            // 场景选择弹窗
            ImVec2 center = ImGui::GetMainViewport()->GetCenter();
            ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            if (ImGui::BeginPopupModal("Select Target Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                auto mi = project_.LoadMapInfos();
                if (mi.entries.empty()) {
                    ImGui::TextDisabled("No scenes available.");
                } else {
                    for (const auto& e : mi.entries) {
                        std::string display = e.name;
                        if (display.size() > 5 && display.substr(display.size() - 5) == ".json") {
                            display = display.substr(0, display.size() - 5);
                        }
                        if (ImGui::Selectable(display.c_str())) {
                            action.target = e.name;
                            dirty = true;
                            ImGui::CloseCurrentPopup();
                        }
                    }
                }
                ImGui::Separator();
                if (ImGui::Button("Cancel")) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            if (ImGui::InputInt("X", &action.int_value)) dirty = true;
            if (ImGui::InputFloat("Y", &action.float_value)) dirty = true;
            break;
        }
        case ActionCmd::MoveEntity: {
            char buf[64];
            strncpy(buf, action.target.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            if (ImGui::InputText("Target Entity", buf, sizeof(buf))) {
                action.target = buf;
                dirty = true;
            }
            char path_buf[256];
            strncpy(path_buf, action.param.c_str(), sizeof(path_buf) - 1);
            path_buf[sizeof(path_buf) - 1] = '\0';
            if (ImGui::InputText("Path (x,y;x,y)", path_buf, sizeof(path_buf))) {
                action.param = path_buf;
                dirty = true;
            }
            if (ImGui::InputInt("Speed (px/s)", &action.int_value)) dirty = true;
            break;
        }
        case ActionCmd::PlayAnimation:
        case ActionCmd::PlaySound:
        case ActionCmd::PlayBGM:
        case ActionCmd::PlayBGS:
        case ActionCmd::PlayME:
        case ActionCmd::ShowUI:
        case ActionCmd::HideUI:
        case ActionCmd::CallScript: {
            char buf[128];
            strncpy(buf, action.text.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            if (ImGui::InputText("Path", buf, sizeof(buf))) {
                action.text = buf;
                dirty = true;
            }
            if (action.cmd == ActionCmd::PlayBGM || action.cmd == ActionCmd::PlayBGS) {
                if (ImGui::InputFloat("Fade In (sec)", &action.float_value)) dirty = true;
                if (action.float_value < 0.0f) action.float_value = 0.0f;
            }
            break;
        }
        case ActionCmd::StopBGM:
        case ActionCmd::StopBGS: {
            if (ImGui::InputFloat("Fade Out (sec)", &action.float_value)) dirty = true;
            if (action.float_value < 0.0f) action.float_value = 0.0f;
            break;
        }
        case ActionCmd::SetVariable: {
            char buf[64];
            strncpy(buf, action.target.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            if (ImGui::InputText("Name", buf, sizeof(buf))) {
                action.target = buf;
                dirty = true;
            }
            if (ImGui::InputInt("Value", &action.int_value)) dirty = true;
            break;
        }
        case ActionCmd::SetSwitch: {
            char buf[64];
            strncpy(buf, action.target.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            if (ImGui::InputText("Name", buf, sizeof(buf))) {
                action.target = buf;
                dirty = true;
            }
            if (ImGui::Checkbox("Value", &action.bool_value)) dirty = true;
            break;
        }
        case ActionCmd::SetSelfSwitch: {
            const char* sw_names[] = {"A", "B", "C", "D"};
            int sw = 0;
            if (!action.target.empty()) sw = action.target[0] - 'A';
            if (sw < 0) sw = 0;
        if (sw > 3) sw = 3;
            if (ImGui::Combo("Switch", &sw, sw_names, IM_ARRAYSIZE(sw_names))) {
                action.target = std::string(1, (char)('A' + sw));
                dirty = true;
            }
            if (ImGui::Checkbox("Value", &action.bool_value)) dirty = true;
            break;
        }
        case ActionCmd::AddItem:
        case ActionCmd::RemoveItem: {
            char buf[64];
            strncpy(buf, action.target.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            if (ImGui::InputText("Item", buf, sizeof(buf))) {
                action.target = buf;
                dirty = true;
            }
            if (ImGui::InputInt("Count", &action.int_value)) dirty = true;
            break;
        }
        case ActionCmd::ShopProcessing: {
            char buf[64];
            strncpy(buf, action.target.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            if (ImGui::InputText("Shop ID", buf, sizeof(buf))) {
                action.target = buf;
                dirty = true;
            }
            break;
        }
        case ActionCmd::BattleProcessing: {
            char buf[64];
            strncpy(buf, action.target.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            if (ImGui::InputText("Enemy ID", buf, sizeof(buf))) {
                action.target = buf;
                dirty = true;
            }
            if (ImGui::InputInt("Count", &action.int_value)) dirty = true;
            strncpy(buf, action.param.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            if (ImGui::InputText("Player Actor ID (empty=Player)", buf, sizeof(buf))) {
                action.param = buf;
                dirty = true;
            }
            break;
        }
        case ActionCmd::Wait: {
            if (ImGui::InputFloat("Seconds", &action.float_value, 0.1f)) dirty = true;
            break;
        }
        case ActionCmd::If: {
            const char* types[] = {"switch", "choice", "variable"};
            int type_idx = 0;
            if (action.param == "choice") type_idx = 1;
            else if (action.param == "variable") type_idx = 2;
            if (ImGui::Combo("Type", &type_idx, types, 3)) {
                action.param = types[type_idx];
                dirty = true;
            }
            if (action.param == "switch") {
                char buf[64];
                strncpy(buf, action.target.c_str(), sizeof(buf) - 1);
                buf[sizeof(buf) - 1] = '\0';
                if (ImGui::InputText("Name", buf, sizeof(buf))) {
                    action.target = buf;
                    dirty = true;
                }
                if (ImGui::Checkbox("Value", &action.bool_value)) dirty = true;
            } else if (action.param == "choice") {
                int choice_idx = (int)action.float_value;
                if (ImGui::InputInt("Choice Index", &choice_idx)) {
                    action.float_value = (float)choice_idx;
                    dirty = true;
                }
            } else {
                char buf[64];
                strncpy(buf, action.target.c_str(), sizeof(buf) - 1);
                buf[sizeof(buf) - 1] = '\0';
                if (ImGui::InputText("Var Name", buf, sizeof(buf))) {
                    action.target = buf;
                    dirty = true;
                }
                const char* ops[] = {"==", "!=", ">", "<", ">=", "<="};
                if (ImGui::Combo("Op", &action.int_value, ops, IM_ARRAYSIZE(ops))) dirty = true;
                if (ImGui::InputFloat("Value", &action.float_value)) dirty = true;
            }
            break;
        }
        case ActionCmd::Loop: {
            if (ImGui::InputInt("Count (-1 = inf)", &action.int_value)) dirty = true;
            break;
        }
        case ActionCmd::Else:
        case ActionCmd::EndIf:
        case ActionCmd::BreakLoop:
        default:
            ImGui::TextDisabled("No parameters.");
            break;
    }
}

// ------------------------------------------------------------------
// 5. Action List
// ------------------------------------------------------------------
static void DrawActionList(EditorProjectManager& project_, EventPage& page, bool& dirty) {
    ImGui::Text("Actions");

    int remove_idx = -1;
    int swap_a = -1, swap_b = -1;

    for (int i = 0; i < (int)page.actions.size(); ++i) {
        auto& action = page.actions[i];
        ImGui::PushID(i);

        // Header row
        bool open = ImGui::TreeNodeEx(GetActionCmdName(action.cmd),
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap,
            "%s", GetActionCmdName(action.cmd));

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60);
        if (ImGui::SmallButton(ICON_FA_TRASH)) {
            remove_idx = i;
            dirty = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton(ICON_FA_ARROW_UP) && i > 0) {
            swap_a = i; swap_b = i - 1;
            dirty = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton(ICON_FA_ARROW_DOWN) && i < (int)page.actions.size() - 1) {
            swap_a = i; swap_b = i + 1;
            dirty = true;
        }

        if (open) {
            // Cmd type combo
            const char* cmd_names[] = {
                "Show Message", "Show Toast", "Show Choices", "Transfer Player", "Move Entity",
                "Play Animation", "Play Sound", "Play BGM", "Stop BGM", "Play BGS", "Stop BGS", "Play ME",
                "Set Variable", "Set Switch", "Set Self Switch",
                "Add Item", "Remove Item", "Shop Processing", "Battle Processing",
                "Show UI", "Hide UI",
                "Wait",
                "If", "Else", "End If",
                "Loop", "Break Loop",
                "Call Script"
            };
            ActionCmd cmd_values[] = {
                ActionCmd::ShowMessage, ActionCmd::ShowToast, ActionCmd::ShowChoices,
                ActionCmd::TransferPlayer, ActionCmd::MoveEntity,
                ActionCmd::PlayAnimation, ActionCmd::PlaySound, ActionCmd::PlayBGM,
                ActionCmd::StopBGM, ActionCmd::PlayBGS, ActionCmd::StopBGS, ActionCmd::PlayME,
                ActionCmd::SetVariable, ActionCmd::SetSwitch, ActionCmd::SetSelfSwitch,
                ActionCmd::AddItem, ActionCmd::RemoveItem, ActionCmd::ShopProcessing, ActionCmd::BattleProcessing,
                ActionCmd::ShowUI, ActionCmd::HideUI,
                ActionCmd::Wait,
                ActionCmd::If, ActionCmd::Else, ActionCmd::EndIf,
                ActionCmd::Loop, ActionCmd::BreakLoop,
                ActionCmd::CallScript
            };
            int cmd_idx = 0;
            for (int j = 0; j < 28; ++j) {
                if (action.cmd == cmd_values[j]) { cmd_idx = j; break; }
            }
            if (ImGui::Combo("Type", &cmd_idx, cmd_names, IM_ARRAYSIZE(cmd_names))) {
                action.cmd = cmd_values[cmd_idx];
                dirty = true;
            }

            DrawActionParams(project_, action, dirty);
            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    if (remove_idx >= 0) {
        page.actions.erase(page.actions.begin() + remove_idx);
    }
    if (swap_a >= 0 && swap_b >= 0) {
        std::swap(page.actions[swap_a], page.actions[swap_b]);
    }

    if (ImGui::Button(ICON_FA_PLUS " Add Action")) {
        ImGui::OpenPopup("AddActionPopup");
    }
    if (ImGui::BeginPopup("AddActionPopup")) {
        const char* cmd_names[] = {
            "Show Message", "Show Toast", "Show Choices", "Transfer Player", "Move Entity",
            "Play Animation", "Play Sound", "Play BGM", "Stop BGM", "Play BGS", "Stop BGS", "Play ME",
            "Set Variable", "Set Switch", "Set Self Switch",
            "Add Item", "Remove Item", "Shop Processing", "Battle Processing",
            "Show UI", "Hide UI",
            "Wait",
            "If", "Else", "End If",
            "Loop", "Break Loop",
            "Call Script"
        };
        ActionCmd cmd_values[] = {
            ActionCmd::ShowMessage, ActionCmd::ShowToast, ActionCmd::ShowChoices,
            ActionCmd::TransferPlayer, ActionCmd::MoveEntity,
            ActionCmd::PlayAnimation, ActionCmd::PlaySound, ActionCmd::PlayBGM,
            ActionCmd::StopBGM, ActionCmd::PlayBGS, ActionCmd::StopBGS, ActionCmd::PlayME,
            ActionCmd::SetVariable, ActionCmd::SetSwitch, ActionCmd::SetSelfSwitch,
            ActionCmd::AddItem, ActionCmd::RemoveItem, ActionCmd::ShopProcessing, ActionCmd::BattleProcessing,
            ActionCmd::ShowUI, ActionCmd::HideUI,
            ActionCmd::Wait,
            ActionCmd::If, ActionCmd::Else, ActionCmd::EndIf,
            ActionCmd::Loop, ActionCmd::BreakLoop,
            ActionCmd::CallScript
        };
        const int cmd_count = IM_ARRAYSIZE(cmd_names);
        for (int i = 0; i < cmd_count; ++i) {
            if (ImGui::MenuItem(cmd_names[i])) {
                page.actions.push_back(Action{cmd_values[i]});
                dirty = true;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }
}

// ------------------------------------------------------------------
// 6. Self Switches
// ------------------------------------------------------------------
static void DrawSelfSwitches(EventTrigger& trigger, bool& dirty) {
    ImGui::Text("Self Switches");
    bool a = trigger.GetSelfSwitch('A');
    bool b = trigger.GetSelfSwitch('B');
    bool c = trigger.GetSelfSwitch('C');
    bool d = trigger.GetSelfSwitch('D');
    if (ImGui::Checkbox("A", &a)) { trigger.SetSelfSwitch('A', a); dirty = true; }
    ImGui::SameLine();
    if (ImGui::Checkbox("B", &b)) { trigger.SetSelfSwitch('B', b); dirty = true; }
    ImGui::SameLine();
    if (ImGui::Checkbox("C", &c)) { trigger.SetSelfSwitch('C', c); dirty = true; }
    ImGui::SameLine();
    if (ImGui::Checkbox("D", &d)) { trigger.SetSelfSwitch('D', d); dirty = true; }
}

// ------------------------------------------------------------------
// Main Draw
// ------------------------------------------------------------------
void EventEditorPanel::Draw() {

    // 动态标题：选中实体时显示实体名
    std::string window_title = "Event Editor";
    if (selection_.GetType() == SelectionManager::Type::Entity) {
        auto* scene = session_.GetActiveScene();
        if (scene && scene->GetWorld()) {
            auto* name_comp = scene->GetWorld()->GetComponent<scene::Name>(selection_.GetSelectedEntity());
            if (name_comp && !name_comp->value.empty()) {
                window_title += ": " + name_comp->value;
            }
        }
    }

    if (!MG_BeginPanel(window_title.c_str(), ICON_FA_LIGHTBULB, &is_open_,
                       ImGuiWindowFlags_NoDocking)) { MG_EndPanel(); return; }
    if (selection_.GetType() != SelectionManager::Type::Entity) {
        ImGui::TextDisabled("Select an entity to edit events.");
        MG_EndPanel();
        return;
    }

    auto* scene = session_.GetActiveScene();
    if (!scene || !scene->GetWorld()) {
        ImGui::TextDisabled("No active scene.");
        MG_EndPanel();
        return;
    }

    auto* world = scene->GetWorld();
    auto e = selection_.GetSelectedEntity();
    if (!world->IsValid(e)) {
        ImGui::TextDisabled("Selected entity is no longer valid.");
        MG_EndPanel();
        return;
    }

    auto* trigger = world->GetComponent<EventTrigger>(e);
    if (!trigger) {
        if (ImGui::Button(ICON_FA_PLUS " Add Event Trigger")) {
            world->AddComponent<EventTrigger>(e);
            session_.MarkSceneDirty();
        }
        MG_EndPanel();
        return;
    }

    auto* name = world->GetComponent<Name>(e);
    ImGui::Text(ICON_FA_LIGHTBULB " Event: %s", name ? name->value.c_str() : "Unnamed");
    ImGui::Separator();

    if (trigger->pages.empty()) {
        trigger->pages.emplace_back();
        session_.MarkSceneDirty();
    }
    if (active_page_ >= (int)trigger->pages.size()) active_page_ = 0;

    bool dirty = false;
    DrawPageTabs(trigger, active_page_, dirty);
    auto& page = trigger->pages[active_page_];

    DrawTriggerConfig(page, dirty);
    ImGui::Separator();
    DrawCondition(page.condition, dirty);
    ImGui::Separator();
    DrawActionList(project_, page, dirty);
    ImGui::Separator();
    DrawSelfSwitches(*trigger, dirty);

    if (dirty) session_.MarkSceneDirty();

    MG_EndPanel();
}

} // namespace editor
} // namespace mg
