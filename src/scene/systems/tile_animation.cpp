#include "tile_animation.h"

namespace mg {
namespace scene {

namespace {

// 当前帧序号；动画无效（fps<=0 / 空帧表）返回 -1
int AnimFrameAt(const TileAnimDef& anim, float time_sec) {
    if (anim.fps <= 0.0f || anim.frames.empty()) return -1;
    if (time_sec < 0.0f) time_sec = 0.0f;
    int n = (int)anim.frames.size();
    return (int)(time_sec * anim.fps) % n;
}

} // namespace

int ResolveAnimatedTile(const std::vector<TileAnimDef>& anims,
                        int global_tile, int start_index, float time_sec) {
    int local = global_tile - start_index;
    for (const auto& anim : anims) {
        if (anim.tile_index != local) continue;
        int f = AnimFrameAt(anim, time_sec);
        if (f < 0) return global_tile;
        return start_index + anim.frames[f];
    }
    return global_tile;
}

uint32_t AnimFrameHash(const std::vector<TileAnimDef>& anims, float time_sec) {
    uint32_t h = 0;
    for (const auto& anim : anims) {
        int f = AnimFrameAt(anim, time_sec);
        if (f < 0) continue;
        h = h * 31u + (uint32_t)anim.tile_index;
        h = h * 31u + (uint32_t)f;
    }
    return h;
}

} // namespace scene
} // namespace mg
