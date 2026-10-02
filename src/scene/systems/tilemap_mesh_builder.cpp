#include "scene/systems/tilemap_mesh_builder.h"
#include "scene/world.h"
#include "scene/components/tilemap.h"
#include "scene/components/transform.h"

namespace mg {
namespace scene {

void BuildTilemapMesh(World* world,
                      const std::function<const TilesetRenderInfo*(const std::string&, int, int)>& get_tileset,
                      bool respect_layer_visibility,
                      float anim_time,
                      std::vector<TilemapVertex>& out_vertices,
                      std::vector<TilemapBatch>& out_batches) {
    out_vertices.clear();
    out_batches.clear();
    if (!world) return;

    std::vector<EntityID> entities;
    world->GetAllEntities(entities);

    for (auto e : entities) {
        auto* tm = world->GetComponent<Tilemap>(e);
        if (!tm || tm->layers.empty()) continue;
        auto names = tm->GetTilesetNames();
        if (names.empty()) continue;

        auto* t = world->GetComponent<Transform>(e);
        float ox = t ? t->position.x : 0.0f;
        float oy = t ? t->position.y : 0.0f;
        float sx = t ? t->scale.x : 1.0f;
        float sy = t ? t->scale.y : 1.0f;
        float tw = (float)tm->tile_width;
        float th = (float)tm->tile_height;

        // 多层绘制：layer 外循环保证下层先画
        for (const auto& layer : tm->layers) {
            if (respect_layer_visibility && !layer.visible) continue;
            for (const std::string& ts_name : names) {
                const TilesetRenderInfo* info = get_tileset(ts_name, tm->tile_width, tm->tile_height);
                if (!info) continue;

                int cols = info->columns > 0 ? info->columns : 1;
                int ts_end = info->start_index + info->tile_count;

                uint32_t batch_start = (uint32_t)out_vertices.size();
                for (int y = 0; y < layer.height; ++y) {
                    for (int x = 0; x < layer.width; ++x) {
                        int global_tile = layer.GetTile(x, y);
                        if (global_tile < info->start_index || global_tile >= ts_end) continue;
                        // 瓦片动画：命中动画的 tile 替换为当前帧
                        if (info->animations)
                            global_tile = ResolveAnimatedTile(*info->animations, global_tile,
                                                              info->start_index, anim_time);
                        int local = global_tile - info->start_index;
                        int col = local % cols;
                        int row = local / cols;
                        float u0 = (float)(col * info->tile_width) / info->tex_width;
                        float u1 = (float)((col + 1) * info->tile_width) / info->tex_width;
                        float v0 = (float)(row * info->tile_height) / info->tex_height;
                        float v1 = (float)((row + 1) * info->tile_height) / info->tex_height;
                        float x0 = ox + x * tw * sx;
                        float x1 = ox + (x + 1) * tw * sx;
                        float y0 = oy + y * th * sy;
                        float y1 = oy + (y + 1) * th * sy;
                        out_vertices.push_back({x0, y0, u0, v0});
                        out_vertices.push_back({x1, y0, u1, v0});
                        out_vertices.push_back({x1, y1, u1, v1});
                        out_vertices.push_back({x0, y0, u0, v0});
                        out_vertices.push_back({x1, y1, u1, v1});
                        out_vertices.push_back({x0, y1, u0, v1});
                    }
                }
                uint32_t batch_count = (uint32_t)out_vertices.size() - batch_start;
                if (batch_count > 0 && info->descriptor_set != VK_NULL_HANDLE) {
                    out_batches.push_back({info->descriptor_set, batch_start, batch_count});
                }
            }
        }
    }
}

} // namespace scene
} // namespace mg
