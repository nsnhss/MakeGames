#include "test_framework.h"
#include "scene/systems/tween_animation_system.h"
#include "scene/world.h"
#include "scene/components/transform.h"
#include "scene/components/sprite.h"
#include <cmath>

using namespace mg::scene;
using mg::math::Vec2;
using mg::math::Color;

// ---- 采样纯函数 ----

MG_TEST(tween_sample_float_empty_fallback) {
    std::vector<FloatKey> keys;
    MG_EXPECT_EQ(SampleFloatTrack(keys, 0.0f, 42.0f), 42.0f);
    MG_EXPECT_EQ(SampleFloatTrack(keys, 1.5f, -7.0f), -7.0f);
}

MG_TEST(tween_sample_float_single_key) {
    std::vector<FloatKey> keys = { {1.0f, 10.0f} };
    MG_EXPECT_EQ(SampleFloatTrack(keys, 0.0f, 0.0f), 10.0f);   // 首 key 前取首值
    MG_EXPECT_EQ(SampleFloatTrack(keys, 1.0f, 0.0f), 10.0f);
    MG_EXPECT_EQ(SampleFloatTrack(keys, 5.0f, 0.0f), 10.0f);   // 末 key 后取末值
}

MG_TEST(tween_sample_float_clamp_and_lerp) {
    std::vector<FloatKey> keys = { {0.0f, 0.0f}, {2.0f, 10.0f} };
    MG_EXPECT_EQ(SampleFloatTrack(keys, -1.0f, 0.0f), 0.0f);   // 首 key 前钳制
    MG_EXPECT_EQ(SampleFloatTrack(keys, 3.0f, 0.0f), 10.0f);   // 末 key 后钳制
    MG_EXPECT_EQ(SampleFloatTrack(keys, 1.0f, 0.0f), 5.0f);    // 中点线性插值
    MG_EXPECT_EQ(SampleFloatTrack(keys, 0.0f, 0.0f), 0.0f);
    MG_EXPECT_EQ(SampleFloatTrack(keys, 2.0f, 0.0f), 10.0f);
}

MG_TEST(tween_sample_vec2_midpoint) {
    std::vector<Vec2Key> keys = { {0.0f, 0.0f, 0.0f}, {1.0f, 10.0f, 20.0f} };
    Vec2 mid = SampleVec2Track(keys, 0.5f, {});
    MG_EXPECT_EQ(mid.x, 5.0f);
    MG_EXPECT_EQ(mid.y, 10.0f);
    MG_EXPECT(SampleVec2Track({}, 0.5f, {3.0f, 4.0f}).x == 3.0f); // 空轨道 fallback
}

MG_TEST(tween_sample_color_midpoint) {
    std::vector<ColorKey> keys = {
        {0.0f, 1.0f, 0.0f, 0.0f, 1.0f},
        {1.0f, 0.0f, 1.0f, 0.0f, 0.5f},
    };
    Color mid = SampleColorTrack(keys, 0.5f, Color::White());
    MG_EXPECT_EQ(mid.r, 0.5f);
    MG_EXPECT_EQ(mid.g, 0.5f);
    MG_EXPECT_EQ(mid.b, 0.0f);
    MG_EXPECT_EQ(mid.a, 0.75f);
}

// ---- 系统行为 ----

MG_TEST(tween_system_advances_and_offsets_position) {
    World world;
    EntityID e = world.CreateEntity();
    auto& tf = world.AddComponent<Transform>(e);
    tf.position = {100.0f, 50.0f};    // 基准位置
    auto& anim = world.AddComponent<TweenAnimation>(e);
    anim.duration = 1.0f;
    anim.loop = false;
    anim.position = { {0.0f, 0.0f, 0.0f}, {1.0f, 10.0f, 0.0f} };

    TweenAnimationSystem sys;
    sys.Update(0.5f, &world);

    MG_EXPECT(anim.base_captured);
    MG_EXPECT_EQ(anim.time, 0.5f);
    MG_EXPECT_EQ(tf.position.x, 105.0f);   // base + 插值偏移
    MG_EXPECT_EQ(tf.position.y, 50.0f);
}

