/**
 * @file llvkcontext.cpp
 * @brief Implementation of the Vulkan render context.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, The Vulkanstorm Project
 * $/LicenseInfo$
 */

#include "llvkcontext.h"

#include "llerror.h"
#include "llformat.h"
#include "llfile.h"

#include <algorithm>
#include <cstring>
#include <set>

namespace
{
    // Route validation-layer / debug-utils messages into the viewer log so we can
    // see the driver's actual complaint (e.g. why a swapchain call fails).
    VKAPI_ATTR VkBool32 VKAPI_CALL vkDebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT /*type*/,
        const VkDebugUtilsMessengerCallbackDataEXT* data,
        void* /*userData*/)
    {
        if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        {
            LL_WARNS("Vulkan") << "[VK] " << (data && data->pMessage ? data->pMessage : "") << LL_ENDL;
        }
        else
        {
            LL_INFOS("Vulkan") << "[VK] " << (data && data->pMessage ? data->pMessage : "") << LL_ENDL;
        }
        return VK_FALSE;
    }
}

#define LL_VK_CHECK(expr, error, msg)                                      \
    do {                                                                   \
        VkResult _res = (expr);                                            \
        if (_res != VK_SUCCESS) {                                          \
            error = llformat("%s (VkResult %d)", msg, (int)_res);          \
            LL_WARNS("Vulkan") << error << LL_ENDL;                        \
            return false;                                                  \
        }                                                                  \
    } while (0)

LLVKContext::~LLVKContext()
{
    destroy();
}

bool LLVKContext::createInstance(bool enableValidation, std::string& error)
{
    if (volkInitialize() != VK_SUCCESS)
    {
        error = "No Vulkan loader present (volkInitialize failed)";
        LL_WARNS("Vulkan") << error << LL_ENDL;
        return false;
    }

    mValidation = enableValidation;

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Vulkanstorm";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "Vulkanstorm";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    // Target Vulkan 1.3: dynamic rendering (used for the clear) is core, and the
    // 1.3 WSI path is the well-exercised one on current drivers.
    app_info.apiVersion = VK_API_VERSION_1_3;

    std::vector<const char*> extensions;
    extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif

    std::vector<const char*> layers;
    if (mValidation)
    {
        uint32_t layer_count = 0;
        vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
        std::vector<VkLayerProperties> available(layer_count);
        vkEnumerateInstanceLayerProperties(&layer_count, available.data());
        bool have_validation = false;
        bool have_apidump = false;
        for (const auto& lp : available)
        {
            if (std::strcmp(lp.layerName, "VK_LAYER_KHRONOS_validation") == 0) have_validation = true;
            if (std::strcmp(lp.layerName, "VK_LAYER_LUNARG_api_dump") == 0) have_apidump = true;
        }
        // API dump first (outermost) so it records the raw calls, then validation.
        if (have_apidump) layers.push_back("VK_LAYER_LUNARG_api_dump");
        if (have_validation)
        {
            layers.push_back("VK_LAYER_KHRONOS_validation");
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        else
        {
            LL_INFOS("Vulkan") << "Validation requested but VK_LAYER_KHRONOS_validation not present; continuing without it." << LL_ENDL;
            mValidation = false;
        }
    }

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = (uint32_t)extensions.size();
    create_info.ppEnabledExtensionNames = extensions.data();
    create_info.enabledLayerCount = (uint32_t)layers.size();
    create_info.ppEnabledLayerNames = layers.data();

    // When validation is on, also capture messages emitted during instance
    // creation itself by chaining a debug-messenger create-info.
    VkDebugUtilsMessengerCreateInfoEXT dbg_create{};
    if (mValidation)
    {
        dbg_create.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        dbg_create.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        dbg_create.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        dbg_create.pfnUserCallback = vkDebugCallback;
        create_info.pNext = &dbg_create;
    }

    LL_VK_CHECK(vkCreateInstance(&create_info, nullptr, &mInstance), error, "vkCreateInstance failed");

    volkLoadInstance(mInstance);

    // Persistent messenger for the instance lifetime.
    if (mValidation && vkCreateDebugUtilsMessengerEXT)
    {
        if (vkCreateDebugUtilsMessengerEXT(mInstance, &dbg_create, nullptr, &mDebugMessenger) != VK_SUCCESS)
        {
            mDebugMessenger = VK_NULL_HANDLE;
        }
    }
    return true;
}

bool LLVKContext::pickPhysicalDevice(VkSurfaceKHR surface, std::string& error)
{
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(mInstance, &count, nullptr);
    if (count == 0)
    {
        error = "No Vulkan physical devices enumerated";
        LL_WARNS("Vulkan") << error << LL_ENDL;
        return false;
    }
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(mInstance, &count, devices.data());

    // Prefer a discrete GPU, then integrated, then anything. Require a queue
    // family that supports both graphics and (where possible) present.
    VkPhysicalDevice best = VK_NULL_HANDLE;
    int best_score = -1;
    uint32_t best_gfx = UINT32_MAX, best_present = UINT32_MAX;

    for (VkPhysicalDevice dev : devices)
    {
        uint32_t qcount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qcount, nullptr);
        std::vector<VkQueueFamilyProperties> qprops(qcount);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qcount, qprops.data());

        uint32_t gfx = UINT32_MAX, present = UINT32_MAX;
        for (uint32_t i = 0; i < qcount; ++i)
        {
            if (qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                if (gfx == UINT32_MAX) gfx = i;
                VkBool32 can_present = VK_FALSE;
                if (surface != VK_NULL_HANDLE)
                {
                    vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &can_present);
                }
                if (can_present && present == UINT32_MAX) present = i;
                if (gfx != UINT32_MAX && present != UINT32_MAX) break;
            }
        }

        VkPhysicalDeviceProperties dprops{};
        vkGetPhysicalDeviceProperties(dev, &dprops);
        const char* dtype = dprops.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? "discrete" :
                            dprops.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? "integrated" : "other";
        LL_INFOS("Vulkan") << "  device '" << (dprops.deviceName ? dprops.deviceName : "?")
                           << "' type=" << dtype << " gfxQueue=" << (gfx == UINT32_MAX ? -1 : (int)gfx)
                           << " presentQueue=" << (present == UINT32_MAX ? -1 : (int)present) << LL_ENDL;

        if (gfx == UINT32_MAX) continue;
        if (surface != VK_NULL_HANDLE && present == UINT32_MAX) continue;

        int score = 0;
        if (dprops.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score = 3;
        else if (dprops.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score = 2;
        else score = 1;

        if (score > best_score)
        {
            best_score = score;
            best = dev;
            best_gfx = gfx;
            best_present = present;
        }
    }

    if (best == VK_NULL_HANDLE)
    {
        error = "No suitable Vulkan device (graphics + present queue) found";
        LL_WARNS("Vulkan") << error << LL_ENDL;
        return false;
    }

    mPhysicalDevice = best;
    mGraphicsQueueFamily = best_gfx;
    mPresentQueueFamily = best_present;

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(best, &props);
    mDeviceName = props.deviceName ? props.deviceName : "";
    LL_INFOS("Vulkan") << "Selected Vulkan device: " << mDeviceName << LL_ENDL;
    return true;
}

