#pragma once
#include "renderer_context.h"
#include <vulkan/vulkan.h>
#include <string>
#include <cstdint>
#include <vector>

namespace mg {
namespace renderer {

class Texture {
public:
    Texture() = default;
    ~Texture() { Destroy(); }

    // 移动语义
    Texture(Texture&& other) noexcept { *this = std::move(other); }
    Texture& operator=(Texture&& other) noexcept;

    // 禁止拷贝
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    // 从 PNG 文件加载
    bool LoadFromFile(RendererContext* ctx, const char* path);

    // 从内存 RGBA 数据创建
    bool CreateFromPixels(RendererContext* ctx,
                          const uint8_t* rgba, uint32_t w, uint32_t h,
                          VkSamplerAddressMode address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT);

    // 只加载像素到 CPU（不上 GPU）。供图集打包等场景使用。成功返回 true。
    static bool LoadPixelsFromFile(const char* path,
                                   std::vector<uint8_t>& out_rgba, int& w, int& h);

    VkImageView GetView() const { return view_; }
    VkSampler GetSampler() const { return sampler_; }
    uint32_t GetWidth() const { return width_; }
    uint32_t GetHeight() const { return height_; }

    void Destroy();
    bool IsValid() const { return image_ != VK_NULL_HANDLE; }

private:
    bool CreateImageAndView(RendererContext* ctx, uint32_t w, uint32_t h);
    bool CreateSampler(RendererContext* ctx, VkSamplerAddressMode address_mode);
    bool UploadPixels(RendererContext* ctx,
                      const uint8_t* rgba, uint32_t w, uint32_t h);

    RendererContext* ctx_ = nullptr;
    VkAllocationCallbacks* allocator_ = nullptr;

    VkImage image_ = VK_NULL_HANDLE;
    VkDeviceMemory image_memory_ = VK_NULL_HANDLE;
    VkImageView view_ = VK_NULL_HANDLE;
    VkSampler sampler_ = VK_NULL_HANDLE;

    uint32_t width_ = 0;
    uint32_t height_ = 0;
};

} // namespace renderer
} // namespace mg
