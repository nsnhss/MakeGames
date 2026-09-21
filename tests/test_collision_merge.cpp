#include "test_framework.h"
#include "scene/systems/collision_merge.h"

using namespace mg::scene;

static TileLayer MakeLayer(int w, int h) { return TileLayer("test", w, h); }

// 合并后矩形并集面积应等于实心 tile 总面积（等价性不变量）
static float TotalArea(const std::vector<CollisionRect>& rects) {
    float area = 0.0f;
    for (auto& r : rects) area += (r.x1 - r.x0) * (r.y1 - r.y0);
    return area;
}

MG_TEST(collision_merge_empty_layer) {
    auto layer = MakeLayer(8, 8);
    auto rects = MergeLayerCollisionRects(layer, 32, 32, 0, 0);
    MG_EXPECT(rects.empty());
}

MG_TEST(collision_merge_single_tile) {
    auto layer = MakeLayer(8, 8);
    layer.SetPassage(3, 2, Pass_All);
    auto rects = MergeLayerCollisionRects(layer, 32, 32, 0, 0);

    MG_EXPECT_EQ(rects.size(), 1u);
    MG_EXPECT_EQ(rects[0].x0, 96.0f);
    MG_EXPECT_EQ(rects[0].y0, 64.0f);
    MG_EXPECT_EQ(rects[0].x1, 128.0f);
    MG_EXPECT_EQ(rects[0].y1, 96.0f);
}

MG_TEST(collision_merge_block_2x2) {
    auto layer = MakeLayer(8, 8);
    for (int y = 2; y < 4; ++y)
        for (int x = 3; x < 5; ++x) layer.SetPassage(x, y, Pass_All);
    auto rects = MergeLayerCollisionRects(layer, 32, 32, 0, 0);

    MG_EXPECT_EQ(rects.size(), 1u);
    MG_EXPECT_EQ(rects[0].x0, 96.0f);
    MG_EXPECT_EQ(rects[0].y0, 64.0f);
    MG_EXPECT_EQ(rects[0].x1, 160.0f);
    MG_EXPECT_EQ(rects[0].y1, 128.0f);
}

MG_TEST(collision_merge_horizontal_and_vertical_runs) {
    // 水平条 10×1
    auto lh = MakeLayer(16, 4);
    for (int x = 0; x < 10; ++x) lh.SetPassage(x, 1, Pass_All);
    auto rh = MergeLayerCollisionRects(lh, 32, 32, 0, 0);
    MG_EXPECT_EQ(rh.size(), 1u);
    MG_EXPECT_EQ(rh[0].x1 - rh[0].x0, 320.0f);
    MG_EXPECT_EQ(rh[0].y1 - rh[0].y0, 32.0f);

    // 垂直条 1×10
    auto lv = MakeLayer(4, 16);
    for (int y = 0; y < 10; ++y) lv.SetPassage(2, y, Pass_All);
    auto rv = MergeLayerCollisionRects(lv, 32, 32, 0, 0);
    MG_EXPECT_EQ(rv.size(), 1u);
    MG_EXPECT_EQ(rv[0].x1 - rv[0].x0, 32.0f);
    MG_EXPECT_EQ(rv[0].y1 - rv[0].y0, 320.0f);
}

MG_TEST(collision_merge_l_shape) {
    // L 形：(0,0)(1,0)(0,1) → 2 rect
    auto layer = MakeLayer(4, 4);
    layer.SetPassage(0, 0, Pass_All);
    layer.SetPassage(1, 0, Pass_All);
    layer.SetPassage(0, 1, Pass_All);
    auto rects = MergeLayerCollisionRects(layer, 32, 32, 0, 0);

    MG_EXPECT_EQ(rects.size(), 2u);
    MG_EXPECT_EQ(TotalArea(rects), 3.0f * 32 * 32);
}

MG_TEST(collision_merge_ring_with_hole) {
    // 3×3 环（中心空）→ 4 rect（上条、下条、左柱、右柱）
    auto layer = MakeLayer(3, 3);
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 3; ++x)
            if (!(x == 1 && y == 1)) layer.SetPassage(x, y, Pass_All);
    auto rects = MergeLayerCollisionRects(layer, 32, 32, 0, 0);

    MG_EXPECT_EQ(rects.size(), 4u);
    MG_EXPECT_EQ(TotalArea(rects), 8.0f * 32 * 32);
}

MG_TEST(collision_merge_checkerboard_no_merge) {
    // 棋盘格：任何相邻都不同行同列连续 → 每格 1 rect
    auto layer = MakeLayer(4, 4);
    int solid = 0;
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x)
            if ((x + y) % 2 == 0) { layer.SetPassage(x, y, Pass_All); ++solid; }
    auto rects = MergeLayerCollisionRects(layer, 32, 32, 0, 0);

    MG_EXPECT_EQ(rects.size(), (size_t)solid);
}

MG_TEST(collision_merge_offset_and_scale) {
    auto layer = MakeLayer(4, 4);
    layer.SetPassage(1, 1, Pass_All);
    layer.SetPassage(2, 1, Pass_All);   // 水平 2 格
    auto rects = MergeLayerCollisionRects(layer, 32, 32, 100.0f, 200.0f, 2.0f, 2.0f);

    MG_EXPECT_EQ(rects.size(), 1u);
    MG_EXPECT_EQ(rects[0].x0, 100.0f + 32 * 2 * 1);
    MG_EXPECT_EQ(rects[0].y0, 200.0f + 32 * 2 * 1);
    MG_EXPECT_EQ(rects[0].x1, 100.0f + 32 * 2 * 3);
    MG_EXPECT_EQ(rects[0].y1, 200.0f + 32 * 2 * 2);
}
