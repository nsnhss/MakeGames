#pragma once
#include "renderer/buffer.h"
#include <vulkan/vulkan.h>
#include <cstdint>

namespace mg {
namespace scene {

class World;

class TilemapRenderSystem {
public:
    struct Config {
        VkPipeline       pipeline;
        VkPipelineLayout layout;
        VkDescriptorSet  ubo_set;         // set=0: VP matrix UBO
        VkBuffer         dummy_instance_vb = VK_NULL_HANDLE; // binding 1: zero instance data

        // 视口 ViewMode：0=Lit（纹理×tint） 1=Unlit（纯纹理）。绑定管线后 push 到片段着色器
        int              view_mode = 0;
    };

    struct VBState {
        const renderer::Buffer* buffer = nullptr;
        uint32_t                first_vertex = 0;
        uint32_t                vertex_count = 0;
    };

    // 只负责渲染，不负责 dirty 检查 / VB 重建
    // tilemap_ds: tileset 纹理的 DS（由 Application 提前查好并传入）
    // vb.first_vertex 允许从同一个合并 VB 中渲染多个 tileset 批次
    void Render(VkCommandBuffer cmd, World* world,
                const VBState& vb,
                VkDescriptorSet tilemap_ds,
                const Config& cfg) const;
};

} // namespace scene
} // namespace mg