bool LLVKContext::createDevice(VkSurfaceKHR surface, std::string& error)
{
    std::set<uint32_t> unique_families{ mGraphicsQueueFamily, mPresentQueueFamily };
    float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queue_infos;
    for (uint32_t fam : unique_families)
    {
        VkDeviceQueueCreateInfo qi{};
        qi.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qi.queueFamilyIndex = fam;
        qi.queueCount = 1;
        qi.pQueuePriorities = &priority;
        queue_infos.push_back(qi);
    }

    std::vector<const char*> device_extensions;
    if (surface != VK_NULL_HANDLE)
    {
        device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        // Dynamic rendering (render-pass-less clear) — core in Vulkan 1.3,
        // extension in 1.2. Enable both the extension and its feature.
        device_extensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
    }

    VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering{};
    dynamic_rendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynamic_rendering.dynamicRendering = VK_TRUE;

    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.pNext = &dynamic_rendering;
    create_info.queueCreateInfoCount = (uint32_t)queue_infos.size();
    create_info.pQueueCreateInfos = queue_infos.data();
    create_info.enabledExtensionCount = (uint32_t)device_extensions.size();
    create_info.ppEnabledExtensionNames = device_extensions.data();

    LL_VK_CHECK(vkCreateDevice(mPhysicalDevice, &create_info, nullptr, &mDevice), error, "vkCreateDevice failed");

    volkLoadDevice(mDevice);

    vkGetDeviceQueue(mDevice, mGraphicsQueueFamily, 0, &mGraphicsQueue);
    vkGetDeviceQueue(mDevice, mPresentQueueFamily, 0, &mPresentQueue);

    // VMA allocator (dynamic functions via volk).
    VmaVulkanFunctions vma_funcs{};
    vma_funcs.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vma_funcs.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo alloc_info{};
    alloc_info.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
    alloc_info.vulkanApiVersion = VK_API_VERSION_1_3;
    alloc_info.physicalDevice = mPhysicalDevice;
    alloc_info.device = mDevice;
    alloc_info.instance = mInstance;
    alloc_info.pVulkanFunctions = &vma_funcs;

    if (vmaCreateAllocator(&alloc_info, &mAllocator) != VK_SUCCESS)
    {
        error = "vmaCreateAllocator failed";
        LL_WARNS("Vulkan") << error << LL_ENDL;
        return false;
    }

    return true;
}

VkSurfaceKHR LLVKContext::createSurface(void* native_window, void* native_instance)
{
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    if (!native_window || mInstance == VK_NULL_HANDLE)
    {
        return VK_NULL_HANDLE;
    }
    VkWin32SurfaceCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    create_info.hinstance = (HINSTANCE)native_instance;
    create_info.hwnd = (HWND)native_window;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (vkCreateWin32SurfaceKHR(mInstance, &create_info, nullptr, &surface) != VK_SUCCESS)
    {
        LL_WARNS("Vulkan") << "vkCreateWin32SurfaceKHR failed" << LL_ENDL;
        return VK_NULL_HANDLE;
    }
    LL_INFOS("Vulkan") << "Created Win32 Vulkan surface" << LL_ENDL;
    return surface;
#else
    (void)native_window; (void)native_instance;
    return VK_NULL_HANDLE;
#endif
}

bool LLVKContext::createSwapchain(VkSurfaceKHR surface, uint32_t width, uint32_t height, std::string& error)
{
    mSurface = surface;

    if (mSwapchain != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(mDevice);
        destroySwapchain();
    }

    VkSurfaceCapabilitiesKHR caps{};
    LL_VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, surface, &caps), error, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed");

    // Diagnostics: log what the driver reports so we can diagnose swapchain
    // failures (VK_ERROR_UNKNOWN often means a degenerate extent or an
    // unsupported format/present-mode combination).
    LL_INFOS("Vulkan") << "Swapchain caps: currentExtent=" << caps.currentExtent.width << "x" << caps.currentExtent.height
                       << " min=" << caps.minImageExtent.width << "x" << caps.minImageExtent.height
                       << " max=" << caps.maxImageExtent.width << "x" << caps.maxImageExtent.height
                       << " minImageCount=" << caps.minImageCount << " maxImageCount=" << caps.maxImageCount
                       << " (requested " << width << "x" << height << ")" << LL_ENDL;

    // Choose a surface format: prefer B8G8R8A8_UNORM + SRGB nonlinear.
    uint32_t fmt_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, surface, &fmt_count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fmt_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, surface, &fmt_count, formats.data());
    VkSurfaceFormatKHR chosen_format = formats.empty() ? VkSurfaceFormatKHR{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR } : formats[0];
    for (const auto& f : formats)
    {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            chosen_format = f;
            break;
        }
    }
    LL_INFOS("Vulkan") << "Swapchain formats available=" << fmt_count << " chosen format=" << chosen_format.format << " colorspace=" << chosen_format.colorSpace << LL_ENDL;

    // Present modes supported on this surface.
    uint32_t pm_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice, surface, &pm_count, nullptr);
    std::vector<VkPresentModeKHR> pmodes(pm_count);
    if (pm_count) vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice, surface, &pm_count, pmodes.data());
    std::string pm_list;
    for (auto m : pmodes) { pm_list += " " + std::to_string((int)m); }
    LL_INFOS("Vulkan") << "Present modes (" << pm_count << "):" << pm_list << LL_ENDL;

    // Confirm the chosen queue family actually supports present on this surface.
    VkBool32 present_ok = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(mPhysicalDevice, mPresentQueueFamily, surface, &present_ok);
    LL_INFOS("Vulkan") << "Queue families: graphics=" << mGraphicsQueueFamily << " present=" << mPresentQueueFamily
                       << " presentSupportedOnSurface=" << (present_ok ? "yes" : "no") << LL_ENDL;

    // Present mode: FIFO (vsync) is guaranteed; use it for Phase 1.
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;

    VkExtent2D extent = caps.currentExtent;
    if (extent.width == UINT32_MAX)
    {
        extent.width = width;
        extent.height = height;
        if (extent.width < caps.minImageExtent.width) extent.width = caps.minImageExtent.width;
        if (extent.width > caps.maxImageExtent.width) extent.width = caps.maxImageExtent.width;
        if (extent.height < caps.minImageExtent.height) extent.height = caps.minImageExtent.height;
        if (extent.height > caps.maxImageExtent.height) extent.height = caps.maxImageExtent.height;
    }

    uint32_t image_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && image_count > caps.maxImageCount)
    {
        image_count = caps.maxImageCount;
    }

    VkSwapchainKHR old_swapchain = VK_NULL_HANDLE;
    VkSwapchainCreateInfoKHR sci{};
    sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface = surface;
    sci.minImageCount = image_count;
    sci.imageFormat = chosen_format.format;
    sci.imageColorSpace = chosen_format.colorSpace;
    sci.imageExtent = extent;
    sci.imageArrayLayers = 1;
    // Minimal, universally-supported usage for a presented swapchain image. We
    // clear via a render pass (not vkCmdClearColorImage), which only needs
    // COLOR_ATTACHMENT — some drivers reject extra usage bits on swapchain images.
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    uint32_t family_indices[] = { mGraphicsQueueFamily, mPresentQueueFamily };
    if (mGraphicsQueueFamily != mPresentQueueFamily)
    {
        sci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        sci.queueFamilyIndexCount = 2;
        sci.pQueueFamilyIndices = family_indices;
    }
    else
    {
        sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = present_mode;
    sci.clipped = VK_TRUE;
    sci.oldSwapchain = old_swapchain;

    LL_VK_CHECK(vkCreateSwapchainKHR(mDevice, &sci, nullptr, &mSwapchain), error, "vkCreateSwapchainKHR failed");

    mSwapchainFormat = chosen_format.format;
    mSwapchainExtent = extent;

    uint32_t actual_count = 0;
    vkGetSwapchainImagesKHR(mDevice, mSwapchain, &actual_count, nullptr);
    mSwapchainImages.resize(actual_count);
    vkGetSwapchainImagesKHR(mDevice, mSwapchain, &actual_count, mSwapchainImages.data());

    mSwapchainViews.resize(actual_count);
    for (uint32_t i = 0; i < actual_count; ++i)
    {
        VkImageViewCreateInfo vi{};
        vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image = mSwapchainImages[i];
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = mSwapchainFormat;
        vi.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
        vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vi.subresourceRange.baseMipLevel = 0;
        vi.subresourceRange.levelCount = 1;
        vi.subresourceRange.baseArrayLayer = 0;
        vi.subresourceRange.layerCount = 1;
        LL_VK_CHECK(vkCreateImageView(mDevice, &vi, nullptr, &mSwapchainViews[i]), error, "vkCreateImageView (swapchain) failed");
    }

    if (mCommandPool == VK_NULL_HANDLE)
    {
        VkCommandPoolCreateInfo cpi{};
        cpi.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cpi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cpi.queueFamilyIndex = mGraphicsQueueFamily;
        LL_VK_CHECK(vkCreateCommandPool(mDevice, &cpi, nullptr, &mCommandPool), error, "vkCreateCommandPool failed");
    }

    if (!createFrameResources())
    {
        error = "Failed to create per-frame resources";
        return false;
    }

    // Per-swapchain-image present semaphores (see llvkcontext.h).
    mImagePresentSem.resize(actual_count, VK_NULL_HANDLE);
    VkSemaphoreCreateInfo sem_ci{};
    sem_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    for (uint32_t i = 0; i < actual_count; ++i)
    {
        LL_VK_CHECK(vkCreateSemaphore(mDevice, &sem_ci, nullptr, &mImagePresentSem[i]), error, "vkCreateSemaphore (present) failed");
    }

    // Recreate the 2D pipeline against the (possibly new) swapchain format.
    // The shader modules persist; only the pipelines are rebuilt. The pipeline
    // is created lazily on the first begin2DFrame if the shaders have not been
    // loaded yet (see beginUIFrame).
    if (mShader2DVert != VK_NULL_HANDLE && mShader2DFrag != VK_NULL_HANDLE)
    {
        if (!create2DPipeline(error))
        {
            return false;
        }
    }

    return true;
}

