/**
 * @file llvkuirender.h
 * @brief LLVKUIRender — the greenfield Vulkan UI renderer (Phase 3 v2, M0).
 *
 * @details
 * The independent Vulkan UI renderer. Per frame it WALKS the live LLView tree
 * and READS each widget's computed layout/visual STATE (rect, visibility,
 * colors, images, label) via public getters, then emits the equivalent
 * primitives into the batched LLVKUI2D sink. It executes NONE of the widget
 * tree's GL-coupled draw() code and never reads gGL.
 *
 * This is the greenfield-plus-cheat-sheet approach (doc/vulkan/
 * phase3_v2_m0_design.md): the tree's layout is the input data; the documented
 * result contracts are the spec; the rendering is Vulkan-native.
 *
 * GL-free: no gGL / LLRender / LLImageGL / LLVertexBuffer.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, The Vulkanstorm Project
 * $/LicenseInfo$
 */

#ifndef LLVKUIRENDER_H
#define LLVKUIRENDER_H

class LLView;
class LLVKContext;

namespace LLVKUIRender
{
    // Render one 2D UI frame from the widget tree's current state. The caller
    // (the frame seam) has already begun the sink (LLVKUI2DSink::begin) with a
    // live command buffer. root is the viewer's root view. device_width/height
    // are the swapchain extent (for scissor + the top-left-origin conversion).
    // ui_scale_x/y are the neutral UI scale factor (LLUI::getScaleFactor()).
    void renderFrame(LLVKContext* ctx, LLView* root,
                     unsigned device_width, unsigned device_height,
                     float ui_scale_x, float ui_scale_y);
}

#endif // LLVKUIRENDER_H
