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

#include <typeinfo>
#include <string>

#include "llrect.h"
#include "v4color.h"

class LLView;
class LLVKContext;
class LLUIImage;

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

    // <VulkanStorm> Per-class render hooks. llvulkan must not depend on
    // newview (where viewer-side widget classes like LLMediaCtrl live), so
    // newview registers a hook per class; the walk calls it after the generic
    // panel-background pass and before recursing into children. The hook reads
    // the widget's state and emits Vulkan primitives via emitScreenRect /
    // LLVKUI2DSink — never GL code.
    typedef void (*ViewHook)(const LLView* view, unsigned device_height,
                             float ui_scale_y, float alpha);
    typedef void (*ViewPrepareHook)(const LLView* view, LLVKContext* context);
    void registerViewHook(const std::type_info& type, ViewHook hook);
    void registerViewPrepareHook(const std::type_info& type, ViewPrepareHook hook);

    // Update dynamic resources required by viewer-side hooks before the
    // swapchain render pass begins.
    void prepareFrame(LLVKContext* context, LLView* root);

    // Emit a screen-space rect (GL bottom-left origin, window pixels) into the
    // sink with the standard GL->top-left conversion. Shared by the built-in
    // passes and by registered hooks.
    void emitScreenRect(const LLRect& gl_rect, unsigned device_height,
                        float ui_scale_y, const LLColor4& color);

    // Emit a screen-space rect filled with the named image (resolved through
    // the GL-free LLVKUIImage registry by the LLUIImage's name), honoring
    // clip + 9-slice regions. Modulated by color.
    void emitScreenRect(const LLRect& gl_rect, unsigned device_height,
                        float ui_scale_y, const LLUIImage* image, const LLColor4& color);

    // Named-image form for viewer-side hooks. This avoids requiring a live
    // LLUIImage/GL texture merely to identify an image already decoded by the
    // Vulkan-native image registry.
    void emitScreenRect(const LLRect& gl_rect, unsigned device_height,
                        float ui_scale_y, const std::string& image_name,
                        const LLColor4& color);

    // Emit the same right/bottom gradient shadow as gl_drop_shadow().
    void emitDropShadow(const LLRect& gl_rect, unsigned device_height,
                        float ui_scale_y, const LLColor4& color, S32 lines);
    // </VulkanStorm>
}

#endif // LLVKUIRENDER_H