MG_TEST(tween_system_base_captured_once) {
    World world;
    EntityID e = world.CreateEntity();
    auto& tf = world.AddComponent<Transform>(e);
    tf.position = {100.0f, 50.0f};
    auto& anim = world.AddComponent<TweenAnimation>(e);
    anim.duration = 2.0f;
    anim.loop = false;
    anim.position = { {0.0f, 0.0f, 0.0f} };

    TweenAnimationSystem sys;
    sys.Update(0.5f, &world);
    MG_EXPECT_EQ(anim.base_position.x, 100.0f);

    // 外部改了 position，base 不应重新捕获
    tf.position = {999.0f, 999.0f};
    sys.Update(0.5f, &world);
    MG_EXPECT_EQ(anim.base_position.x, 100.0f);
    MG_EXPECT_EQ(tf.position.x, 100.0f);   // base + 0 偏移，覆盖外部修改
}

MG_TEST(tween_system_loop_wraps_time) {
    World world;
    EntityID e = world.CreateEntity();
    world.AddComponent<Transform>(e);
    auto& anim = world.AddComponent<TweenAnimation>(e);
    anim.duration = 1.0f;
    anim.loop = true;

    TweenAnimationSystem sys;
    sys.Update(2.5f, &world);

    MG_EXPECT_EQ(anim.time, 0.5f);   // 2.5 % 1.0
    MG_EXPECT(anim.playing);         // 循环不停播
}

MG_TEST(tween_system_non_loop_clamps_and_stops) {
    World world;
    EntityID e = world.CreateEntity();
    auto& tf = world.AddComponent<Transform>(e);
    auto& anim = world.AddComponent<TweenAnimation>(e);
    anim.duration = 1.0f;
    anim.loop = false;
    anim.position = { {0.0f, 0.0f, 0.0f}, {1.0f, 10.0f, 0.0f} };

    TweenAnimationSystem sys;
    sys.Update(1.5f, &world);

    MG_EXPECT_EQ(anim.time, 1.0f);   // clamp 到 duration
    MG_EXPECT(!anim.playing);        // 播完停
    MG_EXPECT_EQ(tf.position.x, 10.0f); // 停在末帧值

    // 停播后不再推进
    sys.Update(1.0f, &world);
    MG_EXPECT_EQ(anim.time, 1.0f);
}

MG_TEST(tween_system_writes_rotation_scale_and_tint) {
    World world;
    EntityID e = world.CreateEntity();
    auto& tf = world.AddComponent<Transform>(e);
    auto& sprite = world.AddComponent<Sprite>(e);
    auto& anim = world.AddComponent<TweenAnimation>(e);
    anim.duration = 1.0f;
    anim.loop = false;
    anim.rotation = { {0.0f, 0.0f}, {1.0f, 90.0f} };
    anim.scale = { {0.0f, 1.0f, 1.0f}, {1.0f, 2.0f, 3.0f} };
    anim.tint = { {0.0f, 1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 0.0f, 1.0f} };

    TweenAnimationSystem sys;
    sys.Update(0.5f, &world);

    MG_EXPECT_EQ(tf.rotation, 45.0f);
    MG_EXPECT_EQ(tf.scale.x, 1.5f);
    MG_EXPECT_EQ(tf.scale.y, 2.0f);
    MG_EXPECT_EQ(sprite.tint.r, 0.5f);
    MG_EXPECT_EQ(sprite.tint.g, 0.5f);
}

// ---- JSON roundtrip ----

MG_TEST(tween_animation_json_roundtrip) {
    TweenAnimation a;
    a.duration = 2.5f;
    a.loop = false;
    a.playing = false;
    a.position = { {0.0f, 1.0f, 2.0f}, {1.0f, 3.0f, 4.0f} };
    a.rotation = { {0.0f, 0.0f}, {2.0f, 45.0f} };
    a.scale = { {0.5f, 2.0f, 2.0f} };
    a.tint = { {0.0f, 1.0f, 0.5f, 0.25f, 1.0f} };
    // 运行时字段不应影响序列化
    a.time = 1.2f;
    a.base_position = {9.0f, 9.0f};
    a.base_captured = true;

    nlohmann::json j = a;
    MG_EXPECT(!j.contains("time"));
    MG_EXPECT(!j.contains("base_position"));

    TweenAnimation b = j.get<TweenAnimation>();
    MG_EXPECT_EQ(b.duration, 2.5f);
    MG_EXPECT_EQ(b.loop, false);
    MG_EXPECT_EQ(b.playing, false);
    MG_EXPECT_EQ(b.position.size(), 2u);
    MG_EXPECT_EQ(b.position[1].t, 1.0f);
    MG_EXPECT_EQ(b.position[1].x, 3.0f);
    MG_EXPECT_EQ(b.position[1].y, 4.0f);
    MG_EXPECT_EQ(b.rotation.size(), 2u);
    MG_EXPECT_EQ(b.rotation[1].v, 45.0f);
    MG_EXPECT_EQ(b.scale.size(), 1u);
    MG_EXPECT_EQ(b.scale[0].x, 2.0f);
    MG_EXPECT_EQ(b.tint.size(), 1u);
    MG_EXPECT_EQ(b.tint[0].g, 0.5f);
    MG_EXPECT_EQ(b.tint[0].a, 1.0f);
    // 运行时字段重置
    MG_EXPECT_EQ(b.time, 0.0f);
    MG_EXPECT(!b.base_captured);
}

