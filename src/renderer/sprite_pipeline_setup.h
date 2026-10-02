#pragma once
// ---------------------------------------------------------------------------
// Sprite 渲染管线共享创建样板（editor 与 game.exe 共用，D0-5 合并）
// 两者原本各自手写同一份：UBO layout/buffer、pipeline config、±16 quad VB、
// 实例缓冲 + tilemap dummy identity 实例。此处收敛为唯一实现。
// ---------------------------------------------------------------------------
#include <vulkan/vulkan.h>
#include <cstdint>

namespace mg {
namespace renderer {

class RendererContext;
class Shader;
class Pipeline;
class Buffer;
class DescriptorSetLayout;
class DescriptorSet;

// sprite 实例缓冲容量 / 步长（host-visible，editor 与 game 一致）
inline constexpr uint64_t kMaxSpriteInstances = 4096;
inline constexpr uint64_t kSpriteInstanceStride = sizeof(float) * 20;

// UBO layout（binding 0 / uniform / vertex 阶段）+ 从 renderer 描述符池分配 set
void CreateSpriteUBOLayoutAndSet(RendererContext& renderer, VkAllocationCallbacks* allocator,
                                 DescriptorSetLayout& out_layout, DescriptorSet& out_set);

// 16-float（mat4 view-proj）UBO buffer；若 set 有效则写入绑定
void CreateSpriteUBOBuffer(RendererContext& renderer, VkAllocationCallbacks* allocator,
                           const DescriptorSet& set, Buffer& out_buffer);

// sprite/tilemap 共享管线：viewport+scissor 动态、cull none、
// descriptor layout = {ubo_layout, sampler_layout}、ViewMode push constant（fragment int）。
// poly_mode 传 VK_POLYGON_MODE_LINE 可得线框变体（需 fillModeNonSolid 特性）。
void CreateSpritePipeline(RendererContext& renderer, VkAllocationCallbacks* allocator,
                          VkRenderPass render_pass, Shader& vert, Shader& frag,
                          DescriptorSetLayout& ubo_layout, VkDescriptorSetLayout sampler_layout,
                          float viewport_w, float viewport_h, VkPolygonMode poly_mode,
                          Pipeline& out_pipeline);

// 单位 quad VB（±scene::kSpriteQuadHalfExtent，6 顶点两三角形）
void CreateSpriteQuadVB(RendererContext& renderer, VkAllocationCallbacks* allocator,
                        Buffer& out_vb);

// 实例 VB（kMaxSpriteInstances 容量）+ tilemap dummy 实例 VB（写入 identity 实例，
// 防止 tilemap 共用管线时读到残留 sprite 实例数据）
void CreateSpriteInstanceBuffers(RendererContext& renderer, VkAllocationCallbacks* allocator,
                                 Buffer& out_instance_vb, Buffer& out_tilemap_dummy_vb);

} // namespace renderer
} // namespace mg
