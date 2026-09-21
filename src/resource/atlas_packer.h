#pragma once
// ---------------------------------------------------------------------------
// 图集打包器（纯逻辑，不依赖 Vulkan，可单测）
// 固定 2048×2048 RGBA 单图集，shelf 算法按请求顺序水平铺，四周 2px 边缘
// 克隆 padding 防双线性采样串色（bleeding）。UV rect 指向 padding 内缩后
// 的真实区域。放不下（超尺寸或空间不足）返回 false，调用方走 fallback。
// ---------------------------------------------------------------------------
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace mg {
namespace resource {

class AtlasPacker {
public:
    static constexpr int kSize = 2048;    // 图集边长（像素）
    static constexpr int kPadding = 2;    // 边缘克隆宽度（像素）

    struct Rect { int x, y, w, h; };      // 像素坐标（真实区域，不含 padding）

    // 成功返回 true 并填 out_uv_rect = (u0, v0, du, dv)；重复 key 幂等直接返回已有 rect。
    // rgba 为 w*h*4 字节 RGBA 像素。放不下返回 false。
    bool Add(const std::string& key, int w, int h, const uint8_t* rgba, float out_uv_rect[4]);

    bool Contains(const std::string& key) const;
    bool TryGetRect(const std::string& key, float out_uv_rect[4]) const;

    const std::vector<uint8_t>& Pixels() const { return pixels_; }  // kSize²×4
    size_t Count() const { return rects_.size(); }

private:
    void Blit(int px, int py, int w, int h, const uint8_t* rgba);

    std::vector<uint8_t> pixels_;                  // 懒初始化 kSize*kSize*4
    std::unordered_map<std::string, Rect> rects_;
    int shelf_x_ = 0;       // 当前行 x 游标（含 padding 起点）
    int shelf_y_ = 0;       // 当前行 y
    int shelf_h_ = 0;       // 当前行高度（含 padding）
    bool initialized_ = false;
};

} // namespace resource
} // namespace mg
