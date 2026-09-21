#include "shop_database.h"
#include <algorithm>

namespace mg {

std::optional<ShopDatabase> ShopDatabase::FromJson(const nlohmann::json& j) {
    ShopDatabase db;
    if (!j.is_array()) return db;
    for (const auto& entry : j) {
        if (!entry.is_object()) continue;
        ShopDef def;
        if (entry.contains("id")) def.id = entry["id"].get<std::string>();
        if (entry.contains("items") && entry["items"].is_array()) {
            for (const auto& it : entry["items"]) {
                if (!it.is_object()) continue;
                ShopItem si;
                if (it.contains("item_id")) si.item_id = it["item_id"].get<std::string>();
                if (it.contains("price")) si.price = it["price"].get<int>();
                if (!si.item_id.empty()) def.items.push_back(std::move(si));
            }
        }
        if (!def.id.empty()) db.shops[def.id] = std::move(def);
    }
    return db;
}

const ShopDatabase::ShopDef* ShopDatabase::Find(const std::string& id) const {
    auto it = shops.find(id);
    return (it != shops.end()) ? &it->second : nullptr;
}

nlohmann::json ShopDatabase::ToJson() const {
    nlohmann::json arr = nlohmann::json::array();
    std::vector<std::string> ids;
    ids.reserve(shops.size());
    for (const auto& [id, _] : shops) ids.push_back(id);
    std::sort(ids.begin(), ids.end());
    for (const auto& id : ids) {
        const auto& def = shops.at(id);
        nlohmann::json entry;
        entry["id"] = def.id;
        nlohmann::json items = nlohmann::json::array();
        for (const auto& si : def.items) {
            nlohmann::json j;
            j["item_id"] = si.item_id;
            j["price"] = si.price;
            items.push_back(j);
        }
        entry["items"] = items;
        arr.push_back(entry);
    }
    return arr;
}

} // namespace mg
