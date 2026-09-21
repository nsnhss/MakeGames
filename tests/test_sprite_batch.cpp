#include "test_framework.h"
#include "scene/world.h"
#include "scene/systems/sprite_batch_builder.h"
#include "scene/components/transform.h"
#include "scene/components/sprite.h"
#include "scene/components/animator.h"

using namespace mg::scene;

// 多纹理实体 → 分组正确、first_instance 偏移累加正确、instance 数据与组件一致
MG_TEST(sprite_batch_grouping_and_offsets) {
    World world;

    // 同 sorting_order=0：纹理字典序决定分组顺序（a.png < b.png）
    EntityID e1 = world.CreateEntity();   // b.png
    world.AddComponent<Transform>(e1).position = { 10.0f, 20.0f };
    world.AddComponent<Sprite>(e1).texture_path = "b.png";

    EntityID e2 = world.CreateEntity();   // a.png
    world.AddComponent<Transform>(e2).position = { 1.0f, 2.0f };
    world.AddComponent<Sprite>(e2).texture_path = "a.png";

    EntityID e3 = world.CreateEntity();   // b.png 第二个
    world.AddComponent<Transform>(e3).position = { 30.0f, 40.0f };
    world.AddComponent<Sprite>(e3).texture_path = "b.png";

    std::vector<SpriteInstance> instances;
    std::vector<SpriteBatch> batches;
    BuildSpriteInstances(&world, instances, batches);

    MG_EXPECT_EQ(instances.size(), 3u);
    MG_EXPECT_EQ(batches.size(), 2u);

    MG_EXPECT(batches[0].texture_path == "a.png");
    MG_EXPECT_EQ(batches[0].first_instance, 0u);
    MG_EXPECT_EQ(batches[0].count, 1u);

    MG_EXPECT(batches[1].texture_path == "b.png");
    MG_EXPECT_EQ(batches[1].first_instance, 1u);
    MG_EXPECT_EQ(batches[1].count, 2u);

    // instance 数据与组件值一致（a.png 实体在首位）
    MG_EXPECT_EQ(instances[0].world_x, 1.0f);
    MG_EXPECT_EQ(instances[0].world_y, 2.0f);
    MG_EXPECT_EQ(instances[0].frame_sx, 1.0f);
    MG_EXPECT_EQ(instances[0].color_a, 1.0f);
    MG_EXPECT_EQ(instances[1].world_x, 10.0f);
    MG_EXPECT_EQ(instances[2].world_x, 30.0f);
}

// sorting_order 是排序主键：低层级先画，同层级内按纹理分组
MG_TEST(sprite_batch_sorting_order_priority) {
    World world;

    EntityID back = world.CreateEntity();    // sorting_order=0, z.png
    world.AddComponent<Transform>(back);
    world.AddComponent<Sprite>(back).texture_path = "z.png";

    EntityID front = world.CreateEntity();   // sorting_order=5, a.png
    auto& ft = world.AddComponent<Transform>(front);
    ft.sorting_order = 5;
    world.AddComponent<Sprite>(front).texture_path = "a.png";

    std::vector<SpriteInstance> instances;
    std::vector<SpriteBatch> batches;
    BuildSpriteInstances(&world, instances, batches);

    // sorting_order 优先于纹理名：z.png(order 0) 在 a.png(order 5) 之前
    MG_EXPECT_EQ(batches.size(), 2u);
    MG_EXPECT(batches[0].texture_path == "z.png");
    MG_EXPECT(batches[1].texture_path == "a.png");
}

// Animator → uv_off / frame_scale 计算正确
MG_TEST(sprite_batch_animator_frame_uv) {
    World world;

    EntityID e = world.CreateEntity();
    world.AddComponent<Transform>(e);
    world.AddComponent<Sprite>(e).texture_path = "sheet.png";
    auto& anim = world.AddComponent<Animator>(e);
    anim.columns = 4;
    anim.frame_count = 8;      // 4 列 × 2 行
    anim.current_frame = 5;    // col=1, row=1

    std::vector<SpriteInstance> instances;
    std::vector<SpriteBatch> batches;
    BuildSpriteInstances(&world, instances, batches);

    MG_EXPECT_EQ(instances.size(), 1u);
    MG_EXPECT_EQ(instances[0].frame_sx, 0.25f);
    MG_EXPECT_EQ(instances[0].frame_sy, 0.5f);
    MG_EXPECT_EQ(instances[0].uv_off_x, 0.25f);
    MG_EXPECT_EQ(instances[0].uv_off_y, 0.5f);
}

