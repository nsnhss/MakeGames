#pragma once
// ---------------------------------------------------------------------------
// Sprite 批处理构建器（纯逻辑，不依赖 Vulkan）
// 从 World 收集可见 Sprite，按 (sorting_order, texture_path, entity_id) 稳定
// 排序，填充 instance 数组并按纹理分组，供 SpriteRenderSystem 单次上传 +
// firstInstance 分组绘制。独立成文件以便单元测试。
// ---------------------------------------------------------------------------
#include "scene/entity.h"
#include <cstdint>
#include <string>
#include <vector>

namespace mg {
namespace scene {

class World;

// 与渲染管线 binding 1 的 instance 顶点布局一致（20 floats = 80B）
struct SpriteInstance {
    float world_x, world_y;
    float uv_off_x, uv_off_y;
    float frame_sx, frame_sy;
    float scale_x, scale_y;
    float color_r, color_g, color_b, color_a;
    float atlas_u0, atlas_v0, atlas_du, atlas_dv;  // 图集 rect；非图集 = (0,0,1,1)
    float pivot_x, pivot_y;                        // 锚点（0~1，默认 0.5,0.5 居中）
    float flip_x, flip_y;                          // 0.0 / 1.0
};

// 一个纹理分组：instances[first_instance, first_instance + count)
struct SpriteBatch {
    std::string texture_path;
    uint32_t    first_instance = 0;
    uint32_t    count = 0;
};

// 收集 world 中所有 Transform+Sprite 实体，排序分组后填充 out_instances / out_batches。
// 排序键：(sorting_order, y_key, !is_atlas, texture_path, entity_id)，稳定排序。
// y_key：Sprite::y_sort 开启时取 position.y（同层 Y 大者靠前）；关闭者沉到层底。
// get_atlas_rect 可空：提供时命中图集的 sprite 归入单一 "@atlas" 组（首组，单 draw）；
// 未命中/未提供走 per-texture 分组 fallback。
using GetAtlasRectFn = bool(*)(void* ctx, const std::string& path, float out_uv_rect[4]);
void BuildSpriteInstances(World* world,
                          std::vector<SpriteInstance>& out_instances,
                          std::vector<SpriteBatch>& out_batches,
                          GetAtlasRectFn get_atlas_rect = nullptr,
                          void* atlas_ctx = nullptr);

} // namespace scene
} // namespace mg
