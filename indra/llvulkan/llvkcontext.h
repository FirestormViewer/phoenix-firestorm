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

    // <VulkanStorm> Capability probe (Stage 2): measure device memory bandwidth
    // with a Vulkan-native timestamped copy loop, to feed the GPU-class
    // thresholds (replacing the GL benchmark, which needs a GL context). Runs
    // once, off the UI path, on a one-shot command buffer. Returns GB/s, or a
    // negative value if timestamp queries are unsupported or the run fails
    // (caller falls back to a safe GPU class). Call after createDevice().
    float measureMemoryBandwidthGBps();
    // </VulkanStorm>

    // --- Phase 3 (2D/UI) -------------------------------------------------
    // The 2D blend modes the UI uses (subset of LLRender::eBlendType), matching
    // the GL factors exactly. Order matters (indexes the pipeline array).
    enum class Blend2D : uint8_t { Alpha = 0, Replace, AddWithAlpha, Add, Count };
    // Create the 2D quad pipelines (one per Blend2D x topology), dynamic
    // viewport/scissor. Call once after createDevice; rebuilt against the
    // swapchain format on each createSwapchain.
    bool create2DPipeline(std::string& error);
    void destroy2DPipeline();

    // Begin a 2D frame: acquire the next swapchain image, begin the command
    // buffer, and start dynamic rendering. Returns the frame's command buffer,
    // or VK_NULL_HANDLE if the swapchain is out of date (caller recreates).
    VkCommandBuffer begin2DFrame(float clear_r, float clear_g, float clear_b, float clear_a);
    // Close the current 2D frame (end rendering, transition to present, submit,
    // present). Returns false on failure.
    bool end2DFrame();

    // Read the most recently presented swapchain image back into out_rgba
    // (row-major RGBA8). For the GL<->Vulkan screenshot-diff harness.
    bool readbackSwapchain(std::vector<uint8_t>& out_rgba, uint32_t& out_w, uint32_t& out_h);

    // A GPU texture + its descriptor for the 2D pipeline (set 0 / binding 0).
    struct Texture2D
    {
        VkImage image = VK_NULL_HANDLE;
        VmaAllocation alloc = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkDescriptorSet descriptor = VK_NULL_HANDLE;
    };
    bool createTexture2D(const uint8_t* rgba, uint32_t w, uint32_t h, Texture2D& out, std::string& error, bool useLinearFilter = false);
    bool updateTexture2D(const uint8_t* rgba, uint32_t w, uint32_t h,
                         Texture2D& texture, std::string& error);
    void destroyTexture2D(Texture2D& tex);
    void bindTexture2D(VkCommandBuffer cmd, VkDescriptorSet descriptor);
    VkDescriptorSet whiteTextureDescriptor() const { return mWhiteTex.descriptor; }

    // Wait for all submitted work before clients release textures referenced
    // by recorded UI command buffers.
    void waitIdle() { if (mDevice != VK_NULL_HANDLE) vkDeviceWaitIdle(mDevice); }

    VkCommandBuffer currentCmd() const { return mFrames[mFrameIndex].cmd; }
    // line=true selects the line-strip topology variant (topology is baked into
    // the pipeline at creation, not dynamic).
    VkPipeline pipeline2D(Blend2D blend, bool line = false) const { return mPipeline2D[(int)blend][line ? 1 : 0]; }
    VkPipelineLayout pipelineLayout2D() const { return mPipelineLayout2D; }
    const VkExtent2D& swapchainExtent() const { return mSwapchainExtent; }

    void destroy();

    bool isValid() const { return mDevice != VK_NULL_HANDLE; }
    VkInstance instance() const { return mInstance; }
    VkDevice device() const { return mDevice; }
    VkPhysicalDevice physicalDevice() const { return mPhysicalDevice; }
    VmaAllocator allocator() const { return mAllocator; }
    VkQueue graphicsQueue() const { return mGraphicsQueue; }
    const std::string& deviceName() const { return mDeviceName; }

private:
    struct FrameSync
    {
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkSemaphore imageAvailable = VK_NULL_HANDLE;
        VkFence inFlight = VK_NULL_HANDLE;
    };

    void destroySwapchain();
    bool createFrameResources();
    void destroyImageSync();

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

    // Per-swapchain-image "present complete" semaphores. Signaled by the
    // graphics submit and waited on by present for the SAME acquired image, so
    // a semaphore is never reused while its swapchain image is still in flight
    // (fixes the swapchain-semaphore-reuse validation warning). Sized to the
    // swapchain image count; created/destroyed with the swapchain.
    std::vector<VkSemaphore> mImagePresentSem;

    // --- Phase 3 (2D/UI) -------------------------------------------------
    VkPipelineLayout mPipelineLayout2D = VK_NULL_HANDLE;
    // [blend][topology]: topology 0 = TRIANGLE_LIST, 1 = LINE_STRIP.
    VkPipeline       mPipeline2D[(int)Blend2D::Count][2] = {};
    VkShaderModule   mShader2DVert = VK_NULL_HANDLE;
    VkShaderModule   mShader2DFrag = VK_NULL_HANDLE;
    VkDescriptorSetLayout mDescSetLayout2D = VK_NULL_HANDLE;
    VkDescriptorPool      mDescPool2D = VK_NULL_HANDLE;
    VkSampler             mSampler2D = VK_NULL_HANDLE;        // NEAREST (exact; default UI)
    VkSampler             mSampler2DLinear = VK_NULL_HANDLE;  // LINEAR (GL TFO_NONE UI images)
    Texture2D             mWhiteTex;   // 1x1 white, bound for solid quads
    uint32_t         mAcquiredImageIndex = 0;
    uint32_t         mLastPresentedImageIndex = 0;
    bool             mFrameActive = false;

    bool        mValidation = false;
    std::string mDeviceName;
};

#endif // LLVKCONTEXT_H