bool LLVKContext::createFrameResources()
{
    for (uint32_t i = 0; i < kFramesInFlight; ++i)
    {
        FrameSync& f = mFrames[i];
        if (f.cmd == VK_NULL_HANDLE)
        {
            VkCommandBufferAllocateInfo cai{};
            cai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            cai.commandPool = mCommandPool;
            cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            cai.commandBufferCount = 1;
            if (vkAllocateCommandBuffers(mDevice, &cai, &f.cmd) != VK_SUCCESS) return false;
        }
        VkSemaphoreCreateInfo sem{};
        sem.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkFenceCreateInfo fence{};
        fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        if (f.imageAvailable == VK_NULL_HANDLE && vkCreateSemaphore(mDevice, &sem, nullptr, &f.imageAvailable) != VK_SUCCESS) return false;
        if (f.inFlight == VK_NULL_HANDLE && vkCreateFence(mDevice, &fence, nullptr, &f.inFlight) != VK_SUCCESS) return false;
    }
    return true;
}

bool LLVKContext::renderClearFrame(float r, float g, float b, float a)
{
    if (mDevice == VK_NULL_HANDLE || mSwapchain == VK_NULL_HANDLE) return false;

    FrameSync& f = mFrames[mFrameIndex];

    vkWaitForFences(mDevice, 1, &f.inFlight, VK_TRUE, UINT64_MAX);

    uint32_t image_index = 0;
    VkResult acquire = vkAcquireNextImageKHR(mDevice, mSwapchain, UINT64_MAX, f.imageAvailable, VK_NULL_HANDLE, &image_index);
    if (acquire == VK_ERROR_OUT_OF_DATE_KHR)
    {
        return false; // caller should recreate the swapchain
    }
    if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR)
    {
        return false;
    }

    vkResetFences(mDevice, 1, &f.inFlight);
    vkResetCommandBuffer(f.cmd, 0);

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(f.cmd, &begin);

    VkImage image = mSwapchainImages[image_index];

    // Transition UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL for the render-pass clear.
    VkImageMemoryBarrier to_attach{};
    to_attach.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_attach.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    to_attach.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    to_attach.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_attach.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_attach.image = image;
    to_attach.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    to_attach.subresourceRange.levelCount = 1;
    to_attach.subresourceRange.layerCount = 1;
    to_attach.srcAccessMask = 0;
    to_attach.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    vkCmdPipelineBarrier(f.cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &to_attach);

    // Clear via dynamic rendering: a single color attachment with loadOp=CLEAR.
    VkClearColorValue clear{};
    clear.float32[0] = r; clear.float32[1] = g; clear.float32[2] = b; clear.float32[3] = a;

    VkRenderingAttachmentInfo color_attach{};
    color_attach.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color_attach.imageView = mSwapchainViews[image_index];
    color_attach.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_attach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attach.clearValue.color = clear;

    VkRenderingInfo rendering{};
    rendering.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering.renderArea.offset = { 0, 0 };
    rendering.renderArea.extent = mSwapchainExtent;
    rendering.layerCount = 1;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachments = &color_attach;

    vkCmdBeginRendering(f.cmd, &rendering);
    vkCmdEndRendering(f.cmd);

    // Transition COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC.
    VkImageMemoryBarrier to_present{};
    to_present.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_present.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    to_present.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_present.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_present.image = image;
    to_present.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    to_present.subresourceRange.levelCount = 1;
    to_present.subresourceRange.layerCount = 1;
    to_present.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    to_present.dstAccessMask = 0;
    vkCmdPipelineBarrier(f.cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &to_present);

    vkEndCommandBuffer(f.cmd);

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    // Signal the present semaphore that belongs to the acquired image (not a
    // per-frame one), so a semaphore is never reused while its swapchain image
    // is still being presented.
    VkSemaphore present_sem = mImagePresentSem[image_index];

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &f.imageAvailable;
    submit.pWaitDstStageMask = &wait_stage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &f.cmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &present_sem;
    if (vkQueueSubmit(mGraphicsQueue, 1, &submit, f.inFlight) != VK_SUCCESS)
    {
        return false;
    }

    VkPresentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &present_sem;
    present.swapchainCount = 1;
    present.pSwapchains = &mSwapchain;
    present.pImageIndices = &image_index;
    VkResult pres = vkQueuePresentKHR(mPresentQueue, &present);

    mFrameIndex = (mFrameIndex + 1) % kFramesInFlight;
    return (pres == VK_SUCCESS || pres == VK_SUBOPTIMAL_KHR);
}

