#include "tilemap_render_system.h"
#include "scene/world.h"
#include "scene/components/transform.h"
#include "scene/components/tilemap.h"
#include <vulkan/vulkan.h>

namespace mg {
namespace scene {

void TilemapRenderSystem::Render(VkCommandBuffer cmd, World* world,
                                  const VBState& vb,
                                  VkDescriptorSet tilemap_ds,
                                  const Config& cfg) const
{
    if (!world) return;
    if (vb.vertex_count == 0) return;
    if (!vb.buffer || !vb.buffer->IsValid()) return;
    if (tilemap_ds == VK_NULL_HANDLE) return;
    // (tilemap world offset is pre-baked into VB vertices)

    // Bind pipeline and UBO (set=0) shared with the sprite renderer.
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, cfg.pipeline);
    vkCmdPushConstants(cmd, cfg.layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(int), &cfg.view_mode);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        cfg.layout, 0, 1, &cfg.ubo_set, 0, nullptr);

    // Bind tilemap VB to binding 0 and a dummy instance buffer to binding 1.
    // The tilemap uses the sprite pipeline which expects per-instance data
    // at binding 1; without this, stale sprite instance data is read.
    VkBuffer vkb = vb.buffer->Get();
    if (cfg.dummy_instance_vb != VK_NULL_HANDLE) {
        VkBuffer vbs[2] = {vkb, cfg.dummy_instance_vb};
        VkDeviceSize offsets[2] = {0, 0};
        vkCmdBindVertexBuffers(cmd, 0, 2, vbs, offsets);
    } else {
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vkb, &offset);
    }

    // Bind tilemap texture DS (set=1)
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        cfg.layout, 1, 1, &tilemap_ds, 0, nullptr);

    vkCmdDraw(cmd, vb.vertex_count, 1, vb.first_vertex, 0);
}

} // namespace scene
} // namespace mg
