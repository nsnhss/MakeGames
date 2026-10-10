// test_tilemap_mesh.cpp — BuildTilemapMesh 网格生成验证（editor/game 共用构建器）
// 背景：冒烟发现 demo 场景瓦片全黑不渲染，用本测试二分定位（网格生成 vs 渲染/纹理）。
#include "test_framework.h"
#include "scene/world.h"
#include "scene/scene.h"
#include "scene/scene_serializer.h"
#include "scene/components/transform.h"
#include "scene/components/tilemap.h"
#include "scene/systems/tilemap_mesh_builder.h"
#include <fstream>
#include <sstream>

using namespace mg::scene;

// 构造 20x15 地图：Ground 全铺 tile 0（含 34 格 tile 2），Buildings 稀疏 10/12/14
static void BuildDemoLikeTilemap(Tilemap& tm) {
    tm.tileset_name = "Overworld";
    tm.tile_width = 32;
    tm.tile_height = 32;
    TileLayer ground("Ground", 20, 15);   // tiles 默认 -1
    for (int i = 0; i < 300; ++i) ground.tiles[i] = 0;
    for (int i = 0; i < 34; ++i) ground.tiles[i * 3 % 300] = 2;
    TileLayer buildings("Buildings", 20, 15);
    buildings.tiles[0] = 10; buildings.tiles[1] = 12; buildings.tiles[2] = 14;
    tm.layers.push_back(std::move(ground));
    tm.layers.push_back(std::move(buildings));
}

static const TilesetRenderInfo* StubTileset(const std::string& name, int tw, int th) {
    static TilesetRenderInfo info;
    if (name != "Overworld") return nullptr;
    info.columns = 8;
    info.tile_width = tw;
    info.tile_height = th;
    info.start_index = 0;
    info.tile_count = 32;        // 8x4
    info.tex_width = 256.0f;
    info.tex_height = 128.0f;
    info.descriptor_set = (VkDescriptorSet)(uintptr_t)1;  // 非空即可（不出批次则判定失败）
    info.animations = nullptr;
    return &info;
}

MG_TEST(tilemap_mesh_produces_vertices) {
    World world;
    EntityID e = world.CreateEntity();
    world.AddComponent<Transform>(e);
    auto& tm = world.AddComponent<Tilemap>(e);
    BuildDemoLikeTilemap(tm);

    std::vector<TilemapVertex> verts;
    std::vector<TilemapBatch> batches;
    BuildTilemapMesh(&world, StubTileset, false, 0.0f, verts, batches);

    // Ground 300 格全在 [0,32) 内 → 300*6 顶点；Buildings 3 格 → 18 顶点
    MG_EXPECT_EQ(verts.size(), (300u + 3u) * 6u);
    MG_EXPECT_EQ(batches.size(), 2u);  // 每层一个批次
    MG_EXPECT(batches[0].ds == (VkDescriptorSet)(uintptr_t)1);
}

MG_TEST(tilemap_mesh_uv_of_tile2) {
    World world;
    EntityID e = world.CreateEntity();
    world.AddComponent<Transform>(e);
    auto& tm = world.AddComponent<Tilemap>(e);
    tm.tileset_name = "Overworld";
    tm.tile_width = 32; tm.tile_height = 32;
    TileLayer ground("Ground", 4, 4);
    for (int i = 0; i < 16; ++i) ground.tiles[i] = 2;  // tile 2: col=2,row=0
    tm.layers.push_back(std::move(ground));

    std::vector<TilemapVertex> verts;
    std::vector<TilemapBatch> batches;
    BuildTilemapMesh(&world, StubTileset, false, 0.0f, verts, batches);

    MG_EXPECT_EQ(verts.size(), 16u * 6u);
    // 首个顶点 UV：u0 = 2*32/256 = 0.25, v0 = 0
    MG_EXPECT(std::abs(verts[0].u - 0.25f) < 1e-5f);
    MG_EXPECT(std::abs(verts[0].v - 0.0f) < 1e-5f);
    // 位置：tile(0,0) 左上角世界原点
    MG_EXPECT(std::abs(verts[0].x) < 1e-5f && std::abs(verts[0].y) < 1e-5f);
}

MG_TEST(tilemap_mesh_skips_unknown_tileset_and_null_ds) {
    World world;
    EntityID e = world.CreateEntity();
    world.AddComponent<Transform>(e);
    auto& tm = world.AddComponent<Tilemap>(e);
    tm.tileset_name = "Missing";  // StubTileset 返回 nullptr → 整个 tilemap 跳过
    TileLayer ground("Ground", 4, 4);
    tm.layers.push_back(std::move(ground));

    std::vector<TilemapVertex> verts;
    std::vector<TilemapBatch> batches;
    BuildTilemapMesh(&world, StubTileset, false, 0.0f, verts, batches);
    MG_EXPECT(verts.empty());
    MG_EXPECT(batches.empty());
}

// 直接加载 demo 场景（冒烟黑屏定位）：反序列化后 Tilemap 组件必须存在且有层
MG_TEST(tilemap_mesh_demo_scene_deserializes) {
    const char* paths[] = {
        "../../demo_project/maps/Map001.json",   // ctest cwd = cmake-build-release
        "../demo_project/maps/Map001.json",
        "demo_project/maps/Map001.json",
    };
    std::string text;
    for (const char* p : paths) {
        std::ifstream f(p);
        if (f) { std::stringstream ss; ss << f.rdbuf(); text = ss.str(); break; }
    }
    if (text.empty()) return;  // 无 demo 项目环境（CI 打包）则跳过
    auto scene = SceneSerializer::Deserialize(nlohmann::json::parse(text));
    MG_EXPECT(scene != nullptr);
    auto* world = scene->GetWorld();
    int tilemap_count = 0, layer_count = 0;
    std::vector<EntityID> es;
    world->GetAllEntities(es);
    for (auto e : es) {
        if (auto* tm = world->GetComponent<Tilemap>(e)) {
            ++tilemap_count;
            layer_count += (int)tm->layers.size();
        }
    }
    MG_EXPECT_EQ(tilemap_count, 1);
    MG_EXPECT(layer_count >= 3);
}
