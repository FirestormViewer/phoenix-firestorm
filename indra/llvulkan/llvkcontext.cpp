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

#include <cstring>
#include <set>

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
    app_info.apiVersion = VK_API_VERSION_1_2;

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
        for (const auto& lp : available)
        {
            if (std::strcmp(lp.layerName, "VK_LAYER_KHRONOS_validation") == 0)
            {
                have_validation = true;
                break;
            }
        }
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

    LL_VK_CHECK(vkCreateInstance(&create_info, nullptr, &mInstance), error, "vkCreateInstance failed");

    volkLoadInstance(mInstance);
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
        if (gfx == UINT32_MAX) continue;
        if (surface != VK_NULL_HANDLE && present == UINT32_MAX) continue;

        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(dev, &props);
        int score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score = 3;
        else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score = 2;
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
    }

    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
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
    alloc_info.vulkanApiVersion = VK_API_VERSION_1_2;
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
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
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
        if (f.renderFinished == VK_NULL_HANDLE && vkCreateSemaphore(mDevice, &sem, nullptr, &f.renderFinished) != VK_SUCCESS) return false;
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

    // Transition to TRANSFER_DST for clearing.
    VkImageMemoryBarrier to_clear{};
    to_clear.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_clear.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    to_clear.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_clear.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_clear.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_clear.image = image;
    to_clear.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    to_clear.subresourceRange.levelCount = 1;
    to_clear.subresourceRange.layerCount = 1;
    to_clear.srcAccessMask = 0;
    to_clear.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(f.cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &to_clear);

    VkClearColorValue clear{};
    clear.float32[0] = r; clear.float32[1] = g; clear.float32[2] = b; clear.float32[3] = a;
    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.levelCount = 1;
    range.layerCount = 1;
    vkCmdClearColorImage(f.cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &range);

    // Transition to PRESENT.
    VkImageMemoryBarrier to_present{};
    to_present.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_present.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    to_present.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_present.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_present.image = image;
    to_present.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    to_present.subresourceRange.levelCount = 1;
    to_present.subresourceRange.layerCount = 1;
    to_present.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    to_present.dstAccessMask = 0;
    vkCmdPipelineBarrier(f.cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &to_present);

    vkEndCommandBuffer(f.cmd);

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &f.imageAvailable;
    submit.pWaitDstStageMask = &wait_stage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &f.cmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &f.renderFinished;
    if (vkQueueSubmit(mGraphicsQueue, 1, &submit, f.inFlight) != VK_SUCCESS)
    {
        return false;
    }

    VkPresentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &f.renderFinished;
    present.swapchainCount = 1;
    present.pSwapchains = &mSwapchain;
    present.pImageIndices = &image_index;
    VkResult pres = vkQueuePresentKHR(mPresentQueue, &present);

    mFrameIndex = (mFrameIndex + 1) % kFramesInFlight;
    return (pres == VK_SUCCESS || pres == VK_SUBOPTIMAL_KHR);
}

void LLVKContext::destroySwapchain()
{
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

void LLVKContext::destroy()
{
    if (mDevice != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(mDevice);
    }

    destroySwapchain();

    for (uint32_t i = 0; i < kFramesInFlight; ++i)
    {
        FrameSync& f = mFrames[i];
        if (mDevice != VK_NULL_HANDLE)
        {
            if (f.imageAvailable != VK_NULL_HANDLE) vkDestroySemaphore(mDevice, f.imageAvailable, nullptr);
            if (f.renderFinished != VK_NULL_HANDLE) vkDestroySemaphore(mDevice, f.renderFinished, nullptr);
            if (f.inFlight != VK_NULL_HANDLE) vkDestroyFence(mDevice, f.inFlight, nullptr);
        }
        f = FrameSync{};
    }

    if (mCommandPool != VK_NULL_HANDLE) { vkDestroyCommandPool(mDevice, mCommandPool, nullptr); mCommandPool = VK_NULL_HANDLE; }
    if (mAllocator != VK_NULL_HANDLE) { vmaDestroyAllocator(mAllocator); mAllocator = VK_NULL_HANDLE; }
    if (mDevice != VK_NULL_HANDLE) { vkDestroyDevice(mDevice, nullptr); mDevice = VK_NULL_HANDLE; }
    if (mInstance != VK_NULL_HANDLE) { vkDestroyInstance(mInstance, nullptr); mInstance = VK_NULL_HANDLE; }

    volkFinalize();
}
