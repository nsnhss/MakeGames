#include "descriptor.h"

namespace mg {
namespace renderer {

bool DescriptorSetLayout::Create(VkDevice device, const VkDescriptorSetLayoutBinding* bindings,
                                  uint32_t count, VkAllocationCallbacks* allocator)
{
    device_ = device;
    allocator_ = allocator;

    VkDescriptorSetLayoutCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = count;
    info.pBindings = bindings;

    VkResult err = vkCreateDescriptorSetLayout(device_, &info, allocator_, &layout_);
    if (err != VK_SUCCESS) {
        layout_ = VK_NULL_HANDLE;
        return false;
    }
    return true;
}

void DescriptorSetLayout::Destroy()
{
    if (layout_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, layout_, allocator_);
        layout_ = VK_NULL_HANDLE;
    }
}

bool DescriptorSet::Allocate(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout)
{
    device_ = device;

    VkDescriptorSetAllocateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.descriptorPool = pool;
    info.descriptorSetCount = 1;
    info.pSetLayouts = &layout;

    VkResult err = vkAllocateDescriptorSets(device_, &info, &set_);
    if (err != VK_SUCCESS) {
        set_ = VK_NULL_HANDLE;
        return false;
    }
    return true;
}

void DescriptorSet::WriteBuffer(uint32_t binding, VkBuffer buffer, VkDeviceSize size,
                                 VkDescriptorType type)
{
    VkDescriptorBufferInfo buffer_info = {};
    buffer_info.buffer = buffer;
    buffer_info.offset = 0;
    buffer_info.range = size;

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set_;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorType = type;
    write.descriptorCount = 1;
    write.pBufferInfo = &buffer_info;

    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
}

void DescriptorSet::WriteImage(uint32_t binding, VkImageView view, VkSampler sampler)
{
    VkDescriptorImageInfo image_info = {};
    image_info.sampler = sampler;
    image_info.imageView = view;
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set_;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &image_info;

    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
}

} // namespace renderer
} // namespace mg