// <VulkanStorm> Capability probe (Stage 2): Vulkan-native memory-bandwidth
// micro-benchmark. Copies a large device-local buffer in a loop, timed with a
// VK_QUERY_TYPE_TIMESTAMP pool. Returns GB/s, or a negative value on failure /
// when timestamp queries are unsupported (caller falls back to a safe class).
// One-shot, off the UI path; uses its own command buffer + fence, never the
// in-flight frame resources.
float LLVKContext::measureMemoryBandwidthGBps()
{
    if (mDevice == VK_NULL_HANDLE || mGraphicsQueue == VK_NULL_HANDLE ||
        mCommandPool == VK_NULL_HANDLE || mAllocator == VK_NULL_HANDLE)
    {
        return -1.f;
    }

    // Timestamp queries must be supported on the graphics queue.
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(mPhysicalDevice, &props);
    if (!props.limits.timestampComputeAndGraphics || props.limits.timestampPeriod <= 0.f)
    {
        LL_WARNS("Vulkan") << "Bandwidth probe: timestamp queries unsupported; cannot measure." << LL_ENDL;
        return -1.f;
    }
    const double ts_period_ns = (double)props.limits.timestampPeriod;

    // Working set: 256 MiB device-local copy, large enough to exceed caches.
    const VkDeviceSize kBytes = (VkDeviceSize)256 * 1024 * 1024;
    const int kIterations = 8;

    VkBuffer src = VK_NULL_HANDLE, dst = VK_NULL_HANDLE;
    VmaAllocation srcAlloc = VK_NULL_HANDLE, dstAlloc = VK_NULL_HANDLE;
    VkBufferCreateInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size = kBytes;
    bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VmaAllocationCreateInfo ai{};
    ai.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE; // device-local
    if (vmaCreateBuffer(mAllocator, &bi, &ai, &src, &srcAlloc, nullptr) != VK_SUCCESS ||
        vmaCreateBuffer(mAllocator, &bi, &ai, &dst, &dstAlloc, nullptr) != VK_SUCCESS)
    {
        LL_WARNS("Vulkan") << "Bandwidth probe: buffer allocation failed." << LL_ENDL;
        if (src) vmaDestroyBuffer(mAllocator, src, srcAlloc);
        if (dst) vmaDestroyBuffer(mAllocator, dst, dstAlloc);
        return -1.f;
    }

    VkQueryPool queryPool = VK_NULL_HANDLE;
    VkQueryPoolCreateInfo qi{};
    qi.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    qi.queryType = VK_QUERY_TYPE_TIMESTAMP;
    qi.queryCount = 2;
    if (vkCreateQueryPool(mDevice, &qi, nullptr, &queryPool) != VK_SUCCESS)
    {
        LL_WARNS("Vulkan") << "Bandwidth probe: query pool creation failed." << LL_ENDL;
        vmaDestroyBuffer(mAllocator, src, srcAlloc);
        vmaDestroyBuffer(mAllocator, dst, dstAlloc);
        return -1.f;
    }

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo cai{};
    cai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cai.commandPool = mCommandPool;
    cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cai.commandBufferCount = 1;
    VkFence fence = VK_NULL_HANDLE;
    VkFenceCreateInfo fi{};
    fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    float result = -1.f;
    if (vkAllocateCommandBuffers(mDevice, &cai, &cmd) == VK_SUCCESS &&
        vkCreateFence(mDevice, &fi, nullptr, &fence) == VK_SUCCESS)
    {
        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &begin);
        vkCmdResetQueryPool(cmd, queryPool, 0, 2);
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPool, 0);
        VkBufferCopy region{ 0, 0, kBytes };
        for (int i = 0; i < kIterations; ++i)
        {
            vkCmdCopyBuffer(cmd, src, dst, 1, &region);
        }
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool, 1);
        vkEndCommandBuffer(cmd);

        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;
        if (vkQueueSubmit(mGraphicsQueue, 1, &submit, fence) == VK_SUCCESS &&
            vkWaitForFences(mDevice, 1, &fence, VK_TRUE, UINT64_MAX) == VK_SUCCESS)
        {
            uint64_t ts[2] = { 0, 0 };
            if (vkGetQueryPoolResults(mDevice, queryPool, 0, 2, sizeof(ts), ts,
                                      sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT) == VK_SUCCESS)
            {
                const double seconds = (double)(ts[1] - ts[0]) * ts_period_ns * 1e-9;
                if (seconds > 0.0)
                {
                    // A copy reads kBytes and writes kBytes per iteration.
                    const double moved = (double)kBytes * 2.0 * (double)kIterations;
                    result = (float)(moved / seconds / 1e9);
                }
            }
        }
    }

    if (fence) vkDestroyFence(mDevice, fence, nullptr);
    if (cmd) vkFreeCommandBuffers(mDevice, mCommandPool, 1, &cmd);
    vkDestroyQueryPool(mDevice, queryPool, nullptr);
    vmaDestroyBuffer(mAllocator, src, srcAlloc);
    vmaDestroyBuffer(mAllocator, dst, dstAlloc);

    LL_INFOS("Vulkan") << "Bandwidth probe: " << (result >= 0.f ? result : 0.f)
                       << " GB/s" << (result >= 0.f ? "" : " (FAILED)") << LL_ENDL;
    return result;
}
// </VulkanStorm>

void LLVKContext::destroySwapchain()
{
    destroyImageSync();
    for (VkImageView v : mSwapchainViews)
    {
        if (v != VK_NULL_HANDLE) vkDestroyImageView(mDevice, v, nullptr);
    }
    mSwapchainViews.clear();
    mSwapchainImages.clear();
    if (mSwapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);
        mSwapchain = VK_NULL_HANDLE;
    }
}

void LLVKContext::destroyImageSync()
{
    for (VkSemaphore s : mImagePresentSem)
    {
        if (s != VK_NULL_HANDLE) vkDestroySemaphore(mDevice, s, nullptr);
    }
    mImagePresentSem.clear();
}

