#pragma once
#include "scene/entity.h"
#include <vulkan/vulkan.h>
#include <string>

namespace mg {
namespace renderer { class Buffer; }
namespace scene {

class World;
struct Sprite;

class SpriteRenderSystem {
public:
    struct Config {
        VkPipeline       pipeline;
        VkPipelineLayout layout;
        VkBuffer         quad_vb;         // 6-vertex quad，所有 Sprite 共享
        renderer::Buffer* instance_vb = nullptr; // host-visible dynamic buffer for per-sprite instance data
        VkDeviceSize     instance_vb_size = 0;
        VkDescriptorSet  ubo_set;         // set=0: VP matrix UBO

        // 视口 ViewMode：0=Lit（纹理×tint） 1=Unlit（纯纹理）。绑定管线后 push 到片段着色器
        int              view_mode = 0;

        // 纹理 DS 获取回调（Application 注入，内部有纹理加载+缓存逻辑）
        VkDescriptorSet (*get_texture_ds)(void* ctx, const std::string& path);
        void*            tex_ctx;

        // 图集回调（可空）：get_atlas_rect 查询/懒打包，get_atlas_ds 返回图集纹理 DS
        bool           (*get_atlas_rect)(void* ctx, const std::string& path, float out_uv_rect[4]) = nullptr;
        VkDescriptorSet (*get_atlas_ds)(void* ctx) = nullptr;
        void*            atlas_ctx = nullptr;
    };

    void Render(VkCommandBuffer cmd, World* world, const Config& cfg) const;
};

} // namespace scene
} // namespace mg
