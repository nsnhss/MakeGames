#include "camera_system.h"
#include "scene/components/camera.h"
#include "core/math/vec2.h"

namespace mg {
namespace scene {

void CameraSystem::ComputeViewProj(float* out_m16,
                                    int viewport_w, int viewport_h,
                                    const Camera& camera,
                                    const math::Vec2& position)
{
    float aspect = (float)viewport_w / (float)viewport_h;
    float half_w = camera.ortho_size * aspect;
    float half_h = camera.ortho_size;

    float sx = 1.0f / half_w;
    float sy = -1.0f / half_h;   // negative: flip world Y-up → NDC Y-down
    float sz = 1.0f / (camera.far_plane - camera.near_plane);

    // Column-major 4x4, matching GLSL std140 mat4 layout
    // Column 0
    out_m16[0]  = sx;
    out_m16[1]  = 0.0f;
    out_m16[2]  = 0.0f;
    out_m16[3]  = 0.0f;
    // Column 1
    out_m16[4]  = 0.0f;
    out_m16[5]  = sy;
    out_m16[6]  = 0.0f;
    out_m16[7]  = 0.0f;
    // Column 2
    out_m16[8]  = 0.0f;
    out_m16[9]  = 0.0f;
    out_m16[10] = sz;
    out_m16[11] = 0.0f;
    // Column 3
    out_m16[12] = -sx * position.x;
    out_m16[13] = -sy * position.y;
    out_m16[14] = 0.0f;
    out_m16[15] = 1.0f;
}

} // namespace scene
} // namespace mg
