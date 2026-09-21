#include "renderer_context.h"
#include <SDL.h>
#include <SDL_vulkan.h>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdlib>
#include "src/core/log.h"

namespace mg {
namespace renderer {

// 检查指定扩展名是否在设备/实例支持的扩展列表中
static bool IsExtensionAvailable(const std::vector<VkExtensionProperties>& properties, const char* extension) {
    for (const auto& p : properties) {
        if (strcmp(p.extensionName, extension) == 0)
            return true;
    }
    return false;
}

// Vulkan Debug Report 回调：输出错误/警告/性能警告
#ifdef _DEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugReportCallback(
    VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT objectType,
    uint64_t object,
    size_t location,
    int32_t messageCode,
    const char* pLayerPrefix,
    const char* pMessage,
    void* pUserData)
{
    (void)flags; (void)object; (void)location; (void)messageCode; (void)pUserData; (void)pLayerPrefix;
    MG_LOG_ERROR("[vulkan] Debug report from ObjectType: " << objectType << "\nMessage: " << pMessage);
    return VK_FALSE;
}
#endif

void RendererContext::CheckVkResult(VkResult err) {
    if (err == VK_SUCCESS) return;
    MG_LOG_ERROR("[vulkan] Error: VkResult = " << err);
    if (err < 0) abort();
}

RendererContext::RendererContext() = default;

RendererContext::~RendererContext() {
    Shutdown();
}

// 初始化 Vulkan 上下文：创建 Instance → 选 GPU → 创建 Device → 创建 DescriptorPool
bool RendererContext::Initialize(SDL_Window* window) {
    uint32_t extensions_count = 0;
    SDL_Vulkan_GetInstanceExtensions(window, &extensions_count, nullptr);
    std::vector<const char*> extensions(extensions_count);
    SDL_Vulkan_GetInstanceExtensions(window, &extensions_count, extensions.data());

    if (!CreateInstance(extensions)) return false;

    // 创建临时 Surface 用于选择同时支持图形和呈现的 GPU/队列族
    VkSurfaceKHR temp_surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, instance_, &temp_surface)) {
        MG_LOG_ERROR("[vulkan] Failed to create temporary surface");
        return false;
    }

    if (!PickPhysicalDevice(temp_surface)) {
        vkDestroySurfaceKHR(instance_, temp_surface, allocator_);
        return false;
    }

    vkDestroySurfaceKHR(instance_, temp_surface, allocator_);
    temp_surface = VK_NULL_HANDLE;

    if (!CreateLogicalDevice()) return false;
    if (!CreateDescriptorPool()) return false;

    return true;
}

// 逆序销毁所有 Vulkan 对象（device → instance 顺序，先 wait idle 避免使用中销毁）
void RendererContext::Shutdown() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
    }

    if (descriptor_pool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, descriptor_pool_, allocator_);
        descriptor_pool_ = VK_NULL_HANDLE;
    }

#ifdef _DEBUG
    if (debug_report_ != VK_NULL_HANDLE) {
        auto f_vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance_, "vkDestroyDebugReportCallbackEXT");
        if (f_vkDestroyDebugReportCallbackEXT)
            f_vkDestroyDebugReportCallbackEXT(instance_, debug_report_, allocator_);
        debug_report_ = VK_NULL_HANDLE;
    }
#endif

    if (device_ != VK_NULL_HANDLE) {
        vkDestroyDevice(device_, allocator_);
        device_ = VK_NULL_HANDLE;
    }

    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, allocator_);
        instance_ = VK_NULL_HANDLE;
    }
}

