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
#include <map>
#include <string>
#include <typeinfo>             // typeid (tree dump)

#include "v4color.h"           // LLColor4
#include "llrect.h"
#include "llui.h"               // LLUI::getScaleFactor (neutral)
#include "lluicolortable.h"     // LLUIColor / LLUIColorTable (neutral)
#include "llview.h"             // LLView
#include "llpanel.h"            // LLPanel (background state)
#include "llbutton.h"           // LLButton (state images)
#include "lliconctrl.h"         // LLIconCtrl (icons)
#include "lluicolor.h"          // LLUIColor
#include "llvkcontext.h"
#include "llvkui2d.h"
#include "llvkuiimage.h"        // LLVKUIImage registry (GL-free)
#include "lluiimage.h"          // LLUIImage (regions)

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
        // <VulkanStorm> one-shot widget-tree dump (VULKANSTORM_TREE_DUMP=1):
        // logs every visited view's class/name/screen rect so the chrome work
        // can be planned from the real login-screen composition.
        bool dump   = false;
        int  depth  = 0;
    };

    // Convert a GL bottom-left-origin screen rect (from calcScreenRect) into
    // the sink's top-left-origin coordinate space.
    void toSinkRect(const RenderCtx& rc, const LLRect& gl_rect,
                    float& left, float& top, float& right, float& bottom)
    {
        const F32 ui_h = (F32)rc.dev_h / rc.ui_scale_y;
        left   = (F32)gl_rect.mLeft;
        right  = (F32)gl_rect.mRight;
        top    = ui_h - (F32)gl_rect.mTop;
        bottom = ui_h - (F32)gl_rect.mBottom;
    }

    // <VulkanStorm> Registered per-class hooks (newview-side classes).
    std::map<const std::type_info*, LLVKUIRender::ViewHook> s_hooks;
    // </VulkanStorm>

    // Emit a panel/floater background. Mirrors LLPanel::draw(): prefer the
    // background IMAGE (opaque/transparent variant) over the solid color; the
    // image is modulated by its overlay color % draw alpha.
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

        if (panel->isBackgroundOpaque())
        {
            LLPointer<LLUIImage> img = panel->getBackgroundImage();
            if (img.notNull())
            {
                // getBackgroundImageOverlay() is non-const; read-only in effect.
                const LLColor4& ov = const_cast<LLPanel*>(panel)->getBackgroundImageOverlay();
                LLColor4 c = LLColor4(ov.mV[0] * rc.parent_alpha, ov.mV[1] * rc.parent_alpha,
                                      ov.mV[2] * rc.parent_alpha, ov.mV[3] * rc.parent_alpha);
                float l, t, r, b; toSinkRect(rc, screen, l, t, r, b);
                LLVKUIImage::draw(img->getName(), l, t, r, b, c);
                return;
            }
        }
        else
        {
            LLPointer<LLUIImage> img = panel->getTransparentImage();
            if (img.notNull())
            {
                const LLColor4& ov = const_cast<LLPanel*>(panel)->getTransparentImageOverlay();
                LLColor4 c = LLColor4(ov.mV[0] * rc.parent_alpha, ov.mV[1] * rc.parent_alpha,
                                      ov.mV[2] * rc.parent_alpha, ov.mV[3] * rc.parent_alpha);
                float l, t, r, b; toSinkRect(rc, screen, l, t, r, b);
                LLVKUIImage::draw(img->getName(), l, t, r, b, c);
                return;
            }
        }

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
        LLVKUIRender::emitScreenRect(screen, rc.dev_h, rc.ui_scale_y, c);
    }

    // Read a widget's own chrome (background/border) and recurse into children.
    // Painter's order: the child list is iterated so that back-most draws first.
    void renderView(RenderCtx& rc, const LLView* view)
    {
        if (!view) return;
        rc.visited++;
        if (rc.dump)
        {
            const LLRect sr = view->calcScreenRect();
            LL_INFOS("Vulkan") << "VULKTREE " << std::string(rc.depth * 2, ' ')
                               << typeid(*view).name() << " '" << view->getName() << "'"
                               << " vis=" << (view->getVisible() ? 1 : 0)
                               << " rect=" << sr.mLeft << "," << sr.mBottom
                               << "-" << sr.mRight << "," << sr.mTop << LL_ENDL;
        }
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

        // <VulkanStorm> M2: button + icon images. These read the widget's
        // state and emit the same image LLButton::draw()/LLIconCtrl::draw()
        // would, resolved by name through the GL-free LLVKUIImage registry.
        if (LLVKUIImage::ready())
        {
            static bool s_dbg = getenv("VULKANSTORM_UI_DEBUG") != nullptr;
            static int  s_dbg_n = 0;
            const LLButton* button = dynamic_cast<const LLButton*>(view);
            if (button)
            {
                LLColor4 imgc;
                const std::string imgname = button->getStateImageName(imgc, rc.parent_alpha);
                if (s_dbg && s_dbg_n < 12)
                {
                    ++s_dbg_n;
                    LL_INFOS("Vulkan") << "VKBUTTON '" << view->getName() << "' img='" << imgname << "'"
                                       << " empty=" << (imgname.empty() ? 1 : 0) << LL_ENDL;
                }
                if (!imgname.empty())
                {
                    const LLRect screen = view->calcScreenRect();
                    float l, t, r, b; toSinkRect(rc, screen, l, t, r, b);
                    LLVKUIImage::draw(imgname, l, t, r, b, imgc);
                    rc.emitted++;
                }
            }
            const LLIconCtrl* icon = dynamic_cast<const LLIconCtrl*>(view);
            if (icon)
            {
                const std::string imgname = icon->getImageVkName();
                if (!imgname.empty())
                {
                    const F32 a = icon->getUseDrawContextAlpha() ? rc.parent_alpha : 1.f;
                    const LLColor4& ic = icon->getColor().get();
                    LLColor4 c = LLColor4(ic.mV[0] * a, ic.mV[1] * a, ic.mV[2] * a, ic.mV[3] * a);
                    const LLRect screen = view->calcScreenRect();
                    float l, t, r, b; toSinkRect(rc, screen, l, t, r, b);
                    LLVKUIImage::draw(imgname, l, t, r, b, c);
                    rc.emitted++;
                }
            }
        }
        // </VulkanStorm>

        // <VulkanStorm> Registered per-class hooks (e.g. LLMediaCtrl's
        // no-media backdrop), supplied by newview for classes llvulkan must
        // not depend on.
        if (!s_hooks.empty())
        {
            auto it = s_hooks.find(&typeid(*view));
            if (it != s_hooks.end())
            {
                it->second(view, rc.dev_h, rc.ui_scale_y, rc.parent_alpha);
            }
        }
        // </VulkanStorm>

        // Recurse children in painter's order. mChildList front = top-most, so
        // reverse iteration draws back-to-front (deepest first).
        rc.depth++;
        for (LLView::child_list_const_reverse_iter_t it = view->getChildList()->rbegin();
             it != view->getChildList()->rend(); ++it)
        {
            renderView(rc, *it);
        }
        rc.depth--;
    }
}

