#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

struct SDL_Window;

namespace mg {
namespace renderer {

// Vulkan 渲染上下文：封装 Instance / PhysicalDevice / Device / Queue / DescriptorPool 的生命周期。
// 不负责 SwapChain（由 SwapChain 类独立管理），也不负责具体绘制命令。
// 使用方式：Initialize(window) → 获取 device/queue → 传给 SwapChain 和 ImGui backend。
class RendererContext {
public:
    RendererContext();
    ~RendererContext();

    bool Initialize(SDL_Window* window);
    void Shutdown();

    VkInstance GetInstance() const { return instance_; }
    VkPhysicalDevice GetPhysicalDevice() const { return physical_device_; }
    VkDevice GetDevice() const { return device_; }
    VkQueue GetQueue() const { return queue_; }
    uint32_t GetQueueFamily() const { return queue_family_; }
    VkDescriptorPool GetDescriptorPool() const { return descriptor_pool_; }
    VkPipelineCache GetPipelineCache() const { return pipeline_cache_; }
    VkAllocationCallbacks* GetAllocator() const { return allocator_; }
    // Wireframe（VK_POLYGON_MODE_LINE）是否可用（fillModeNonSolid 设备特性）
    bool IsWireframeSupported() const { return fill_mode_non_solid_; }

    static void CheckVkResult(VkResult err);

private:
    bool CreateInstance(const std::vector<const char*>& extensions);
    bool PickPhysicalDevice(VkSurfaceKHR surface);
    bool CreateLogicalDevice();
    bool CreateDescriptorPool();

    VkAllocationCallbacks* allocator_ = nullptr;
    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    bool fill_mode_non_solid_ = false;
    VkDevice device_ = VK_NULL_HANDLE;
    uint32_t queue_family_ = static_cast<uint32_t>(-1);
    VkQueue queue_ = VK_NULL_HANDLE;
    VkPipelineCache pipeline_cache_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;

#ifdef _DEBUG
    VkDebugReportCallbackEXT debug_report_ = VK_NULL_HANDLE;
#endif
};

} // namespace renderer
} // namespace mg
