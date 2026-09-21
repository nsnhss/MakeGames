#pragma once
#include "scene/component.h"
#include "core/math/vec2.h"
#include "third_party/json.hpp"

namespace mg {
namespace scene {

struct Transform : IComponent {
    math::Vec2 position = {0.0f, 0.0f};
    float rotation = 0.0f;  // 角度（度）
    math::Vec2 scale = {1.0f, 1.0f};
    int sorting_order = 0;  // 渲染排序层级

    Transform() = default;
    Transform(const math::Vec2& pos, float rot = 0.0f, const math::Vec2& scl = {1,1})
        : position(pos), rotation(rot), scale(scl) {}
};

inline void to_json(nlohmann::json& j, const Transform& t) {
    j["position"] = t.position;
    j["rotation"] = t.rotation;
    j["scale"] = t.scale;
    j["sorting_order"] = t.sorting_order;
}
inline void from_json(const nlohmann::json& j, Transform& t) {
    if (j.contains("position")) t.position = j["position"].get<math::Vec2>();
    if (j.contains("rotation")) t.rotation = j["rotation"].get<float>();
    if (j.contains("scale")) t.scale = j["scale"].get<math::Vec2>();
    if (j.contains("sorting_order")) t.sorting_order = j["sorting_order"].get<int>();
}

} // namespace scene
} // namespace mg

REGISTER_COMPONENT(mg::scene::Transform, "Transform")
