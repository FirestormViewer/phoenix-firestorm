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

#include "llerror.h"

#if LL_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace
{
    LLVKContext* g_ctx = nullptr;      // owned; deleted on shutdown
    int          g_frames_rendered = 0;
    constexpr int kTargetFrames = 120; // ~2s at 60fps
    bool         g_begun = false;
    bool         g_failed = false;

#if LL_WINDOWS
    HWND         g_test_hwnd = NULL;
    HINSTANCE    g_test_hinstance = NULL;
    VkSurfaceKHR g_surface = VK_NULL_HANDLE;

    LRESULT CALLBACK SelfTestWndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l)
    {
        return DefWindowProc(hwnd, msg, w, l);
    }

    // The self-test renders into its OWN window. The viewer's main window is
    // unusable: it already has an OpenGL surface bound, and a window can be
    // associated with only one graphics API at a time (vkCreateSwapchainKHR ->
    // VK_ERROR_NATIVE_WINDOW_IN_USE_KHR on a GL-owned window).
    HWND createTestWindow(HINSTANCE& out_instance)
    {
        HINSTANCE hinst = GetModuleHandle(NULL);
        out_instance = hinst;

        const char* kClass = "VulkanstormVKSelfTest";
        WNDCLASSA wc{};
        wc.style = CS_OWNDC;
        wc.lpfnWndProc = SelfTestWndProc;
        wc.hInstance = hinst;
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = kClass;
        RegisterClassA(&wc);

        int w = 640, h = 480;
        int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
        int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
        HWND hwnd = CreateWindowExA(0, kClass, "Vulkanstorm Vulkan self-test",
                                    WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                    x, y, w, h, NULL, NULL, hinst, NULL);
        if (hwnd)
        {
            ShowWindow(hwnd, SW_SHOW);
            UpdateWindow(hwnd);
        }
        return hwnd;
    }
#endif
}

namespace LLVKSelfTest
{
    bool begin(bool enableValidation)
    {
#if LL_WINDOWS
        if (g_begun || g_failed)
        {
            return g_begun && !g_failed;
        }

        std::string error;
        LLVKContext* ctx = new LLVKContext();

        if (!ctx->createInstance(enableValidation, error))
        {
            LL_WARNS("Vulkan") << "SelfTest: createInstance failed: " << error << LL_ENDL;
            delete ctx;
            g_failed = true;
            return false;
        }

        // Our own window — the viewer's window already has a GL surface.
        g_test_hwnd = createTestWindow(g_test_hinstance);
        if (!g_test_hwnd)
        {
            LL_WARNS("Vulkan") << "SelfTest: failed to create test window" << LL_ENDL;
            delete ctx;
            g_failed = true;
            return false;
        }

        g_surface = ctx->createSurface((void*)g_test_hwnd, (void*)g_test_hinstance);
        if (g_surface == VK_NULL_HANDLE)
        {
            LL_WARNS("Vulkan") << "SelfTest: surface creation failed" << LL_ENDL;
            delete ctx;
            g_failed = true;
            return false;
        }

        if (!ctx->pickPhysicalDevice(g_surface, error) ||
            !ctx->createDevice(g_surface, error))
        {
            LL_WARNS("Vulkan") << "SelfTest: device setup failed: " << error << LL_ENDL;
            vkDestroySurfaceKHR(ctx->instance(), g_surface, nullptr);
            delete ctx;
            g_failed = true;
            return false;
        }

        RECT rc;
        GetClientRect(g_test_hwnd, &rc);
        uint32_t w = (uint32_t)(rc.right - rc.left);
        uint32_t h = (uint32_t)(rc.bottom - rc.top);
        if (w == 0) w = 640;
        if (h == 0) h = 480;

        if (!ctx->createSwapchain(g_surface, w, h, error))
        {
            LL_WARNS("Vulkan") << "SelfTest: createSwapchain failed: " << error << LL_ENDL;
            vkDestroySurfaceKHR(ctx->instance(), g_surface, nullptr);
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
#else
        (void)enableValidation;
        return false;
#endif
    }

    void renderFrame()
    {
#if LL_WINDOWS
        if (!g_begun || g_failed || !g_ctx)
        {
            return;
        }

        // Pump the test window's messages so it stays responsive.
        MSG msg;
        while (PeekMessage(&msg, g_test_hwnd, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
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
#endif
    }

    bool finished()
    {
        return g_begun && g_frames_rendered >= kTargetFrames;
    }

    void shutdown()
    {
#if LL_WINDOWS
        if (g_ctx)
        {
            // Destroy the swapchain/surface before the context tears down the
            // instance, then destroy the test window.
            if (g_surface != VK_NULL_HANDLE)
            {
                vkDestroySurfaceKHR(g_ctx->instance(), g_surface, nullptr);
                g_surface = VK_NULL_HANDLE;
            }
            delete g_ctx;
            g_ctx = nullptr;
        }
        if (g_test_hwnd)
        {
            DestroyWindow(g_test_hwnd);
            g_test_hwnd = NULL;
        }
#endif
        g_begun = false;
        g_frames_rendered = 0;
    }
}
