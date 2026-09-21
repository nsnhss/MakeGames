#include "test_framework.h"
#include "resource/atlas_packer.h"
#include <vector>
#include <cstring>

using namespace mg::resource;

static std::vector<uint8_t> MakePixels(int w, int h, uint8_t r, uint8_t g, uint8_t b) {
    std::vector<uint8_t> p((size_t)w * h * 4);
    for (size_t i = 0; i < (size_t)w * h; ++i) {
        p[i * 4 + 0] = r; p[i * 4 + 1] = g; p[i * 4 + 2] = b; p[i * 4 + 3] = 255;
    }
    return p;
}

// 基本打包：rect 在界内、UV 换算正确、不重叠
MG_TEST(atlas_packer_basic_rects) {
    AtlasPacker packer;
    float r1[4], r2[4];
    auto p1 = MakePixels(32, 32, 255, 0, 0);
    auto p2 = MakePixels(64, 16, 0, 255, 0);

    MG_EXPECT(packer.Add("a.png", 32, 32, p1.data(), r1));
    MG_EXPECT(packer.Add("b.png", 64, 16, p2.data(), r2));

    // UV 界内
    for (auto* r : { r1, r2 }) {
        MG_EXPECT(r[0] >= 0.0f && r[1] >= 0.0f);
        MG_EXPECT(r[0] + r[2] <= 1.0f);
        MG_EXPECT(r[1] + r[3] <= 1.0f);
    }
    // UV 尺寸与像素尺寸一致
    MG_EXPECT_EQ(r1[2], 32.0f / AtlasPacker::kSize);
    MG_EXPECT_EQ(r1[3], 32.0f / AtlasPacker::kSize);
    MG_EXPECT_EQ(r2[2], 64.0f / AtlasPacker::kSize);

    // 不重叠（像素域）
    int x1 = (int)(r1[0] * AtlasPacker::kSize), y1 = (int)(r1[1] * AtlasPacker::kSize);
    int x2 = (int)(r2[0] * AtlasPacker::kSize), y2 = (int)(r2[1] * AtlasPacker::kSize);
    bool overlap = x1 < x2 + 64 && x2 < x1 + 32 && y1 < y2 + 16 && y2 < y1 + 32;
    MG_EXPECT(!overlap);

    MG_EXPECT_EQ(packer.Count(), 2u);
    MG_EXPECT(packer.Contains("a.png"));
    MG_EXPECT(!packer.Contains("c.png"));
}

// padding：图集像素中，rect 外 1px 处应等于边缘像素颜色（边缘克隆）
MG_TEST(atlas_packer_padding_edge_clone) {
    AtlasPacker packer;
    float r[4];
    auto p = MakePixels(8, 8, 10, 20, 30);
    MG_EXPECT(packer.Add("edge.png", 8, 8, p.data(), r));

    int px = (int)(r[0] * AtlasPacker::kSize);
    int py = (int)(r[1] * AtlasPacker::kSize);
    const auto& pixels = packer.Pixels();

    // 真实区域左上角
    const uint8_t* inside = pixels.data() + ((size_t)py * AtlasPacker::kSize + px) * 4;
    MG_EXPECT_EQ(inside[0], 10);
    // 上方 padding（py-1）与左方 padding（px-1）均为边缘克隆
    const uint8_t* pad_top = pixels.data() + ((size_t)(py - 1) * AtlasPacker::kSize + px) * 4;
    const uint8_t* pad_left = pixels.data() + ((size_t)py * AtlasPacker::kSize + px - 1) * 4;
    MG_EXPECT_EQ(pad_top[0], 10);
    MG_EXPECT_EQ(pad_top[1], 20);
    MG_EXPECT_EQ(pad_left[2], 30);
    // padding 宽度为 2：py-2 也应是克隆色
    const uint8_t* pad_top2 = pixels.data() + ((size_t)(py - 2) * AtlasPacker::kSize + px) * 4;
    MG_EXPECT_EQ(pad_top2[0], 10);
}

// 超尺寸返回 false；重复 Add 幂等
MG_TEST(atlas_packer_overflow_and_idempotent) {
    AtlasPacker packer;
    float r[4];

    auto big = MakePixels(AtlasPacker::kSize, 4, 0, 0, 0);
    MG_EXPECT(!packer.Add("too_big.png", AtlasPacker::kSize, 4, big.data(), r));
    MG_EXPECT_EQ(packer.Count(), 0u);

    auto p = MakePixels(16, 16, 1, 2, 3);
    float r1[4], r2[4];
    MG_EXPECT(packer.Add("dup.png", 16, 16, p.data(), r1));
    MG_EXPECT(packer.Add("dup.png", 16, 16, p.data(), r2));
    MG_EXPECT_EQ(packer.Count(), 1u);
    MG_EXPECT_EQ(r1[0], r2[0]);
    MG_EXPECT_EQ(r1[1], r2[1]);

    // 空/非法输入
    MG_EXPECT(!packer.Add("null.png", 16, 16, nullptr, r));
    MG_EXPECT(!packer.Add("zero.png", 0, 16, p.data(), r));
}

// shelf 换行：两张超宽图，第二张换到新行（y 更大）
MG_TEST(atlas_packer_shelf_wrap) {
    AtlasPacker packer;
    float r1[4], r2[4];
    auto wide = MakePixels(1500, 10, 7, 7, 7);

    MG_EXPECT(packer.Add("w1.png", 1500, 10, wide.data(), r1));
    MG_EXPECT(packer.Add("w2.png", 1500, 10, wide.data(), r2));
    MG_EXPECT(r2[1] > r1[1]);   // 换行后 v0 更大
    MG_EXPECT(r2[1] + r2[3] <= 1.0f);
}
