#include "collision_merge.h"

namespace mg {
namespace scene {

std::vector<CollisionRect> MergeLayerCollisionRects(
    const TileLayer& layer, float tile_w, float tile_h,
    float offset_x, float offset_y,
    float scale_x, float scale_y)
{
    std::vector<CollisionRect> out;
    if (layer.width <= 0 || layer.height <= 0) return out;

    // 活跃 run：上一行未收尾的水平段（格子坐标，y1 为开区间）
    struct Run { int x0, x1, y0, y1; };
    std::vector<Run> active;

    auto emit = [&](const Run& r) {
        out.push_back({
            offset_x + r.x0 * tile_w * scale_x,
            offset_y + r.y0 * tile_h * scale_y,
            offset_x + r.x1 * tile_w * scale_x,
            offset_y + r.y1 * tile_h * scale_y
        });
    };

    for (int y = 0; y < layer.height; ++y) {
        std::vector<Run> row_runs;
        int x = 0;
        while (x < layer.width) {
            if (layer.GetPassage(x, y) != Pass_All) { ++x; continue; }
            int x0 = x;
            while (x < layer.width && layer.GetPassage(x, y) == Pass_All) ++x;
            row_runs.push_back({ x0, x, y, y + 1 });
        }

        // 与活跃 run 按 x 区间匹配：相同则纵向延伸，否则收尾
        std::vector<Run> next_active;
        std::vector<bool> row_matched(row_runs.size(), false);
        for (auto& a : active) {
            bool matched = false;
            for (size_t i = 0; i < row_runs.size(); ++i) {
                if (!row_matched[i] && row_runs[i].x0 == a.x0 && row_runs[i].x1 == a.x1) {
                    row_runs[i].y0 = a.y0;      // 继承起点，纵向延伸
                    row_matched[i] = true;
                    matched = true;
                    break;
                }
            }
            if (!matched) emit(a);              // 无后继，收尾
        }
        for (size_t i = 0; i < row_runs.size(); ++i) {
            next_active.push_back(row_runs[i]);
        }
        active = std::move(next_active);
    }
    for (auto& a : active) emit(a);

    return out;
}

} // namespace scene
} // namespace mg
