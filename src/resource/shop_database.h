#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include "third_party/json.hpp"

namespace mg {

struct ShopDatabase {
    struct ShopItem {
        std::string item_id;
        int price = 0;
    };
    struct ShopDef {
        std::string id;
        std::vector<ShopItem> items;
    };

    std::unordered_map<std::string, ShopDef> shops;

    static std::optional<ShopDatabase> FromJson(const nlohmann::json& j);
    nlohmann::json ToJson() const;
    const ShopDef* Find(const std::string& id) const;
    bool IsEmpty() const { return shops.empty(); }
};

} // namespace mg
