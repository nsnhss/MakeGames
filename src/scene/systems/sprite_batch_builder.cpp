#include "sprite_batch_builder.h"
#include "scene/world.h"
#include "scene/components/transform.h"
#include "scene/components/sprite.h"
#include "scene/components/animator.h"
#include <algorithm>

namespace mg {
namespace scene {

void BuildSpriteInstances(World* world,
                          std::vector<SpriteInstance>& out_instances,
                          std::vector<SpriteBatch>& out_batches,
                          GetAtlasRectFn get_atlas_rect,
                          void* atlas_ctx)
{
    out_instances.clear();
    out_batches.clear();
    if (!world) return;

    struct Item {
        EntityID    entity;
        int         sorting_order;
        float       y_key;          // y_sort ? position.y : -FLT_MAX（关闭者沉到层底）
        bool        is_atlas;
        std::string texture_path;   // 图集命中时为 "@atlas" 哨兵（分组键）
        SpriteInstance instance;
    };

    std::vector<EntityID> entities;
    world->GetAllEntities(entities);

    std::vector<Item> items;
    items.reserve(entities.size());

    for (auto e : entities) {
        auto* transform = world->GetComponent<Transform>(e);
        if (!transform) continue;
        auto* sprite = world->GetComponent<Sprite>(e);
        if (!sprite) continue;

        float u_off = 0.0f, v_off = 0.0f;
        float frame_sx = 1.0f, frame_sy = 1.0f;
        auto* animator = world->GetComponent<Animator>(e);
        if (animator && animator->columns > 0 && animator->frame_count > 0) {
            int col = animator->current_frame % animator->columns;
            int row = animator->current_frame / animator->columns;
            frame_sx = 1.0f / animator->columns;
            frame_sy = 1.0f / ((animator->frame_count + animator->columns - 1) / animator->columns);
            u_off = col * frame_sx;
            v_off = row * frame_sy;
        }

        // 图集查询：命中则 instance 携带 atlas rect，分组键用哨兵 "@atlas"
        float atlas_rect[4] = {0.0f, 0.0f, 1.0f, 1.0f};
        bool is_atlas = get_atlas_rect
            && get_atlas_rect(atlas_ctx, sprite->texture_path, atlas_rect);

        Item item;
        item.entity = e;
        item.sorting_order = transform->sorting_order;
        item.y_key = sprite->y_sort ? transform->position.y : -3.4e38f;
        item.is_atlas = is_atlas;
        item.texture_path = is_atlas ? "@atlas" : sprite->texture_path;
        item.instance = {
            transform->position.x, transform->position.y,
            u_off, v_off,
            frame_sx, frame_sy,
            transform->scale.x, transform->scale.y,
            sprite->tint.r, sprite->tint.g, sprite->tint.b, sprite->tint.a,
            atlas_rect[0], atlas_rect[1], atlas_rect[2], atlas_rect[3],
            sprite->pivot.x, sprite->pivot.y,
            sprite->flip_x ? 1.0f : 0.0f, sprite->flip_y ? 1.0f : 0.0f
        };
        items.push_back(std::move(item));
    }

    // 排序键：(sorting_order, y_key, !is_atlas, texture_path, entity_id)，稳定排序。
    // sorting_order 主键分层；同层内 y_sort 实体按 Y 排序（Y 大者后画=靠前，俯视图遮挡）；
    // 图集组优先连续（合并单 draw）；texture 次键保证非图集同纹理连续分组；entity_id 兜底。
    std::stable_sort(items.begin(), items.end(),
        [](const Item& a, const Item& b) {
            if (a.sorting_order != b.sorting_order) return a.sorting_order < b.sorting_order;
            if (a.y_key != b.y_key) return a.y_key < b.y_key;
            if (a.is_atlas != b.is_atlas) return a.is_atlas > b.is_atlas;
            if (a.texture_path != b.texture_path) return a.texture_path < b.texture_path;
            return a.entity < b.entity;
        });

    out_instances.reserve(items.size());
    for (size_t i = 0; i < items.size(); ) {
        SpriteBatch batch;
        batch.texture_path = items[i].texture_path;
        batch.first_instance = (uint32_t)out_instances.size();
        size_t j = i;
        for (; j < items.size() && items[j].texture_path == batch.texture_path; ++j) {
            out_instances.push_back(items[j].instance);
        }
        batch.count = (uint32_t)(j - i);
        out_batches.push_back(std::move(batch));
        i = j;
    }
}

} // namespace scene
} // namespace mg
