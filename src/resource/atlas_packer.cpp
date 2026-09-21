#include "atlas_packer.h"
#include <cstring>

namespace mg {
namespace resource {

bool AtlasPacker::Add(const std::string& key, int w, int h, const uint8_t* rgba, float out_uv_rect[4])
{
    if (TryGetRect(key, out_uv_rect)) return true;   // 幂等
    if (!rgba || w <= 0 || h <= 0) return false;
    if (w > kSize - 2 * kPadding || h > kSize - 2 * kPadding) return false;

    if (!initialized_) {
        pixels_.assign((size_t)kSize * kSize * 4, 0);
        initialized_ = true;
    }

    // shelf 放置：每个条目占 (w + 2*padding) × (h + 2*padding)
    int cell_w = w + 2 * kPadding;
    int cell_h = h + 2 * kPadding;
    if (shelf_x_ + cell_w > kSize) {          // 换行
        shelf_y_ += shelf_h_;
        shelf_x_ = 0;
        shelf_h_ = 0;
    }
    if (shelf_y_ + cell_h > kSize) return false;  // 垂直空间耗尽

    int px = shelf_x_ + kPadding;             // 真实区域起点
    int py = shelf_y_ + kPadding;
    Blit(px, py, w, h, rgba);

    rects_[key] = { px, py, w, h };
    shelf_x_ += cell_w;
    if (cell_h > shelf_h_) shelf_h_ = cell_h;

    out_uv_rect[0] = (float)px / kSize;
    out_uv_rect[1] = (float)py / kSize;
    out_uv_rect[2] = (float)w / kSize;
    out_uv_rect[3] = (float)h / kSize;
    return true;
}

bool AtlasPacker::Contains(const std::string& key) const
{
    return rects_.find(key) != rects_.end();
}

bool AtlasPacker::TryGetRect(const std::string& key, float out_uv_rect[4]) const
{
    auto it = rects_.find(key);
    if (it == rects_.end()) return false;
    const Rect& r = it->second;
    out_uv_rect[0] = (float)r.x / kSize;
    out_uv_rect[1] = (float)r.y / kSize;
    out_uv_rect[2] = (float)r.w / kSize;
    out_uv_rect[3] = (float)r.h / kSize;
    return true;
}

void AtlasPacker::Blit(int px, int py, int w, int h, const uint8_t* rgba)
{
    // 拷贝真实区域
    for (int row = 0; row < h; ++row) {
        uint8_t* dst = pixels_.data() + ((size_t)(py + row) * kSize + px) * 4;
        std::memcpy(dst, rgba + (size_t)row * w * 4, (size_t)w * 4);
    }
    // 边缘克隆 padding：上下行 + 左右列 + 四角
    for (int p = 1; p <= kPadding; ++p) {
        // 上 / 下（含左右各扩 padding 的整行，含角）
        for (int col = -kPadding; col < w + kPadding; ++col) {
            int sx = px + (col < 0 ? 0 : (col >= w ? w - 1 : col));
            const uint8_t* top = pixels_.data() + ((size_t)py * kSize + sx) * 4;
            const uint8_t* bot = pixels_.data() + ((size_t)(py + h - 1) * kSize + sx) * 4;
            uint8_t* dt = pixels_.data() + ((size_t)(py - p) * kSize + px + col) * 4;
            uint8_t* db = pixels_.data() + ((size_t)(py + h - 1 + p) * kSize + px + col) * 4;
            std::memcpy(dt, top, 4);
            std::memcpy(db, bot, 4);
        }
        // 左 / 右（不含角，角已由上行处理）
        for (int row = 0; row < h; ++row) {
            const uint8_t* left  = pixels_.data() + ((size_t)(py + row) * kSize + px) * 4;
            const uint8_t* right = pixels_.data() + ((size_t)(py + row) * kSize + px + w - 1) * 4;
            uint8_t* dl = pixels_.data() + ((size_t)(py + row) * kSize + px - p) * 4;
            uint8_t* dr = pixels_.data() + ((size_t)(py + row) * kSize + px + w - 1 + p) * 4;
            std::memcpy(dl, left, 4);
            std::memcpy(dr, right, 4);
        }
    }
}

} // namespace resource
} // namespace mg
