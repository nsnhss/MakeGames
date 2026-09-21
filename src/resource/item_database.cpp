#include "item_database.h"

namespace mg {

std::optional<ItemDatabase> ItemDatabase::FromJson(const nlohmann::json& j) {
    ItemDatabase db;
    if (!j.is_array()) return db;
    for (const auto& entry : j) {
        if (!entry.is_object()) continue;
        ItemDef def;
        if (entry.contains("id")) def.id = entry["id"].get<std::string>();
        if (entry.contains("name")) def.name = entry["name"].get<std::string>();
        if (entry.contains("description")) def.description = entry["description"].get<std::string>();
        if (entry.contains("price")) def.price = entry["price"].get<int>();
        if (!def.id.empty()) {
            db.items[def.id] = std::move(def);
        }
    }
    return db;
}

const ItemDatabase::ItemDef* ItemDatabase::Find(const std::string& id) const {
    auto it = items.find(id);
    return (it != items.end()) ? &it->second : nullptr;
}

nlohmann::json ItemDatabase::ToJson() const {
    nlohmann::json arr = nlohmann::json::array();
    std::vector<std::string> ids;
    ids.reserve(items.size());
    for (const auto& [id, _] : items) ids.push_back(id);
    std::sort(ids.begin(), ids.end());
    for (const auto& id : ids) {
        const auto& def = items.at(id);
        nlohmann::json entry;
        entry["id"] = def.id;
        entry["name"] = def.name;
        entry["description"] = def.description;
        entry["price"] = def.price;
        arr.push_back(entry);
    }
    return arr;
}

} // namespace mg
