#include "shader.h"
#include "renderer_context.h"

namespace mg {
namespace renderer {

bool Shader::Create(VkDevice device, const uint32_t* spv_data, size_t spv_size,
                    VkAllocationCallbacks* allocator)
{
    device_ = device;
    allocator_ = allocator;

    VkShaderModuleCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = spv_size;
    info.pCode = spv_data;

    VkResult err = vkCreateShaderModule(device_, &info, allocator_, &module_);
    if (err != VK_SUCCESS) {
        module_ = VK_NULL_HANDLE;
        return false;
    }
    return true;
}

VkPipelineShaderStageCreateInfo Shader::StageInfo(VkShaderStageFlagBits stage) const
{
    VkPipelineShaderStageCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.stage = stage;
    info.module = module_;
    info.pName = "main";
    return info;
}

void Shader::Destroy()
{
    if (module_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device_, module_, allocator_);
        module_ = VK_NULL_HANDLE;
    }
}

} // namespace renderer
} // namespace mg
