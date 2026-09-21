#include "test_framework.h"
#include "scene/systems/tile_animation.h"

using namespace mg::scene;

// 无动画 → 原样返回
MG_TEST(tile_anim_passthrough) {
    std::vector<TileAnimDef> anims;
    MG_EXPECT_EQ(ResolveAnimatedTile(anims, 42, 0, 1.0f), 42);

    // 动画存在但不命中该 tile
    TileAnimDef a; a.tile_index = 3; a.frames = {3, 4, 5}; a.fps = 4.0f;
    anims.push_back(a);
    MG_EXPECT_EQ(ResolveAnimatedTile(anims, 42, 0, 1.0f), 42);
}

// 帧循环回绕：fps=4 → 每 0.25s 一帧，3 帧循环
MG_TEST(tile_anim_frame_wrap) {
    std::vector<TileAnimDef> anims;
    TileAnimDef a; a.tile_index = 10; a.frames = {10, 11, 12}; a.fps = 4.0f;
    anims.push_back(a);

    MG_EXPECT_EQ(ResolveAnimatedTile(anims, 110, 100, 0.0f), 110);   // 帧0
    MG_EXPECT_EQ(ResolveAnimatedTile(anims, 110, 100, 0.25f), 111);  // 帧1
    MG_EXPECT_EQ(ResolveAnimatedTile(anims, 110, 100, 0.5f), 112);   // 帧2
    MG_EXPECT_EQ(ResolveAnimatedTile(anims, 110, 100, 0.75f), 110);  // 回绕帧0
    MG_EXPECT_EQ(ResolveAnimatedTile(anims, 110, 100, 1.0f), 111);   // 帧1
}

// 边界：fps<=0 / 空帧表 / 负时间 → 原样返回
MG_TEST(tile_anim_invalid_defs) {
    std::vector<TileAnimDef> anims;

    TileAnimDef zero_fps; zero_fps.tile_index = 1; zero_fps.frames = {2, 3}; zero_fps.fps = 0.0f;
    anims.push_back(zero_fps);
    TileAnimDef empty_frames; empty_frames.tile_index = 5; empty_frames.fps = 4.0f;
    anims.push_back(empty_frames);

    MG_EXPECT_EQ(ResolveAnimatedTile(anims, 101, 100, 1.0f), 101);
    MG_EXPECT_EQ(ResolveAnimatedTile(anims, 105, 100, 1.0f), 105);

    // 负时间按 0 处理
    TileAnimDef a; a.tile_index = 7; a.frames = {8, 9}; a.fps = 2.0f;
    anims.clear(); anims.push_back(a);
    MG_EXPECT_EQ(ResolveAnimatedTile(anims, 107, 100, -1.0f), 108);
}

// 多动画共存：各自独立解析；start_index 偏移正确
MG_TEST(tile_anim_multiple_and_offset) {
    std::vector<TileAnimDef> anims;
    TileAnimDef a; a.tile_index = 0; a.frames = {0, 1}; a.fps = 2.0f;
    TileAnimDef b; b.tile_index = 5; b.frames = {5, 6, 7}; b.fps = 3.0f;
    anims.push_back(a); anims.push_back(b);

    // t=0.5s：a 帧=(0.5*2)%2=1 → 201；b 帧=(0.5*3)%3=1 → 206
    MG_EXPECT_EQ(ResolveAnimatedTile(anims, 200, 200, 0.5f), 201);
    MG_EXPECT_EQ(ResolveAnimatedTile(anims, 205, 200, 0.5f), 206);
}

// 帧哈希：帧变化 → 哈希变化；无动画恒为 0
MG_TEST(tile_anim_frame_hash) {
    std::vector<TileAnimDef> anims;
    TileAnimDef a; a.tile_index = 3; a.frames = {3, 4}; a.fps = 2.0f;
    anims.push_back(a);

    uint32_t h0 = AnimFrameHash(anims, 0.0f);
    uint32_t h1 = AnimFrameHash(anims, 0.5f);   // 帧1
    uint32_t h2 = AnimFrameHash(anims, 1.0f);   // 回绕帧0
    MG_EXPECT(h0 != h1);
    MG_EXPECT(h0 == h2);

    std::vector<TileAnimDef> none;
    MG_EXPECT_EQ(AnimFrameHash(none, 3.0f), 0u);
}
