#pragma once
// ---------------------------------------------------------------------------
// ShopSession：商店子对象（从 EventSystem 拆出，D1）
// 职责：商店打开/关闭挂起状态、购买（扣 gold + 加 item_* 库存）。
// 货币与库存读写经 GameVariables& 注入。
// ---------------------------------------------------------------------------
#include <string>

namespace mg {

class ShopDatabase;

namespace scene {

class GameVariables;

class ShopSession {
public:
    void SetDatabase(const ShopDatabase* db) { db_ = db; }
    const ShopDatabase* Database() const { return db_; }

    bool IsPending() const { return pending_; }
    const std::string& PendingShopId() const { return pending_id_; }

    void Open(const std::string& shop_id);
    void Close();
    bool Buy(const std::string& item_id, int price, GameVariables& vars);

private:
    const ShopDatabase* db_ = nullptr;
    bool pending_ = false;
    std::string pending_id_;
};

} // namespace scene
} // namespace mg
