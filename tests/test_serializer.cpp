#include "test_framework.h"
#include "scene/scene.h"
#include "scene/scene_serializer.h"
#include "scene/components/transform.h"
#include "scene/components/name.h"

using namespace mg::scene;

// 场景 → JSON → 场景 的 round-trip 一致性
MG_TEST(scene_serialize_roundtrip) {
    Scene scene;
    scene.Initialize(); // World 由 Initialize 创建
    scene.SetName("TestMap");
    scene.SetSize(20, 15);
    scene.SetTileSize(32);
    scene.SetMode(Scene::Mode::SideScroller);
    scene.SetBGMPath("assets/audio/bgm/theme.ogg");

    auto* world = scene.GetWorld();
    EntityID e1 = world->CreateEntity();
    auto& t1 = world->AddComponent<Transform>(e1);
    t1.position = { 5.0f, 6.0f };
    t1.rotation = 45.0f;
    t1.sorting_order = 2;
    auto& n1 = world->AddComponent<Name>(e1);
    n1.value = "VillageChief";

    EntityID e2 = world->CreateEntity();
    world->AddComponent<Transform>(e2);

    nlohmann::json j = SceneSerializer::Serialize(&scene);
    auto restored = SceneSerializer::Deserialize(j);
    MG_EXPECT(restored != nullptr);

    // 元数据
    MG_EXPECT_EQ(restored->GetName(), std::string("TestMap"));
    MG_EXPECT_EQ(restored->GetWidth(), 20);
    MG_EXPECT_EQ(restored->GetHeight(), 15);
    MG_EXPECT_EQ(restored->GetTileSize(), 32);
    MG_EXPECT(restored->GetMode() == Scene::Mode::SideScroller);
    MG_EXPECT_EQ(restored->GetBGMPath(), std::string("assets/audio/bgm/theme.ogg"));

    // 实体与组件
    auto* rw = restored->GetWorld();
    MG_EXPECT_EQ(rw->GetEntityCount(), 2u);

    std::vector<EntityID> named;
    rw->Query<Transform, Name>(named);
    MG_EXPECT_EQ(named.size(), 1u);
    auto* rt = rw->GetComponent<Transform>(named[0]);
    auto* rn = rw->GetComponent<Name>(named[0]);
    MG_EXPECT(rn != nullptr);
    MG_EXPECT_EQ(rn->value, std::string("VillageChief"));
    MG_EXPECT(rt != nullptr);
    MG_EXPECT_EQ(rt->position.x, 5.0f);
    MG_EXPECT_EQ(rt->rotation, 45.0f);
    MG_EXPECT_EQ(rt->sorting_order, 2);
}

MG_TEST(scene_serialize_empty_scene) {
    Scene scene;
    scene.Initialize();
    scene.SetName("Empty");
    nlohmann::json j = SceneSerializer::Serialize(&scene);
    auto restored = SceneSerializer::Deserialize(j);
    MG_EXPECT(restored != nullptr);
    MG_EXPECT_EQ(restored->GetName(), std::string("Empty"));
    MG_EXPECT_EQ(restored->GetWorld()->GetEntityCount(), 0u);
}

// 二次序列化稳定性：serialize(deserialize(serialize(s))) == serialize(s)
MG_TEST(scene_serialize_idempotent) {
    Scene scene;
    scene.Initialize();
    scene.SetName("Idem");
    auto* world = scene.GetWorld();
    EntityID e = world->CreateEntity();
    auto& n = world->AddComponent<Name>(e);
    n.value = "A";

    nlohmann::json j1 = SceneSerializer::Serialize(&scene);
    auto s2 = SceneSerializer::Deserialize(j1);
    nlohmann::json j2 = SceneSerializer::Serialize(s2.get());
    MG_EXPECT(j1 == j2);
}