// 创建 Vulkan Instance：启用 SDL 需要的 Surface 扩展 + 调试层（Debug 模式）
bool RendererContext::CreateInstance(const std::vector<const char*>& extensions) {
    VkResult err;

    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

    uint32_t properties_count;
    vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, nullptr);
    std::vector<VkExtensionProperties> properties(properties_count);
    err = vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, properties.data());
    CheckVkResult(err);

    std::vector<const char*> instance_extensions = extensions;

    if (IsExtensionAvailable(properties, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
        instance_extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
    if (IsExtensionAvailable(properties, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
        instance_extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }
#endif

#ifdef _DEBUG
    const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
    create_info.enabledLayerCount = 1;
    create_info.ppEnabledLayerNames = layers;
    instance_extensions.push_back("VK_EXT_debug_report");
#endif

    create_info.enabledExtensionCount = static_cast<uint32_t>(instance_extensions.size());
    create_info.ppEnabledExtensionNames = instance_extensions.data();

    err = vkCreateInstance(&create_info, allocator_, &instance_);
    CheckVkResult(err);

#ifdef _DEBUG
    auto f_vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance_, "vkCreateDebugReportCallbackEXT");
    if (f_vkCreateDebugReportCallbackEXT) {
        VkDebugReportCallbackCreateInfoEXT debug_report_ci = {};
        debug_report_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
        debug_report_ci.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
        debug_report_ci.pfnCallback = DebugReportCallback;
        debug_report_ci.pUserData = nullptr;
        err = f_vkCreateDebugReportCallbackEXT(instance_, &debug_report_ci, allocator_, &debug_report_);
        CheckVkResult(err);
    }
#endif

    return true;
}

// 选择物理设备：优先独立显卡，必须同时支持图形队列和指定 Surface 的呈现
bool RendererContext::PickPhysicalDevice(VkSurfaceKHR surface) {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance_, &device_count, nullptr);
    if (device_count == 0) {
        MG_LOG_ERROR("[vulkan] No physical devices found");
        return false;
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance_, &device_count, devices.data());

    auto score_device = [](VkPhysicalDevice pd) -> int {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(pd, &props);
        int score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 1000;
        else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 100;
        score += (int)props.limits.maxImageDimension2D;
        return score;
    };

    auto find_graphics_present_queue = [surface](VkPhysicalDevice pd, uint32_t& out_family) -> bool {
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &queue_family_count, nullptr);
        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &queue_family_count, queue_families.data());
        for (uint32_t i = 0; i < queue_family_count; ++i) {
            if ((queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) continue;
            VkBool32 present_support = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, surface, &present_support);
            if (present_support == VK_TRUE) {
                out_family = i;
                return true;
            }
        }
        return false;
    };

    // 按分数排序
    std::vector<std::pair<int, VkPhysicalDevice>> scored;
    for (auto pd : devices) scored.push_back({score_device(pd), pd});
    std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) { return a.first > b.first; });

    for (const auto& [score, pd] : scored) {
        uint32_t family = 0;
        if (find_graphics_present_queue(pd, family)) {
            physical_device_ = pd;
            queue_family_ = family;
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(pd, &props);
            MG_LOG_INFO("[vulkan] Selected GPU: " << props.deviceName);
            return true;
        }
    }

    MG_LOG_ERROR("[vulkan] Failed to find a suitable GPU with graphics+present support");
    return false;
}

// 创建逻辑设备：启用 SwapChain 扩展，创建单队列
bool RendererContext::CreateLogicalDevice() {
    VkResult err;

    std::vector<const char*> device_extensions;
    device_extensions.push_back("VK_KHR_swapchain");

    uint32_t properties_count;
    vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, &properties_count, nullptr);
    std::vector<VkExtensionProperties> properties(properties_count);
    vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, &properties_count, properties.data());

#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
    if (IsExtensionAvailable(properties, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
        device_extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif

    const float queue_priority[] = { 1.0f };
    VkDeviceQueueCreateInfo queue_info[1] = {};
    queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info[0].queueFamilyIndex = queue_family_;
    queue_info[0].queueCount = 1;
    queue_info[0].pQueuePriorities = queue_priority;

    // Wireframe 视口模式需要非实心填充；查询支持后开启（不支持则由编辑器禁用 Wire 按钮）
    VkPhysicalDeviceFeatures supported_features = {};
    vkGetPhysicalDeviceFeatures(physical_device_, &supported_features);
    VkPhysicalDeviceFeatures enabled_features = {};
    if (supported_features.fillModeNonSolid) {
        enabled_features.fillModeNonSolid = VK_TRUE;
        fill_mode_non_solid_ = true;
    } else {
        MG_LOG_WARN("RendererContext: fillModeNonSolid not supported, wireframe view mode disabled");
    }

    VkDeviceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = sizeof(queue_info) / sizeof(queue_info[0]);
    create_info.pQueueCreateInfos = queue_info;
    create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
    create_info.ppEnabledExtensionNames = device_extensions.data();
    create_info.pEnabledFeatures = &enabled_features;

    err = vkCreateDevice(physical_device_, &create_info, allocator_, &device_);
    CheckVkResult(err);

    vkGetDeviceQueue(device_, queue_family_, 0, &queue_);

    return true;
}

// 创建 DescriptorPool：供 UBO / ImGui / 运行时采样器使用
bool RendererContext::CreateDescriptorPool() {
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         256 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          512 },
        { VK_DESCRIPTOR_TYPE_SAMPLER,                128 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256 },
    };

    uint32_t max_sets = 0;
    for (auto& pool_size : pool_sizes)
        max_sets += pool_size.descriptorCount;

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = max_sets;
    pool_info.poolSizeCount = sizeof(pool_sizes) / sizeof(pool_sizes[0]);
    pool_info.pPoolSizes = pool_sizes;

    VkResult err = vkCreateDescriptorPool(device_, &pool_info, allocator_, &descriptor_pool_);
    CheckVkResult(err);

    return true;
}

} // namespace renderer
} // namespace mg
