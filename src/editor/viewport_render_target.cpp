#include "editor/viewport_render_target.h"
#include "renderer/renderer_context.h"
#include "renderer/renderer_utils.h"
#include "renderer/swapchain.h"
#include "backends/imgui_impl_vulkan.h"

namespace mg {
namespace editor {

void ViewportRenderTarget::Create(renderer::RendererContext& renderer, renderer::SwapChain& swapchain,
                                  VkAllocationCallbacks* allocator)
{
    renderer_ = &renderer;
    allocator_ = allocator;
    VkDevice device = renderer.GetDevice();
    VkPhysicalDevice pd = renderer.GetPhysicalDevice();
    if (device == VK_NULL_HANDLE) return;

    w_ = swapchain.GetExtent().width;
    h_ = swapchain.GetExtent().height;
    VkFormat format = swapchain.GetFormat();

    // 1. Create Image (COLOR_ATTACHMENT | SAMPLED)
    VkImageCreateInfo image_info = {};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = format;
    image_info.extent.width = w_;
    image_info.extent.height = h_;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkResult err = vkCreateImage(device, &image_info, allocator_, &image_);
    renderer::RendererContext::CheckVkResult(err);

    // 2. Allocate & Bind Memory
    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(device, image_, &mem_reqs);

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = renderer::FindMemoryType(pd, mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    err = vkAllocateMemory(device, &alloc_info, allocator_, &image_memory_);
    renderer::RendererContext::CheckVkResult(err);

    err = vkBindImageMemory(device, image_, image_memory_, 0);
    renderer::RendererContext::CheckVkResult(err);

    // 3. Create ImageView
    VkImageViewCreateInfo view_info = {};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image_;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.layerCount = 1;

    err = vkCreateImageView(device, &view_info, allocator_, &image_view_);
    renderer::RendererContext::CheckVkResult(err);

    // 4. Create RenderPass (once, reused across resizes)
    // 预留：若 swapchain format 变化（极少发生），需重建 render pass 以匹配新格式。
    if (rp_ == VK_NULL_HANDLE) {
        VkAttachmentDescription attachment = {};
        attachment.format = format;
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentReference color_ref = {};
        color_ref.attachment = 0;
        color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_ref;

        VkSubpassDependency dependency = {};
        dependency.srcSubpass = 0;
        dependency.dstSubpass = VK_SUBPASS_EXTERNAL;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        VkRenderPassCreateInfo rp_info = {};
        rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rp_info.attachmentCount = 1;
        rp_info.pAttachments = &attachment;
        rp_info.subpassCount = 1;
        rp_info.pSubpasses = &subpass;
        rp_info.dependencyCount = 1;
        rp_info.pDependencies = &dependency;

        err = vkCreateRenderPass(device, &rp_info, allocator_, &rp_);
        renderer::RendererContext::CheckVkResult(err);
    }

    // 5. Create Framebuffer
    VkFramebufferCreateInfo fb_info = {};
    fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb_info.renderPass = rp_;
    fb_info.attachmentCount = 1;
    fb_info.pAttachments = &image_view_;
    fb_info.width = w_;
    fb_info.height = h_;
    fb_info.layers = 1;

    err = vkCreateFramebuffer(device, &fb_info, allocator_, &fb_);
    renderer::RendererContext::CheckVkResult(err);

    // 6. Register with ImGui
    imgui_ds_ = ImGui_ImplVulkan_AddTexture(image_view_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void ViewportRenderTarget::Destroy()
{
    VkDevice device = renderer_ ? renderer_->GetDevice() : VK_NULL_HANDLE;
    if (device == VK_NULL_HANDLE) return;

    if (imgui_ds_ != VK_NULL_HANDLE) {
        ImGui_ImplVulkan_RemoveTexture(imgui_ds_);
        imgui_ds_ = VK_NULL_HANDLE;
    }
    if (fb_ != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device, fb_, allocator_);
        fb_ = VK_NULL_HANDLE;
    }
    if (image_view_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device, image_view_, allocator_);
        image_view_ = VK_NULL_HANDLE;
    }
    if (image_ != VK_NULL_HANDLE) {
        vkDestroyImage(device, image_, allocator_);
        image_ = VK_NULL_HANDLE;
    }
    if (image_memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device, image_memory_, allocator_);
        image_memory_ = VK_NULL_HANDLE;
    }

    w_ = 0;
    h_ = 0;
}

void ViewportRenderTarget::DestroyRenderPass()
{
    VkDevice device = renderer_ ? renderer_->GetDevice() : VK_NULL_HANDLE;
    if (device == VK_NULL_HANDLE) return;
    if (rp_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, rp_, allocator_);
        rp_ = VK_NULL_HANDLE;
    }
}

} // namespace editor
} // namespace mg
