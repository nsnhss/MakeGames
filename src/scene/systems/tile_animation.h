#pragma once
// ---------------------------------------------------------------------------
// 瓦片动画（纯逻辑，不依赖 Vulkan/SDL，可单测）
// 数据存于 tilesets.json 每 tileset 的 "animations" 字段：
//   { "tile_index": 局部 tile, "frames": [局部 tile...], "fps": 帧率 }
// 运行时按累计时间把动画 tile 替换为当前帧 tile（MVP：CPU 替换 + 重建 VB）。
// ---------------------------------------------------------------------------
#include <cstdint>
#include <vector>

namespace mg {
namespace scene {

struct TileAnimDef {
    int tile_index = 0;          // 哪个 tile 有动画（tileset 局部索引）
    std::vector<int> frames;     // 动画帧序列（tileset 局部索引）
    float fps = 4.0f;
};

// 若 global_tile 命中动画（global_tile - start_index == anim.tile_index），
// 返回当前帧对应的全局 tile index；否则原样返回。
// fps<=0 或 frames 为空 → 原样返回；time_sec 为负按 0 处理。
int ResolveAnimatedTile(const std::vector<TileAnimDef>& anims,
                        int global_tile, int start_index, float time_sec);

// 所有动画当前帧序号的组合哈希：帧变化 → 值变化（用于决定是否重建 VB）。
// 无有效动画时返回 0。
uint32_t AnimFrameHash(const std::vector<TileAnimDef>& anims, float time_sec);

} // namespace scene
} // namespace mg
