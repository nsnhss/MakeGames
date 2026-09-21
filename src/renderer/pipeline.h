#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>

namespace mg {
namespace renderer {

class RendererContext;
class Shader;

// Vulkan Graphics Pipeline 封装
// 构造函数一把梭，不接受运行时重配。所有状态在 Create() 时固化。
class Pipeline {
public:
    Pipeline() = default;
    ~Pipeline() { Destroy(); }

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    struct Config {
        VkRenderPass render_pass = VK_NULL_HANDLE;
        const Shader* vert = nullptr;
        const Shader* frag = nullptr;
        uint32_t subpass = 0;
        VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPolygonMode poly_mode = VK_POLYGON_MODE_FILL;
        VkCullModeFlags cull_mode = VK_CULL_MODE_BACK_BIT;
        VkFrontFace front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        // Vertex input layout
        const VkVertexInputBindingDescription* vertex_bindings = nullptr;
        uint32_t vertex_binding_count = 0;
        const VkVertexInputAttributeDescription* vertex_attributes = nullptr;
        uint32_t vertex_attribute_count = 0;

        // Dynamic states（如 VK_DYNAMIC_STATE_VIEWPORT / SCISSOR）
        const VkDynamicState* dynamic_states = nullptr;
        uint32_t dynamic_state_count = 0;

        // 静态 viewport / scissor（仅在未指定 dynamic states 时使用）
        float viewport_width = 0.0f;
        float viewport_height = 0.0f;

        // Descriptor set layouts
        const VkDescriptorSetLayout* descriptor_set_layouts = nullptr;
        uint32_t descriptor_set_layout_count = 0;

        // Push constant ranges
        const VkPushConstantRange* push_constant_ranges = nullptr;
        uint32_t push_constant_range_count = 0;
    };

    bool Create(RendererContext* ctx, const Config& config,
                VkAllocationCallbacks* allocator = nullptr);

    void Destroy();

    VkPipeline Get() const { return pipeline_; }
    VkPipelineLayout GetLayout() const { return layout_; }
    bool IsValid() const { return pipeline_ != VK_NULL_HANDLE; }

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkAllocationCallbacks* allocator_ = nullptr;
};

} // namespace renderer
} // namespace mg
