#pragma once
// ---------------------------------------------------------------------------
// ViewportRenderTarget：编辑器 Viewport 离屏渲染目标（D3 从 EditorApplication 拆出）
// 自持 image/memory/view/framebuffer + ImGui 纹理 DS；
// render pass 跨 resize 复用（Destroy 不毁 pass，仅 shutdown 时 DestroyRenderPass）。
// ---------------------------------------------------------------------------
#include <vulkan/vulkan.h>
#include <cstdint>

namespace mg {
namespace renderer { class RendererContext; class SwapChain; }
namespace editor {

class ViewportRenderTarget {
public:
    // 尺寸/格式取自 swapchain 当前状态；重复调用前需先 Destroy
    void Create(renderer::RendererContext& renderer, renderer::SwapChain& swapchain,
                VkAllocationCallbacks* allocator);
    void Destroy();            // RT 资源（保留 render pass，供 resize 重建复用）
    void DestroyRenderPass();  // 仅 shutdown 时调用

    VkRenderPass GetRenderPass() const { return rp_; }
    VkFramebuffer GetFramebuffer() const { return fb_; }
    VkDescriptorSet GetImGuiDS() const { return imgui_ds_; }
    uint32_t GetWidth() const { return w_; }
    uint32_t GetHeight() const { return h_; }

private:
    renderer::RendererContext* renderer_ = nullptr;  // device/physical_device 来源
    VkAllocationCallbacks* allocator_ = nullptr;
    VkRenderPass rp_ = VK_NULL_HANDLE;
    VkImage image_ = VK_NULL_HANDLE;
    VkDeviceMemory image_memory_ = VK_NULL_HANDLE;
    VkImageView image_view_ = VK_NULL_HANDLE;
    VkFramebuffer fb_ = VK_NULL_HANDLE;
    VkDescriptorSet imgui_ds_ = VK_NULL_HANDLE;
    uint32_t w_ = 0;
    uint32_t h_ = 0;
};

} // namespace editor
} // namespace mg
