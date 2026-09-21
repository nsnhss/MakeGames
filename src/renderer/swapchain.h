#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>

struct SDL_Window;

namespace mg {
namespace renderer {

class RendererContext;

// Vulkan SwapChain 封装
// 管理：Surface、SwapChain、ImageViews、RenderPass、Framebuffers、
//       每帧的 CommandPool/CommandBuffer/Fence/Semaphore
class SwapChain {
public:
    struct FrameData {
        VkCommandPool   CommandPool   = VK_NULL_HANDLE;
        VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
        VkFence         Fence         = VK_NULL_HANDLE;
        VkFramebuffer   Framebuffer   = VK_NULL_HANDLE;
    };

    SwapChain() = default;
    ~SwapChain();

    // 禁止拷贝
    SwapChain(const SwapChain&) = delete;
    SwapChain& operator=(const SwapChain&) = delete;

    bool Initialize(SDL_Window* window, RendererContext* renderer,
                    VkAllocationCallbacks* allocator, uint32_t min_image_count = 2);
    void Destroy();

    // 窗口大小变化时重建
    void Resize(int width, int height);

    // 获取下一帧图像索引，返回 true 表示成功
    // 若返回 false，说明 SwapChain 需要重建（OUT_OF_DATE），调用方应跳过本帧渲染
    bool AcquireNextImage();

    // 开始当前帧的 CommandBuffer
    VkCommandBuffer BeginCommandBuffer();

    // 开始 RenderPass（会自动设置 ClearValue）
    void BeginRenderPass(VkCommandBuffer cmd, const VkClearValue* clear_value = nullptr);
    void EndRenderPass(VkCommandBuffer cmd);

    // 结束 CommandBuffer 并 Submit + Present
    void SubmitAndPresent(VkCommandBuffer cmd);

    // 请求重建（如收到窗口 RESIZED 事件时）
    void RequestRebuild() { rebuild_requested_ = true; }
    bool NeedsRebuild() const { return rebuild_requested_; }

    VkRenderPass GetRenderPass() const { return render_pass_; }
    VkSurfaceKHR GetSurface() const { return surface_; }
    VkExtent2D   GetExtent() const { return extent_; }
    VkFormat     GetFormat() const { return surface_format_.format; }
    uint32_t     GetImageCount() const { return image_count_; }
    uint32_t     GetCurrentFrameIndex() const { return current_frame_index_; }

    // 获取当前帧数据（用于外部直接访问）
    const FrameData& GetCurrentFrame() const { return frames_[current_frame_index_]; }

private:
    void CreateSurface(SDL_Window* window);
    void SelectSurfaceFormat();
    void SelectPresentMode();
    void CreateSwapChain(int width, int height);
    void CreateImageViews();
    void CreateRenderPass();
    void CreateFramebuffers();
    void CreateSyncObjects();
    void DestroyFramebuffers();
    void DestroyImageViews();
    void DestroySwapChain();
    void DestroySyncObjects();

    RendererContext* renderer_ = nullptr;
    SDL_Window* window_ = nullptr;
    VkAllocationCallbacks* allocator_ = nullptr;

    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkSurfaceFormatKHR surface_format_ = {};
    VkPresentModeKHR present_mode_ = VK_PRESENT_MODE_FIFO_KHR;

    VkRenderPass render_pass_ = VK_NULL_HANDLE;
    std::vector<VkImage> images_;
    std::vector<VkImageView> image_views_;
    std::vector<FrameData> frames_;
    std::vector<VkSemaphore> image_acquired_semaphores_;
    std::vector<VkSemaphore> render_complete_semaphores_;

    uint32_t current_frame_index_ = 0;
    uint32_t semaphore_index_ = 0;
    uint32_t image_count_ = 0;
    uint32_t min_image_count_ = 2;
    VkExtent2D extent_ = {};

    bool rebuild_requested_ = false;
    bool initialized_ = false;
};

} // namespace renderer
} // namespace mg
