// buffer.cpp
// Buffer — Vulkan 缓冲区封装实现。
// 管理 VkBuffer + VkDeviceMemory 的创建/映射/销毁，支持 host-visible 直接上传。
#include "buffer.h"
#include "renderer_context.h"
#include "renderer_utils.h"
#include <stdio.h>
#include "src/core/log.h"

namespace mg {
namespace renderer {

bool Buffer::Create(RendererContext* ctx, VkDeviceSize size, VkBufferUsageFlags usage,
                    bool host_visible, VkAllocationCallbacks* allocator)
{
    ctx_ = ctx;
    device_ = ctx->GetDevice();
    size_ = size;
    host_visible_ = host_visible;
    allocator_ = allocator;

    VkBufferCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = size;
    info.usage = usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult err = vkCreateBuffer(device_, &info, allocator_, &buffer_);
    if (err != VK_SUCCESS) {
        buffer_ = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(device_, buffer_, &mem_reqs);

    VkMemoryPropertyFlags mem_props = host_visible
        ? (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    uint32_t mem_type = renderer::FindMemoryType(ctx->GetPhysicalDevice(), mem_reqs.memoryTypeBits, mem_props);
    if (mem_type == UINT32_MAX) {
        MG_LOG_ERROR("Failed to find suitable memory type!");
    }
    if (mem_type == UINT32_MAX) {
        vkDestroyBuffer(device_, buffer_, allocator_);
        buffer_ = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = mem_type;

    err = vkAllocateMemory(device_, &alloc_info, allocator_, &memory_);
    if (err != VK_SUCCESS) {
        memory_ = VK_NULL_HANDLE;
        vkDestroyBuffer(device_, buffer_, allocator_);
        buffer_ = VK_NULL_HANDLE;
        return false;
    }

    err = vkBindBufferMemory(device_, buffer_, memory_, 0);
    if (err != VK_SUCCESS) {
        vkFreeMemory(device_, memory_, allocator_);
        memory_ = VK_NULL_HANDLE;
        vkDestroyBuffer(device_, buffer_, allocator_);
        buffer_ = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

void* Buffer::Map()
{
    if (!host_visible_ || !memory_) return nullptr;
    void* data = nullptr;
    vkMapMemory(device_, memory_, 0, size_, 0, &data);
    return data;
}

void Buffer::Unmap()
{
    if (memory_) vkUnmapMemory(device_, memory_);
}

bool Buffer::Upload(const void* data, VkDeviceSize size)
{
    if (size > size_) size = size_;

    if (host_visible_) {
        void* mapped = Map();
        if (!mapped) return false;
        memcpy(mapped, data, static_cast<size_t>(size));
        Unmap();
        return true;
    }

    // 预留：设备本地 buffer 暂不支持直接上传，需要 staging buffer + command buffer 中转。
    // 当前所有 buffer 均使用 host-visible 内存类型，此路径暂不可达。
    MG_LOG_ERROR("Buffer::Upload for device-local buffer not yet implemented. Use host-visible.");
    return false;
}

void Buffer::Destroy()
{
    if (memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, memory_, allocator_);
        memory_ = VK_NULL_HANDLE;
    }
    if (buffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, buffer_, allocator_);
        buffer_ = VK_NULL_HANDLE;
    }
    size_ = 0;
}

} // namespace renderer
} // namespace mg
