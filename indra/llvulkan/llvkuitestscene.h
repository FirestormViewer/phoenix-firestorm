/**
 * @file llvkuitestscene.h
 * @brief Deterministic UI test scene for the GL<->Vulkan A/B capture harness.
 *
 * @details
 * Env-gated (VULKANSTORM_UITEST=1). When enabled, the pre-login frame renders
 * a FIXED set of UI primitives (solid rects, lines, textured images incl. a
 * 9-slice) instead of the live login widget tree, so the two backends can be
 * captured (VULKANSTORM_CAPTURE=<path.rgba>) and diffed numerically without
 * login/network/world.
 *
 * This module is the SINGLE SOURCE OF TRUTH for the scene geometry: pure
 * neutral data (no GL, no Vulkan API). Each backend renders the data with its
 * OWN methods only:
 *   - GL reference (READ-ONLY authority): gl_render_ui_test_scene() in
 *     newview/llviewerdisplay.cpp drives gl_rect_2d / gl_line_2d /
 *     LLUIImage::draw (the real GL 2D path).
 *   - Vulkan: LLVKUITestScene::emitVulkan() drives the existing LLVKUI2D sink
 *     (rect/lineStrip) + the GL-free LLVKUIImage registry. No GL code runs.
 *
 * Coordinate convention: ALL scene coordinates are TOP-LEFT-origin device
 * pixels (the LLVKUI2D sink's native space). The GL path works in
 * bottom-left-origin y-up space (gl_state_for_2d ortho), so the GL emitter
 * converts with gl_y = window_height - scene_y at emit time. This mirrors the
 * proven byte-exact archived harness (vulkan-ui branch, scenes 0-4).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, The Vulkanstorm Project
 * $/LicenseInfo$
 */

#ifndef LLVKUITESTSCENE_H
#define LLVKUITESTSCENE_H

#include <string>
#include <vector>

namespace LLVKUITestScene
{
    // True when the harness scene replaces the login UI (VULKANSTORM_UITEST
    // is set). Cached on first call.
    bool enabled();

    // One scene primitive. Coordinates are TOP-LEFT-origin device pixels.
    struct Rect
    {
        float l, t, r, b;              // left/top/right/bottom edges
        float cr, cg, cb, ca;          // RGBA 0..1
        bool  opaque;                  // true -> REPLACE blend (tol-0 anchor),
                                       // false -> ALPHA blend (tol-1 class)
    };
    struct Line
    {
        float x0, y0, x1, y1;          // endpoints
        float cr, cg, cb, ca;
    };
    struct Image
    {
        std::string name;              // textures.xml name (both registries parse it)
        float l, t, r, b;              // destination rect
        float cr, cg, cb, ca;          // tint (modulates the texture)
    };

    struct Scene
    {
        std::vector<Rect>  rects;      // filled solid rects
        std::vector<Line>  lines;      // 1px axis-aligned lines
        std::vector<Image> images;     // textured images (ALPHA blend, real UI state)
    };

    // The fixed scene. Identical data for both backends, by construction.
    const Scene& scene();

    // Vulkan emitter: issue the whole scene into the live LLVKUI2D sink.
    // The caller (LLVKSession::renderUIFrame) has already begun the sink for
    // the frame (identity transform, BT_ALPHA, no scissor). Images resolve
    // through the GL-free LLVKUIImage registry (9-slice honored).
    void emitVulkan();
}

#endif // LLVKUITESTSCENE_H
