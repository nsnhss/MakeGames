#include "test_framework.h"
#include "scene/map_settings.h"
#include "scene/world.h"
#include "scene/components/tilemap.h"

using namespace mg::scene;

// ---- TileLayer::Resize 数据保留语义 ----

MG_TEST(tile_layer_resize_grow_preserves) {
    TileLayer layer("L", 3, 2);
    layer.SetTile(1, 0, 7);
    layer.SetPassage(2, 1, Pass_All);
    layer.SetTerrain(0, 1, 5);

    layer.Resize(5, 4);

    MG_EXPECT_EQ(layer.width, 5);
    MG_EXPECT_EQ(layer.height, 4);
    MG_EXPECT_EQ((int)layer.tiles.size(), 20);
    MG_EXPECT_EQ((int)layer.passages.size(), 20);
    MG_EXPECT_EQ((int)layer.terrain.size(), 20);
    // 原区域保留
    MG_EXPECT_EQ(layer.GetTile(1, 0), 7);
    MG_EXPECT_EQ(layer.GetPassage(2, 1), (uint8_t)Pass_All);
    MG_EXPECT_EQ(layer.GetTerrain(0, 1), (uint8_t)5);
    // 新增区域默认值
    MG_EXPECT_EQ(layer.GetTile(4, 3), -1);
    MG_EXPECT_EQ(layer.GetPassage(4, 3), (uint8_t)Pass_None);
    MG_EXPECT_EQ(layer.GetTerrain(3, 0), (uint8_t)0);
}

MG_TEST(tile_layer_resize_shrink_clips) {
    TileLayer layer("L", 5, 4);
    layer.SetTile(1, 0, 7);
    layer.SetTile(4, 3, 9);   // 裁剪区外

    layer.Resize(2, 1);

    MG_EXPECT_EQ(layer.width, 2);
    MG_EXPECT_EQ(layer.height, 1);
    MG_EXPECT_EQ((int)layer.tiles.size(), 2);
    MG_EXPECT_EQ(layer.GetTile(1, 0), 7);   // 重叠区保留
    // 被裁掉的数据访问越界返回默认
    MG_EXPECT_EQ(layer.GetTile(4, 3), -1);
}

// ---- ResizeAllTilemaps ----

MG_TEST(resize_all_tilemaps_multi_entity) {
    World world;
    EntityID a = world.CreateEntity();
    EntityID b = world.CreateEntity();
    EntityID c = world.CreateEntity();   // 无 Tilemap，不受影响
    (void)c;
    world.AddComponent<Tilemap>(a);
    world.AddComponent<Tilemap>(b);
    // 注意：引用须在全部 AddComponent 之后取（dense vector 可能重分配）
    auto& tm1 = *world.GetComponent<Tilemap>(a);
    auto& tm2 = *world.GetComponent<Tilemap>(b);
    tm1.layers.emplace_back("Ground", 4, 4);
    tm2.layers.emplace_back("Ground", 4, 4);
    tm2.layers.emplace_back("Upper", 4, 4);
    tm1.SetTile(0, 2, 2, 42);

    int affected = ResizeAllTilemaps(&world, 10, 8);

    MG_EXPECT_EQ(affected, 2);
    MG_EXPECT_EQ(tm1.layers[0].width, 10);
    MG_EXPECT_EQ(tm1.layers[0].height, 8);
    MG_EXPECT_EQ(tm2.layers[0].width, 10);
    MG_EXPECT_EQ(tm2.layers[1].height, 8);
    MG_EXPECT_EQ(tm1.GetTile(0, 2, 2), 42);   // 数据保留
}

MG_TEST(resize_all_tilemaps_empty_world_and_clamp) {
    World world;
    MG_EXPECT_EQ(ResizeAllTilemaps(&world, 10, 8), 0);   // 空 World
    MG_EXPECT_EQ(ResizeAllTilemaps(nullptr, 10, 8), 0);  // nullptr 安全

    EntityID e = world.CreateEntity();
    auto& tm = world.AddComponent<Tilemap>(e);
    tm.layers.emplace_back("Ground", 4, 4);
    ResizeAllTilemaps(&world, 0, -3);   // 钳制到 1
    MG_EXPECT_EQ(tm.layers[0].width, 1);
    MG_EXPECT_EQ(tm.layers[0].height, 1);
}

// ---- SyncAllTilemapTileSize ----

MG_TEST(sync_tilemap_tile_size) {
    World world;
    EntityID a = world.CreateEntity();
    EntityID b = world.CreateEntity();
    world.AddComponent<Tilemap>(a);
    world.AddComponent<Tilemap>(b);
    auto& tm1 = *world.GetComponent<Tilemap>(a);
    auto& tm2 = *world.GetComponent<Tilemap>(b);
    tm1.tile_width = tm1.tile_height = 32;
    tm2.tile_width = 16; tm2.tile_height = 24;

    int affected = SyncAllTilemapTileSize(&world, 48);

    MG_EXPECT_EQ(affected, 2);
    MG_EXPECT_EQ(tm1.tile_width, 48);
    MG_EXPECT_EQ(tm1.tile_height, 48);
    MG_EXPECT_EQ(tm2.tile_width, 48);
    MG_EXPECT_EQ(tm2.tile_height, 48);

    // 钳制 + 空指针安全
    SyncAllTilemapTileSize(&world, 0);
    MG_EXPECT_EQ(tm1.tile_width, 1);
    MG_EXPECT_EQ(SyncAllTilemapTileSize(nullptr, 8), 0);
}
