#pragma once
#include "scene/component.h"
#include "third_party/json.hpp"
#include <string>

namespace mg {
namespace scene {

struct Name : IComponent {
    std::string value = "Entity";

    Name() = default;
    explicit Name(const std::string& name) : value(name) {}
};

inline void to_json(nlohmann::json& j, const Name& n) { j["value"] = n.value; }
inline void from_json(const nlohmann::json& j, Name& n) {
    if (j.contains("value")) n.value = j["value"].get<std::string>();
}

} // namespace scene
} // namespace mg

REGISTER_COMPONENT(mg::scene::Name, "Name")
