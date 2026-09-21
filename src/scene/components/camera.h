#pragma once
#include "scene/component.h"
#include "scene/entity.h"
#include "core/math/vec2.h"
#include "core/math/color.h"
#include "third_party/json.hpp"

namespace mg {
namespace scene {

struct Camera : IComponent {
    enum class Projection { Orthographic, Perspective };
    Projection projection = Projection::Orthographic;

    // 正交参数
    float ortho_size = 5.0f;       // 半高（世界单位）
    float near_plane = -100.0f;
    float far_plane = 100.0f;

    // 背景色
    math::Color clear_color = math::Color(0.07f, 0.07f, 0.08f);

    // 视口归一化区域（预留分屏，当前全屏 = [0,0,1,1]）
    float viewport_x = 0.0f;
    float viewport_y = 0.0f;
    float viewport_w = 1.0f;
    float viewport_h = 1.0f;

    bool is_primary = false;       // 标记为主相机

    // 跟随参数（从原 CameraTarget 迁入）
    EntityID follow_target = kInvalidEntity;
    bool smooth_follow = true;
    float follow_speed = 5.0f;
    math::Vec2 dead_zone = {32.0f, 32.0f};
};

inline void to_json(nlohmann::json& j, const Camera& c) {
    j["projection"] = static_cast<int>(c.projection);
    j["ortho_size"] = c.ortho_size;
    j["near_plane"] = c.near_plane;
    j["far_plane"] = c.far_plane;
    j["clear_color"] = c.clear_color;
    j["viewport_x"] = c.viewport_x;
    j["viewport_y"] = c.viewport_y;
    j["viewport_w"] = c.viewport_w;
    j["viewport_h"] = c.viewport_h;
    j["is_primary"] = c.is_primary;
    j["follow_target"] = c.follow_target;
    j["smooth_follow"] = c.smooth_follow;
    j["follow_speed"] = c.follow_speed;
    j["dead_zone"] = c.dead_zone;
}
inline void from_json(const nlohmann::json& j, Camera& c) {
    if (j.contains("projection")) c.projection = static_cast<Camera::Projection>(j["projection"].get<int>());
    if (j.contains("ortho_size")) c.ortho_size = j["ortho_size"].get<float>();
    if (j.contains("near_plane")) c.near_plane = j["near_plane"].get<float>();
    if (j.contains("far_plane")) c.far_plane = j["far_plane"].get<float>();
    if (j.contains("clear_color")) c.clear_color = j["clear_color"].get<math::Color>();
    if (j.contains("viewport_x")) c.viewport_x = j["viewport_x"].get<float>();
    if (j.contains("viewport_y")) c.viewport_y = j["viewport_y"].get<float>();
    if (j.contains("viewport_w")) c.viewport_w = j["viewport_w"].get<float>();
    if (j.contains("viewport_h")) c.viewport_h = j["viewport_h"].get<float>();
    if (j.contains("is_primary")) c.is_primary = j["is_primary"].get<bool>();
    if (j.contains("follow_target")) c.follow_target = j["follow_target"].get<EntityID>();
    if (j.contains("smooth_follow")) c.smooth_follow = j["smooth_follow"].get<bool>();
    if (j.contains("follow_speed")) c.follow_speed = j["follow_speed"].get<float>();
    if (j.contains("dead_zone")) c.dead_zone = j["dead_zone"].get<math::Vec2>();
}

} // namespace scene
} // namespace mg

REGISTER_COMPONENT(mg::scene::Camera, "Camera")
