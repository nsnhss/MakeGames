// event_system_shop.cpp — 商店命令（从 event_system.cpp 拆分，保持类成员语义不变）
#include "scene/event_system.h"

namespace mg {
namespace scene {

void EventSystem::OpenShop(const std::string& shop_id) {
    shop_pending_ = true;
    pending_shop_id_ = shop_id;
}

void EventSystem::CloseShop() {
    shop_pending_ = false;
    pending_shop_id_.clear();
}

void EventSystem::AcknowledgeShop() {
    // 保留钩子：当前商店通过 CloseShop 结束，无需额外确认标记
}

bool EventSystem::BuyItem(const std::string& item_id, int price) {
    int gold = GetVariableInt("gold");
    if (gold < price) return false;

    SetVariable("gold", gold - price);

    std::string key = "item_" + item_id;
    int_variables_[key] = int_variables_.count(key) ? int_variables_[key] + 1 : 1;
    TriggerOnVariableChange(key);
    return true;
}

} // namespace scene
} // namespace mg