void LLVKContext::destroy()
{
    if (mDevice != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(mDevice);
    }

    destroySwapchain();

    // The surface was adopted by createSwapchain() (mSurface). It must outlive
    // the swapchain but not the instance, so destroy it here.
    if (mSurface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
        mSurface = VK_NULL_HANDLE;
    }

    for (uint32_t i = 0; i < kFramesInFlight; ++i)
    {
        FrameSync& f = mFrames[i];
        if (mDevice != VK_NULL_HANDLE)
        {
            if (f.imageAvailable != VK_NULL_HANDLE) vkDestroySemaphore(mDevice, f.imageAvailable, nullptr);
            if (f.inFlight != VK_NULL_HANDLE) vkDestroyFence(mDevice, f.inFlight, nullptr);
        }
        f = FrameSync{};
    }

    if (mCommandPool != VK_NULL_HANDLE) { vkDestroyCommandPool(mDevice, mCommandPool, nullptr); mCommandPool = VK_NULL_HANDLE; }
    if (mAllocator != VK_NULL_HANDLE) { vmaDestroyAllocator(mAllocator); mAllocator = VK_NULL_HANDLE; }
    if (mDevice != VK_NULL_HANDLE) { vkDestroyDevice(mDevice, nullptr); mDevice = VK_NULL_HANDLE; }
    if (mDebugMessenger != VK_NULL_HANDLE && vkDestroyDebugUtilsMessengerEXT)
    {
        vkDestroyDebugUtilsMessengerEXT(mInstance, mDebugMessenger, nullptr);
        mDebugMessenger = VK_NULL_HANDLE;
    }
    if (mInstance != VK_NULL_HANDLE) { vkDestroyInstance(mInstance, nullptr); mInstance = VK_NULL_HANDLE; }

    volkFinalize();
}

// ===========================================================================
// Phase 3 (2D/UI)
// ===========================================================================

namespace
{
    // Resolve a path relative to the executable's own directory (robust to the
    // process CWD, which is not the exe dir at runtime).
    std::string shaderPath(const std::string& relative)
    {
#if LL_WINDOWS
        char exe[MAX_PATH] = { 0 };
        if (GetModuleFileNameA(nullptr, exe, MAX_PATH) > 0)
        {
            std::string dir(exe);
            size_t sep = dir.find_last_of("\\/");
            if (sep != std::string::npos)
            {
                return dir.substr(0, sep + 1) + relative;
            }
        }
#endif
        return relative;
    }

    VkShaderModule loadShaderModule(VkDevice device, const std::string& path)
    {
        std::string full = shaderPath(path);
        std::string contents = LLFile::getContents(full);
        if (contents.empty())
        {
            LL_WARNS("Vulkan") << "Shader not found or empty: " << full << LL_ENDL;
            return VK_NULL_HANDLE;
        }

        VkShaderModuleCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        ci.codeSize = contents.size();
        ci.pCode = reinterpret_cast<const uint32_t*>(contents.data());
        VkShaderModule mod = VK_NULL_HANDLE;
        if (vkCreateShaderModule(device, &ci, nullptr, &mod) != VK_SUCCESS)
        {
            LL_WARNS("Vulkan") << "vkCreateShaderModule failed: " << path << LL_ENDL;
            return VK_NULL_HANDLE;
        }
        return mod;
    }
}

