#include "renderer/sprite_pipeline_setup.h"
#include "renderer/renderer_context.h"
#include "renderer/shader.h"
#include "renderer/pipeline.h"
#include "renderer/buffer.h"
#include "renderer/descriptor.h"
#include "scene/constants.h"
#include "scene/systems/sprite_vertex_layout.h"

namespace mg {
namespace renderer {

void CreateSpriteUBOLayoutAndSet(RendererContext& renderer, VkAllocationCallbacks* allocator,
                                 DescriptorSetLayout& out_layout, DescriptorSet& out_set) {
    VkDescriptorSetLayoutBinding binding = {};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    out_layout.Create(renderer.GetDevice(), &binding, 1, allocator);
    out_set.Allocate(renderer.GetDevice(), renderer.GetDescriptorPool(), out_layout.Get());
}

void CreateSpriteUBOBuffer(RendererContext& renderer, VkAllocationCallbacks* allocator,
                           const DescriptorSet& set, Buffer& out_buffer) {
    out_buffer.Create(&renderer, sizeof(float) * 16, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true, allocator);
    if (set.IsValid()) {
        // const_cast: WriteBuffer 为既有非 const API；此处不修改 set 内容
        const_cast<DescriptorSet&>(set).WriteBuffer(0, out_buffer.Get(), sizeof(float) * 16);
    }
}

void CreateSpritePipeline(RendererContext& renderer, VkAllocationCallbacks* allocator,
                          VkRenderPass render_pass, Shader& vert, Shader& frag,
                          DescriptorSetLayout& ubo_layout, VkDescriptorSetLayout sampler_layout,
                          float viewport_w, float viewport_h, VkPolygonMode poly_mode,
                          Pipeline& out_pipeline) {
    auto bindings = scene::GetSpriteVertexBindings();
    auto attrs = scene::GetSpriteVertexAttributes();

    VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkDescriptorSetLayout layouts[2] = { ubo_layout.Get(), sampler_layout };

    Pipeline::Config cfg = {};
    cfg.render_pass = render_pass;
    cfg.vert = &vert;
    cfg.frag = &frag;
    cfg.vertex_bindings = bindings.data();
    cfg.vertex_binding_count = (uint32_t)bindings.size();
    cfg.vertex_attributes = attrs.data();
    cfg.vertex_attribute_count = (uint32_t)attrs.size();
    cfg.dynamic_states = dynamic_states;
    cfg.dynamic_state_count = 2;
    cfg.viewport_width = viewport_w;
    cfg.viewport_height = viewport_h;
    cfg.cull_mode = VK_CULL_MODE_NONE;
    cfg.poly_mode = poly_mode;
    cfg.descriptor_set_layouts = layouts;
    cfg.descriptor_set_layout_count = 2;
    // sprite.frag 声明了 ViewMode push constant（0=Lit 1=Unlit）；layout 必须包含该 range
    VkPushConstantRange pc_range = {};
    pc_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pc_range.offset = 0;
    pc_range.size = sizeof(int);
    cfg.push_constant_ranges = &pc_range;
    cfg.push_constant_range_count = 1;
    out_pipeline.Create(&renderer, cfg, allocator);
}

void CreateSpriteQuadVB(RendererContext& renderer, VkAllocationCallbacks* allocator,
                        Buffer& out_vb) {
    struct Vertex { float x, y, u, v; };
    const float kH = scene::kSpriteQuadHalfExtent;
    Vertex quad[6] = {
        {-kH, -kH, 0.0f, 0.0f}, { kH, -kH, 1.0f, 0.0f}, { kH,  kH, 1.0f, 1.0f},
        {-kH, -kH, 0.0f, 0.0f}, { kH,  kH, 1.0f, 1.0f}, {-kH,  kH, 0.0f, 1.0f},
    };
    if (out_vb.Create(&renderer, sizeof(quad), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, true, allocator)) {
        out_vb.Upload(quad, sizeof(quad));
    }
}

void CreateSpriteInstanceBuffers(RendererContext& renderer, VkAllocationCallbacks* allocator,
                                 Buffer& out_instance_vb, Buffer& out_tilemap_dummy_vb) {
    out_instance_vb.Create(&renderer, kMaxSpriteInstances * kSpriteInstanceStride,
                           VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, true, allocator);
    out_tilemap_dummy_vb.Create(&renderer, kSpriteInstanceStride,
                                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, true, allocator);
    // Identity instance data: no world offset, no UV offset, unit frame/scale, white color.
    float identity_instance[20] = {
        0.0f, 0.0f,   // iWorldPos
        0.0f, 0.0f,   // iUVOffset
        1.0f, 1.0f,   // iFrameScale
        1.0f, 1.0f,   // iScale
        1.0f, 1.0f, 1.0f, 1.0f,  // iColor
        0.0f, 0.0f, 1.0f, 1.0f,  // iAtlasRect（identity = 整图）
        0.5f, 0.5f,              // iPivotFlip: pivot 居中
        0.0f, 0.0f               // iPivotFlip: 不翻转
    };
    out_tilemap_dummy_vb.Upload(identity_instance, sizeof(identity_instance));
}

} // namespace renderer
} // namespace mg
