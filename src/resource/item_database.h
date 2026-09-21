#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include "third_party/json.hpp"

namespace mg {

struct ItemDatabase {
    struct ItemDef {
        std::string id;
        std::string name;
        std::string description;
        int price = 0;
    };

    std::unordered_map<std::string, ItemDef> items; // id → def

    static std::optional<ItemDatabase> FromJson(const nlohmann::json& j);
    nlohmann::json ToJson() const;
    const ItemDef* Find(const std::string& id) const;
    bool IsEmpty() const { return items.empty(); }
};

} // namespace mg
