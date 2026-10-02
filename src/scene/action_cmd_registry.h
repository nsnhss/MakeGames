#pragma once
// ---------------------------------------------------------------------------
// ActionCmd 注册表（D0-6：唯一来源）
// 收敛原先三处重复：scene_serializer 的序列化/反序列化映射 +
// event_editor_panel 的显示名映射与 Add Action 菜单数组。
// ---------------------------------------------------------------------------
#include "scene/action_commands.h"
#include <cstring>

namespace mg {
namespace scene {

struct ActionCmdInfo {
    ActionCmd cmd;
    const char* serialized;  // 存档格式名（格式兼容性冻结，勿改）
    const char* display;     // UI 显示名
};

// 顺序即 editor "Add Action" 菜单顺序
inline constexpr ActionCmdInfo kActionCmdRegistry[] = {
    { ActionCmd::ShowMessage,      "ShowMessage",      "Show Message" },
    { ActionCmd::ShowToast,        "ShowToast",        "Show Toast" },
    { ActionCmd::ShowChoices,      "ShowChoices",      "Show Choices" },
    { ActionCmd::TransferPlayer,   "TransferPlayer",   "Transfer Player" },
    { ActionCmd::MoveEntity,       "MoveEntity",       "Move Entity" },
    { ActionCmd::PlayAnimation,    "PlayAnimation",    "Play Animation" },
    { ActionCmd::PlaySound,        "PlaySound",        "Play Sound" },
    { ActionCmd::PlayBGM,          "PlayBGM",          "Play BGM" },
    { ActionCmd::StopBGM,          "StopBGM",          "Stop BGM" },
    { ActionCmd::PlayBGS,          "PlayBGS",          "Play BGS" },
    { ActionCmd::StopBGS,          "StopBGS",          "Stop BGS" },
    { ActionCmd::PlayME,           "PlayME",           "Play ME" },
    { ActionCmd::SetVariable,      "SetVariable",      "Set Variable" },
    { ActionCmd::SetSwitch,        "SetSwitch",        "Set Switch" },
    { ActionCmd::SetSelfSwitch,    "SetSelfSwitch",    "Set Self Switch" },
    { ActionCmd::AddItem,          "AddItem",          "Add Item" },
    { ActionCmd::RemoveItem,       "RemoveItem",       "Remove Item" },
    { ActionCmd::ShopProcessing,   "ShopProcessing",   "Shop Processing" },
    { ActionCmd::BattleProcessing, "BattleProcessing", "Battle Processing" },
    { ActionCmd::ShowUI,           "ShowUI",           "Show UI" },
    { ActionCmd::HideUI,           "HideUI",           "Hide UI" },
    { ActionCmd::Wait,             "Wait",             "Wait" },
    { ActionCmd::If,               "If",               "If" },
    { ActionCmd::Else,             "Else",             "Else" },
    { ActionCmd::EndIf,            "EndIf",            "End If" },
    { ActionCmd::Loop,             "Loop",             "Loop" },
    { ActionCmd::BreakLoop,        "BreakLoop",        "Break Loop" },
    { ActionCmd::CallScript,       "CallScript",       "Call Script" },
};

// 序列化名（存档格式）；未知命令返回 "Unknown"（与原 scene_serializer 一致）
inline const char* ActionCmdToSerializedName(ActionCmd cmd) {
    for (const auto& info : kActionCmdRegistry)
        if (info.cmd == cmd) return info.serialized;
    return "Unknown";
}

// UI 显示名；未知命令返回 "Unknown"
inline const char* GetActionCmdDisplayName(ActionCmd cmd) {
    for (const auto& info : kActionCmdRegistry)
        if (info.cmd == cmd) return info.display;
    return "Unknown";
}

// 反序列化；未识别时 out 置 ActionCmd::Unknown 并返回 false（与原行为一致）
inline bool ActionCmdFromSerializedName(const char* name, ActionCmd& out) {
    for (const auto& info : kActionCmdRegistry) {
        if (std::strcmp(info.serialized, name) == 0) {
            out = info.cmd;
            return true;
        }
    }
    out = ActionCmd::Unknown;
    return false;
}

} // namespace scene
} // namespace mg
