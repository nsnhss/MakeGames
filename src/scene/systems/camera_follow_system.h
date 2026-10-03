#pragma once
// ---------------------------------------------------------------------------
// CameraFollowSystem：相机跟随（D2 从 GameRuntime 拆出，无状态自由函数）
// 显式 follow_target，缺省跟随首个 CameraFollowTarget；
// smooth_follow 时 dead_zone + follow_speed 插值；
// 最后按首个 Tilemap 边界 clamp（viewport 大于地图时有效区间向中心收缩）。
// ---------------------------------------------------------------------------
#include <cstdint>

namespace mg {
namespace scene {

class World;

void UpdateCameraFollowSystem(World* world, float dt, uint32_t viewport_w, uint32_t viewport_h);

} // namespace scene
} // namespace mg
