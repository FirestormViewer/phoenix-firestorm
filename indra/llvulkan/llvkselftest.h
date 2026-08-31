/**
 * @file llvkselftest.h
 * @brief Isolated Vulkan self-test driver (Phase 1 bring-up).
 *
 * @details
 * Drives the Vulkan chain end-to-end on real hardware — Win32 surface creation
 * -> LLVKContext (instance/device/swapchain) -> clear-color frame -> present —
 * for a handful of frames, then stops, leaving the viewer on its normal GL path.
 *
 * This is gated behind the RenderVulkanSelfTest debug setting and is strictly
 * opt-in; it does not alter the GL render path. It exists to validate the
 * Phase-1 Vulkan infrastructure on real hardware before the viewer's render
 * loop is decoupled from GL (Phase 2).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, The Vulkanstorm Project
 * $/LicenseInfo$
 */

#ifndef LLVKSELFTEST_H
#define LLVKSELFTEST_H

class LLWindow;

namespace LLVKSelfTest
{
    // Begin the self-test against the given window's Vulkan surface. Creates the
    // LLVKContext (instance/device/queues/swapchain). Safe to call repeatedly;
    // only the first call initializes. Returns true if the context came up.
    bool begin(LLWindow* window);

    // Render one clear-color frame and present it. No-op if not begun. Call once
    // per frame from the viewer's swap path while the self-test is active.
    void renderFrame();

    // True once the self-test has produced its target number of frames.
    bool finished();

    // Tear down the context (called automatically when finished, or at shutdown).
    void shutdown();
}

#endif // LLVKSELFTEST_H
