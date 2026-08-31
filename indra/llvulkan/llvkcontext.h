/**
 * @file llvkcontext.h
 * @brief Vulkan render context: instance, device, queues, swapchain, frame loop.
 *
 * @details
 * LLVKContext is the root of the Vulkan render backend. It owns the VkInstance,
 * the selected VkPhysicalDevice + logical VkDevice, the graphics/present queues,
 * the swapchain, the VMA allocator, and the per-frame command buffers used to
 * drive a minimal render (Phase 1: a clear color).
 *
 * Lifetime: created after the window exists (a VkSurfaceKHR is supplied by the
 * LLWindow backend), lives for the rest of the process, destroyed at shutdown.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, The Vulkanstorm Project
 * $/LicenseInfo$
 */

#ifndef LLVKCONTEXT_H
#define LLVKCONTEXT_H

#include "volk/volk.h"
#include "vma/vk_mem_alloc.h"

#include <string>
#include <vector>

class LLVKContext
{
public:
    LLVKContext() = default;
    ~LLVKContext();

    // Non-copyable, non-movable (owns raw Vulkan handles).
    LLVKContext(const LLVKContext&) = delete;
    LLVKContext& operator=(const LLVKContext&) = delete;

    // Create instance + device (no surface yet). enableValidation turns on the
    // standard validation layer when available (RenderVulkanDebug). Returns
    // false with an explanatory message in error on failure.
    bool createInstance(bool enableValidation, std::string& error);
    bool pickPhysicalDevice(VkSurfaceKHR surface, std::string& error);
    bool createDevice(VkSurfaceKHR surface, std::string& error);

    // Attach the platform surface and (re)create the swapchain for the given
    // Create the platform VkSurfaceKHR from the window's native handles
    // (Win32: HWND + HINSTANCE). Typed here (llvulkan owns the volk table).
    // Returns VK_NULL_HANDLE on failure.
    VkSurfaceKHR createSurface(void* native_window, void* native_instance);

    // Attach the platform surface and (re)create the swapchain for the given
    // extent. Call on window resize with the new extent.
    bool createSwapchain(VkSurfaceKHR surface, uint32_t width, uint32_t height, std::string& error);

    // Record + submit one frame that clears the swapchain image to the given
    // color and presents it. Returns false on failure.
    bool renderClearFrame(float r, float g, float b, float a);

    void destroy();

    bool isValid() const { return mDevice != VK_NULL_HANDLE; }
    VkInstance instance() const { return mInstance; }
    VkDevice device() const { return mDevice; }
    VkPhysicalDevice physicalDevice() const { return mPhysicalDevice; }
    const std::string& deviceName() const { return mDeviceName; }

private:
    struct FrameSync
    {
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkSemaphore imageAvailable = VK_NULL_HANDLE;
        VkSemaphore renderFinished = VK_NULL_HANDLE;
        VkFence inFlight = VK_NULL_HANDLE;
    };

    void destroySwapchain();
    bool createFrameResources();

    VkInstance       mInstance = VK_NULL_HANDLE;
    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkDevice         mDevice = VK_NULL_HANDLE;
    VmaAllocator     mAllocator = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT mDebugMessenger = VK_NULL_HANDLE;

    uint32_t  mGraphicsQueueFamily = UINT32_MAX;
    uint32_t  mPresentQueueFamily = UINT32_MAX;
    VkQueue   mGraphicsQueue = VK_NULL_HANDLE;
    VkQueue   mPresentQueue = VK_NULL_HANDLE;

    VkSurfaceKHR   mSurface = VK_NULL_HANDLE;
    VkSwapchainKHR mSwapchain = VK_NULL_HANDLE;
    VkFormat       mSwapchainFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D     mSwapchainExtent{ 0, 0 };
    std::vector<VkImage>     mSwapchainImages;
    std::vector<VkImageView> mSwapchainViews;

    VkCommandPool mCommandPool = VK_NULL_HANDLE;
    static constexpr uint32_t kFramesInFlight = 2;
    FrameSync mFrames[kFramesInFlight];
    uint32_t  mFrameIndex = 0;

    bool        mValidation = false;
    std::string mDeviceName;
};

#endif // LLVKCONTEXT_H
