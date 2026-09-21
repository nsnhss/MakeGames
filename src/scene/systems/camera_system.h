#pragma once
#include <cstdint>

namespace mg {
namespace math { struct Vec2; }
namespace scene {

struct Camera;

class CameraSystem {
public:
    // 根据 Camera Component + Transform position 计算正交 VP 矩阵（列主序 mat4）
    // ortho_size = 半高（世界单位），匹配 Unity 语义
    // out_m16: float[16]，列主序，适配 GLSL std140 mat4
    static void ComputeViewProj(float* out_m16,
                                int viewport_w, int viewport_h,
                                const Camera& camera,
                                const math::Vec2& position);
};

} // namespace scene
} // namespace mg
