#include "texture.h"
#include "renderer_utils.h"
#include "buffer.h"
#include <cstring>
#include <fstream>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"

namespace {

#ifdef _WIN32
static std::wstring Utf8ToWide(const char* utf8) {
    if (!utf8 || !utf8[0]) return {};
    int len = ::MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    if (len <= 0) return {};
    std::wstring w(len - 1, 0);
    ::MultiByteToWideChar(CP_UTF8, 0, utf8, -1, w.data(), len);
    return w;
}
#endif

static std::vector<uint8_t> ReadFileBytes(const char* path) {
#ifdef _WIN32
    // 用 Win32 宽字符 API 打开文件，避免 MinGW libstdc++ wifstream 对 UTF-8 中文路径的兼容性问题
    std::wstring wpath = Utf8ToWide(path);
    HANDLE h = ::CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return {};
    LARGE_INTEGER size_li;
    if (!::GetFileSizeEx(h, &size_li)) {
        ::CloseHandle(h);
        return {};
    }
    size_t size = static_cast<size_t>(size_li.QuadPart);
    std::vector<uint8_t> buffer(size);
    DWORD read = 0;
    BOOL ok = ::ReadFile(h, buffer.data(), static_cast<DWORD>(size), &read, nullptr);
    ::CloseHandle(h);
    if (!ok || read != size) return {};
    return buffer;
#else
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return {};
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) return {};
    return buffer;
#endif
}

} // namespace

namespace mg {
namespace renderer {

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        Destroy();
        ctx_ = other.ctx_;
        allocator_ = other.allocator_;
        image_ = other.image_;
        image_memory_ = other.image_memory_;
        view_ = other.view_;
        sampler_ = other.sampler_;
        width_ = other.width_;
        height_ = other.height_;
        other.image_ = VK_NULL_HANDLE;
        other.image_memory_ = VK_NULL_HANDLE;
        other.view_ = VK_NULL_HANDLE;
        other.sampler_ = VK_NULL_HANDLE;
    }
    return *this;
}

void Texture::Destroy() {
    if (!ctx_) return;
    VkDevice device = ctx_->GetDevice();
    if (sampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device, sampler_, allocator_);
        sampler_ = VK_NULL_HANDLE;
    }
    if (view_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device, view_, allocator_);
        view_ = VK_NULL_HANDLE;
    }
    if (image_ != VK_NULL_HANDLE) {
        vkDestroyImage(device, image_, allocator_);
        image_ = VK_NULL_HANDLE;
    }
    if (image_memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device, image_memory_, allocator_);
        image_memory_ = VK_NULL_HANDLE;
    }
    width_ = 0;
    height_ = 0;
}

bool Texture::LoadFromFile(RendererContext* ctx, const char* path) {
    auto buffer = ReadFileBytes(path);
    if (buffer.empty()) return false;

    int w = 0, h = 0, channels = 0;
    stbi_uc* pixels = stbi_load_from_memory(
        buffer.data(), static_cast<int>(buffer.size()),
        &w, &h, &channels, STBI_rgb_alpha);
    if (!pixels) return false;
    bool ok = CreateFromPixels(ctx, pixels, static_cast<uint32_t>(w), static_cast<uint32_t>(h));
    stbi_image_free(pixels);
    return ok;
}

bool Texture::LoadPixelsFromFile(const char* path,
                                 std::vector<uint8_t>& out_rgba, int& w, int& h) {
    auto buffer = ReadFileBytes(path);
    if (buffer.empty()) return false;

    int channels = 0;
    stbi_uc* pixels = stbi_load_from_memory(
        buffer.data(), static_cast<int>(buffer.size()),
        &w, &h, &channels, STBI_rgb_alpha);
    if (!pixels) return false;
    out_rgba.assign(pixels, pixels + (size_t)w * h * 4);
    stbi_image_free(pixels);
    return true;
}

bool Texture::CreateFromPixels(RendererContext* ctx,
                               const uint8_t* rgba, uint32_t w, uint32_t h,
                               VkSamplerAddressMode address_mode) {
    Destroy();
    ctx_ = ctx;
    allocator_ = ctx->GetAllocator();
    if (!CreateImageAndView(ctx, w, h)) return false;
    if (!CreateSampler(ctx, address_mode)) return false;
    if (!UploadPixels(ctx, rgba, w, h)) return false;
    width_ = w;
    height_ = h;
    return true;
}

