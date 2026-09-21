#pragma once
#include "scene/component.h"
#include "core/math/vec2.h"
#include "core/math/color.h"
#include "third_party/json.hpp"
#include <string>

namespace mg {
namespace scene {

struct Sprite : IComponent {
    std::string texture_path;       // 纹理路径（引用 assets/）
    math::Vec2 pivot = {0.5f, 0.5f}; // 锚点（0~1）
    math::Color tint = math::Color::White(); // 着色
    bool flip_x = false;
    bool flip_y = false;
    // Y-sort：同 sorting_order 层内按 position.y 排序（Y 越大越靠前，俯视图遮挡）
    bool y_sort = true;
    // 动画帧索引（由 AnimatorSystem 写入，SpriteRenderSystem 读取）
    int anim_frame = 0;
};

inline void to_json(nlohmann::json& j, const Sprite& s) {
    j["texture_path"] = s.texture_path;
    j["pivot"] = s.pivot;
    j["tint"] = s.tint;
    j["flip_x"] = s.flip_x;
    j["flip_y"] = s.flip_y;
    j["y_sort"] = s.y_sort;
    j["anim_frame"] = s.anim_frame;
}
inline void from_json(const nlohmann::json& j, Sprite& s) {
    if (j.contains("texture_path")) s.texture_path = j["texture_path"].get<std::string>();
    if (j.contains("pivot")) s.pivot = j["pivot"].get<math::Vec2>();
    if (j.contains("tint")) s.tint = j["tint"].get<math::Color>();
    if (j.contains("flip_x")) s.flip_x = j["flip_x"].get<bool>();
    if (j.contains("flip_y")) s.flip_y = j["flip_y"].get<bool>();
    if (j.contains("y_sort")) s.y_sort = j["y_sort"].get<bool>();
    if (j.contains("anim_frame")) s.anim_frame = j["anim_frame"].get<int>();
}

} // namespace scene
} // namespace mg

REGISTER_COMPONENT(mg::scene::Sprite, "Sprite")
