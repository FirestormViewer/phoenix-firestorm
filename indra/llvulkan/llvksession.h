/**
 * @file llvksession.h
 * @brief Vulkan session: owns the LLVKContext bound to the viewer's window.
 *
 * @details
 * Phase 2 (boot-on-Vulkan): when RenderBackend=Vulkan, the viewer window is
 * created WITHOUT an OpenGL context (a window is owned by exactly one
 * graphics API for its lifetime) and this session drives the frame path —
 * currently a clear color presented through the swapchain, standing in for
 * the not-yet-ported 2D/3D pipelines.
 *
 * The session is a process-wide singleton (one viewer window, one swapchain),
 * started from LLAppViewer::initWindow() once the window exists, pumped from
 * the display loop, and stopped before the window is destroyed at shutdown.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, The Vulkanstorm Project
 * $/LicenseInfo$
 */

#ifndef LLVKSESSION_H
#define LLVKSESSION_H

#include <string>

class LLWindow;

class LLVKSession
{
public:
    // Bind Vulkan to the given window: instance, surface, physical/logical
    // device, swapchain sized to the window's client area. enable_validation
    // turns on the standard validation layer (RenderVulkanDebug).
    // Returns false (and logs the reason) on failure; the caller should treat
    // that as "no presentation this session" — the process stays alive.
    static bool start(LLWindow* window, bool enable_validation);

    // True between a successful start() and stop().
    static bool isRunning();

    // Record + submit one frame (clear color) and present it. No-op when not
    // running.
    static void renderFrame();

    // <VulkanStorm> Phase 3 v2 (M0 greenfield): render one 2D UI frame from the
    // widget tree's current state. Begins the 2D render pass + the LLVKUI2D
    // sink, runs LLVKUIRender over the root view, then closes + presents.
    // ui_scale_x/y are the neutral UI scale factor (the caller reads
    // LLUI::getScaleFactor(); llvulkan does not depend on llui). No-op when not
    // running or when the swapchain is out of date (caller resizes).
    static void renderUIFrame(class LLView* root, float ui_scale_x, float ui_scale_y);
    // </VulkanStorm>

    // Recreate the swapchain if the window's client size changed since the
    // last frame. No-op when not running.
    static void resizeIfNeeded(LLWindow* window);

    // Idle the device and tear down surface/swapchain/device. Call BEFORE the
    // window is destroyed. No-op when not running.
    static void stop();

    static std::string deviceName();
};

#endif // LLVKSESSION_H