bool Texture::CreateImageAndView(RendererContext* ctx, uint32_t w, uint32_t h) {
    VkDevice device = ctx->GetDevice();

    VkImageCreateInfo image_info = {};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    image_info.extent.width = w;
    image_info.extent.height = h;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult err = vkCreateImage(device, &image_info, allocator_, &image_);
    if (err != VK_SUCCESS) return false;

    VkMemoryRequirements mem_req;
    vkGetImageMemoryRequirements(device, image_, &mem_req);

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_req.size;
    alloc_info.memoryTypeIndex = FindMemoryType(ctx->GetPhysicalDevice(),
        mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    err = vkAllocateMemory(device, &alloc_info, allocator_, &image_memory_);
    if (err != VK_SUCCESS) return false;

    vkBindImageMemory(device, image_, image_memory_, 0);

    VkImageViewCreateInfo view_info = {};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image_;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    err = vkCreateImageView(device, &view_info, allocator_, &view_);
    if (err != VK_SUCCESS) return false;

    return true;
}

bool Texture::CreateSampler(RendererContext* ctx, VkSamplerAddressMode address_mode) {
    VkDevice device = ctx->GetDevice();
    VkSamplerCreateInfo sampler_info = {};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.addressModeU = address_mode;
    sampler_info.addressModeV = address_mode;
    sampler_info.addressModeW = address_mode;
    sampler_info.anisotropyEnable = VK_FALSE;
    sampler_info.maxAnisotropy = 1.0f;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.mipLodBias = 0.0f;
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = 0.0f;

    VkResult err = vkCreateSampler(device, &sampler_info, allocator_, &sampler_);
    return err == VK_SUCCESS;
}

bool Texture::UploadPixels(RendererContext* ctx,
                           const uint8_t* rgba, uint32_t w, uint32_t h) {
    VkDevice device = ctx->GetDevice();
    VkQueue queue = ctx->GetQueue();
    uint32_t queue_family = ctx->GetQueueFamily();

    VkDeviceSize image_size = static_cast<VkDeviceSize>(w * h * 4);

    // Staging buffer
    Buffer staging;
    if (!staging.Create(ctx, image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true, allocator_)) {
        return false;
    }
    void* mapped = staging.Map();
    std::memcpy(mapped, rgba, static_cast<size_t>(image_size));
    staging.Unmap();

    // Temporary command pool + buffer
    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = queue_family;
    pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

    VkCommandPool temp_pool = VK_NULL_HANDLE;
    vkCreateCommandPool(device, &pool_info, allocator_, &temp_pool);

    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = temp_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(device, &alloc_info, &cmd);

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin_info);

    // Barrier: UNDEFINED -> TRANSFER_DST_OPTIMAL
    VkImageMemoryBarrier barrier1 = {};
    barrier1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier1.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier1.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier1.image = image_;
    barrier1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier1.subresourceRange.baseMipLevel = 0;
    barrier1.subresourceRange.levelCount = 1;
    barrier1.subresourceRange.baseArrayLayer = 0;
    barrier1.subresourceRange.layerCount = 1;
    barrier1.srcAccessMask = 0;
    barrier1.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier1);

    // Copy buffer to image
    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {w, h, 1};

    vkCmdCopyBufferToImage(cmd, staging.Get(), image_,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Barrier: TRANSFER_DST_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL
    VkImageMemoryBarrier barrier2 = {};
    barrier2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier2.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier2.image = image_;
    barrier2.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier2.subresourceRange.baseMipLevel = 0;
    barrier2.subresourceRange.levelCount = 1;
    barrier2.subresourceRange.baseArrayLayer = 0;
    barrier2.subresourceRange.layerCount = 1;
    barrier2.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier2);

    vkEndCommandBuffer(cmd);

    // Submit and wait
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;

    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = VK_NULL_HANDLE;
    vkCreateFence(device, &fence_info, allocator_, &fence);

    vkQueueSubmit(queue, 1, &submit_info, fence);
    vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);

    // Cleanup
    vkDestroyFence(device, fence, allocator_);
    vkFreeCommandBuffers(device, temp_pool, 1, &cmd);
    vkDestroyCommandPool(device, temp_pool, allocator_);
    staging.Destroy();

    return true;
}

} // namespace renderer
} // namespace mg
