#pragma once
// ---------------------------------------------------------------------------
// 地图设置纯逻辑（不依赖 Vulkan/SDL，可单测）
// 对 World 内所有 Tilemap 组件批量应用尺寸 / tile 尺寸变更。
// ---------------------------------------------------------------------------

namespace mg {
namespace scene {

class World;

// 将所有 Tilemap 的每个 layer 调整为 w×h（保留重叠区域数据，见 TileLayer::Resize）。
// w/h 钳制为 >= 1。返回受影响的 Tilemap 数量。
int ResizeAllTilemaps(World* world, int w, int h);

// 将所有 Tilemap 的 tile_width/tile_height 同步为 tile_size（钳制 >= 1）。
// 返回受影响的 Tilemap 数量。
int SyncAllTilemapTileSize(World* world, int tile_size);

} // namespace scene
} // namespace mg
