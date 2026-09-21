#include "test_framework.h"
#include "resource/spritesheet_grid.h"
#include <cstdio>

using namespace mg;

// 整除：128x64 / 32x32 = 4x2 = 8 帧
MG_TEST(spritesheet_grid_exact) {
    auto g = ComputeSheetGrid(128, 64, 32, 32);
    MG_EXPECT_EQ(g.columns, 4);
    MG_EXPECT_EQ(g.rows, 2);
    MG_EXPECT_EQ(g.frame_count, 8);
}

// 不能整除时丢弃边缘余量：100x32 / 32 = 3 列
MG_TEST(spritesheet_grid_remainder_dropped) {
    auto g = ComputeSheetGrid(100, 32, 32, 32);
    MG_EXPECT_EQ(g.columns, 3);
    MG_EXPECT_EQ(g.rows, 1);
    MG_EXPECT_EQ(g.frame_count, 3);
}

// 帧大于图 / 非正输入 → 钳制 1x1
MG_TEST(spritesheet_grid_clamp) {
    auto g = ComputeSheetGrid(32, 32, 64, 64);
    MG_EXPECT_EQ(g.frame_count, 1);

    g = ComputeSheetGrid(0, 32, 32, 32);
    MG_EXPECT_EQ(g.frame_count, 1);
    g = ComputeSheetGrid(32, 32, 0, -1);
    MG_EXPECT_EQ(g.frame_count, 1);
}

// GetImageSize：解析最小合法 1x1 PNG；不存在文件返回 false
MG_TEST(spritesheet_image_size) {
    // 标准 1x1 透明 PNG（68 字节）
    static const unsigned char kPng1x1[] = {
        0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A, 0x00,0x00,0x00,0x0D,0x49,0x48,0x44,0x52,
        0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01, 0x08,0x06,0x00,0x00,0x00,0x1F,0x15,0xC4,
        0x89,0x00,0x00,0x00,0x0A,0x49,0x44,0x41, 0x54,0x78,0x9C,0x63,0x00,0x01,0x00,0x00,
        0x05,0x00,0x01,0x0D,0x0A,0x2D,0xB4,0x00, 0x00,0x00,0x00,0x49,0x45,0x4E,0x44,0xAE,
        0x42,0x60,0x82
    };
    const char* path = "test_tmp_1x1.png";
    {
        FILE* f = std::fopen(path, "wb");
        MG_EXPECT(f != nullptr);
        std::fwrite(kPng1x1, 1, sizeof(kPng1x1), f);
        std::fclose(f);
    }
    int w = 0, h = 0;
    MG_EXPECT(GetImageSize(path, w, h));
    MG_EXPECT_EQ(w, 1);
    MG_EXPECT_EQ(h, 1);
    std::remove(path);

    MG_EXPECT(!GetImageSize("definitely_not_exists_12345.png", w, h));
}
