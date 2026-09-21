#include "pipeline.h"
#include "renderer_context.h"
#include "shader.h"
#include <stdio.h>
#include "src/core/log.h"

namespace mg {
namespace renderer {

bool Pipeline::Create(RendererContext* ctx, const Config& config, VkAllocationCallbacks* allocator)
{
    device_ = ctx->GetDevice();
    allocator_ = allocator;

    if (!config.vert || !config.frag || !config.vert->IsValid() || !config.frag->IsValid()) {
        MG_LOG_ERROR("Pipeline::Create: invalid shaders.");
        return false;
    }

    // Shader stages
    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0] = config.vert->StageInfo(VK_SHADER_STAGE_VERTEX_BIT);
    stages[1] = config.frag->StageInfo(VK_SHADER_STAGE_FRAGMENT_BIT);

    // Vertex input
    VkPipelineVertexInputStateCreateInfo vertex_input = {};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = config.vertex_binding_count;
    vertex_input.pVertexBindingDescriptions = config.vertex_bindings;
    vertex_input.vertexAttributeDescriptionCount = config.vertex_attribute_count;
    vertex_input.pVertexAttributeDescriptions = config.vertex_attributes;

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo input_asm = {};
    input_asm.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_asm.topology = config.topology;
    input_asm.primitiveRestartEnable = VK_FALSE;

    // Viewport & Scissor
    VkPipelineViewportStateCreateInfo viewport_state = {};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    VkViewport viewport = {};
    viewport.width = config.viewport_width;
    viewport.height = config.viewport_height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.extent.width = static_cast<uint32_t>(config.viewport_width);
    scissor.extent.height = static_cast<uint32_t>(config.viewport_height);

    // If no dynamic states specified, use static viewport/scissor from config
    bool has_dynamic_viewport = false;
    bool has_dynamic_scissor = false;
    for (uint32_t i = 0; i < config.dynamic_state_count; ++i) {
        if (config.dynamic_states[i] == VK_DYNAMIC_STATE_VIEWPORT) has_dynamic_viewport = true;
        if (config.dynamic_states[i] == VK_DYNAMIC_STATE_SCISSOR) has_dynamic_scissor = true;
    }
    if (!has_dynamic_viewport || !has_dynamic_scissor) {
        viewport_state.pViewports = &viewport;
        viewport_state.pScissors = &scissor;
    }

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo raster = {};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.depthClampEnable = VK_FALSE;
    raster.rasterizerDiscardEnable = VK_FALSE;
    raster.polygonMode = config.poly_mode;
    raster.cullMode = config.cull_mode;
    raster.frontFace = config.front_face;
    raster.depthBiasEnable = VK_FALSE;
    raster.lineWidth = 1.0f;

    // Multisample
    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth/Stencil (disabled for First Triangle)
    VkPipelineDepthStencilStateCreateInfo depth_stencil = {};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

    // Color blend
    VkPipelineColorBlendAttachmentState blend_attachment = {};
    blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                                    | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo blend = {};
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1;
    blend.pAttachments = &blend_attachment;

    // Dynamic state
    VkPipelineDynamicStateCreateInfo dynamic_state = {};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = config.dynamic_state_count;
    dynamic_state.pDynamicStates = config.dynamic_states;

    // Pipeline layout
    VkPipelineLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = config.descriptor_set_layout_count;
    layout_info.pSetLayouts = config.descriptor_set_layouts;
    layout_info.pushConstantRangeCount = config.push_constant_range_count;
    layout_info.pPushConstantRanges = config.push_constant_ranges;
    VkResult err = vkCreatePipelineLayout(device_, &layout_info, allocator_, &layout_);
    if (err != VK_SUCCESS) {
        layout_ = VK_NULL_HANDLE;
        return false;
    }

    // Graphics pipeline
    VkGraphicsPipelineCreateInfo pipe_info = {};
    pipe_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipe_info.stageCount = 2;
    pipe_info.pStages = stages;
    pipe_info.pVertexInputState = &vertex_input;
    pipe_info.pInputAssemblyState = &input_asm;
    pipe_info.pViewportState = &viewport_state;
    pipe_info.pRasterizationState = &raster;
    pipe_info.pMultisampleState = &multisample;
    pipe_info.pDepthStencilState = &depth_stencil;
    pipe_info.pColorBlendState = &blend;
    pipe_info.pDynamicState = config.dynamic_state_count > 0 ? &dynamic_state : nullptr;
    pipe_info.layout = layout_;
    pipe_info.renderPass = config.render_pass;
    pipe_info.subpass = config.subpass;

    err = vkCreateGraphicsPipelines(device_, ctx->GetPipelineCache(), 1, &pipe_info, allocator_, &pipeline_);
    if (err != VK_SUCCESS) {
        vkDestroyPipelineLayout(device_, layout_, allocator_);
        layout_ = VK_NULL_HANDLE;
        pipeline_ = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

void Pipeline::Destroy()
{
    if (pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, pipeline_, allocator_);
        pipeline_ = VK_NULL_HANDLE;
    }
    if (layout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, layout_, allocator_);
        layout_ = VK_NULL_HANDLE;
    }
}

} // namespace renderer
} // namespace mg
