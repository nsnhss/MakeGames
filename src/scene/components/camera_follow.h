#pragma once
#include "scene/component.h"
#include "third_party/json.hpp"

namespace mg {
namespace scene {

// 标记 Entity 是相机跟随目标。
// 跟随参数（smooth_speed, dead_zone, follow_target）已迁入 Camera Component。
struct CameraFollowTarget : IComponent {
};

inline void to_json(nlohmann::json&, const CameraFollowTarget&) {}
inline void from_json(const nlohmann::json&, CameraFollowTarget&) {}

} // namespace scene
} // namespace mg

REGISTER_COMPONENT(mg::scene::CameraFollowTarget, "Camera Follow Target")
