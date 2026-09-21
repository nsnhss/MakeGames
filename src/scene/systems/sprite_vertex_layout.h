#pragma once
// ---------------------------------------------------------------------------
// Sprite 管线 vertex input 布局（editor / game 共享，消除两处重复）
// binding 0：quad 顶点（vec2 pos + vec2 uv）
// binding 1：instance 数据（SpriteInstance = 20 floats，含 atlas rect + pivot/flip）
// 与 assets/shaders/sprite.vert 的 location 声明一一对应，改动必须同步。
// ---------------------------------------------------------------------------
#include <vulkan/vulkan.h>
#include <array>

namespace mg {
namespace scene {

inline std::array<VkVertexInputBindingDescription, 2> GetSpriteVertexBindings()
{
    std::array<VkVertexInputBindingDescription, 2> bindings = {};
    bindings[0].binding = 0;
    bindings[0].stride = sizeof(float) * 4;   // vec2 pos + vec2 uv
    bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    bindings[1].binding = 1;
    bindings[1].stride = sizeof(float) * 20;  // SpriteInstance（含 vec4 atlas rect + pivot/flip）
    bindings[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
    return bindings;
}

inline std::array<VkVertexInputAttributeDescription, 9> GetSpriteVertexAttributes()
{
    std::array<VkVertexInputAttributeDescription, 9> attrs = {};
    attrs[0].location = 0; attrs[0].binding = 0; attrs[0].format = VK_FORMAT_R32G32_SFLOAT;       attrs[0].offset = 0;
    attrs[1].location = 1; attrs[1].binding = 0; attrs[1].format = VK_FORMAT_R32G32_SFLOAT;       attrs[1].offset = sizeof(float) * 2;
    attrs[2].location = 2; attrs[2].binding = 1; attrs[2].format = VK_FORMAT_R32G32_SFLOAT;       attrs[2].offset = sizeof(float) * 0;   // iWorldPos
    attrs[3].location = 3; attrs[3].binding = 1; attrs[3].format = VK_FORMAT_R32G32_SFLOAT;       attrs[3].offset = sizeof(float) * 2;   // iUVOffset
    attrs[4].location = 4; attrs[4].binding = 1; attrs[4].format = VK_FORMAT_R32G32_SFLOAT;       attrs[4].offset = sizeof(float) * 4;   // iFrameScale
    attrs[5].location = 5; attrs[5].binding = 1; attrs[5].format = VK_FORMAT_R32G32_SFLOAT;       attrs[5].offset = sizeof(float) * 6;   // iScale
    attrs[6].location = 6; attrs[6].binding = 1; attrs[6].format = VK_FORMAT_R32G32B32A32_SFLOAT; attrs[6].offset = sizeof(float) * 8;   // iColor
    attrs[7].location = 7; attrs[7].binding = 1; attrs[7].format = VK_FORMAT_R32G32B32A32_SFLOAT; attrs[7].offset = sizeof(float) * 12;  // iAtlasRect
    attrs[8].location = 8; attrs[8].binding = 1; attrs[8].format = VK_FORMAT_R32G32B32A32_SFLOAT; attrs[8].offset = sizeof(float) * 16;  // iPivotFlip (pivot.xy, flip 0/1)
    return attrs;
}

} // namespace scene
} // namespace mg
