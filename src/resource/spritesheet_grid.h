#pragma once
// ---------------------------------------------------------------------------
// SpriteSheet 切分网格（纯逻辑，可单测）+ 图片尺寸读取（stb_image stbi_info）
// ---------------------------------------------------------------------------
#include <string>

namespace mg {

struct SpriteSheetGrid {
    int columns = 1;
    int rows = 1;
    int frame_count = 1;   // columns * rows
};

// 按帧尺寸切分整张图：整除取列/行数；除零、越界、非正输入 → 钳制为 1x1。
// 不能整除时丢弃边缘余量（与常见 spritesheet 工具一致）。
SpriteSheetGrid ComputeSheetGrid(int image_w, int image_h, int frame_w, int frame_h);

// 读取图片像素尺寸（stb_image stbi_info，不解码像素）；失败返回 false。
bool GetImageSize(const std::string& path, int& out_w, int& out_h);

} // namespace mg
