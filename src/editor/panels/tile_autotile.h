#pragma once
// ---------------------------------------------------------------------------
// 4-bit autotile（自动图块）邻接映射：从 viewport_panel.cpp 抽出的独立模块。
// 16 种邻接模式 × 4 变体 → 48-tile auto-tile block 内的 tile 偏移。
// 48 tiles per auto-tile type = 12 tiles per variant（MVP：模式 >11 复用前面的 tile）。
// ---------------------------------------------------------------------------
#include "scene/components/tilemap.h"

namespace mg {
namespace editor {

constexpr int kAutoTileBlockSize = 48;

// 计算 (tx,ty) 处 auto-tile 的全局 tile index：按四邻是否同 block 查 16 模式表
int ComputeAutoTileIndex(const scene::TileLayer& layer, int tx, int ty,
                         int base_type, int variant, int tileset_start);

// 刷新中心及四邻（仅当属于 auto-tile block）
void RefreshAutoTiles(scene::TileLayer& layer, int cx, int cy, int tileset_start);

} // namespace editor
} // namespace mg
