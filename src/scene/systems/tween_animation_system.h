#pragma once
// ---------------------------------------------------------------------------
// 关键帧补间动画系统（纯逻辑采样 + ECS 驱动，不依赖 Vulkan/SDL，可单测）
// 采样语义：keys 按 t 升序；t 在首 key 前取首值、末 key 后取末值、中间线性插值。
// ---------------------------------------------------------------------------
#include "scene/components/tween_animation.h"
#include "core/math/vec2.h"
#include "core/math/color.h"
#include <vector>

namespace mg {
namespace scene {

class World;

// ---- 轨道采样纯函数 ----

// 缓动求值：输入/输出均为 [0,1] 插值系数
float ApplyEasing(Easing easing, float t);

float SampleFloatTrack(const std::vector<FloatKey>& keys, float t, float fallback, Easing easing = Easing::Linear);
math::Vec2 SampleVec2Track(const std::vector<Vec2Key>& keys, float t, math::Vec2 fallback, Easing easing = Easing::Linear);
math::Color SampleColorTrack(const std::vector<ColorKey>& keys, float t, math::Color fallback, Easing easing = Easing::Linear);

// ---- 播放系统 ----

class TweenAnimationSystem {
public:
    // 推进 time（loop 回绕 / 非 loop clamp 后停播），采样四轨道写 Transform / Sprite。
    // 首次调用捕获 base_position/base_scale（position 轨道为相对偏移）。
    void Update(float dt, World* world);
    // 只驱动单个实体（编辑器编辑模式预览用，避免整个场景的 clip 都被推进）
    void UpdateOne(float dt, World* world, EntityID entity);
};

} // namespace scene
} // namespace mg