namespace LLVKUIRender
{
    void registerViewHook(const std::type_info& type, ViewHook hook)
    {
        if (hook)
        {
            s_hooks[&type] = hook;
        }
    }

    void emitScreenRect(const LLRect& gl_rect, unsigned device_height,
                        float ui_scale_y, const LLColor4& color)
    {
        if (gl_rect.isEmpty()) return;
        // GL bottom-left -> sink top-left: y_top_left = device_height - y_gl.
        // In scaled UI space the device height is device_height / ui_scale_y.
        const F32 ui_h = (F32)device_height / ui_scale_y;
        const F32 left   = (F32)gl_rect.mLeft;
        const F32 right  = (F32)gl_rect.mRight;
        const F32 top    = ui_h - (F32)gl_rect.mTop;     // GL top -> smaller top-left y
        const F32 bottom = ui_h - (F32)gl_rect.mBottom;  // GL bottom -> larger top-left y
        LLVKUI2DSink::get().rect(left, top, right, bottom,
                                 color.mV[VRED], color.mV[VGREEN], color.mV[VBLUE], color.mV[VALPHA]);
    }

    void emitScreenRect(const LLRect& gl_rect, unsigned device_height,
                        float ui_scale_y, const LLUIImage* image, const LLColor4& color)
    {
        if (gl_rect.isEmpty()) return;
        const F32 ui_h = (F32)device_height / ui_scale_y;
        const F32 left   = (F32)gl_rect.mLeft;
        const F32 right  = (F32)gl_rect.mRight;
        const F32 top    = ui_h - (F32)gl_rect.mTop;
        const F32 bottom = ui_h - (F32)gl_rect.mBottom;
        LLVKUIImage::draw(image ? image->getName() : std::string(), left, top, right, bottom, color);
    }

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

        // <VulkanStorm> one-shot widget-tree dump at frame 60
        static bool s_treedump = getenv("VULKANSTORM_TREE_DUMP") != nullptr;
        static int  s_dump_frame = 0;
        rc.dump = s_treedump && (++s_dump_frame == 60);

        renderView(rc, root);

        if (rc.dump)
        {
            LL_INFOS("Vulkan") << "VULKTREE dump complete (" << rc.visited << " views)" << LL_ENDL;
        }

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
