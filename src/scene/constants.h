#pragma once
// ---------------------------------------------------------------------------
// 场景级共享常量（D0-5/D0-7 统一收敛点）
// ---------------------------------------------------------------------------

namespace mg {
namespace scene {

// 默认精灵 quad 半宽/半高（32px 单元格的一半），editor 与 game 渲染共用
inline constexpr float kSpriteQuadHalfExtent = 16.0f;

// 玩家 AABB 碰撞半宽（略小于半格 16，避免卡缝），game_runtime 与 event_system 共用
inline constexpr float kPlayerCollisionHalfExtent = 14.0f;

// 事件触发点半径（32px tile 的半格），event_system 点判定用
inline constexpr float kEventTouchHalfExtent = 16.0f;

// 编辑器视口点选实体半径
inline constexpr float kPickRadius = 16.0f;

} // namespace scene
} // namespace mg
