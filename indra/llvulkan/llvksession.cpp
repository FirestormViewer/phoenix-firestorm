/**
 * @file llvksession.cpp
 * @brief Implementation of the Vulkan session (see llvksession.h).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, The Vulkanstorm Project
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "llvksession.h"

#include "llerror.h"

#include <cstdlib>

#if LL_WINDOWS

#include "llvkcontext.h"
#include "llvkgpufacts.h"
#include "llvkui2d.h"
#include "llvkuirender.h"
#include "llwindow.h"

namespace
{
    LLVKContext* s_context = nullptr;
    VkSurfaceKHR s_surface = VK_NULL_HANDLE;
    uint32_t     s_width = 0;
    uint32_t     s_height = 0;

    // Boot-path clear color. Matches the Phase-1 self-test teal so a Vulkan-
    // owned frame is unmistakable during bring-up.
    constexpr float kClearR = 0.0f;
    constexpr float kClearG = 0.5f;
    constexpr float kClearB = 0.5f;
    constexpr float kClearA = 1.0f;

    void queryClientSize(LLWindow* window, uint32_t& width, uint32_t& height)
    {
        width = 0;
        height = 0;
        if (window)
        {
            LLCoordWindow size;
            if (window->getSize(&size))
            {
                width = (uint32_t)llmax(1, size.mX);
                height = (uint32_t)llmax(1, size.mY);
            }
        }
    }
}

bool LLVKSession::start(LLWindow* window, bool enable_validation)
{
    if (s_context)
    {
        LL_WARNS("Vulkan") << "Session: start() called while already running" << LL_ENDL;
        return true;
    }
    if (!window)
    {
        LL_WARNS("Vulkan") << "Session: no window to bind to" << LL_ENDL;
        return false;
    }

    void* native_window = window->getNativeHandle();
    void* native_instance = window->getNativeInstance();
    if (!native_window || !native_instance)
    {
        LL_WARNS("Vulkan") << "Session: window exposes no native handles" << LL_ENDL;
        return false;
    }

    std::string error;
    LLVKContext* ctx = new LLVKContext();

    if (!ctx->createInstance(enable_validation, error))
    {
        LL_WARNS("Vulkan") << "Session: createInstance failed: " << error << LL_ENDL;
        delete ctx;
        return false;
    }

    // The window was created with no GL context (LLWindow::setSkipGLContext),
    // so it is free to be owned by Vulkan.
    s_surface = ctx->createSurface(native_window, native_instance);
    if (s_surface == VK_NULL_HANDLE)
    {
        LL_WARNS("Vulkan") << "Session: surface creation failed" << LL_ENDL;
        delete ctx;
        return false;
    }

    if (!ctx->pickPhysicalDevice(s_surface, error) ||
        !ctx->createDevice(s_surface, error))
    {
        LL_WARNS("Vulkan") << "Session: device setup failed: " << error << LL_ENDL;
        vkDestroySurfaceKHR(ctx->instance(), s_surface, nullptr);
        s_surface = VK_NULL_HANDLE;
        delete ctx;
        return false;
    }

    queryClientSize(window, s_width, s_height);
    if (!ctx->createSwapchain(s_surface, s_width, s_height, error))
    {
        LL_WARNS("Vulkan") << "Session: swapchain creation failed: " << error << LL_ENDL;
        // createSwapchain() adopts the surface on entry; the context destroys
        // it on teardown.
        delete ctx;
        s_surface = VK_NULL_HANDLE;
        return false;
    }

    s_context = ctx;
    LL_INFOS("Vulkan") << "Session: Vulkan owns the viewer window ("
                       << s_width << "x" << s_height << ", device: "
                       << ctx->deviceName() << ")" << LL_ENDL;

    // <VulkanStorm> Capability probe Stage 2: now that a logical device exists,
    // measure memory bandwidth and publish it to the facts snapshot. The static
    // facts were captured in the early LLVKProbe enumeration (Stage 1). The
    // class-dependent feature settings were withheld (bounded-defer); the
    // caller (newview, llappviewer.cpp) resolves the GPU class right after this
    // returns, since LLFeatureManager lives in newview and llvulkan must not
    // depend on it (would be a circular newview<->llvulkan link).
    LLVKGpuFacts::setBandwidth(ctx->measureMemoryBandwidthGBps());
    // </VulkanStorm>
    return true;
}

bool LLVKSession::isRunning()
{
    return s_context != nullptr;
}

void LLVKSession::renderFrame()
{
    if (!s_context)
    {
        return;
    }
    if (!s_context->renderClearFrame(kClearR, kClearG, kClearB, kClearA))
    {
        LL_WARNS("Vulkan") << "Session: renderClearFrame failed" << LL_ENDL;
    }
}

// <VulkanStorm> Phase 3 v2 (M0 greenfield)
void LLVKSession::renderUIFrame(LLView* root, float ui_scale_x, float ui_scale_y)
{
    if (!s_context)
    {
        return;
    }

    // Lazily create the 2D pipeline once the device exists (shaders load on
    // first use; createSwapchain rebuilds the pipelines on resize).
    if (s_context->pipeline2D(LLVKContext::Blend2D::Alpha) == VK_NULL_HANDLE)
    {
        std::string error;
        if (!s_context->create2DPipeline(error))
        {
            LL_WARNS("Vulkan") << "Session: create2DPipeline failed: " << error << LL_ENDL;
            return;
        }
    }

    // Begin the 2D render pass (clears to the boot teal so any uncovered region
    // is unmistakable during bring-up).
    VkCommandBuffer cmd = s_context->begin2DFrame(kClearR, kClearG, kClearB, kClearA);
    if (cmd == VK_NULL_HANDLE)
    {
        return; // swapchain out of date; the caller resizes next frame
    }

    // Begin the sink, render the widget tree from its readable state, flush.
    LLVKUI2DSink::get().begin(s_context, cmd);
    LLVKUIRender::renderFrame(s_context, root, s_width, s_height, ui_scale_x, ui_scale_y);
    LLVKUI2DSink::get().end();

    // <VulkanStorm> M0 diagnostic: log the emitted vertex count so we can tell
    // whether the state-reading tree walk produced any geometry (teal = zero
    // verts emitted or all off-screen). VULKANSTORM_UI_DEBUG=1 enables it.
    static bool s_dbg = getenv("VULKANSTORM_UI_DEBUG") != nullptr;
    if (s_dbg)
    {
        static int s_f = 0;
        if ((s_f++ % 60) == 0)
        {
            LL_INFOS("Vulkan") << "renderUIFrame: vertsDrawn=" << LLVKUI2DSink::get().frameVertsEmitted()
                               << " flushes=" << LLVKUI2DSink::get().frameFlushes()
                               << " extent=" << s_width << "x" << s_height << LL_ENDL;
        }
    }
    // </VulkanStorm>

    if (!s_context->end2DFrame())
    {
        LL_WARNS("Vulkan") << "Session: end2DFrame failed" << LL_ENDL;
    }
}
// </VulkanStorm>

void LLVKSession::resizeIfNeeded(LLWindow* window)
{
    if (!s_context)
    {
        return;
    }

    uint32_t width, height;
    queryClientSize(window, width, height);
    if (width == 0 || height == 0 || (width == s_width && height == s_height))
    {
        return;
    }

    std::string error;
    if (!s_context->createSwapchain(s_surface, width, height, error))
    {
        LL_WARNS("Vulkan") << "Session: swapchain recreate failed ("
                           << width << "x" << height << "): " << error << LL_ENDL;
        return;
    }
    s_width = width;
    s_height = height;
    LL_INFOS("Vulkan") << "Session: swapchain resized to " << s_width << "x" << s_height << LL_ENDL;
}

void LLVKSession::stop()
{
    if (!s_context)
    {
        return;
    }
    // destroy() idles the device and releases the swapchain + surface.
    s_context->destroy();
    delete s_context;
    s_context = nullptr;
    s_surface = VK_NULL_HANDLE;
    s_width = s_height = 0;
    LL_INFOS("Vulkan") << "Session: stopped" << LL_ENDL;
}

std::string LLVKSession::deviceName()
{
    return s_context ? s_context->deviceName() : std::string();
}

#else // !LL_WINDOWS

bool LLVKSession::start(LLWindow* window, bool enable_validation)
{
    LL_WARNS("Vulkan") << "Session: Vulkan boot is only implemented on Windows" << LL_ENDL;
    return false;
}

bool LLVKSession::isRunning() { return false; }
void LLVKSession::renderFrame() {}
void LLVKSession::resizeIfNeeded(LLWindow* window) {}
void LLVKSession::stop() {}
std::string LLVKSession::deviceName() { return std::string(); }

#endif // LL_WINDOWS
