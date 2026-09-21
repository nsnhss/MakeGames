#include "test_framework.h"
#include "scene/world.h"
#include "scene/components/transform.h"
#include "scene/components/name.h"
#include "scene/components/tag.h"
#include "scene/components/sprite.h"

using namespace mg::scene;

MG_TEST(world_entity_lifecycle) {
    World world;
    EntityID a = world.CreateEntity();
    EntityID b = world.CreateEntity();
    MG_EXPECT(world.IsValid(a));
    MG_EXPECT(world.IsValid(b));
    MG_EXPECT(a != b);
    MG_EXPECT_EQ(world.GetEntityCount(), 2u);

    world.DestroyEntity(a);
    MG_EXPECT(!world.IsValid(a));
    MG_EXPECT(world.IsValid(b));
    MG_EXPECT_EQ(world.GetEntityCount(), 1u);

    // ID 复用：新建实体应复用已销毁的 ID
    EntityID c = world.CreateEntity();
    MG_EXPECT_EQ(c, a);
    MG_EXPECT(world.IsValid(c));
}

MG_TEST(world_component_add_get_remove) {
    World world;
    EntityID e = world.CreateEntity();

    MG_EXPECT(!world.HasComponent<Transform>(e));
    MG_EXPECT(world.GetComponent<Transform>(e) == nullptr);

    auto& t = world.AddComponent<Transform>(e);
    t.position = { 3.0f, 4.0f };
    t.rotation = 90.0f;

    MG_EXPECT(world.HasComponent<Transform>(e));
    auto* got = world.GetComponent<Transform>(e);
    MG_EXPECT(got != nullptr);
    MG_EXPECT_EQ(got->position.x, 3.0f);
    MG_EXPECT_EQ(got->rotation, 90.0f);

    world.RemoveComponent<Transform>(e);
    MG_EXPECT(!world.HasComponent<Transform>(e));
}

MG_TEST(world_destroy_entity_recycles_components) {
    World world;
    EntityID e = world.CreateEntity();
    world.AddComponent<Transform>(e);
    world.AddComponent<Name>(e);
    world.DestroyEntity(e);
    MG_EXPECT(!world.HasComponent<Transform>(e));
    MG_EXPECT(!world.HasComponent<Name>(e));
}

MG_TEST(world_query_single_and_multi) {
    World world;
    EntityID e1 = world.CreateEntity();
    EntityID e2 = world.CreateEntity();
    EntityID e3 = world.CreateEntity();
    world.AddComponent<Transform>(e1);
    world.AddComponent<Transform>(e2);
    world.AddComponent<Transform>(e3);
    world.AddComponent<Name>(e2);
    world.AddComponent<Name>(e3);
    world.AddComponent<Tag>(e3);

    std::vector<EntityID> out;
    world.Query<Transform>(out);
    MG_EXPECT_EQ(out.size(), 3u);

    out.clear();
    world.Query<Transform, Name>(out);
    MG_EXPECT_EQ(out.size(), 2u);

    out.clear();
    world.Query<Transform, Name, Tag>(out);
    MG_EXPECT_EQ(out.size(), 1u);
    MG_EXPECT_EQ(out[0], e3);
}

MG_TEST(world_clone_entity) {
    World world;
    EntityID src = world.CreateEntity();
    auto& t = world.AddComponent<Transform>(src);
    t.position = { 7.0f, 8.0f };
    auto& n = world.AddComponent<Name>(src);
    n.value = "Original";

    EntityID copy = world.CloneEntity(src);
    MG_EXPECT(copy != src);
    MG_EXPECT(world.IsValid(copy));
    auto* ct = world.GetComponent<Transform>(copy);
    auto* cn = world.GetComponent<Name>(copy);
    MG_EXPECT(ct != nullptr);
    MG_EXPECT(cn != nullptr);
    MG_EXPECT_EQ(ct->position.x, 7.0f);
    MG_EXPECT_EQ(cn->value, std::string("Original"));
}