// 无 Sprite 或无 Transform 的实体被跳过；空 World 安全
MG_TEST(sprite_batch_filters_and_empty) {
    World world;

    EntityID no_sprite = world.CreateEntity();
    world.AddComponent<Transform>(no_sprite);

    EntityID no_transform = world.CreateEntity();
    world.AddComponent<Sprite>(no_transform).texture_path = "x.png";

    std::vector<SpriteInstance> instances;
    std::vector<SpriteBatch> batches;
    BuildSpriteInstances(&world, instances, batches);

    MG_EXPECT(instances.empty());
    MG_EXPECT(batches.empty());

    BuildSpriteInstances(nullptr, instances, batches);
    MG_EXPECT(instances.empty());
    MG_EXPECT(batches.empty());
}

// 图集命中 → 单一 "@atlas" 组；未命中走 per-texture fallback 共存
MG_TEST(sprite_batch_atlas_grouping) {
    World world;

    EntityID e1 = world.CreateEntity();   // 图集命中
    world.AddComponent<Transform>(e1);
    world.AddComponent<Sprite>(e1).texture_path = "in_atlas_a.png";

    EntityID e2 = world.CreateEntity();   // fallback
    world.AddComponent<Transform>(e2);
    world.AddComponent<Sprite>(e2).texture_path = "loose.png";

    EntityID e3 = world.CreateEntity();   // 图集命中
    world.AddComponent<Transform>(e3);
    world.AddComponent<Sprite>(e3).texture_path = "in_atlas_b.png";

    // 假图集回调：路径含 "in_atlas" 即命中，返回固定 rect
    auto fake_atlas = [](void*, const std::string& path, float out[4]) -> bool {
        if (path.find("in_atlas") == std::string::npos) return false;
        out[0] = 0.5f; out[1] = 0.25f; out[2] = 0.125f; out[3] = 0.0625f;
        return true;
    };

    std::vector<SpriteInstance> instances;
    std::vector<SpriteBatch> batches;
    BuildSpriteInstances(&world, instances, batches, fake_atlas, nullptr);

    MG_EXPECT_EQ(instances.size(), 3u);
    MG_EXPECT_EQ(batches.size(), 2u);

    // 图集组优先连续，合并为单 batch
    MG_EXPECT(batches[0].texture_path == "@atlas");
    MG_EXPECT_EQ(batches[0].first_instance, 0u);
    MG_EXPECT_EQ(batches[0].count, 2u);

    // fallback 组保持 per-texture
    MG_EXPECT(batches[1].texture_path == "loose.png");
    MG_EXPECT_EQ(batches[1].first_instance, 2u);
    MG_EXPECT_EQ(batches[1].count, 1u);

    // 图集 instance 携带 rect，fallback instance 为 identity (0,0,1,1)
    MG_EXPECT_EQ(instances[0].atlas_u0, 0.5f);
    MG_EXPECT_EQ(instances[0].atlas_du, 0.125f);
    MG_EXPECT_EQ(instances[2].atlas_u0, 0.0f);
    MG_EXPECT_EQ(instances[2].atlas_du, 1.0f);
}

// 不传图集回调 → 行为与之前一致（instance atlas rect 为 identity）
MG_TEST(sprite_batch_no_atlas_callback_identity) {
    World world;
    EntityID e = world.CreateEntity();
    world.AddComponent<Transform>(e);
    world.AddComponent<Sprite>(e).texture_path = "x.png";

    std::vector<SpriteInstance> instances;
    std::vector<SpriteBatch> batches;
    BuildSpriteInstances(&world, instances, batches);

    MG_EXPECT_EQ(instances.size(), 1u);
    MG_EXPECT(batches[0].texture_path == "x.png");
    MG_EXPECT_EQ(instances[0].atlas_u0, 0.0f);
    MG_EXPECT_EQ(instances[0].atlas_v0, 0.0f);
    MG_EXPECT_EQ(instances[0].atlas_du, 1.0f);
    MG_EXPECT_EQ(instances[0].atlas_dv, 1.0f);
}

