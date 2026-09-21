#include "tween_animation_system.h"
#include "scene/world.h"
#include "scene/components/transform.h"
#include "scene/components/sprite.h"
#include <algorithm>

namespace mg {
namespace scene {

float ApplyEasing(Easing easing, float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    switch (easing) {
        case Easing::InQuad:     return t * t;
        case Easing::OutQuad:    return t * (2.0f - t);
        case Easing::InOutQuad:  return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
        case Easing::InCubic:    return t * t * t;
        case Easing::OutCubic:   { float u = t - 1.0f; return u * u * u + 1.0f; }
        case Easing::InOutCubic: return t < 0.5f ? 4.0f * t * t * t : (t - 1.0f) * (2.0f * t - 2.0f) * (2.0f * t - 2.0f) + 1.0f;
        case Easing::Linear:
        default:                 return t;
    }
}

namespace {

template<typename TKey, typename TValue, typename GetValue, typename Lerp>
TValue SampleTrackImpl(const std::vector<TKey>& keys, float t, TValue fallback,
                       Easing easing, GetValue get_value, Lerp lerp) {
    if (keys.empty()) return fallback;
    if (t <= keys.front().t) return get_value(keys.front());
    if (t >= keys.back().t) return get_value(keys.back());
    for (size_t i = 0; i + 1 < keys.size(); ++i) {
        if (t >= keys[i].t && t <= keys[i + 1].t) {
            float span = keys[i + 1].t - keys[i].t;
            float a = span > 0.0f ? (t - keys[i].t) / span : 0.0f;
            a = ApplyEasing(easing, a);
            return lerp(get_value(keys[i]), get_value(keys[i + 1]), a);
        }
    }
    return get_value(keys.back());
}

float LerpF(float a, float b, float t) { return a + (b - a) * t; }
math::Vec2 LerpV(const math::Vec2& a, const math::Vec2& b, float t) {
    return { LerpF(a.x, b.x, t), LerpF(a.y, b.y, t) };
}
math::Color LerpC(const math::Color& a, const math::Color& b, float t) {
    return { LerpF(a.r, b.r, t), LerpF(a.g, b.g, t), LerpF(a.b, b.b, t), LerpF(a.a, b.a, t) };
}

} // namespace

float SampleFloatTrack(const std::vector<FloatKey>& keys, float t, float fallback, Easing easing) {
    return SampleTrackImpl(keys, t, fallback, easing,
        [](const FloatKey& k) { return k.v; }, LerpF);
}

math::Vec2 SampleVec2Track(const std::vector<Vec2Key>& keys, float t, math::Vec2 fallback, Easing easing) {
    return SampleTrackImpl(keys, t, fallback, easing,
        [](const Vec2Key& k) { return math::Vec2{k.x, k.y}; }, LerpV);
}

math::Color SampleColorTrack(const std::vector<ColorKey>& keys, float t, math::Color fallback, Easing easing) {
    return SampleTrackImpl(keys, t, fallback, easing,
        [](const ColorKey& k) { return math::Color{k.r, k.g, k.b, k.a}; }, LerpC);
}

void TweenAnimationSystem::Update(float dt, World* world) {
    if (!world) return;

    std::vector<EntityID> entities;
    world->Query<TweenAnimation>(entities);
    for (auto e : entities) UpdateOne(dt, world, e);
}

void TweenAnimationSystem::UpdateOne(float dt, World* world, EntityID e) {
    if (!world) return;
    auto* anim = world->GetComponent<TweenAnimation>(e);
    auto* transform = world->GetComponent<Transform>(e);
    if (!anim || !transform || !anim->playing) return;

    // 首次捕获基准值
    if (!anim->base_captured) {
        anim->base_position = transform->position;
        anim->base_scale = transform->scale;
        anim->base_captured = true;
    }

    // 推进时间
    anim->time += dt;
    if (anim->duration > 0.0f) {
        if (anim->loop) {
            while (anim->time > anim->duration) anim->time -= anim->duration;
        } else if (anim->time >= anim->duration) {
            anim->time = anim->duration;
            anim->playing = false;   // 播完停在末帧
        }
    }
    float t = anim->time;

    transform->position = anim->base_position + SampleVec2Track(anim->position, t, {}, anim->easing);
    transform->rotation = SampleFloatTrack(anim->rotation, t, transform->rotation, anim->easing);
    transform->scale = SampleVec2Track(anim->scale, t, anim->base_scale, anim->easing);

    if (auto* sprite = world->GetComponent<Sprite>(e)) {
        sprite->tint = SampleColorTrack(anim->tint, t, sprite->tint, anim->easing);
    }
}

} // namespace scene
} // namespace mg
