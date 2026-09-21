#include "spritesheet_grid.h"
#include "third_party/stb_image.h"   // 声明模式；实现编译于 renderer/texture.cpp

namespace mg {

SpriteSheetGrid ComputeSheetGrid(int image_w, int image_h, int frame_w, int frame_h) {
    SpriteSheetGrid g;
    if (image_w <= 0 || image_h <= 0 || frame_w <= 0 || frame_h <= 0) return g;
    if (frame_w > image_w || frame_h > image_h) return g;
    g.columns = image_w / frame_w;
    g.rows = image_h / frame_h;
    g.frame_count = g.columns * g.rows;
    return g;
}

bool GetImageSize(const std::string& path, int& out_w, int& out_h) {
    int channels = 0;
    if (!stbi_info(path.c_str(), &out_w, &out_h, &channels)) return false;
    return out_w > 0 && out_h > 0;
}

} // namespace mg
