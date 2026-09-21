#version 450

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;

layout(location = 2) in vec2 iWorldPos;
layout(location = 3) in vec2 iUVOffset;
layout(location = 4) in vec2 iFrameScale;
layout(location = 5) in vec2 iScale;
layout(location = 6) in vec4 iColor;
layout(location = 7) in vec4 iAtlasRect;  // (u0, v0, du, dv)，非图集传 (0,0,1,1)
layout(location = 8) in vec4 iPivotFlip;  // (pivot.x, pivot.y, flip_x, flip_y)

layout(binding = 0) uniform UniformBufferObject {
    mat4 vp;
} ubo;

layout(location = 0) out vec2 vUV;
layout(location = 1) out vec4 vColor;

void main() {
    // quad 几何为 ±16px（居中锚点）；pivot 偏离 0.5 时平移相应偏移量
    vec2 p = aPos - (iPivotFlip.xy - 0.5) * 32.0;
    vec4 world = vec4(p * iScale + iWorldPos, 0.0, 1.0);
    gl_Position = ubo.vp * world;
    vec2 uv = mix(aUV, 1.0 - aUV, iPivotFlip.zw);  // flip_x / flip_y
    vUV = iAtlasRect.xy + (uv * iFrameScale + iUVOffset) * iAtlasRect.zw;
    vColor = iColor;
}
