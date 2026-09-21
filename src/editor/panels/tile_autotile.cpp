#include "tile_autotile.h"

namespace mg {
namespace editor {

namespace {

// 4-bit auto-tile lookup: 16 adjacency patterns × 4 variants → tile offset within a 48-tile auto-tile block.
// 48 tiles per auto-tile type = 12 tiles per variant. Patterns beyond 11 reuse earlier tiles (MVP simplification).
const int kAutoTileTable[16][4] = {
    { 0, 12, 24, 36}, // 0000 isolated
    { 1, 13, 25, 37}, // 0001 top
    { 2, 14, 26, 38}, // 0010 bottom
    { 3, 15, 27, 39}, // 0011 top+bottom
    { 4, 16, 28, 40}, // 0100 left
    { 5, 17, 29, 41}, // 0101 top+left
    { 6, 18, 30, 42}, // 0110 bottom+left
    { 7, 19, 31, 43}, // 0111 top+bottom+left
    { 8, 20, 32, 44}, // 1000 right
    { 9, 21, 33, 45}, // 1001 top+right
    {10, 22, 34, 46}, // 1010 bottom+right
    {11, 23, 35, 47}, // 1011 top+bottom+right
    { 0, 12, 24, 36}, // 1100 left+right (reuse isolated)
    { 1, 13, 25, 37}, // 1101 top+left+right
    { 2, 14, 26, 38}, // 1110 bottom+left+right
    { 3, 15, 27, 39}, // 1111 all four
};

} // namespace

int ComputeAutoTileIndex(const scene::TileLayer& layer, int tx, int ty,
                         int base_type, int variant, int tileset_start) {
    int adj = 0;
    auto same_block = [&](int global_t) -> bool {
        if (global_t < 0) return false;
        int local = global_t - tileset_start;
        return local >= 0 && local / kAutoTileBlockSize == base_type;
    };
    if (ty > 0)      if (same_block(layer.GetTile(tx, ty - 1))) adj |= 1; // top
    if (ty < layer.height - 1) if (same_block(layer.GetTile(tx, ty + 1))) adj |= 2; // bottom
    if (tx > 0)      if (same_block(layer.GetTile(tx - 1, ty))) adj |= 4; // left
    if (tx < layer.width - 1) if (same_block(layer.GetTile(tx + 1, ty))) adj |= 8; // right
    return tileset_start + base_type * kAutoTileBlockSize + kAutoTileTable[adj][variant];
}

void RefreshAutoTiles(scene::TileLayer& layer, int cx, int cy, int tileset_start) {
    // Refresh center and its 4 neighbors if they belong to an auto-tile block.
    const int dx[5] = {0, 0, 0, -1, 1};
    const int dy[5] = {0, -1, 1, 0, 0};
    for (int i = 0; i < 5; ++i) {
        int tx = cx + dx[i];
        int ty = cy + dy[i];
        if (tx < 0 || tx >= layer.width || ty < 0 || ty >= layer.height) continue;
        int cur = layer.GetTile(tx, ty);
        if (cur < 0) continue;
        int local = cur - tileset_start;
        if (local < 0) continue;
        int base_type = local / kAutoTileBlockSize;
        int variant = local % 4;
        int next = ComputeAutoTileIndex(layer, tx, ty, base_type, variant, tileset_start);
        if (next != cur) layer.SetTile(tx, ty, next);
    }
}

} // namespace editor
} // namespace mg
