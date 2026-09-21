#version 450

layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vColor;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D texSampler;

// 视口 ViewMode：0=Lit（纹理×tint，默认） 1=Unlit（忽略 tint，输出纯纹理原色）
layout(push_constant) uniform ViewModePC {
    int mode;
} pc;

void main() {
    vec4 tex = texture(texSampler, vUV);
    outColor = (pc.mode == 1) ? tex : tex * vColor;
}