// Y-sort：同 sorting_order 层内按 position.y 排序（Y 大者后画=靠前）；关闭者沉到层底
MG_TEST(sprite_batch_y_sort) {
    World world;

    EntityID lower = world.CreateEntity();   // y=100，应最后画
    world.AddComponent<Transform>(lower).position = { 0.0f, 100.0f };
    world.AddComponent<Sprite>(lower).texture_path = "same.png";

    EntityID upper = world.CreateEntity();   // y=10，应先画
    world.AddComponent<Transform>(upper).position = { 0.0f, 10.0f };
    world.AddComponent<Sprite>(upper).texture_path = "same.png";

    EntityID pinned = world.CreateEntity();  // y_sort=false，沉到层底（最先画）
    world.AddComponent<Transform>(pinned).position = { 0.0f, 9999.0f };
    auto& ps = world.AddComponent<Sprite>(pinned);
    ps.texture_path = "same.png";
    ps.y_sort = false;

    std::vector<SpriteInstance> instances;
    std::vector<SpriteBatch> batches;
    BuildSpriteInstances(&world, instances, batches);

    // 同纹理同层：单组，顺序 = pinned(y_sort off) → upper(y=10) → lower(y=100)
    MG_EXPECT_EQ(batches.size(), 1u);
    MG_EXPECT_EQ(instances.size(), 3u);
    MG_EXPECT_EQ(instances[0].world_y, 9999.0f);
    MG_EXPECT_EQ(instances[1].world_y, 10.0f);
    MG_EXPECT_EQ(instances[2].world_y, 100.0f);
}

// sorting_order 仍优先于 Y-sort
MG_TEST(sprite_batch_sorting_order_beats_y) {
    World world;

    EntityID front_low_y = world.CreateEntity();  // order=5, y=0
    auto& t1 = world.AddComponent<Transform>(front_low_y);
    t1.sorting_order = 5;
    world.AddComponent<Sprite>(front_low_y).texture_path = "a.png";

    EntityID back_high_y = world.CreateEntity();  // order=0, y=500
    world.AddComponent<Transform>(back_high_y).position = { 0.0f, 500.0f };
    world.AddComponent<Sprite>(back_high_y).texture_path = "a.png";

    std::vector<SpriteInstance> instances;
    std::vector<SpriteBatch> batches;
    BuildSpriteInstances(&world, instances, batches);

    MG_EXPECT_EQ(instances.size(), 2u);
    MG_EXPECT_EQ(instances[0].world_y, 500.0f);   // order=0 先画
    MG_EXPECT_EQ(instances[1].world_y, 0.0f);
}

// pivot / flip 字段进入 instance（供 shader 锚点偏移与 UV 翻转）
MG_TEST(sprite_batch_pivot_flip) {
    World world;

    EntityID e = world.CreateEntity();
    world.AddComponent<Transform>(e);
    auto& s = world.AddComponent<Sprite>(e);
    s.texture_path = "x.png";
    s.pivot = { 0.0f, 1.0f };
    s.flip_x = true;
    s.flip_y = false;

    std::vector<SpriteInstance> instances;
    std::vector<SpriteBatch> batches;
    BuildSpriteInstances(&world, instances, batches);

    MG_EXPECT_EQ(instances.size(), 1u);
    MG_EXPECT_EQ(instances[0].pivot_x, 0.0f);
    MG_EXPECT_EQ(instances[0].pivot_y, 1.0f);
    MG_EXPECT_EQ(instances[0].flip_x, 1.0f);
    MG_EXPECT_EQ(instances[0].flip_y, 0.0f);
}

// pivot / flip 默认值向后兼容（居中锚点、不翻转）
MG_TEST(sprite_batch_pivot_flip_defaults) {
    World world;
    EntityID e = world.CreateEntity();
    world.AddComponent<Transform>(e);
    world.AddComponent<Sprite>(e).texture_path = "x.png";

    std::vector<SpriteInstance> instances;
    std::vector<SpriteBatch> batches;
    BuildSpriteInstances(&world, instances, batches);

    MG_EXPECT_EQ(instances.size(), 1u);
    MG_EXPECT_EQ(instances[0].pivot_x, 0.5f);
    MG_EXPECT_EQ(instances[0].pivot_y, 0.5f);
    MG_EXPECT_EQ(instances[0].flip_x, 0.0f);
    MG_EXPECT_EQ(instances[0].flip_y, 0.0f);
}
