#pragma once
#include "atlas_packer.h"
#include "renderer/texture.h"
#include "renderer/renderer_context.h"
#include <vulkan/vulkan.h>
#include <string>
#include <unordered_map>
#include <memory>

namespace mg {

// 纹理 + DescriptorSet 统一管理器
// - 纹理常驻（不自动 GC），Shutdown() 统一释放
// - DS pool + layout 自给自足，Application 只需 Initialize/Shutdown
class ResourceManager {
public:
    static ResourceManager& Get();

    void Initialize(renderer::RendererContext* renderer,
                    VkAllocationCallbacks* allocator);
    void Shutdown();

    void SetProjectPath(const std::string& path) { project_path_ = path; }

    // ---- 纹理 ----
    renderer::Texture* LoadTexture(const std::string& path);
    renderer::Texture* CreateTextureFromPixels(
        const std::string& key, const uint8_t* rgba, uint32_t w, uint32_t h);
    renderer::Texture* GetTexture(const std::string& path) const;
    renderer::Texture* GetDefaultTexture();
    bool IsLoaded(const std::string& path) const;

    // ---- Sampler DescriptorSet ----
    VkDescriptorSetLayout GetSamplerLayout() const { return sampler_layout_; }
    VkDescriptorSet GetOrCreateSamplerDS(const std::string& path);
    VkDescriptorSet GetOrCreateSamplerDS(renderer::Texture* tex);

    // ---- Sprite 图集（懒打包：首次请求时加载 CPU 像素并 shelf 打包进 2048²） ----
    // 成功返回 true 并填 out_uv_rect (u0,v0,du,dv)；加载失败/放不下返回 false（调用方走 per-texture fallback）
    bool GetSpriteAtlasRect(const std::string& path, float out_uv_rect[4]);
    // 返回图集纹理的 sampler DS；dirty 时先重建 GPU 纹理并重写 DS（阻塞式上传，须在命令录制前调用）
    VkDescriptorSet GetSpriteAtlasDS();

private:
    void CreateDefaultTexture();
    void CreateDSLayoutAndPool();

    struct TextureEntry {
        std::unique_ptr<renderer::Texture> texture;
        VkDescriptorSet sampler_ds = VK_NULL_HANDLE;
    };

    renderer::RendererContext* renderer_ = nullptr;
    VkAllocationCallbacks* allocator_ = nullptr;

    VkDescriptorSetLayout sampler_layout_ = VK_NULL_HANDLE;
    VkDescriptorPool sampler_pool_ = VK_NULL_HANDLE;

    std::string ResolveTexturePath(const std::string& path) const;

    std::unordered_map<std::string, TextureEntry> textures_;
    renderer::Texture default_texture_;
    VkDescriptorSet default_sampler_ds_ = VK_NULL_HANDLE;
    std::string project_path_;

    // Sprite 图集状态
    resource::AtlasPacker atlas_packer_;
    renderer::Texture atlas_texture_;
    VkDescriptorSet atlas_ds_ = VK_NULL_HANDLE;
    bool atlas_dirty_ = false;
};

} // namespace mg
