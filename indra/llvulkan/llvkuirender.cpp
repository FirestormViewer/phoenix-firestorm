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
#include "lllineeditor.h"       // LLLineEditor (field backgrounds)
#include "llviewborder.h"       // LLViewBorder (bevel lines)
#include "llmenugl.h"           // LLMenuGL (menu bar strip + drop shadow)
#include "llfocusmgr.h"         // gFocusMgr (focus border color)
#include "lluictrl.h"           // DROP_SHADOW_FLOATER
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

    // <VulkanStorm> M3: non-text chrome helpers.

    // Emit one GL-space line segment into the sink (GL bottom-left -> top-left
    // conversion, same mapping as toSinkRect). Each gl_line_2d edge becomes its
    // own 2-vertex strip so independent segments never connect.
    void emitBorderLine(const RenderCtx& rc, S32 x1, S32 y1, S32 x2, S32 y2,
                        const LLColor4& c)
    {
        const F32 ui_h = (F32)rc.dev_h / rc.ui_scale_y;
        const float xy[4] = { (F32)x1, ui_h - (F32)y1, (F32)x2, ui_h - (F32)y2 };
        LLVKUI2DSink::get().lineStrip(xy, 2, c.mV[VRED], c.mV[VGREEN], c.mV[VBLUE], c.mV[VALPHA]);
    }

    // Mirror LLViewBorder::drawOnePixelLines()/drawTwoPixelLines()
    // (llviewborder.cpp): identical endpoints and per-edge colors, in sink
    // space. STYLE_LINE only (STYLE_TEXTURE draws nothing in GL either);
    // width 0 = no visible border; widths > 2 are llassert'ed in GL.
    void renderViewBorder(RenderCtx& rc, const LLViewBorder* border)
    {
        const LLViewBorder::VkBorderState bs = border->getVkBorderState();
        if (bs.style != LLViewBorder::STYLE_LINE || bs.width < 1 || bs.width > 2) return;

        const LLRect screen = border->calcScreenRect();
        const S32 left = screen.mLeft, top = screen.mTop,
                  right = screen.mRight, bottom = screen.mBottom;

        if (bs.width == 1)
        {
            LLColor4 top_color    = bs.highlight_light;
            LLColor4 bottom_color = bs.highlight_light;
            switch (bs.bevel)
            {
            case LLViewBorder::BEVEL_OUT:
                top_color    = bs.highlight_light;
                bottom_color = bs.shadow_dark;
                break;
            case LLViewBorder::BEVEL_IN:
                top_color    = bs.shadow_dark;
                bottom_color = bs.highlight_light;
                break;
            case LLViewBorder::BEVEL_NONE:
                break; // use defaults (GL comment: "use defaults")
            default:
                break; // GL llassert(0)s on BEVEL_BRIGHT here; keep defaults
            }
            if (bs.keyboard_focus)
            {
                top_color = gFocusMgr.getFocusColor();
                bottom_color = top_color;
                // NOTE: GL also widens the line to lerp(1,2,focusFlashAmt);
                // the 2D sink has no line-width state, so the focused border
                // stays 1px wide (the color is exact).
            }
            emitBorderLine(rc, left, bottom, left, top, top_color);
            emitBorderLine(rc, left, top, right, top, top_color);
            emitBorderLine(rc, right, top, right, bottom, bottom_color);
            emitBorderLine(rc, left, bottom, right, bottom, bottom_color);
        }
        else // width == 2
        {
            LLColor4 top_in_color, top_out_color, bottom_in_color, bottom_out_color;
            switch (bs.bevel)
            {
            case LLViewBorder::BEVEL_OUT:
                top_in_color     = bs.highlight_light;
                top_out_color    = bs.highlight_dark;
                bottom_in_color  = bs.shadow_light;
                bottom_out_color = bs.shadow_dark;
                break;
            case LLViewBorder::BEVEL_IN:
                top_in_color     = bs.shadow_dark;
                top_out_color    = bs.shadow_light;
                bottom_in_color  = bs.highlight_dark;
                bottom_out_color = bs.highlight_light;
                break;
            case LLViewBorder::BEVEL_BRIGHT:
                top_in_color = top_out_color = bottom_in_color = bottom_out_color = bs.highlight_light;
                break;
            case LLViewBorder::BEVEL_NONE:
                top_in_color = top_out_color = bottom_in_color = bottom_out_color = bs.shadow_dark;
                break;
            default:
                break;
            }
            if (bs.keyboard_focus)
            {
                top_out_color = bottom_out_color = gFocusMgr.getFocusColor();
            }
            emitBorderLine(rc, left, bottom, left, top - 1, top_out_color);
            emitBorderLine(rc, left, top - 1, right, top - 1, top_out_color);
            emitBorderLine(rc, left + 1, bottom + 1, left + 1, top - 2, top_in_color);
            emitBorderLine(rc, left + 1, top - 2, right - 1, top - 2, top_in_color);
            emitBorderLine(rc, right - 1, top - 1, right - 1, bottom, bottom_out_color);
            emitBorderLine(rc, left, bottom, right, bottom, bottom_out_color);
            emitBorderLine(rc, right - 2, top - 2, right - 2, bottom + 1, bottom_in_color);
            emitBorderLine(rc, left + 1, bottom + 1, right - 1, bottom + 1, bottom_in_color);
        }
        rc.emitted++;
    }

    // Mirror gl_drop_shadow (llrender2dutils.cpp:165): the same 30-vertex
    // gradient fan hugging the right/bottom edges, with the same 1px overlap
    // hack and per-vertex alpha fade, in sink space.
    void emitDropShadow(const RenderCtx& rc, const LLRect& gl_screen,
                        const LLColor4& start_color, S32 lines)
    {
        // GL: right--, bottom++, lines++ (overlap with the rectangle).
        const F32 left   = (F32)gl_screen.mLeft;
        const F32 top    = (F32)gl_screen.mTop;
        const F32 right  = (F32)gl_screen.mRight - 1.f;
        const F32 bottom = (F32)gl_screen.mBottom + 1.f;
        const F32 ln     = (F32)(lines + 1);
        const F32 ui_h   = (F32)rc.dev_h / rc.ui_scale_y;

        LLColor4 end_color = start_color;
        end_color.mV[VALPHA] = 0.f;

        // Vertex stream identical to gl_drop_shadow; each vertex carries its
        // GL-space position (y flipped to sink space) and the start/end color.
        struct V { F32 x, y; bool start; };
        const V gv[30] = {
            // right edge
            { right, top - ln, true },   { right, bottom, true },             { right + ln, bottom, false },
            { right, top - ln, true },   { right + ln, bottom, false },       { right + ln, top - ln, false },
            // bottom edge
            { right, bottom, true },     { left + ln, bottom, true },         { left + ln, bottom - ln, false },
            { right, bottom, true },     { left + ln, bottom - ln, false },   { right, bottom - ln, false },
            // bottom-left corner
            { left + ln, bottom, true }, { left, bottom, false },             { left + 1, bottom - ln + 1, false },
            { left + ln, bottom, true }, { left + 1, bottom - ln + 1, false },{ left + ln, bottom - ln, false },
            // bottom-right corner
            { right, bottom, true },     { right, bottom - ln, false },       { right + ln - 1, bottom - ln + 1, false },
            { right, bottom, true },     { right + ln - 1, bottom - ln + 1, false }, { right + ln, bottom, false },
            // top-right corner
            { right, top - ln, true },   { right + ln, top - ln, false },     { right + ln - 1, top - 1, false },
            { right, top - ln, true },   { right + ln - 1, top - 1, false },  { right, top, false },
        };
        float xy[60], rgba[120];
        for (int i = 0; i < 30; ++i)
        {
            xy[i * 2]     = gv[i].x;
            xy[i * 2 + 1] = ui_h - gv[i].y;
            const LLColor4& c = gv[i].start ? start_color : end_color;
            rgba[i * 4]     = c.mV[VRED];
            rgba[i * 4 + 1] = c.mV[VGREEN];
            rgba[i * 4 + 2] = c.mV[VBLUE];
            rgba[i * 4 + 3] = c.mV[VALPHA];
        }
        LLVKUI2DSink::get().rawTris(xy, rgba, 30);
    }

    // Mirror the non-item chrome of LLMenuGL::draw() (llmenugl.cpp:3267): the
    // drop shadow first, then the background strip (bg color *
    // FSMenuBackgroundAlpha). Menu item text/highlight is out of scope.
    void renderMenuChrome(RenderCtx& rc, const LLMenuGL* menu)
    {
        const LLRect screen = menu->calcScreenRect();
        if (menu->getVkDropShadow())
        {
            static LLUIColor color_drop_shadow = LLUIColorTable::instance().getColor("ColorDropShadow");
            emitDropShadow(rc, screen, color_drop_shadow.get(), DROP_SHADOW_FLOATER);
            rc.emitted++;
        }
        if (menu->getVkBgVisible())
        {
            LLVKUIRender::emitScreenRect(screen, rc.dev_h, rc.ui_scale_y, menu->getVkBgColor());
            rc.emitted++;
        }
    }
    // </VulkanStorm>

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

        // <VulkanStorm> M3: non-text login chrome.
        // Line-editor backgrounds: solid color, or the TextField_* 9-slice
        // image (+ focus border ring) chosen by readOnly/focus state. Mirrors
        // LLLineEditor::drawBackground()'s selection and draw order (border
        // first, then the image over it).
        const LLLineEditor* line_editor = dynamic_cast<const LLLineEditor*>(view);
        static const bool s_no_lineedit = getenv("VULKANSTORM_NO_LINEEDIT") != nullptr;
        if (line_editor && !s_no_lineedit)
        {
            const LLLineEditor::VkBackground bg = line_editor->getVkBackground(rc.parent_alpha);
            const LLRect screen = view->calcScreenRect();
            if (bg.solid_color)
            {
                LLVKUIRender::emitScreenRect(screen, rc.dev_h, rc.ui_scale_y, bg.bg_color);
                rc.emitted++;
            }
            else if (!bg.image_name.empty() && LLVKUIImage::ready())
            {
                float l, t, r, b; toSinkRect(rc, screen, l, t, r, b);
                if (bg.focus_border)
                {
                    LLVKUIImage::drawBorder(bg.image_name, l, t, r, b,
                                            bg.focus_color, bg.focus_border_width);
                }
                // GL tints with UI_VERTEX_COLOR (white) at the draw alpha.
                LLVKUIImage::draw(bg.image_name, l, t, r, b,
                                  LLColor4(1.f, 1.f, 1.f, rc.parent_alpha));
                rc.emitted++;
            }
        }

        // View borders: 1-2px bevel line rings (LLViewBorder::draw()).
        static const bool s_no_border = getenv("VULKANSTORM_NO_BORDER") != nullptr;
        const LLViewBorder* border = dynamic_cast<const LLViewBorder*>(view);
        if (border && !s_no_border)
        {
            renderViewBorder(rc, border);
        }

        // Menu background strip + drop shadow (LLMenuBarGL/LLMenuGL::draw()).
        static const bool s_no_menu = getenv("VULKANSTORM_NO_MENU") != nullptr;
        const LLMenuGL* menu = dynamic_cast<const LLMenuGL*>(view);
        if (menu && !s_no_menu)
        {
            renderMenuChrome(rc, menu);
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
