#include "swapchain.h"
#include "renderer_context.h"
#include <SDL_vulkan.h>
#include <stdio.h>
#include <algorithm>
#include "src/core/log.h"

namespace mg {
namespace renderer {

SwapChain::~SwapChain()
{
    if (initialized_) Destroy();
}

// 初始化完整 SwapChain 管线：Surface → Format → PresentMode → SwapChain → ImageViews → RenderPass → Framebuffers → Sync
bool SwapChain::Initialize(SDL_Window* window, RendererContext* renderer,
                           VkAllocationCallbacks* allocator, uint32_t min_image_count)
{
    window_ = window;
    renderer_ = renderer;
    allocator_ = allocator;
    min_image_count_ = std::max(min_image_count, 2u);

    CreateSurface(window);
    if (surface_ == VK_NULL_HANDLE) return false;

    SelectSurfaceFormat();
    SelectPresentMode();

    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    CreateSwapChain(w, h);
    CreateImageViews();
    CreateRenderPass();
    CreateFramebuffers();
    CreateSyncObjects();

    initialized_ = true;
    return true;
}

// 逆序销毁所有 SwapChain 资源（先 wait idle），重置 initialized_ 标志
void SwapChain::Destroy()
{
    if (!initialized_) return;

    VkDevice device = renderer_->GetDevice();
    vkDeviceWaitIdle(device);

    DestroySyncObjects();
    DestroyFramebuffers();
    vkDestroyRenderPass(device, render_pass_, allocator_);
    render_pass_ = VK_NULL_HANDLE;
    DestroyImageViews();
    DestroySwapChain();
    vkDestroySurfaceKHR(renderer_->GetInstance(), surface_, allocator_);
    surface_ = VK_NULL_HANDLE;

    initialized_ = false;
}

// 窗口大小变化时重建 SwapChain：销毁旧 Framebuffer/ImageView/SwapChain → 按新尺寸重建
void SwapChain::Resize(int width, int height)
{
    if (!initialized_) return;

    VkDevice device = renderer_->GetDevice();
    vkDeviceWaitIdle(device);

    DestroyFramebuffers();
    DestroyImageViews();
    DestroySwapChain();

    CreateSwapChain(width, height);
    CreateImageViews();
    CreateFramebuffers();

    rebuild_requested_ = false;
}

// 获取下一帧可用图像，返回 false 表示 SwapChain 已过期需要跳帧重建
bool SwapChain::AcquireNextImage()
{
    if (rebuild_requested_) {
        int w, h;
        SDL_GetWindowSize(window_, &w, &h);
        Resize(w, h);
    }

    VkDevice device = renderer_->GetDevice();
    VkSemaphore image_acquired = image_acquired_semaphores_[semaphore_index_];

    VkResult err = vkAcquireNextImageKHR(device, swapchain_, UINT64_MAX,
                                         image_acquired, VK_NULL_HANDLE, &current_frame_index_);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
        rebuild_requested_ = true;
    }
    if (err == VK_ERROR_OUT_OF_DATE_KHR) {
        return false;
    }
    if (err != VK_SUBOPTIMAL_KHR)
        RendererContext::CheckVkResult(err);

