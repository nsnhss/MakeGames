#include "scene/systems/player_movement_system.h"
#include "scene/world.h"
#include "scene/constants.h"
#include "scene/components/transform.h"
#include "scene/components/movement.h"
#include "scene/components/tilemap.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>

namespace mg {
namespace scene {

bool PlayerMovementSystem::CacheMatches(World* world) const {
    std::vector<EntityID> all;
    world->GetAllEntities(all);

    size_t tm_index = 0;
    for (auto e : all) {
        auto* tm = world->GetComponent<Tilemap>(e);
        if (!tm) continue;
        if (tm_index >= cache_entries_.size()) return false;
        const auto& entry = cache_entries_[tm_index++];
        if (entry.entity != e) return false;

        auto* t = world->GetComponent<Transform>(e);
        float ox = t ? t->position.x : 0.0f;
        float oy = t ? t->position.y : 0.0f;
        float sx = t ? t->scale.x : 1.0f;
        float sy = t ? t->scale.y : 1.0f;

        std::vector<uint32_t> versions;
        versions.reserve(tm->layers.size());
        for (const auto& layer : tm->layers) versions.push_back(layer.collision_version);

        if (!entry.fingerprint.Equals(tm, tm->tile_width, tm->tile_height,
                                      ox, oy, sx, sy, versions))
            return false;
    }
    return tm_index == cache_entries_.size();
}

void PlayerMovementSystem::RebuildCollisionCache(World* world) {
    collision_rects_.clear();
    cache_entries_.clear();
    has_map_bounds_ = false;

    std::vector<EntityID> all;
    world->GetAllEntities(all);
    for (auto e : all) {
        auto* tm = world->GetComponent<Tilemap>(e);
        if (!tm) continue;
        auto* t = world->GetComponent<Transform>(e);
        float ox = t ? t->position.x : 0.0f;
        float oy = t ? t->position.y : 0.0f;
        float sx = t ? t->scale.x : 1.0f;
        float sy = t ? t->scale.y : 1.0f;

        // 实心格子（Pass_All 四方向全阻挡）合并为更大矩形，减少碰撞测试次数
        for (auto& layer : tm->layers) {
            auto merged = MergeLayerCollisionRects(
                layer, (float)tm->tile_width, (float)tm->tile_height, ox, oy, sx, sy);
            collision_rects_.insert(collision_rects_.end(), merged.begin(), merged.end());
        }

        CacheEntry entry;
        entry.entity = e;
        entry.fingerprint.tilemap_ptr = tm;
        entry.fingerprint.tile_w = tm->tile_width;
        entry.fingerprint.tile_h = tm->tile_height;
        entry.fingerprint.ox = ox;
        entry.fingerprint.oy = oy;
        entry.fingerprint.sx = sx;
        entry.fingerprint.sy = sy;
        for (const auto& layer : tm->layers)
            entry.fingerprint.layer_versions.push_back(layer.collision_version);
        cache_entries_.push_back(std::move(entry));

        // 首个 Tilemap 层推导地图边界（玩家 clamp 用）
        if (!has_map_bounds_ && !tm->layers.empty()) {
            map_min_x_ = ox;
            map_min_y_ = oy;
            map_max_x_ = ox + tm->layers[0].width * tm->tile_width * sx;
            map_max_y_ = oy + tm->layers[0].height * tm->tile_height * sy;
            has_map_bounds_ = true;
        }
    }
}

void PlayerMovementSystem::Update(float dt, World* world, const uint8_t* keys) {
    if (!world || !keys) return;

    if (!cache_valid_ || !CacheMatches(world)) {
        RebuildCollisionCache(world);
        cache_valid_ = true;
    }

    std::vector<EntityID> move_entities;
    world->Query<Transform, Movement>(move_entities);

    constexpr float HALF = kPlayerCollisionHalfExtent; // 略小于半格 16，避免卡缝

    for (auto entity : move_entities) {
        auto* t = world->GetComponent<Transform>(entity);
        auto* m = world->GetComponent<Movement>(entity);
        if (!t || !m) continue;

        math::Vec2 input(0, 0);
        if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP])    input.y += 1;
        if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN])  input.y -= 1;
        if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT])  input.x -= 1;
        if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) input.x += 1;

        if (input.x != 0 || input.y != 0) {
            float len = std::sqrt(input.x * input.x + input.y * input.y);
            input.x /= len; input.y /= len;
        }

        float dx = input.x * m->speed * dt;
        float dy = input.y * m->speed * dt;

        float px0 = t->position.x - HALF, py0 = t->position.y - HALF;
        float px1 = t->position.x + HALF, py1 = t->position.y + HALF;

        // X axis
        bool bx = false;
        float tx0 = px0 + dx, tx1 = px1 + dx;
        for (auto& r : collision_rects_)
            if (tx0 < r.x1 && tx1 > r.x0 && py0 < r.y1 && py1 > r.y0) { bx = true; break; }
        if (!bx) t->position.x += dx;

        // Y axis
        bool by = false;
        for (auto& r : collision_rects_)
            if (px0 < r.x1 && px1 > r.x0 && py0 + dy < r.y1 && py1 + dy > r.y0) { by = true; break; }
        if (!by) t->position.y += dy;

        // Clamp player inside the map so it cannot walk off the edge.
        if (has_map_bounds_) {
            t->position.x = std::clamp(t->position.x, map_min_x_ + HALF, map_max_x_ - HALF);
            t->position.y = std::clamp(t->position.y, map_min_y_ + HALF, map_max_y_ - HALF);
        }
    }
}

} // namespace scene
} // namespace mg
