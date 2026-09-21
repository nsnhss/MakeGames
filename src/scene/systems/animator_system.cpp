#include "animator_system.h"
#include "scene/world.h"
#include "scene/components/animator.h"
#include "scene/components/sprite.h"

namespace mg {
namespace scene {

void AnimatorSystem::Update(float dt, World* world) {
    if (!world) return;

    std::vector<EntityID> entities;
    world->Query<Animator>(entities);

    for (auto e : entities) {
        auto* animator = world->GetComponent<Animator>(e);
        if (!animator || !animator->playing) continue;
        if (animator->frame_count <= 1) continue;

        animator->timer += dt;
        float frame_duration = 1.0f / animator->fps;
        while (animator->timer >= frame_duration) {
            animator->timer -= frame_duration;
            animator->current_frame++;
            if (animator->current_frame >= animator->frame_count) {
                animator->current_frame = 0;
            }
        }

        // 同步到 Sprite 的 anim_frame（供渲染器读取）
        auto* sprite = world->GetComponent<Sprite>(e);
        if (sprite) {
            sprite->anim_frame = animator->current_frame;
        }
    }
}

} // namespace scene
} // namespace mg
