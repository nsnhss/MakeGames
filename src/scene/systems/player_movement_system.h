#pragma once
// ---------------------------------------------------------------------------
// PlayerMovementSystem：玩家移动 + AABB 碰撞 + 地图边界 clamp（D2 从 GameRuntime 拆出）
//
// 性能修复（高危，已标注）：原实现每帧对每个 Tilemap 层全量重跑
// MergeLayerCollisionRects。现按 (Tilemap 指针, tile 尺寸, Transform, 层版本号)
// 指纹缓存合并结果，每帧仅比对指纹，变化时才重建；
// 场景切换由 GameRuntime 显式 InvalidateCollisionCache()。
// 碰撞矩形集合与原实现逐位等价（同一 MergeLayerCollisionRects 输出）。
// ---------------------------------------------------------------------------
#include "scene/entity.h"
#include "scene/systems/collision_merge.h"
#include <cstdint>
#include <vector>

namespace mg {
namespace scene {

class World;

class PlayerMovementSystem {
public:
    // keys = SDL_GetKeyboardState 结果（nullptr 时本帧不移动）。
    // 碰撞半宽用 kPlayerCollisionHalfExtent（略小于半格，避免卡缝）。
    void Update(float dt, World* world, const uint8_t* keys);

    // 场景切换/地图重载后调用（EntityID 可能被新场景复用）
    void InvalidateCollisionCache() { cache_valid_ = false; }

    // 测试/调试用：当前生效的碰撞矩形与地图边界
    const std::vector<CollisionRect>& GetCollisionRects() const { return collision_rects_; }
    bool HasMapBounds() const { return has_map_bounds_; }
    void GetMapBounds(float& min_x, float& min_y, float& max_x, float& max_y) const {
        min_x = map_min_x_; min_y = map_min_y_; max_x = map_max_x_; max_y = map_max_y_;
    }

private:
    struct Fingerprint {
        const void* tilemap_ptr = nullptr;
        int tile_w = 0, tile_h = 0;
        float ox = 0.0f, oy = 0.0f, sx = 1.0f, sy = 1.0f;
        std::vector<uint32_t> layer_versions;
        bool Equals(const void* ptr, int tw, int th,
                    float x, float y, float sx2, float sy2,
                    const std::vector<uint32_t>& versions) const {
            return tilemap_ptr == ptr && tile_w == tw && tile_h == th &&
                   ox == x && oy == y && sx == sx2 && sy == sy2 &&
                   layer_versions == versions;
        }
    };
    struct CacheEntry {
        EntityID entity = kInvalidEntity;
        Fingerprint fingerprint;
    };

    bool CacheMatches(World* world) const;   // 指纹逐条比对（迭代序确定）
    void RebuildCollisionCache(World* world);

    std::vector<CollisionRect> collision_rects_;
    std::vector<CacheEntry> cache_entries_;
    bool cache_valid_ = false;

    // 首个 Tilemap 实体推导的地图边界（随缓存重建）
    bool has_map_bounds_ = false;
    float map_min_x_ = 0.0f, map_min_y_ = 0.0f, map_max_x_ = 0.0f, map_max_y_ = 0.0f;
};

} // namespace scene
} // namespace mg