// ---- 缓动曲线 ----

// ApplyEasing 端点不变式：f(0)=0, f(1)=1
MG_TEST(tween_easing_endpoints) {
    const Easing easings[] = {
        Easing::Linear, Easing::InQuad, Easing::OutQuad, Easing::InOutQuad,
        Easing::InCubic, Easing::OutCubic, Easing::InOutCubic
    };
    for (auto e : easings) {
        MG_EXPECT(std::fabs(ApplyEasing(e, 0.0f) - 0.0f) < 1e-6f);
        MG_EXPECT(std::fabs(ApplyEasing(e, 1.0f) - 1.0f) < 1e-6f);
    }
}

// 中点序关系：EaseIn < Linear < EaseOut；InOut 过中点 0.5
MG_TEST(tween_easing_midpoint_ordering) {
    float linear = ApplyEasing(Easing::Linear, 0.5f);
    MG_EXPECT(ApplyEasing(Easing::InQuad, 0.5f) < linear);
    MG_EXPECT(ApplyEasing(Easing::OutQuad, 0.5f) > linear);
    MG_EXPECT(ApplyEasing(Easing::InCubic, 0.5f) < linear);
    MG_EXPECT(ApplyEasing(Easing::OutCubic, 0.5f) > linear);
    MG_EXPECT(std::fabs(ApplyEasing(Easing::InOutQuad, 0.5f) - 0.5f) < 1e-6f);
    MG_EXPECT(std::fabs(ApplyEasing(Easing::InOutCubic, 0.5f) - 0.5f) < 1e-6f);
}

// 缓动作用于轨道采样：中点值随 easing 变化，两端钳制不受影响
MG_TEST(tween_easing_track_sampling) {
    std::vector<FloatKey> keys = { {0.0f, 0.0f}, {1.0f, 10.0f} };

    MG_EXPECT_EQ(SampleFloatTrack(keys, 0.5f, 0.0f), 5.0f);                          // 默认 Linear
    MG_EXPECT_EQ(SampleFloatTrack(keys, 0.5f, 0.0f, Easing::InQuad), 2.5f);          // 0.5^2 * 10
    MG_EXPECT_EQ(SampleFloatTrack(keys, 0.5f, 0.0f, Easing::OutQuad), 7.5f);         // 0.75 * 10
    // 两端钳制：超出轨道范围直接取端点值，不经过缓动
    MG_EXPECT_EQ(SampleFloatTrack(keys, -1.0f, 0.0f, Easing::InCubic), 0.0f);
    MG_EXPECT_EQ(SampleFloatTrack(keys, 2.0f, 0.0f, Easing::InCubic), 10.0f);
}

// easing 字段 JSON roundtrip + 旧格式缺省 Linear
MG_TEST(tween_easing_json_roundtrip) {
    TweenAnimation a;
    a.easing = Easing::InOutCubic;
    nlohmann::json j = a;
    MG_EXPECT_EQ(j["easing"].get<int>(), (int)Easing::InOutCubic);

    TweenAnimation b = j.get<TweenAnimation>();
    MG_EXPECT(b.easing == Easing::InOutCubic);

    // 旧格式无 easing 字段 → Linear
    nlohmann::json legacy;
    legacy["duration"] = 1.0f;
    TweenAnimation c = legacy.get<TweenAnimation>();
    MG_EXPECT(c.easing == Easing::Linear);
}
