#pragma once
// ---------------------------------------------------------------------------
// Tilemap 网格构建（editor 与 game.exe 共用，D0-4 合并）
// 从 World 中所有 Tilemap 组件生成三角形顶点与按 tileset 分批的绘制批次。
// layer 外循环保证下层先画；tileset 数据源由调用方回调提供
// （editor：TilesetAsset + 真实纹理尺寸；game：tilesets.json + 地图 tile 尺寸）。
// ---------------------------------------------------------------------------
#include <vulkan/vulkan.h>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "scene/systems/tile_animation.h"

namespace mg {
namespace scene {

class World;

struct TilemapVertex {
    float x, y, u, v;
};

struct TilemapBatch {
    VkDescriptorSet ds = VK_NULL_HANDLE;
    uint32_t first_vertex = 0;
    uint32_t vertex_count = 0;
};

// 单个 tileset 的渲染信息（调用方按各自数据源填充）。
// descriptor_set == VK_NULL_HANDLE 时该 tileset 不产生绘制批次。
struct TilesetRenderInfo {
    int columns = 1;              // 图集列数（>=1，内部会 clamp）
    int tile_width = 0;           // 图集中 tile 像素宽（UV 用）
    int tile_height = 0;
    int start_index = 0;          // 全局起始 tile 索引
    int tile_count = 0;           // 该 tileset 的全局 tile 数
    float tex_width = 1.0f;       // UV 归一化分母（纹理实际像素尺寸）
    float tex_height = 1.0f;
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
    const std::vector<TileAnimDef>* animations = nullptr;  // null = 无动画
};

// 构建顶点与批次。输出数组先被 clear。
// get_tileset(ts_name, map_tile_w, map_tile_h)：map_tile_* 为当前 Tilemap 的 tile 尺寸
// （game 数据源无独立 tileset tile 尺寸，UV 由地图 tile 尺寸推导）。
// 返回 nullptr = 跳过该 tileset（未找到 / 纹理无效）。
// respect_layer_visibility = true 时跳过 layer.visible == false 的层（editor 用）。
void BuildTilemapMesh(World* world,
                      const std::function<const TilesetRenderInfo*(const std::string&, int, int)>& get_tileset,
                      bool respect_layer_visibility,
                      float anim_time,
                      std::vector<TilemapVertex>& out_vertices,
                      std::vector<TilemapBatch>& out_batches);

} // namespace scene
} // namespace mg
