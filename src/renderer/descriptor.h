#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>

namespace mg {
namespace renderer {

// DescriptorSetLayout 封装：定义 shader 绑定接口
class DescriptorSetLayout {
public:
    DescriptorSetLayout() = default;
    ~DescriptorSetLayout() { Destroy(); }
    DescriptorSetLayout(const DescriptorSetLayout&) = delete;
    DescriptorSetLayout& operator=(const DescriptorSetLayout&) = delete;

    bool Create(VkDevice device, const VkDescriptorSetLayoutBinding* bindings, uint32_t count,
                VkAllocationCallbacks* allocator = nullptr);
    void Destroy();
    VkDescriptorSetLayout Get() const { return layout_; }
    bool IsValid() const { return layout_ != VK_NULL_HANDLE; }

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout layout_ = VK_NULL_HANDLE;
    VkAllocationCallbacks* allocator_ = nullptr;
};

// DescriptorSet 封装：从 pool 分配并写入 UBO/Texture 绑定
class DescriptorSet {
public:
    DescriptorSet() = default;

    bool Allocate(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout);

    // 写入 UBO/SSBO 绑定（type = UNIFORM_BUFFER / STORAGE_BUFFER）
    void WriteBuffer(uint32_t binding, VkBuffer buffer, VkDeviceSize size,
                     VkDescriptorType type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    // 写入纹理绑定（type = COMBINED_IMAGE_SAMPLER）
    void WriteImage(uint32_t binding, VkImageView view, VkSampler sampler);

    VkDescriptorSet Get() const { return set_; }
    bool IsValid() const { return set_ != VK_NULL_HANDLE; }

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkDescriptorSet set_ = VK_NULL_HANDLE;
};

} // namespace renderer
} // namespace mg
