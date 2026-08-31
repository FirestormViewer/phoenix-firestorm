/**
 * @file llvkselftest.cpp
 * @brief Implementation of the isolated Vulkan self-test driver.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, The Vulkanstorm Project
 * $/LicenseInfo$
 */

#include "llvkselftest.h"

#include "llvkcontext.h"
#include "llvkprobe.h"
#include "llwindow.h"

#include "llerror.h"

namespace
{
    LLVKContext* g_ctx = nullptr;      // owned; deleted on shutdown
    int          g_frames_rendered = 0;
    constexpr int kTargetFrames = 120; // ~2s at 60fps
    bool         g_begun = false;
    bool         g_failed = false;
}

namespace LLVKSelfTest
{
    bool begin(LLWindow* window)
    {
        if (g_begun || g_failed)
        {
            return g_begun && !g_failed;
        }

        if (!window)
        {
            g_failed = true;
            return false;
        }

        std::string error;
        LLVKContext* ctx = new LLVKContext();

        if (!ctx->createInstance(false, error))
        {
            LL_WARNS("Vulkan") << "SelfTest: createInstance failed: " << error << LL_ENDL;
            delete ctx;
            g_failed = true;
            return false;
        }

        // Create the Win32 surface for the window.
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        if (!window->createVulkanSurface((void*)ctx->instance(), (void*)&surface) || surface == VK_NULL_HANDLE)
        {
            LL_WARNS("Vulkan") << "SelfTest: createVulkanSurface failed" << LL_ENDL;
            delete ctx;
            g_failed = true;
            return false;
        }

        if (!ctx->pickPhysicalDevice(surface, error) ||
            !ctx->createDevice(surface, error))
        {
            LL_WARNS("Vulkan") << "SelfTest: device setup failed: " << error << LL_ENDL;
            vkDestroySurfaceKHR(ctx->instance(), surface, nullptr);
            delete ctx;
            g_failed = true;
            return false;
        }

        // Size the swapchain to the window's client area.
        LLCoordWindow size;
        window->getSize(&size);
        uint32_t w = size.mX > 0 ? (uint32_t)size.mX : 640;
        uint32_t h = size.mY > 0 ? (uint32_t)size.mY : 480;

        if (!ctx->createSwapchain(surface, w, h, error))
        {
            LL_WARNS("Vulkan") << "SelfTest: createSwapchain failed: " << error << LL_ENDL;
            vkDestroySurfaceKHR(ctx->instance(), surface, nullptr);
            delete ctx;
            g_failed = true;
            return false;
        }

        g_ctx = ctx;
        g_begun = true;
        g_frames_rendered = 0;
        LL_INFOS("Vulkan") << "SelfTest: Vulkan context up on '" << ctx->deviceName()
                           << "', swapchain " << w << "x" << h << ". Rendering test frames." << LL_ENDL;
        return true;
    }

    void renderFrame()
    {
        if (!g_begun || g_failed || !g_ctx)
        {
            return;
        }

        // A recognizable clear color (a deep teal) so it's obvious on screen.
        if (g_ctx->renderClearFrame(0.05f, 0.35f, 0.45f, 1.0f))
        {
            ++g_frames_rendered;
        }

        if (finished())
        {
            LL_INFOS("Vulkan") << "SelfTest: rendered " << g_frames_rendered
                               << " frames; Vulkan chain validated. Shutting down test context." << LL_ENDL;
            shutdown();
        }
    }

    bool finished()
    {
        return g_begun && g_frames_rendered >= kTargetFrames;
    }

    void shutdown()
    {
        if (g_ctx)
        {
            // The surface is owned/destroyed alongside the context's swapchain
            // reference; destroy the context (which destroys device + instance).
            delete g_ctx;
            g_ctx = nullptr;
        }
        g_begun = false;
        g_frames_rendered = 0;
    }
}
