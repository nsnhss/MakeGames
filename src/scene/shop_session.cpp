// shop_session.cpp — 商店子对象（原 event_system_shop.cpp，D1 拆为独立子对象）
#include "scene/shop_session.h"
#include "scene/game_variables.h"

namespace mg {
namespace scene {

void ShopSession::Open(const std::string& shop_id) {
    pending_ = true;
    pending_id_ = shop_id;
}

void ShopSession::Close() {
    pending_ = false;
    pending_id_.clear();
}

bool ShopSession::Buy(const std::string& item_id, int price, GameVariables& vars) {
    int gold = vars.GetInt("gold");
    if (gold < price) return false;

    vars.SetInt("gold", gold - price);

    // 物品 +1（SetInt 内部触发 on_variable_change，与原 BuyItem 手动触发一次等价）
    std::string key = "item_" + item_id;
    vars.SetInt(key, vars.GetInt(key, 0) + 1);
    return true;
}

} // namespace scene
} // namespace mg
