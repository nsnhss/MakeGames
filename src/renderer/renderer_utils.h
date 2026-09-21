#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>

namespace mg {
namespace renderer {

// 查找满足 memoryTypeBits 和 propertyFlags 的内存类型索引
inline uint32_t FindMemoryType(VkPhysicalDevice pd, uint32_t type_filter, VkMemoryPropertyFlags props)
{
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(pd, &mem_props);
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) && (mem_props.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    return UINT32_MAX;
}

} // namespace renderer
} // namespace mg
