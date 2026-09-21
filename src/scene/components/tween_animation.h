#pragma once
#include "scene/component.h"
#include "core/math/vec2.h"
#include "core/math/color.h"
#include "third_party/json.hpp"
#include <vector>

namespace mg {
namespace scene {

// 关键帧（t 单位：秒）
struct Vec2Key  { float t; float x, y; };
struct FloatKey { float t; float v; };
struct ColorKey { float t; float r, g, b, a; };

// 缓动曲线
enum class Easing {
    Linear = 0,
    InQuad, OutQuad, InOutQuad,
    InCubic, OutCubic, InOutCubic,
};

// 关键帧补间动画（组件内嵌 clip，MVP：4 条轨道 + 组件级缓动）
// position 轨道存「相对 base_position 的偏移」，base 在首次 Update 时捕获（不序列化）。
struct TweenAnimation : IComponent {
    std::vector<Vec2Key>  position;   // 位置偏移轨道
    std::vector<FloatKey> rotation;   // 旋转（度）
    std::vector<Vec2Key>  scale;      // 缩放（绝对值）
    std::vector<ColorKey> tint;       // 写入 Sprite.tint
    float duration = 1.0f;
    bool  loop = true;
    bool  playing = true;
    float time = 0.0f;                // 运行时
    Easing easing = Easing::Linear;

    // 运行时（不序列化）：首次 Update 捕获
    math::Vec2 base_position = {};
    math::Vec2 base_scale = {1.0f, 1.0f};
    bool base_captured = false;
};

inline void to_json(nlohmann::json& j, const Vec2Key& k)  { j = {k.t, k.x, k.y}; }
inline void from_json(const nlohmann::json& j, Vec2Key& k) { k = {j[0], j[1], j[2]}; }
inline void to_json(nlohmann::json& j, const FloatKey& k)  { j = {k.t, k.v}; }
inline void from_json(const nlohmann::json& j, FloatKey& k) { k = {j[0], j[1]}; }
inline void to_json(nlohmann::json& j, const ColorKey& k)  { j = {k.t, k.r, k.g, k.b, k.a}; }
inline void from_json(const nlohmann::json& j, ColorKey& k) { k = {j[0], j[1], j[2], j[3], j[4]}; }

inline void to_json(nlohmann::json& j, const TweenAnimation& a) {
    j["position"] = a.position;
    j["rotation"] = a.rotation;
    j["scale"] = a.scale;
    j["tint"] = a.tint;
    j["duration"] = a.duration;
    j["loop"] = a.loop;
    j["playing"] = a.playing;
    j["easing"] = (int)a.easing;
}
inline void from_json(const nlohmann::json& j, TweenAnimation& a) {
    if (j.contains("position")) a.position = j["position"].get<std::vector<Vec2Key>>();
    if (j.contains("rotation")) a.rotation = j["rotation"].get<std::vector<FloatKey>>();
    if (j.contains("scale")) a.scale = j["scale"].get<std::vector<Vec2Key>>();
    if (j.contains("tint")) a.tint = j["tint"].get<std::vector<ColorKey>>();
    if (j.contains("duration")) a.duration = j["duration"].get<float>();
    if (j.contains("loop")) a.loop = j["loop"].get<bool>();
    if (j.contains("playing")) a.playing = j["playing"].get<bool>();
    if (j.contains("easing")) a.easing = (Easing)j["easing"].get<int>();
}

} // namespace scene
} // namespace mg

REGISTER_COMPONENT(mg::scene::TweenAnimation, "TweenAnimation")
