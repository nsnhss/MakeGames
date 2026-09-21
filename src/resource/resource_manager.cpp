#include "resource_manager.h"
#include <cstdio>
#include <filesystem>
#include <vector>
#include "src/core/log.h"

namespace mg {

ResourceManager& ResourceManager::Get() {
    static ResourceManager instance;
    return instance;
}


// 前向声明（CreateDefaultTexture 需要调用）
static VkDescriptorSet AllocateAndWriteDS(
    VkDevice device, VkDescriptorPool pool,
    VkDescriptorSetLayout layout,
    VkImageView view, VkSampler sampler);
void ResourceManager::Initialize(renderer::RendererContext* renderer,
                                  VkAllocationCallbacks* allocator)
{
    renderer_ = renderer;
    allocator_ = allocator;

    CreateDSLayoutAndPool();
    CreateDefaultTexture();
}

void ResourceManager::Shutdown() {
    for (auto& [path, entry] : textures_) {
        entry.texture->Destroy();
    }
    textures_.clear();

    if (default_sampler_ds_ != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(renderer_->GetDevice(), sampler_pool_, 1, &default_sampler_ds_);
        default_sampler_ds_ = VK_NULL_HANDLE;
    }
    default_texture_.Destroy();

    if (atlas_ds_ != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(renderer_->GetDevice(), sampler_pool_, 1, &atlas_ds_);
        atlas_ds_ = VK_NULL_HANDLE;
    }
    atlas_texture_.Destroy();

    if (sampler_pool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(renderer_->GetDevice(), sampler_pool_, allocator_);
        sampler_pool_ = VK_NULL_HANDLE;
    }
    renderer_ = nullptr;
}

void ResourceManager::CreateDSLayoutAndPool() {
    VkDevice device = renderer_->GetDevice();

    VkDescriptorSetLayoutBinding tex_binding = {};
    tex_binding.binding = 0;
    tex_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    tex_binding.descriptorCount = 1;
    tex_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 1;
    layout_info.pBindings = &tex_binding;

    vkCreateDescriptorSetLayout(device, &layout_info, allocator_, &sampler_layout_);

    VkDescriptorPoolSize pool_size = {};
    pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_size.descriptorCount = 256;

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 256;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;

    vkCreateDescriptorPool(device, &pool_info, allocator_, &sampler_pool_);
}

void ResourceManager::CreateDefaultTexture() {
    uint8_t white[4] = {255, 255, 255, 255};
    default_texture_.CreateFromPixels(renderer_, white, 1, 1);
    default_sampler_ds_ = AllocateAndWriteDS(
        renderer_->GetDevice(), sampler_pool_, sampler_layout_,
        default_texture_.GetView(), default_texture_.GetSampler());
}

// ---- 纹理 ----

std::string ResourceManager::ResolveTexturePath(const std::string& path) const {
    if (path.empty()) return path;
    namespace fs = std::filesystem;
    if (fs::path(path).is_absolute()) return path;
    if (!project_path_.empty()) {
        // 用字符串拼接，保持 UTF-8 编码，避免 Windows std::filesystem 对中文路径的编码问题
        std::string full = project_path_;
        if (!full.empty() && full.back() != '/' && full.back() != '\\') full += '/';
        full += path;
        return full;
    }
    return path;
}

renderer::Texture* ResourceManager::LoadTexture(const std::string& path) {
    if (path.empty()) return &default_texture_;

    auto it = textures_.find(path);
    if (it != textures_.end()) return it->second.texture.get();

    std::string full_path = ResolveTexturePath(path);

    auto entry = TextureEntry{};
    entry.texture = std::make_unique<renderer::Texture>();
    if (!entry.texture->LoadFromFile(renderer_, full_path.c_str())) {
        MG_LOG_ERROR("[ResourceManager] Failed to load texture: " << path << " (resolved: " << full_path << ")");
        return &default_texture_;
    }

    auto* tex = entry.texture.get();
    textures_[path] = std::move(entry);
    return tex;
}

renderer::Texture* ResourceManager::CreateTextureFromPixels(
    const std::string& key, const uint8_t* rgba, uint32_t w, uint32_t h)
{
    auto it = textures_.find(key);
    if (it != textures_.end()) return it->second.texture.get();

    auto entry = TextureEntry{};
    entry.texture = std::make_unique<renderer::Texture>();
    if (!entry.texture->CreateFromPixels(renderer_, rgba, w, h)) {
        MG_LOG_ERROR("[ResourceManager] Failed to create texture from pixels: " << key);
        return &default_texture_;
    }

    auto* tex = entry.texture.get();
    textures_[key] = std::move(entry);
    return tex;
}

renderer::Texture* ResourceManager::GetTexture(const std::string& path) const {
    if (path.empty()) return const_cast<renderer::Texture*>(&default_texture_);
    auto it = textures_.find(path);
    return (it != textures_.end()) ? it->second.texture.get() : nullptr;
}

renderer::Texture* ResourceManager::GetDefaultTexture() {
    return &default_texture_;
}

bool ResourceManager::IsLoaded(const std::string& path) const {
    if (path.empty()) return true;
    return textures_.find(path) != textures_.end();
}

// ---- Sampler DescriptorSet ----

static VkDescriptorSet AllocateAndWriteDS(
    VkDevice device, VkDescriptorPool pool,
    VkDescriptorSetLayout layout,
    VkImageView view, VkSampler sampler)
{
    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &layout;

    VkDescriptorSet ds = VK_NULL_HANDLE;
    VkResult err = vkAllocateDescriptorSets(device, &alloc_info, &ds);
    if (err != VK_SUCCESS) return VK_NULL_HANDLE;

    VkDescriptorImageInfo image_info = {};
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info.imageView = view;
    image_info.sampler = sampler;

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = ds;
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &image_info;

    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    return ds;
}

VkDescriptorSet ResourceManager::GetOrCreateSamplerDS(const std::string& path) {
    auto* tex = LoadTexture(path);
    if (!tex) return VK_NULL_HANDLE;
    return GetOrCreateSamplerDS(tex);
}

VkDescriptorSet ResourceManager::GetOrCreateSamplerDS(renderer::Texture* tex) {
    if (!tex) return VK_NULL_HANDLE;
    if (tex == &default_texture_) return default_sampler_ds_;

    for (auto& [path, entry] : textures_) {
        if (entry.texture.get() == tex) {
            if (entry.sampler_ds == VK_NULL_HANDLE) {
                entry.sampler_ds = AllocateAndWriteDS(
                    renderer_->GetDevice(), sampler_pool_, sampler_layout_,
                    tex->GetView(), tex->GetSampler());
            }
            return entry.sampler_ds;
        }
    }

    return default_sampler_ds_;
}

// ---- Sprite 图集 ----

bool ResourceManager::GetSpriteAtlasRect(const std::string& path, float out_uv_rect[4]) {
    if (path.empty()) return false;
    if (atlas_packer_.TryGetRect(path, out_uv_rect)) return true;

    std::string full_path = ResolveTexturePath(path);
    std::vector<uint8_t> rgba;
    int w = 0, h = 0;
    if (!renderer::Texture::LoadPixelsFromFile(full_path.c_str(), rgba, w, h)) return false;
    if (!atlas_packer_.Add(path, w, h, rgba.data(), out_uv_rect)) {
        MG_LOG_WARN("[ResourceManager] Atlas full, fallback to per-texture: " << path);
        return false;
    }
    atlas_dirty_ = true;
    return true;
}

VkDescriptorSet ResourceManager::GetSpriteAtlasDS() {
    if (atlas_dirty_) {
        if (!atlas_texture_.CreateFromPixels(renderer_, atlas_packer_.Pixels().data(),
                                             resource::AtlasPacker::kSize, resource::AtlasPacker::kSize,
                                             VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)) {
            MG_LOG_ERROR("[ResourceManager] Failed to rebuild sprite atlas texture");
            return default_sampler_ds_;
        }
        if (atlas_ds_ == VK_NULL_HANDLE) {
            atlas_ds_ = AllocateAndWriteDS(
                renderer_->GetDevice(), sampler_pool_, sampler_layout_,
                atlas_texture_.GetView(), atlas_texture_.GetSampler());
        } else {
            // 纹理重建后旧 DS 指向已销毁的 view/sampler，必须重写
            VkDescriptorImageInfo image_info = {};
            image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_info.imageView = atlas_texture_.GetView();
            image_info.sampler = atlas_texture_.GetSampler();
            VkWriteDescriptorSet write = {};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = atlas_ds_;
            write.dstBinding = 0;
            write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.descriptorCount = 1;
            write.pImageInfo = &image_info;
            vkUpdateDescriptorSets(renderer_->GetDevice(), 1, &write, 0, nullptr);
        }
        atlas_dirty_ = false;
    }
    return (atlas_ds_ != VK_NULL_HANDLE) ? atlas_ds_ : default_sampler_ds_;
}

} // namespace mg
