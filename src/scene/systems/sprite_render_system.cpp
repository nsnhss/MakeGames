#include "sprite_render_system.h"
#include "sprite_batch_builder.h"
#include "scene/world.h"
#include "renderer/buffer.h"
#include "core/log.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace mg {
namespace scene {

void SpriteRenderSystem::Render(VkCommandBuffer cmd, World* world, const Config& cfg) const
{
    if (!world || !cfg.instance_vb) return;

    // 收集 + 排序 + 分组（纯逻辑，见 sprite_batch_builder）
    std::vector<SpriteInstance> instances;
    std::vector<SpriteBatch> batches;
    BuildSpriteInstances(world, instances, batches, cfg.get_atlas_rect, cfg.atlas_ctx);
    if (instances.empty()) return;

    // 整帧只上传一次 instance 数据（超容量截断），避免多组覆盖同一缓冲的录制期隐患
    uint32_t capacity = (uint32_t)(cfg.instance_vb_size / sizeof(SpriteInstance));
    if (instances.size() > capacity) {
        MG_LOG_WARN("SpriteRenderSystem: " << instances.size()
                    << " instances exceed capacity " << capacity << ", truncated");
        instances.resize(capacity);
        for (auto& b : batches) {
            if (b.first_instance >= capacity) { b.count = 0; continue; }
            if (b.first_instance + b.count > capacity) b.count = capacity - b.first_instance;
        }
    }
    cfg.instance_vb->Upload(instances.data(), instances.size() * sizeof(SpriteInstance));

    // 绑定共享 quad 几何、instance 缓冲与 UBO 各一次
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, cfg.pipeline);
    vkCmdPushConstants(cmd, cfg.layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(int), &cfg.view_mode);
    VkBuffer vbs[2] = {cfg.quad_vb, cfg.instance_vb->Get()};
    VkDeviceSize offsets[2] = {0, 0};
    vkCmdBindVertexBuffers(cmd, 0, 2, vbs, offsets);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        cfg.layout, 0, 1, &cfg.ubo_set, 0, nullptr);

    // 逐纹理分组：切换纹理 DS，用 firstInstance 偏移引用已上传的实例数据
    for (const auto& batch : batches) {
        if (batch.count == 0) continue;

        VkDescriptorSet tex_ds = VK_NULL_HANDLE;
        if (batch.texture_path == "@atlas") {
            tex_ds = cfg.get_atlas_ds ? cfg.get_atlas_ds(cfg.atlas_ctx) : VK_NULL_HANDLE;
        } else if (cfg.get_texture_ds) {
            tex_ds = cfg.get_texture_ds(cfg.tex_ctx, batch.texture_path);
        }
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            cfg.layout, 1, 1, &tex_ds, 0, nullptr);

        vkCmdDraw(cmd, 6, batch.count, 0, batch.first_instance);
    }
}

} // namespace scene
} // namespace mg
