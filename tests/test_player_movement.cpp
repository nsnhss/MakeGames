// test_player_movement.cpp — PlayerMovementSystem：碰撞矩形指纹缓存 + AABB 阻挡/地图 clamp
#include "test_framework.h"
#include "scene/world.h"
#include "scene/components/transform.h"
#include "scene/components/movement.h"
#include "scene/components/tilemap.h"
#include "scene/systems/player_movement_system.h"
#include <SDL.h>
#include <cstring>

using namespace mg::scene;

namespace {

// 4x4 地图（tile 32x32），可后续按需设置阻挡
EntityID MakeTilemap(World& world) {
    EntityID e = world.CreateEntity();
    world.AddComponent<Transform>(e);
    Tilemap tm;
    tm.tile_width = 32;
    tm.tile_height = 32;
    tm.layers.emplace_back("ground", 4, 4);
    world.AddComponent<Tilemap>(e, tm);
    return e;
}

EntityID MakePlayer(World& world, float x, float y) {
    EntityID e = world.CreateEntity();
    auto& t = world.AddComponent<Transform>(e);
    t.position.x = x;
    t.position.y = y;
    auto& m = world.AddComponent<Movement>(e);
    m.speed = 60.0f;
    return e;
}

struct Keys {
    uint8_t state[SDL_NUM_SCANCODES] = {};
    const uint8_t* get() { return state; }
};

} // namespace

// 碰撞矩形按指纹缓存：无变化时结果稳定；SetPassage 触发版本号变化 → 重建
MG_TEST(player_movement_collision_cache) {
    World world;
    EntityID tm_e = MakeTilemap(world);
    MakePlayer(world, 16.0f, 16.0f);

    Keys keys;
    PlayerMovementSystem sys;

    sys.Update(1.0f / 60.0f, &world, keys.get());
    MG_EXPECT(sys.GetCollisionRects().empty()); // 无阻挡 → 无矩形
    MG_EXPECT(sys.HasMapBounds());

    // 设置阻挡 (1,1) → 缓存重建，出现 1 个矩形，覆盖该格
    auto* tm = world.GetComponent<Tilemap>(tm_e);
    tm->SetPassage(0, 1, 1, Pass_All);
    sys.Update(1.0f / 60.0f, &world, keys.get());
    MG_EXPECT_EQ(sys.GetCollisionRects().size(), (size_t)1);
    const auto& r = sys.GetCollisionRects()[0];
    MG_EXPECT_EQ(r.x0, 32.0f); MG_EXPECT_EQ(r.y0, 32.0f);
    MG_EXPECT_EQ(r.x1, 64.0f); MG_EXPECT_EQ(r.y1, 64.0f);

    // 再设置 (2,1) → 相邻同行合并为 1 个矩形（2 格宽）
    tm->SetPassage(0, 2, 1, Pass_All);
    sys.Update(1.0f / 60.0f, &world, keys.get());
    MG_EXPECT_EQ(sys.GetCollisionRects().size(), (size_t)1);
    MG_EXPECT_EQ(sys.GetCollisionRects()[0].x1, 96.0f);

    // 显式失效 → 重建结果一致（场景切换路径）
    sys.InvalidateCollisionCache();
    sys.Update(1.0f / 60.0f, &world, keys.get());
    MG_EXPECT_EQ(sys.GetCollisionRects().size(), (size_t)1);
}

// AABB 阻挡：玩家向右被实心格挡住；无输入位置不变；地图边界 clamp
MG_TEST(player_movement_blocking_and_clamp) {
    World world;
    EntityID tm_e = MakeTilemap(world);
    auto* tm = world.GetComponent<Tilemap>(tm_e);
    tm->SetPassage(0, 2, 1, Pass_All); // 实心格 x∈[64,96), y∈[32,64)

    EntityID player = MakePlayer(world, 48.0f, 48.0f); // tile (1,1) 中心
    auto* pt = world.GetComponent<Transform>(player);

    PlayerMovementSystem sys;
    const float dt = 1.0f / 60.0f;

    // 无输入：位置不变
    {
        Keys keys;
        sys.Update(dt, &world, keys.get());
        MG_EXPECT_EQ(pt->position.x, 48.0f);
        MG_EXPECT_EQ(pt->position.y, 48.0f);
    }

    // 按住 D 60 帧：应被挡在实心格左侧（x + HALF(14) <= 64）
    {
        Keys keys;
        keys.state[SDL_SCANCODE_D] = 1;
        for (int i = 0; i < 60; ++i) sys.Update(dt, &world, keys.get());
        MG_EXPECT(pt->position.x > 48.0f);          // 确实移动了
        MG_EXPECT(pt->position.x <= 50.01f);        // 但穿不过 64-14=50
        MG_EXPECT(pt->position.x + 14.0f <= 64.01f);
    }

    // 地图边界：按住 A 大量帧，玩家不会越出左边界（x >= 0+14）
    {
        Keys keys;
        keys.state[SDL_SCANCODE_A] = 1;
        for (int i = 0; i < 600; ++i) sys.Update(dt, &world, keys.get());
        MG_EXPECT(pt->position.x >= 14.0f - 1e-4f);
    }
}