bool LLVKContext::create2DPipeline(std::string& error)
{
    // (Re)create the graphics pipelines against the current swapchain format.
    for (int i = 0; i < (int)Blend2D::Count; ++i)
    {
        for (int t = 0; t < 2; ++t)
        {
            if (mPipeline2D[i][t] != VK_NULL_HANDLE) { vkDestroyPipeline(mDevice, mPipeline2D[i][t], nullptr); mPipeline2D[i][t] = VK_NULL_HANDLE; }
        }
    }

    // Shader modules are loaded once (not per swapchain recreate).
    if (mShader2DVert == VK_NULL_HANDLE)
    {
        mShader2DVert = loadShaderModule(mDevice, "shaders/compiled/ui2d.vert.spv");
        mShader2DFrag = loadShaderModule(mDevice, "shaders/compiled/ui2d.frag.spv");
        if (mShader2DVert == VK_NULL_HANDLE || mShader2DFrag == VK_NULL_HANDLE)
        {
            error = "Failed to load 2D shaders";
            return false;
        }
    }

    // Push constants: ortho projection matrix (vertex) + (later) tint.
    VkPushConstantRange push{};
    push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    push.offset = 0;
    push.size = 16 * sizeof(float); // mat4

    // Descriptor set layout + sampler + pool for the texture binding
    // (set 0 / binding 0). Created once; the pipeline layout references them.
    if (mSampler2D == VK_NULL_HANDLE)
    {
        VkSamplerCreateInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        si.magFilter = VK_FILTER_NEAREST;
        si.minFilter = VK_FILTER_NEAREST;
        si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        si.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        si.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        LL_VK_CHECK(vkCreateSampler(mDevice, &si, nullptr, &mSampler2D), error, "vkCreateSampler (2D) failed");
    }
    // GL-matched LINEAR sampler (UI images decoded with no mips + default
    // filtering sample bilinear in GL). Nearest stays the default for exact
    // (1:1) images.
    if (mSampler2DLinear == VK_NULL_HANDLE)
    {
        VkSamplerCreateInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        si.magFilter = VK_FILTER_LINEAR;
        si.minFilter = VK_FILTER_LINEAR;
        si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        si.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        si.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        LL_VK_CHECK(vkCreateSampler(mDevice, &si, nullptr, &mSampler2DLinear), error, "vkCreateSampler (2D linear) failed");
    }
    if (mDescSetLayout2D == VK_NULL_HANDLE)
    {
        VkDescriptorSetLayoutBinding bind0{};
        bind0.binding = 0;
        bind0.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bind0.descriptorCount = 1;
        bind0.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo dli{};
        dli.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        dli.bindingCount = 1;
        dli.pBindings = &bind0;
        LL_VK_CHECK(vkCreateDescriptorSetLayout(mDevice, &dli, nullptr, &mDescSetLayout2D), error, "vkCreateDescriptorSetLayout (2D) failed");
    }
    if (mDescPool2D == VK_NULL_HANDLE)
    {
        // <VulkanStorm> M2: the UI-image registry uploads ~1k textures, each
        // with its own descriptor set. Size the pool for the real count with
        // generous headroom (was 64 — exhausted, dropping every image).
        const uint32_t kMaxUISets = 4096;
        VkDescriptorPoolSize ps{};
        ps.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        ps.descriptorCount = kMaxUISets;
        VkDescriptorPoolCreateInfo pi{};
        pi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pi.maxSets = kMaxUISets;
        pi.poolSizeCount = 1;
        pi.pPoolSizes = &ps;
        LL_VK_CHECK(vkCreateDescriptorPool(mDevice, &pi, nullptr, &mDescPool2D), error, "vkCreateDescriptorPool (2D) failed");
    }

    if (mPipelineLayout2D == VK_NULL_HANDLE)
    {
        VkPipelineLayoutCreateInfo li{};
        li.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        li.setLayoutCount = 1;
        li.pSetLayouts = &mDescSetLayout2D;
        li.pushConstantRangeCount = 1;
        li.pPushConstantRanges = &push;
        LL_VK_CHECK(vkCreatePipelineLayout(mDevice, &li, nullptr, &mPipelineLayout2D), error, "vkCreatePipelineLayout (2D) failed");
    }

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = mShader2DVert;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = mShader2DFrag;
    stages[1].pName = "main";

    // Vertex input: pos(vec2), uv(vec2), color(vec4) interleaved.
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = (2 + 2 + 4) * sizeof(float);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription attrs[3]{};
    attrs[0] = { 0, 0, VK_FORMAT_R32G32_SFLOAT, 0 };
    attrs[1] = { 1, 0, VK_FORMAT_R32G32_SFLOAT, 2 * sizeof(float) };
    attrs[2] = { 2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 4 * sizeof(float) };
    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &binding;
    vi.vertexAttributeDescriptionCount = 3;
    vi.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp{};
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_FALSE;
    ds.depthWriteEnable = VK_FALSE;

    VkDynamicState dyn_states[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates = dyn_states;

    // Dynamic rendering: single color attachment at the swapchain format.
    VkPipelineRenderingCreateInfo rendering{};
    rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachmentFormats = &mSwapchainFormat;

    // One pipeline per blend mode. Factors match GL exactly (see
    // LLRender::setSceneBlendType): BT_ALPHA = SRC_ALPHA/ONE_MINUS_SRC_ALPHA
    // (both color+alpha), BT_REPLACE = ONE/ZERO (blend off), BT_ADD_WITH_ALPHA
    // = SRC_ALPHA/ONE, BT_ADD = ONE/ONE.
    for (int b = 0; b < (int)Blend2D::Count; ++b)
    {
        VkPipelineColorBlendAttachmentState blend_att{};
        blend_att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        Blend2D mode = (Blend2D)b;
        if (mode == Blend2D::Replace)
        {
            blend_att.blendEnable = VK_FALSE;
        }
        else
        {
            blend_att.blendEnable = VK_TRUE;
            blend_att.colorBlendOp = VK_BLEND_OP_ADD;
            blend_att.alphaBlendOp = VK_BLEND_OP_ADD;
            switch (mode)
            {
            case Blend2D::Alpha:
                blend_att.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                blend_att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                blend_att.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                blend_att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                break;
            case Blend2D::AddWithAlpha:
                blend_att.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                blend_att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
                blend_att.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                blend_att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                break;
            case Blend2D::Add:
                blend_att.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
                blend_att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
                blend_att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                blend_att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                break;
            default: break;
            }
        }
        VkPipelineColorBlendStateCreateInfo cb{};
        cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        cb.attachmentCount = 1;
        cb.pAttachments = &blend_att;

        VkGraphicsPipelineCreateInfo gp{};
        gp.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        gp.pNext = &rendering;
        gp.stageCount = 2;
        gp.pStages = stages;
        gp.pVertexInputState = &vi;
        gp.pViewportState = &vp;
        gp.pRasterizationState = &rs;
        gp.pMultisampleState = &ms;
        gp.pDepthStencilState = &ds;
        gp.pColorBlendState = &cb;
        gp.pDynamicState = &dyn;
        gp.layout = mPipelineLayout2D;
        // Create both topology variants (triangle list + line strip). Topology is
        // fixed at pipeline creation (not a dynamic state here), so lines need
        // their own pipeline or they'd rasterize as garbage triangles.
        for (int topo = 0; topo < 2; ++topo)
        {
            ia.topology = (topo == 0) ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST : VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
            gp.pInputAssemblyState = &ia;
            LL_VK_CHECK(vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &gp, nullptr, &mPipeline2D[b][topo]), error, "vkCreateGraphicsPipelines (2D) failed");
        }
    }

    // 1x1 white texture bound for solid (untextured) quads so the fragment
    // shader's texture() returns white and the output is just the vertex color.
    if (mWhiteTex.image == VK_NULL_HANDLE)
    {
        const uint8_t white[4] = { 255, 255, 255, 255 };
        if (!createTexture2D(white, 1, 1, mWhiteTex, error))
        {
            return false;
        }
    }

    LL_INFOS("Vulkan") << "2D UI pipeline created (format=" << (int)mSwapchainFormat << ")" << LL_ENDL;
    return true;
}

void LLVKContext::destroy2DPipeline()
{
    destroyTexture2D(mWhiteTex);
    if (mDescPool2D != VK_NULL_HANDLE) { vkDestroyDescriptorPool(mDevice, mDescPool2D, nullptr); mDescPool2D = VK_NULL_HANDLE; }
    if (mDescSetLayout2D != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(mDevice, mDescSetLayout2D, nullptr); mDescSetLayout2D = VK_NULL_HANDLE; }
    if (mSampler2D != VK_NULL_HANDLE) { vkDestroySampler(mDevice, mSampler2D, nullptr); mSampler2D = VK_NULL_HANDLE; }
    for (int i = 0; i < (int)Blend2D::Count; ++i)
    {
        for (int t = 0; t < 2; ++t)
        {
            if (mPipeline2D[i][t] != VK_NULL_HANDLE) { vkDestroyPipeline(mDevice, mPipeline2D[i][t], nullptr); mPipeline2D[i][t] = VK_NULL_HANDLE; }
        }
    }
    if (mPipelineLayout2D != VK_NULL_HANDLE) { vkDestroyPipelineLayout(mDevice, mPipelineLayout2D, nullptr); mPipelineLayout2D = VK_NULL_HANDLE; }
    if (mShader2DVert != VK_NULL_HANDLE) { vkDestroyShaderModule(mDevice, mShader2DVert, nullptr); mShader2DVert = VK_NULL_HANDLE; }
    if (mShader2DFrag != VK_NULL_HANDLE) { vkDestroyShaderModule(mDevice, mShader2DFrag, nullptr); mShader2DFrag = VK_NULL_HANDLE; }
}