    return true;
}

// 等待当前帧 Fence → Reset CommandPool → 开始记录 CommandBuffer（ONE_TIME_SUBMIT）
VkCommandBuffer SwapChain::BeginCommandBuffer()
{
    VkDevice device = renderer_->GetDevice();
    FrameData& fd = frames_[current_frame_index_];

    VkResult err = vkWaitForFences(device, 1, &fd.Fence, VK_TRUE, UINT64_MAX);
    RendererContext::CheckVkResult(err);
    err = vkResetFences(device, 1, &fd.Fence);
    RendererContext::CheckVkResult(err);

    err = vkResetCommandPool(device, fd.CommandPool, 0);
    RendererContext::CheckVkResult(err);

    VkCommandBufferBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err = vkBeginCommandBuffer(fd.CommandBuffer, &info);
    RendererContext::CheckVkResult(err);

    return fd.CommandBuffer;
}

// 开始 RenderPass，绑定当前帧的 Framebuffer，自动应用 ClearValue
void SwapChain::BeginRenderPass(VkCommandBuffer cmd, const VkClearValue* clear_value)
{
    VkRenderPassBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass = render_pass_;
    info.framebuffer = frames_[current_frame_index_].Framebuffer;
    info.renderArea.extent = extent_;
    info.clearValueCount = 1;
    info.pClearValues = clear_value;
    vkCmdBeginRenderPass(cmd, &info, VK_SUBPASS_CONTENTS_INLINE);
}

void SwapChain::EndRenderPass(VkCommandBuffer cmd)
{
    vkCmdEndRenderPass(cmd);
}

// 结束 CommandBuffer → Submit（等待 image_acquired，信号 render_complete）→ Present → 轮换 Semaphore 索引
void SwapChain::SubmitAndPresent(VkCommandBuffer cmd)
{
    VkDevice device = renderer_->GetDevice();
    FrameData& fd = frames_[current_frame_index_];
    VkSemaphore image_acquired = image_acquired_semaphores_[semaphore_index_];
    VkSemaphore render_complete = render_complete_semaphores_[semaphore_index_];

    VkResult err = vkEndCommandBuffer(cmd);
    RendererContext::CheckVkResult(err);

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &image_acquired;
    submit.pWaitDstStageMask = &wait_stage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &render_complete;

    err = vkQueueSubmit(renderer_->GetQueue(), 1, &submit, fd.Fence);
    RendererContext::CheckVkResult(err);

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &render_complete;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &swapchain_;
    present_info.pImageIndices = &current_frame_index_;

    err = vkQueuePresentKHR(renderer_->GetQueue(), &present_info);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
        rebuild_requested_ = true;
    if (err == VK_ERROR_OUT_OF_DATE_KHR) return;
    if (err != VK_SUBOPTIMAL_KHR)
        RendererContext::CheckVkResult(err);

    semaphore_index_ = (semaphore_index_ + 1) % image_count_;
}

// ── 内部辅助 ─────────────────────────────────────

// 通过 SDL 创建 Vulkan Surface，并验证物理设备是否支持 WSI
void SwapChain::CreateSurface(SDL_Window* window)
{
    if (SDL_Vulkan_CreateSurface(window, renderer_->GetInstance(), &surface_) == 0) {
        MG_LOG_ERROR("Failed to create Vulkan surface.");
        surface_ = VK_NULL_HANDLE;
        return;
    }

    VkBool32 supported = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(renderer_->GetPhysicalDevice(),
                                         renderer_->GetQueueFamily(), surface_, &supported);
    if (supported != VK_TRUE) {
        MG_LOG_ERROR("Error: no WSI support on physical device 0");
        vkDestroySurfaceKHR(renderer_->GetInstance(), surface_, allocator_);
        surface_ = VK_NULL_HANDLE;
    }
}

// 选择最优 Surface Format：优先 B8G8R8A8_UNORM，其次是 R8G8B8A8_UNORM
void SwapChain::SelectSurfaceFormat()
{
    const VkFormat requestFormats[] = {
        VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM
    };
    const VkColorSpaceKHR requestColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;

    uint32_t count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(renderer_->GetPhysicalDevice(), surface_, &count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(renderer_->GetPhysicalDevice(), surface_, &count, formats.data());

    if (count == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
        surface_format_ = { requestFormats[0], requestColorSpace };
        return;
    }

    for (const auto& req : requestFormats) {
        for (const auto& fmt : formats) {
            if (fmt.format == req && fmt.colorSpace == requestColorSpace) {
                surface_format_ = fmt;
                return;
            }
        }
    }
    surface_format_ = formats[0];
}

// 选择 PresentMode：优先 FIFO（VSync），保证在所有平台上可用
void SwapChain::SelectPresentMode()
{
    uint32_t count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(renderer_->GetPhysicalDevice(), surface_, &count, nullptr);
    std::vector<VkPresentModeKHR> modes(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(renderer_->GetPhysicalDevice(), surface_, &count, modes.data());

    for (auto mode : modes) {
        if (mode == VK_PRESENT_MODE_FIFO_KHR) {
            present_mode_ = mode;
            return;
        }
    }
    present_mode_ = modes.empty() ? VK_PRESENT_MODE_FIFO_KHR : modes[0];
}

// 创建 SwapChain：按窗口尺寸 clamp 到 SurfaceCapabilities 范围，设置 Exclusive 队列共享模式
void SwapChain::CreateSwapChain(int width, int height)
{
    VkPhysicalDevice pd = renderer_->GetPhysicalDevice();
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pd, surface_, &caps);

    extent_.width = std::clamp((uint32_t)width, caps.minImageExtent.width, caps.maxImageExtent.width);
    extent_.height = std::clamp((uint32_t)height, caps.minImageExtent.height, caps.maxImageExtent.height);

    image_count_ = min_image_count_;
    if (image_count_ < caps.minImageCount) image_count_ = caps.minImageCount;
    if (caps.maxImageCount > 0 && image_count_ > caps.maxImageCount) image_count_ = caps.maxImageCount;

    uint32_t queue_family = renderer_->GetQueueFamily();
    VkSwapchainCreateInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = surface_;
    info.minImageCount = image_count_;
    info.imageFormat = surface_format_.format;
    info.imageColorSpace = surface_format_.colorSpace;
    info.imageExtent = extent_;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = 1;
    info.pQueueFamilyIndices = &queue_family;
    info.preTransform = caps.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode = present_mode_;
    info.clipped = VK_TRUE;

    VkResult err = vkCreateSwapchainKHR(renderer_->GetDevice(), &info, allocator_, &swapchain_);
    RendererContext::CheckVkResult(err);

    uint32_t img_count = 0;
    vkGetSwapchainImagesKHR(renderer_->GetDevice(), swapchain_, &img_count, nullptr);
    images_.resize(img_count);
    vkGetSwapchainImagesKHR(renderer_->GetDevice(), swapchain_, &img_count, images_.data());
    image_count_ = img_count;
}

// 为每个 SwapChain Image 创建 2D Color ImageView（无 Mip，单层）
void SwapChain::CreateImageViews()
{
    image_views_.resize(image_count_);
    for (uint32_t i = 0; i < image_count_; ++i) {
        VkImageViewCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.image = images_[i];
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = surface_format_.format;
        info.components.r = VK_COMPONENT_SWIZZLE_R;
        info.components.g = VK_COMPONENT_SWIZZLE_G;
        info.components.b = VK_COMPONENT_SWIZZLE_B;
        info.components.a = VK_COMPONENT_SWIZZLE_A;
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.baseMipLevel = 0;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.baseArrayLayer = 0;
        info.subresourceRange.layerCount = 1;
        VkResult err = vkCreateImageView(renderer_->GetDevice(), &info, allocator_, &image_views_[i]);
        RendererContext::CheckVkResult(err);
    }
}

// 创建单 Subpass 的 RenderPass：Color Attachment Clear → Store，最终 Layout 为 PRESENT_SRC
void SwapChain::CreateRenderPass()
{
    VkAttachmentDescription attachment = {};
    attachment.format = surface_format_.format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment = {};
    color_attachment.attachment = 0;
    color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 1;
    info.pAttachments = &attachment;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dependency;

    VkResult err = vkCreateRenderPass(renderer_->GetDevice(), &info, allocator_, &render_pass_);
    RendererContext::CheckVkResult(err);
}

// 为每个 ImageView 创建 Framebuffer，绑定到 RenderPass
void SwapChain::CreateFramebuffers()
{
    frames_.resize(image_count_);
    for (uint32_t i = 0; i < image_count_; ++i) {
        VkFramebufferCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = render_pass_;
        info.attachmentCount = 1;
        info.pAttachments = &image_views_[i];
        info.width = extent_.width;
        info.height = extent_.height;
        info.layers = 1;
        VkResult err = vkCreateFramebuffer(renderer_->GetDevice(), &info, allocator_, &frames_[i].Framebuffer);
        RendererContext::CheckVkResult(err);
    }
}

// 创建每帧同步对象：Fence（已信号态）、CommandPool、CommandBuffer；
// 以及每帧 Semaphore：image_acquired（GPU 通知 CPU 哪张图可用）和 render_complete（CPU 通知 GPU 渲染完成）
void SwapChain::CreateSyncObjects()
{
    frames_.resize(image_count_);
    image_acquired_semaphores_.resize(image_count_);
    render_complete_semaphores_.resize(image_count_);

    VkDevice device = renderer_->GetDevice();
    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = renderer_->GetQueueFamily();

    for (uint32_t i = 0; i < image_count_; ++i) {
        // Fence
        VkResult err = vkCreateFence(device, &fence_info, allocator_, &frames_[i].Fence);
        RendererContext::CheckVkResult(err);

        // CommandPool
        err = vkCreateCommandPool(device, &pool_info, allocator_, &frames_[i].CommandPool);
        RendererContext::CheckVkResult(err);

        // CommandBuffer
        VkCommandBufferAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.commandPool = frames_[i].CommandPool;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = 1;
        err = vkAllocateCommandBuffers(device, &alloc_info, &frames_[i].CommandBuffer);
        RendererContext::CheckVkResult(err);
    }

    VkSemaphoreCreateInfo sem_info = {};
    sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    for (uint32_t i = 0; i < image_count_; ++i) {
        VkResult err = vkCreateSemaphore(device, &sem_info, allocator_, &image_acquired_semaphores_[i]);
        RendererContext::CheckVkResult(err);
        err = vkCreateSemaphore(device, &sem_info, allocator_, &render_complete_semaphores_[i]);
        RendererContext::CheckVkResult(err);
    }
}

// 销毁所有 Framebuffer（Resize 时复用）
void SwapChain::DestroyFramebuffers()
{
    VkDevice device = renderer_->GetDevice();
    for (auto& fd : frames_) {
        if (fd.Framebuffer) {
            vkDestroyFramebuffer(device, fd.Framebuffer, allocator_);
            fd.Framebuffer = VK_NULL_HANDLE;
        }
    }
}

// 销毁所有 ImageView 并清空 images_ / image_views_ 数组
void SwapChain::DestroyImageViews()
{
    VkDevice device = renderer_->GetDevice();
    for (auto& view : image_views_) {
        if (view) {
            vkDestroyImageView(device, view, allocator_);
        }
    }
    image_views_.clear();
    images_.clear();
}

// 销毁 SwapChain 句柄
void SwapChain::DestroySwapChain()
{
    if (swapchain_) {
        vkDestroySwapchainKHR(renderer_->GetDevice(), swapchain_, allocator_);
        swapchain_ = VK_NULL_HANDLE;
    }
}

// 销毁 Fence、CommandPool、CommandBuffer、Semaphore 并清空数组
void SwapChain::DestroySyncObjects()
{
    VkDevice device = renderer_->GetDevice();
    for (auto& fd : frames_) {
        if (fd.CommandPool) {
            vkFreeCommandBuffers(device, fd.CommandPool, 1, &fd.CommandBuffer);
            vkDestroyCommandPool(device, fd.CommandPool, allocator_);
        }
        if (fd.Fence) vkDestroyFence(device, fd.Fence, allocator_);
    }
    frames_.clear();

    for (auto& sem : image_acquired_semaphores_) {
        if (sem) vkDestroySemaphore(device, sem, allocator_);
    }
    for (auto& sem : render_complete_semaphores_) {
        if (sem) vkDestroySemaphore(device, sem, allocator_);
    }
    image_acquired_semaphores_.clear();
    render_complete_semaphores_.clear();
}

} // namespace renderer
} // namespace mg
