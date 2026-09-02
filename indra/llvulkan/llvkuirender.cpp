/**
 * @file llvkuirender.cpp
 * @brief Implementation of LLVKUIRender — the greenfield Vulkan UI renderer.
 *
 * Walks the live LLView tree, reads each widget's computed layout/visual state
 * via public getters, and emits the equivalent primitives into the batched
 * LLVKUI2D sink. Never calls the tree's GL-coupled draw() and never reads gGL.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, The Vulkanstorm Project
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "llvkuirender.h"

#include "llerror.h"            // LL_INFOS (diagnostic)

#include <cstdlib>              // getenv

#include "v4color.h"           // LLColor4
#include "llrect.h"
#include "llui.h"               // LLUI::getScaleFactor (neutral)
#include "lluicolortable.h"     // LLUIColor / LLUIColorTable (neutral)
#include "llview.h"             // LLView
#include "llpanel.h"            // LLPanel (background state)
#include "llvkcontext.h"
#include "llvkui2d.h"

namespace
{
    // Per-frame render context, threaded through the tree walk.
    struct RenderCtx
    {
        unsigned dev_w = 0;
        unsigned dev_h = 0;
        float    ui_scale_x = 1.f;
        float    ui_scale_y = 1.f;
        float    parent_alpha = 1.f;   // accumulated draw-context alpha
        // <VulkanStorm> M0 diagnostics
        int visited = 0;      // views walked
        int visible = 0;      // views passing getVisible()
        int panels  = 0;      // views that are LLPanel
        int emitted = 0;      // rects actually emitted
    };

    // Convert a GL bottom-left-origin screen rect (from calcScreenRect, in
    // window pixels) into the sink's top-left-origin coordinate space, then
    // apply the UI scale. The sink's transform is left at identity; we bake the
    // absolute position here.
    void emitRectGL(RenderCtx& rc, const LLRect& gl_rect, const LLColor4& color)
    {
        if (gl_rect.isEmpty()) return;
        // GL bottom-left -> sink top-left: y_top_left = device_height - y_gl.
        // In scaled UI space the device height is dev_h / ui_scale_y.
        const F32 ui_h = (F32)rc.dev_h / rc.ui_scale_y;
        const F32 left   = (F32)gl_rect.mLeft;
        const F32 right  = (F32)gl_rect.mRight;
        const F32 top    = ui_h - (F32)gl_rect.mTop;     // GL top -> smaller top-left y
        const F32 bottom = ui_h - (F32)gl_rect.mBottom;  // GL bottom -> larger top-left y
        LLVKUI2DSink::get().rect(left, top, right, bottom,
                                 color.mV[VRED], color.mV[VGREEN], color.mV[VBLUE], color.mV[VALPHA]);
    }

    // Emit a panel/floater background (solid color; images land in M2).
    void renderPanelBackground(RenderCtx& rc, const LLPanel* panel)
    {
        // <VulkanStorm> M0 diagnostic: log why a panel is skipped.
        static bool s_dbg = getenv("VULKANSTORM_UI_DEBUG") != nullptr;
        if (!panel) return;
        if (s_dbg && !panel->isBackgroundVisible())
        {
            LL_INFOS("Vulkan") << "panel skip: bg not visible" << LL_ENDL;
        }
        if (!panel->isBackgroundVisible()) return;

        LLRect local = panel->getLocalRect();
        LLRect screen;
        panel->localRectToScreen(local, &screen);

        LLColor4 c = panel->isBackgroundOpaque() ? panel->getBackgroundColor()
                                                 : panel->getTransparentColor();
        c.mV[VALPHA] *= rc.parent_alpha;
        if (s_dbg)
        {
            LL_INFOS("Vulkan") << "panel emit: rect=" << screen.mLeft << "," << screen.mBottom
                               << " to " << screen.mRight << "," << screen.mTop
                               << " empty=" << (screen.isEmpty() ? 1 : 0)
                               << " rgba=" << c.mV[0] << "," << c.mV[1] << "," << c.mV[2] << "," << c.mV[3] << LL_ENDL;
        }
        emitRectGL(rc, screen, c);
    }

    // Read a widget's own chrome (background/border) and recurse into children.
    // Painter's order: the child list is iterated so that back-most draws first.
    void renderView(RenderCtx& rc, const LLView* view)
    {
        if (!view) return;
        rc.visited++;
        if (!view->getVisible()) return;
        rc.visible++;

        // Widget-specific chrome. (v1: panels/floaters backgrounds; borders +
        // images + text land next.)
        const LLPanel* panel = dynamic_cast<const LLPanel*>(view);
        if (panel)
        {
            rc.panels++;
            size_t vbefore = LLVKUI2DSink::get().pendingVerts();
            renderPanelBackground(rc, panel);
            if (LLVKUI2DSink::get().pendingVerts() > vbefore) rc.emitted++;
        }

        // Recurse children in painter's order. mChildList front = top-most, so
        // reverse iteration draws back-to-front (deepest first).
        for (LLView::child_list_const_reverse_iter_t it = view->getChildList()->rbegin();
             it != view->getChildList()->rend(); ++it)
        {
            renderView(rc, *it);
        }
    }
}

namespace LLVKUIRender
{
    void renderFrame(LLVKContext* ctx, LLView* root,
                     unsigned device_width, unsigned device_height,
                     float ui_scale_x, float ui_scale_y)
    {
        if (!root || !LLVKUI2DSink::get().isActive()) return;

        RenderCtx rc;
        rc.dev_w = device_width;
        rc.dev_h = device_height;
        rc.ui_scale_x = ui_scale_x;
        rc.ui_scale_y = ui_scale_y;

        // Identity transform: the renderer bakes absolute positions via
        // calcScreenRect/localRectToScreen + the GL->top-left conversion.
        LLVKUI2DSink::get().setTransform(0.f, 0.f, ui_scale_x, ui_scale_y);
        LLVKUI2DSink::get().setBlend(LLVKBlend::Alpha);
        LLVKUI2DSink::get().clearScissor();

        renderView(rc, root);

        // <VulkanStorm> M0 diagnostic: what did the walk find?
        static bool s_dbg = getenv("VULKANSTORM_UI_DEBUG") != nullptr;
        if (s_dbg)
        {
            static int s_f = 0;
            if ((s_f++ % 60) == 0)
            {
                LL_INFOS("Vulkan") << "LLVKUIRender: visited=" << rc.visited
                                   << " visible=" << rc.visible
                                   << " panels=" << rc.panels
                                   << " emitted=" << rc.emitted << LL_ENDL;
            }
        }
        // </VulkanStorm>
    }
}
