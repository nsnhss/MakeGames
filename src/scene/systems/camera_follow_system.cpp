#include "scene/systems/camera_follow_system.h"
#include "scene/world.h"
#include "scene/components/transform.h"
#include "scene/components/camera.h"
#include "scene/components/camera_follow.h"
#include "scene/components/tilemap.h"
#include <algorithm>
#include <cmath>

namespace mg {
namespace scene {

void UpdateCameraFollowSystem(World* world, float dt, uint32_t vp_w, uint32_t vp_h) {
    if (!world) return;

    std::vector<EntityID> cameras;
    world->Query<Camera, Transform>(cameras);

    // Find default follow target (first entity with CameraFollowTarget) for cameras without explicit target.
    EntityID default_follow = kInvalidEntity;
    std::vector<EntityID> follow_targets;
    world->Query<CameraFollowTarget>(follow_targets);
    if (!follow_targets.empty()) default_follow = follow_targets[0];

    for (auto e : cameras) {
        auto* cam = world->GetComponent<Camera>(e);
        auto* cam_t = world->GetComponent<Transform>(e);
        if (!cam || !cam_t) continue;
        EntityID target_id = cam->follow_target;
        if (target_id == kInvalidEntity || target_id == 0) target_id = default_follow;
        if (target_id == kInvalidEntity || target_id == 0) continue;
        if (!world->IsValid(target_id)) continue;
        auto* target_t = world->GetComponent<Transform>(target_id);
        if (!target_t) continue;

        math::Vec2 target = target_t->position;
        math::Vec2 curr = cam_t->position;
        math::Vec2 delta = target - curr;

        if (cam->smooth_follow) {
            if (std::fabs(delta.x) < cam->dead_zone.x) delta.x = 0;
            if (std::fabs(delta.y) < cam->dead_zone.y) delta.y = 0;
            curr.x += delta.x * cam->follow_speed * dt;
            curr.y += delta.y * cam->follow_speed * dt;
        } else {
            curr = target;
        }
        cam_t->position = curr;

        // Clamp camera position to tilemap bounds so the viewport never shows
        // the empty area outside the map.
        {
            // 运行时改为查找第一个 Tilemap 实体（editor 原用 TilesetEditor 选中态）
            Tilemap* tm = nullptr;
            std::vector<EntityID> all;
            world->GetAllEntities(all);
            for (auto te : all) {
                tm = world->GetComponent<Tilemap>(te);
                if (tm) break;
            }
            if (tm && !tm->layers.empty()) {
                float map_w = tm->layers[0].width  * tm->tile_width;
                float map_h = tm->layers[0].height * tm->tile_height;
                if (vp_w > 0 && vp_h > 0) {
                    float aspect = (float)vp_w / (float)vp_h;
                    float half_h = cam->ortho_size;
                    float half_w = half_h * aspect;
                    // Clamp camera so the viewport does not show too much outside the map,
                    // but still allow it to follow the player when the viewport is larger
                    // than the map (in which case the valid range collapses toward center).
                    float min_x = std::min(half_w, map_w - half_w);
                    float max_x = std::max(half_w, map_w - half_w);
                    float min_y = std::min(half_h, map_h - half_h);
                    float max_y = std::max(half_h, map_h - half_h);
                    cam_t->position.x = std::clamp(cam_t->position.x, min_x, max_x);
                    cam_t->position.y = std::clamp(cam_t->position.y, min_y, max_y);
                }
            }
        }
    }
}

} // namespace scene
} // namespace mg