VkCommandBuffer LLVKContext::begin2DFrame(float clear_r, float clear_g, float clear_b, float clear_a)
{
    if (mDevice == VK_NULL_HANDLE || mSwapchain == VK_NULL_HANDLE || mPipeline2D[(int)Blend2D::Alpha][0] == VK_NULL_HANDLE) return VK_NULL_HANDLE;

    // Degenerate extent (window not yet sized / minimized): nothing valid to
    // render into; skip the frame. Avoids the VUID renderArea>0 violation.
    if (mSwapchainExtent.width == 0 || mSwapchainExtent.height == 0)
    {
        return VK_NULL_HANDLE;
    }

    FrameSync& f = mFrames[mFrameIndex];
    vkWaitForFences(mDevice, 1, &f.inFlight, VK_TRUE, UINT64_MAX);

    VkResult acquire = vkAcquireNextImageKHR(mDevice, mSwapchain, UINT64_MAX, f.imageAvailable, VK_NULL_HANDLE, &mAcquiredImageIndex);
    if (acquire == VK_ERROR_OUT_OF_DATE_KHR) return VK_NULL_HANDLE; // caller recreates swapchain
    if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR) return VK_NULL_HANDLE;

    vkResetFences(mDevice, 1, &f.inFlight);
    vkResetCommandBuffer(f.cmd, 0);

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(f.cmd, &begin);

    // UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL.
    VkImageMemoryBarrier to_attach{};
    to_attach.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_attach.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    to_attach.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    to_attach.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_attach.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_attach.image = mSwapchainImages[mAcquiredImageIndex];
    to_attach.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    to_attach.subresourceRange.levelCount = 1;
    to_attach.subresourceRange.layerCount = 1;
    to_attach.srcAccessMask = 0;
    to_attach.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    vkCmdPipelineBarrier(f.cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &to_attach);

    VkClearColorValue clear{};
    clear.float32[0] = clear_r; clear.float32[1] = clear_g; clear.float32[2] = clear_b; clear.float32[3] = clear_a;

    VkRenderingAttachmentInfo color_attach{};
    color_attach.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color_attach.imageView = mSwapchainViews[mAcquiredImageIndex];
    color_attach.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_attach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attach.clearValue.color = clear;

    VkRenderingInfo rendering{};
    rendering.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering.renderArea.offset = { 0, 0 };
    rendering.renderArea.extent = mSwapchainExtent;
    rendering.layerCount = 1;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachments = &color_attach;

    vkCmdBeginRendering(f.cmd, &rendering);

    // Positive-height viewport: the 2D pipeline's ortho matrix maps the
    // top-left-origin UI coords to clip space directly (y inverted in the
    // matrix), so no global viewport flip is applied. Positions AND textured
    // content both land upright.
    VkViewport viewport{ 0.f, 0.f, (float)mSwapchainExtent.width, (float)mSwapchainExtent.height, 0.f, 1.f };
    vkCmdSetViewport(f.cmd, 0, 1, &viewport);
    VkRect2D scissor{ { 0, 0 }, mSwapchainExtent };
    vkCmdSetScissor(f.cmd, 0, 1, &scissor);

    vkCmdBindPipeline(f.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline2D[(int)Blend2D::Alpha][0]);
    mFrameActive = true;
    return f.cmd;
}

bool LLVKContext::end2DFrame()
{
    if (!mFrameActive) return false;
    FrameSync& f = mFrames[mFrameIndex];

    vkCmdEndRendering(f.cmd);

    VkImageMemoryBarrier to_present{};
    to_present.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_present.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    to_present.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_present.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_present.image = mSwapchainImages[mAcquiredImageIndex];
    to_present.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    to_present.subresourceRange.levelCount = 1;
    to_present.subresourceRange.layerCount = 1;
    to_present.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    to_present.dstAccessMask = 0;
    vkCmdPipelineBarrier(f.cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &to_present);

    vkEndCommandBuffer(f.cmd);

    VkSemaphore present_sem = mImagePresentSem[mAcquiredImageIndex];
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &f.imageAvailable;
    submit.pWaitDstStageMask = &wait_stage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &f.cmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &present_sem;
    if (vkQueueSubmit(mGraphicsQueue, 1, &submit, f.inFlight) != VK_SUCCESS)
    {
        mFrameActive = false;
        return false;
    }

    VkPresentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &present_sem;
    present.swapchainCount = 1;
    present.pSwapchains = &mSwapchain;
    present.pImageIndices = &mAcquiredImageIndex;
    VkResult pres = vkQueuePresentKHR(mPresentQueue, &present);

    mLastPresentedImageIndex = mAcquiredImageIndex;
    mFrameActive = false;
    mFrameIndex = (mFrameIndex + 1) % kFramesInFlight;
    return (pres == VK_SUCCESS || pres == VK_SUBOPTIMAL_KHR);
}

bool LLVKContext::readbackSwapchain(std::vector<uint8_t>& out_rgba, uint32_t& out_w, uint32_t& out_h)
{
    if (mDevice == VK_NULL_HANDLE || mSwapchain == VK_NULL_HANDLE || mSwapchainImages.empty()) return false;

    const uint32_t w = mSwapchainExtent.width;
    const uint32_t h = mSwapchainExtent.height;
    out_w = w; out_h = h;

    // The capture races the frame loop (the "last acquired" image may not be
    // the one most recently presented, and may still be in flight). Idle the
    // device so every image is settled, then read the presented image
    // deterministically.
    vkDeviceWaitIdle(mDevice);

    // Host-visible destination buffer for the copy.
    VkDeviceSize buf_size = (VkDeviceSize)w * h * 4;
    VkBufferCreateInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size = buf_size;
    bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VmaAllocationCreateInfo ai{};
    ai.usage = VMA_MEMORY_USAGE_AUTO;
    ai.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    VkBuffer staging = VK_NULL_HANDLE;
    VmaAllocation staging_alloc = VK_NULL_HANDLE;
    VmaAllocationInfo alloc_info{};
    if (vmaCreateBuffer(mAllocator, &bi, &ai, &staging, &staging_alloc, &alloc_info) != VK_SUCCESS) return false;

    // One-shot command buffer: copy the swapchain image to the buffer.
    VkCommandBufferAllocateInfo cai{};
    cai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cai.commandPool = mCommandPool;
    cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cai.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(mDevice, &cai, &cmd) != VK_SUCCESS)
    {
        vmaDestroyBuffer(mAllocator, staging, staging_alloc);
        return false;
    }

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);

    // Read the image that was most recently PRESENTED (it holds the current
    // frame's content). mAcquiredImageIndex would point at the image acquired
    // for the NEXT frame, which may be stale.
    VkImage img = mSwapchainImages[mLastPresentedImageIndex];

    VkImageMemoryBarrier to_src{};
    to_src.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_src.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    to_src.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    to_src.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_src.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_src.image = img;
    to_src.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    to_src.subresourceRange.levelCount = 1;
    to_src.subresourceRange.layerCount = 1;
    to_src.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    to_src.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &to_src);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { w, h, 1 };
    vkCmdCopyImageToBuffer(cmd, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging, 1, &region);

    VkImageMemoryBarrier to_present{};
    to_present.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_present.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    to_present.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_present.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_present.image = img;
    to_present.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    to_present.subresourceRange.levelCount = 1;
    to_present.subresourceRange.layerCount = 1;
    to_present.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    to_present.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &to_present);

    vkEndCommandBuffer(cmd);

    VkFenceCreateInfo fi{};
    fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = VK_NULL_HANDLE;
    vkCreateFence(mDevice, &fi, nullptr, &fence);

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    bool ok = (vkQueueSubmit(mGraphicsQueue, 1, &submit, fence) == VK_SUCCESS) &&
              (vkWaitForFences(mDevice, 1, &fence, VK_TRUE, UINT64_MAX) == VK_SUCCESS);

    if (ok)
    {
        out_rgba.resize((size_t)buf_size);
        memcpy(out_rgba.data(), alloc_info.pMappedData, (size_t)buf_size);

        // Swapchain is B8G8R8A8 on this driver; normalize to RGBA8 so the
        // diff harness compares the same channel order as the GL reference.
        if (mSwapchainFormat == VK_FORMAT_B8G8R8A8_UNORM ||
            mSwapchainFormat == VK_FORMAT_B8G8R8A8_SRGB)
        {
            for (size_t i = 0; i + 3 < out_rgba.size(); i += 4)
            {
                std::swap(out_rgba[i], out_rgba[i + 2]);
            }
        }

        // <VulkanStorm> Flip to the harness's BOTTOM-origin row order so the
        // Vulkan capture compares directly against the GL glReadPixels frame.
        // The swapchain copy is TOP-origin (image row 0 = screen top); the GL
        // reference is bottom-origin (row 0 = screen bottom). Without this the
        // readback is vertically inverted relative to GL and the diff measures
        // the OPPOSITE of the on-screen truth (this masked the real orientation
        // bug for a whole debugging pass).
        const size_t row = (size_t)w * 4;
        std::vector<uint8_t> tmp(row);
        for (uint32_t y = 0; y < h / 2; ++y)
        {
            uint8_t* top    = out_rgba.data() + (size_t)y * row;
            uint8_t* bottom = out_rgba.data() + (size_t)(h - 1 - y) * row;
            memcpy(tmp.data(), top, row);
            memcpy(top, bottom, row);
            memcpy(bottom, tmp.data(), row);
        }
        // </VulkanStorm>
    }

    vkDestroyFence(mDevice, fence, nullptr);
    vkFreeCommandBuffers(mDevice, mCommandPool, 1, &cmd);
    vmaDestroyBuffer(mAllocator, staging, staging_alloc);
    return ok;
}

