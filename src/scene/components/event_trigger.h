#pragma once
#include "scene/component.h"
#include "scene/action_commands.h"
#include "third_party/json.hpp"
#include <vector>
#include <string>

namespace mg {
namespace scene {

struct EventPage {
    enum class TriggerType {
        OnStart,          // 地图加载后自动执行
        OnInteract,       // 玩家按下交互键
        OnEnter,          // 玩家/目标进入区域
        OnExit,           // 玩家/目标离开区域
        OnTouch,          // 任意 entity 触碰
        OnUpdate,         // 每帧检测
        OnVariableChange, // 全局变量变化
        OnInput,          // 按键输入
        OnCollision       // 物理碰撞
    };

    // 触发条件
    struct Condition {
        enum class Type { None, Variable, Switch, SelfSwitch };
        Type type = Type::None;
        std::string name;        // 变量/开关名
        int compare_op = 0;      // 0:==, 1:!=, 2:>, 3:<, 4:>=, 5:<=
        int int_value = 0;       // 用于 Variable 比较
        bool bool_value = false; // 用于 Switch 比较
    };

    TriggerType trigger = TriggerType::OnInteract;
    Condition condition;
    std::vector<Action> actions;

    // 执行控制
    bool run_once = false;     // 是否只执行一次
    bool has_run = false;      // 是否已经执行过
};

struct EventTrigger : IComponent {
    std::vector<EventPage> pages;
    // 自身开关（A/B/C/D，每个事件独立）
    bool self_switch_a = false;
    bool self_switch_b = false;
    bool self_switch_c = false;
    bool self_switch_d = false;

    bool GetSelfSwitch(char sw) const {
        switch (sw) {
            case 'A': case 'a': return self_switch_a;
            case 'B': case 'b': return self_switch_b;
            case 'C': case 'c': return self_switch_c;
            case 'D': case 'd': return self_switch_d;
            default: return false;
        }
    }

    void SetSelfSwitch(char sw, bool value) {
        switch (sw) {
            case 'A': case 'a': self_switch_a = value; break;
            case 'B': case 'b': self_switch_b = value; break;
            case 'C': case 'c': self_switch_c = value; break;
            case 'D': case 'd': self_switch_d = value; break;
        }
    }
};

// Snapshot 用轻量序列化（与 SceneSerializer 保持字段一致即可）
inline void to_json(nlohmann::json& j, const EventPage::Condition& c) {
    j["type"] = static_cast<int>(c.type);
    j["name"] = c.name;
    j["compare_op"] = c.compare_op;
    j["int_value"] = c.int_value;
    j["bool_value"] = c.bool_value;
}
inline void from_json(const nlohmann::json& j, EventPage::Condition& c) {
    if (j.contains("type")) c.type = static_cast<EventPage::Condition::Type>(j["type"].get<int>());
    if (j.contains("name")) c.name = j["name"].get<std::string>();
    if (j.contains("compare_op")) c.compare_op = j["compare_op"].get<int>();
    if (j.contains("int_value")) c.int_value = j["int_value"].get<int>();
    if (j.contains("bool_value")) c.bool_value = j["bool_value"].get<bool>();
}

inline void to_json(nlohmann::json& j, const Action& a) {
    j["cmd"] = static_cast<int>(a.cmd);
    j["text"] = a.text;
    j["target"] = a.target;
    j["param"] = a.param;
    j["int_value"] = a.int_value;
    j["float_value"] = a.float_value;
    j["bool_value"] = a.bool_value;
}
inline void from_json(const nlohmann::json& j, Action& a) {
    if (j.contains("cmd")) a.cmd = static_cast<ActionCmd>(j["cmd"].get<int>());
    if (j.contains("text")) a.text = j["text"].get<std::string>();
    if (j.contains("target")) a.target = j["target"].get<std::string>();
    if (j.contains("param")) a.param = j["param"].get<std::string>();
    if (j.contains("int_value")) a.int_value = j["int_value"].get<int>();
    if (j.contains("float_value")) a.float_value = j["float_value"].get<float>();
    if (j.contains("bool_value")) a.bool_value = j["bool_value"].get<bool>();
}

inline void to_json(nlohmann::json& j, const EventPage& p) {
    j["trigger"] = static_cast<int>(p.trigger);
    j["condition"] = p.condition;
    j["actions"] = p.actions;
    j["run_once"] = p.run_once;
    j["has_run"] = p.has_run;
}
inline void from_json(const nlohmann::json& j, EventPage& p) {
    if (j.contains("trigger")) p.trigger = static_cast<EventPage::TriggerType>(j["trigger"].get<int>());
    if (j.contains("condition")) p.condition = j["condition"].get<EventPage::Condition>();
    if (j.contains("actions")) p.actions = j["actions"].get<std::vector<Action>>();
    if (j.contains("run_once")) p.run_once = j["run_once"].get<bool>();
    if (j.contains("has_run")) p.has_run = j["has_run"].get<bool>();
}

inline void to_json(nlohmann::json& j, const EventTrigger& et) {
    j["pages"] = et.pages;
    j["self_switch_a"] = et.self_switch_a;
    j["self_switch_b"] = et.self_switch_b;
    j["self_switch_c"] = et.self_switch_c;
    j["self_switch_d"] = et.self_switch_d;
}
inline void from_json(const nlohmann::json& j, EventTrigger& et) {
    if (j.contains("pages")) et.pages = j["pages"].get<std::vector<EventPage>>();
    if (j.contains("self_switch_a")) et.self_switch_a = j["self_switch_a"].get<bool>();
    if (j.contains("self_switch_b")) et.self_switch_b = j["self_switch_b"].get<bool>();
    if (j.contains("self_switch_c")) et.self_switch_c = j["self_switch_c"].get<bool>();
    if (j.contains("self_switch_d")) et.self_switch_d = j["self_switch_d"].get<bool>();
}

} // namespace scene
} // namespace mg

REGISTER_COMPONENT(mg::scene::EventTrigger, "Event Trigger")
