#pragma once
// ---------------------------------------------------------------------------
// 碰撞图块合并器（纯逻辑，不依赖 Vulkan/SDL，可单测）
// 把 TileLayer 中 Pass_All（四方向全阻挡）的实心格子合并为最少 AABB 矩形集。
// 算法：逐行扫水平实心段（run），相邻行 x 区间完全相同的 run 纵向合并。
// 合并结果的矩形并集与逐 tile AABB 完全等价，碰撞行为零变化。
// ---------------------------------------------------------------------------
#include "scene/components/tilemap.h"
#include <vector>

namespace mg {
namespace scene {

struct CollisionRect { float x0, y0, x1, y1; };

// offset = tilemap 实体 Transform.position，scale = Transform.scale
std::vector<CollisionRect> MergeLayerCollisionRects(
    const TileLayer& layer, float tile_w, float tile_h,
    float offset_x, float offset_y,
    float scale_x = 1.0f, float scale_y = 1.0f);

} // namespace scene
} // namespace mg
