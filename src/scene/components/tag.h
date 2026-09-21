#pragma once
#include "scene/component.h"
#include "third_party/json.hpp"
#include <vector>
#include <string>

namespace mg {
namespace scene {

struct Tag : IComponent {
    std::vector<std::string> tags;

    Tag() = default;
    explicit Tag(std::initializer_list<std::string> list) : tags(list) {}

    bool Has(const std::string& tag) const {
        for (const auto& t : tags) {
            if (t == tag) return true;
        }
        return false;
    }

    void Add(const std::string& tag) {
        if (!Has(tag)) tags.push_back(tag);
    }

    void Remove(const std::string& tag) {
        for (auto it = tags.begin(); it != tags.end(); ++it) {
            if (*it == tag) {
                tags.erase(it);
                return;
            }
        }
    }
};

inline void to_json(nlohmann::json& j, const Tag& t) { j["tags"] = t.tags; }
inline void from_json(const nlohmann::json& j, Tag& t) {
    if (j.contains("tags")) t.tags = j["tags"].get<std::vector<std::string>>();
}

} // namespace scene
} // namespace mg

REGISTER_COMPONENT(mg::scene::Tag, "Tag")