// --- Textures (Phase 3) ----------------------------------------------------

bool LLVKContext::createTexture2D(const uint8_t* rgba, uint32_t w, uint32_t h, Texture2D& out, std::string& error, bool useLinearFilter)
{
    if (mDevice == VK_NULL_HANDLE || mDescPool2D == VK_NULL_HANDLE || !rgba || w == 0 || h == 0)
    {
        error = "createTexture2D: bad state or args";
        return false;
    }

    // Staging buffer (host-visible) -> device image.
    VkDeviceSize size = (VkDeviceSize)w * h * 4;
    VkBufferCreateInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size = size;
    bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VmaAllocationCreateInfo sai{};
    sai.usage = VMA_MEMORY_USAGE_AUTO;
    sai.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    VkBuffer staging = VK_NULL_HANDLE;
    VmaAllocation staging_alloc = VK_NULL_HANDLE;
    VmaAllocationInfo sinfo{};
    LL_VK_CHECK(vmaCreateBuffer(mAllocator, &bi, &sai, &staging, &staging_alloc, &sinfo), error, "vmaCreateBuffer (tex staging) failed");
    memcpy(sinfo.pMappedData, rgba, (size_t)size);

    VkImageCreateInfo ii{};
    ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = VK_FORMAT_R8G8B8A8_UNORM;
    ii.extent = { w, h, 1 };
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VmaAllocationCreateInfo iai{};
    iai.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    if (vmaCreateImage(mAllocator, &ii, &iai, &out.image, &out.alloc, nullptr) != VK_SUCCESS)
    {
        vmaDestroyBuffer(mAllocator, staging, staging_alloc);
        error = "vmaCreateImage failed";
        return false;
    }

    // One-shot transfer: UNDEFINED -> TRANSFER_DST -> copy -> SHADER_READ_ONLY.
    VkCommandBufferAllocateInfo cai{};
    cai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cai.commandPool = mCommandPool;
    cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cai.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(mDevice, &cai, &cmd);
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);

    VkImageMemoryBarrier pre{};
    pre.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    pre.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    pre.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    pre.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    pre.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    pre.image = out.image;
    pre.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    pre.subresourceRange.levelCount = 1;
    pre.subresourceRange.layerCount = 1;
    pre.srcAccessMask = 0;
    pre.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &pre);

    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = { w, h, 1 };
    vkCmdCopyBufferToImage(cmd, staging, out.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    VkImageMemoryBarrier post = pre;
    post.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    post.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    post.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    post.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &post);
    vkEndCommandBuffer(cmd);

    VkFenceCreateInfo fi{};
    fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = VK_NULL_HANDLE;
    vkCreateFence(mDevice, &fi, nullptr, &fence);
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    bool ok = (vkQueueSubmit(mGraphicsQueue, 1, &submit, fence) == VK_SUCCESS) &&
              (vkWaitForFences(mDevice, 1, &fence, VK_TRUE, UINT64_MAX) == VK_SUCCESS);
    vkDestroyFence(mDevice, fence, nullptr);
    vkFreeCommandBuffers(mDevice, mCommandPool, 1, &cmd);
    vmaDestroyBuffer(mAllocator, staging, staging_alloc);
    if (!ok)
    {
        vmaDestroyImage(mAllocator, out.image, out.alloc);
        out.image = VK_NULL_HANDLE; out.alloc = VK_NULL_HANDLE;
        error = "texture upload submit failed";
        return false;
    }

    VkImageViewCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image = out.image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = VK_FORMAT_R8G8B8A8_UNORM;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.layerCount = 1;
    LL_VK_CHECK(vkCreateImageView(mDevice, &vi, nullptr, &out.view), error, "vkCreateImageView (tex) failed");

    // Allocate + write the descriptor set.
    VkDescriptorSetAllocateInfo dai{};
    dai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dai.descriptorPool = mDescPool2D;
    dai.descriptorSetCount = 1;
    dai.pSetLayouts = &mDescSetLayout2D;
    LL_VK_CHECK(vkAllocateDescriptorSets(mDevice, &dai, &out.descriptor), error, "vkAllocateDescriptorSets failed");

    VkDescriptorImageInfo dii{};
    dii.sampler = useLinearFilter ? mSampler2DLinear : mSampler2D;
    dii.imageView = out.view;
    dii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = out.descriptor;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &dii;
    vkUpdateDescriptorSets(mDevice, 1, &write, 0, nullptr);

    return true;
}

void LLVKContext::destroyTexture2D(Texture2D& tex)
{
    if (mDevice == VK_NULL_HANDLE) return;
    if (tex.view != VK_NULL_HANDLE) { vkDestroyImageView(mDevice, tex.view, nullptr); tex.view = VK_NULL_HANDLE; }
    if (tex.image != VK_NULL_HANDLE) { vmaDestroyImage(mAllocator, tex.image, tex.alloc); tex.image = VK_NULL_HANDLE; tex.alloc = VK_NULL_HANDLE; }
    // Descriptor sets free with the pool.
    tex.descriptor = VK_NULL_HANDLE;
}

void LLVKContext::bindTexture2D(VkCommandBuffer cmd, VkDescriptorSet descriptor)
{
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout2D, 0, 1, &descriptor, 0, nullptr);
}
