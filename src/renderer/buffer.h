#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>

namespace mg {
namespace renderer {

class RendererContext;

// Vulkan Buffer 封装
// 支持 host-visible（CPU 可 Map）和 device-local（GPU 专用）两种模式。
// 小数据量（如顶点缓冲）建议用 host-visible，直接 Map/Unmap，不走 staging。
class Buffer {
public:
    Buffer() = default;
    ~Buffer() { Destroy(); }

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    // 创建 Buffer 并分配内存
    // @param host_visible  true = HOST_VISIBLE | HOST_COHERENT（适合小数据直接 Map）
    //                      false = DEVICE_LOCAL（需通过 staging buffer 上传）
    bool Create(RendererContext* ctx, VkDeviceSize size, VkBufferUsageFlags usage,
                bool host_visible, VkAllocationCallbacks* allocator = nullptr);

    // 仅对 host-visible buffer 有效
    void* Map();
    void Unmap();

    // 上传数据。若 host-visible 则直接 Map/Unmap；否则通过内部 staging buffer 上传。
    bool Upload(const void* data, VkDeviceSize size);

    VkBuffer Get() const { return buffer_; }
    VkDeviceSize GetSize() const { return size_; }
    bool IsValid() const { return buffer_ != VK_NULL_HANDLE; }

    void Destroy();

private:
    RendererContext* ctx_ = nullptr;
    VkDevice device_ = VK_NULL_HANDLE;
    VkBuffer buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory memory_ = VK_NULL_HANDLE;
    VkDeviceSize size_ = 0;
    bool host_visible_ = false;
    VkAllocationCallbacks* allocator_ = nullptr;
};

} // namespace renderer
} // namespace mg
