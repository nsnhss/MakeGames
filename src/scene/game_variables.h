#pragma once
// ---------------------------------------------------------------------------
// GameVariables：全局变量/开关/自开关/库存（从 EventSystem 拆出，D1）
// 职责：int/float/bool 变量、全局开关、事件自开关 cache、item_* 前缀库存、
//       场景切换快照与存档序列化。不持有 World —— 自开关的组件同步由
//       EventSystem 负责（本类只存 cache）。
// 通知语义（与原 EventSystem 逐位一致）：
//   仅 SetInt / AddItem / RemoveItem 触发 on_variable_change；
//   SetFloat/SetBool/SetSwitch/SetSelfSwitch/SetIntSilent/DecrementItemRaw 不触发。
// ---------------------------------------------------------------------------
#include "scene/entity.h"
#include "third_party/json.hpp"
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mg {
namespace scene {

class GameVariables {
public:
    using OnVariableChange = std::function<void(const std::string&)>;

    void SetOnVariableChange(OnVariableChange cb) { on_variable_change_ = std::move(cb); }

    // ---- int 变量 ----
    void SetInt(const std::string& name, int value);            // 触发通知
    void SetIntSilent(const std::string& name, int value);      // 不触发（内部状态，如 _choice）
    int GetInt(const std::string& name, int default_val = 0) const;

    // ---- float / bool 变量（不触发通知）----
    void SetFloat(const std::string& name, float value);
    float GetFloat(const std::string& name, float default_val = 0.0f) const;
    void SetBool(const std::string& name, bool value);
    bool GetBool(const std::string& name, bool default_val = false) const;

    // ---- 全局开关（不触发通知）----
    void SetSwitch(const std::string& name, bool value);
    bool GetSwitch(const std::string& name, bool default_val = false) const;

    // ---- 自开关（cache；组件同步由 EventSystem 负责）----
    void SetSelfSwitch(EntityID entity, char sw, bool value);
    // cache 命中返回指针，未命中返回 nullptr（调用方回退读组件）
    const bool* FindSelfSwitch(EntityID entity, char sw) const;

    // ---- 库存（item_* 前缀的 int 变量）----
    void AddItem(const std::string& item_id, int count);        // += count（<=0 按 1）；触发
    void RemoveItem(const std::string& item_id, int count);     // clamp 0、归零擦除；触发
    std::vector<std::pair<std::string, int>> GetInventoryItems() const;  // 数量 > 0
    std::string FindFirstInventoryKey() const;                  // 战斗消耗品用；无则空串
    void DecrementItemRaw(const std::string& key);              // 裸 --（不触发不擦除，与原战斗行为一致）

    // ---- 场景切换快照 ----
    struct Snapshot {
        std::unordered_map<std::string, int>    int_vars;
        std::unordered_map<std::string, float>  float_vars;
        std::unordered_map<std::string, bool>   bool_vars;
        std::unordered_map<std::string, bool>   switches;
        std::unordered_map<std::string, bool>   self_switches;
    };
    Snapshot TakeSnapshot() const;
    void Restore(const Snapshot& snap);

    // ---- 存档序列化（{"globals": {...}}，格式冻结）----
    nlohmann::json SaveToJson() const;
    void LoadFromJson(const nlohmann::json& j);

private:
    void Notify(const std::string& name) {
        if (on_variable_change_) on_variable_change_(name);
    }
    static std::string SelfSwitchKey(EntityID entity, char sw) {
        return std::to_string(entity) + "_" + sw;
    }

    std::unordered_map<std::string, int> int_variables_;
    std::unordered_map<std::string, float> float_variables_;
    std::unordered_map<std::string, bool> bool_variables_;
    std::unordered_map<std::string, bool> switches_;
    std::unordered_map<std::string, bool> self_switches_;
    OnVariableChange on_variable_change_;
};

} // namespace scene
} // namespace mg