// Undo/Redo 命令的底层机制：快照 → 销毁 → 恢复 应保持数据等价
MG_TEST(world_snapshot_restore_symmetry) {
    World world;
    EntityID e = world.CreateEntity();
    auto& t = world.AddComponent<Transform>(e);
    t.position = { 1.5f, -2.5f };
    t.sorting_order = 3;
    auto& n = world.AddComponent<Name>(e);
    n.value = "Hero";
    auto& tag = world.AddComponent<Tag>(e);
    tag.Add("player");

    nlohmann::json snapshot = world.SnapshotEntity(e);
    world.DestroyEntity(e);
    MG_EXPECT(!world.IsValid(e));

    EntityID restored = world.RestoreEntity(snapshot);
    MG_EXPECT(world.IsValid(restored));
    auto* rt = world.GetComponent<Transform>(restored);
    auto* rn = world.GetComponent<Name>(restored);
    auto* rtag = world.GetComponent<Tag>(restored);
    MG_EXPECT(rt != nullptr);
    MG_EXPECT(rn != nullptr);
    MG_EXPECT(rtag != nullptr);
    MG_EXPECT_EQ(rt->position.x, 1.5f);
    MG_EXPECT_EQ(rt->position.y, -2.5f);
    MG_EXPECT_EQ(rt->sorting_order, 3);
    MG_EXPECT_EQ(rn->value, std::string("Hero"));
    MG_EXPECT(rtag->Has("player"));
}

// Prefab 流程：SnapshotEntity → JSON 文本 → 解析 → RestoreEntity → 数据等价、新实体 ID 不同
// （PrefabManager 的文件 I/O 为薄封装，核心序列化路径在此覆盖）
MG_TEST(world_restore_entity_into) {
    World world;
    EntityID e = world.CreateEntity();
    world.AddComponent<Transform>(e).position = { 5.0f, 6.0f };
    world.AddComponent<Name>(e).value = "Original";

    auto snapshot = world.SnapshotEntity(e);

    // 修改 + 加组件
    world.GetComponent<Transform>(e)->position = { 99.0f, 99.0f };
    world.GetComponent<Name>(e)->value = "Modified";
    world.AddComponent<Sprite>(e).texture_path = "extra.png";

    // 原地还原
    MG_EXPECT(world.RestoreEntityInto(e, snapshot));

    // ID 不变、值还原、多余组件被移除
    auto* t = world.GetComponent<Transform>(e);
    MG_EXPECT(t != nullptr);
    MG_EXPECT_EQ(t->position.x, 5.0f);
    MG_EXPECT_EQ(t->position.y, 6.0f);
    MG_EXPECT_EQ(world.GetComponent<Name>(e)->value, "Original");
    MG_EXPECT(world.GetComponent<Sprite>(e) == nullptr);

    // 实体仍然有效，且可用于后续操作
    MG_EXPECT(world.IsValid(e));
    world.GetComponent<Transform>(e)->position.x = 42.0f;
    MG_EXPECT_EQ(world.GetComponent<Transform>(e)->position.x, 42.0f);

    // 无效实体返回 false
    MG_EXPECT(!world.RestoreEntityInto(9999, snapshot));
}

MG_TEST(world_prefab_json_roundtrip) {
    World world;
    EntityID e = world.CreateEntity();
    auto& t = world.AddComponent<Transform>(e);
    t.position = { 32.0f, 64.0f };
    auto& n = world.AddComponent<Name>(e);
    n.value = "Slime";
    auto& s = world.AddComponent<Sprite>(e);
    s.texture_path = "assets/sprites/slime.png";

    // 模拟 prefab 文件写读：dump → parse
    nlohmann::json prefab;
    prefab["name"] = "slime_enemy";
    prefab["entity"] = world.SnapshotEntity(e);
    nlohmann::json parsed = nlohmann::json::parse(prefab.dump());

    EntityID inst = world.RestoreEntity(parsed["entity"]);
    MG_EXPECT(world.IsValid(inst));
    MG_EXPECT(inst != e);

    auto* it = world.GetComponent<Transform>(inst);
    auto* in = world.GetComponent<Name>(inst);
    auto* is = world.GetComponent<Sprite>(inst);
    MG_EXPECT(it != nullptr);
    MG_EXPECT(in != nullptr);
    MG_EXPECT(is != nullptr);
    MG_EXPECT_EQ(it->position.x, 32.0f);
    MG_EXPECT_EQ(it->position.y, 64.0f);
    MG_EXPECT_EQ(in->value, std::string("Slime"));
    MG_EXPECT_EQ(is->texture_path, std::string("assets/sprites/slime.png"));

    // 原实体不受影响
    MG_EXPECT(world.IsValid(e));
}
