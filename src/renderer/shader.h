#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>

namespace mg {
namespace renderer {

// Vulkan ShaderModule 封装
// 负责：从 SPIR-V 字节码创建 VkShaderModule，提供 Pipeline 所需的 StageInfo。
class Shader {
public:
    Shader() = default;
    ~Shader() { Destroy(); }

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    // 从 SPIR-V 字节码创建 ShaderModule
    bool Create(VkDevice device, const uint32_t* spv_data, size_t spv_size,
                VkAllocationCallbacks* allocator = nullptr);

    // 构造 Pipeline Shader Stage CreateInfo
    VkPipelineShaderStageCreateInfo StageInfo(VkShaderStageFlagBits stage) const;

    void Destroy();

    VkShaderModule Get() const { return module_; }
    bool IsValid() const { return module_ != VK_NULL_HANDLE; }

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkShaderModule module_ = VK_NULL_HANDLE;
    VkAllocationCallbacks* allocator_ = nullptr;
};

} // namespace renderer
} // namespace mg
