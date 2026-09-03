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
#include <algorithm>            // std::swap (rect normalization)
#include <map>
#include <string>
#include <typeinfo>             // typeid (tree dump)
#include <vector>

#include "v4color.h"           // LLColor4
#include "llrect.h"
#include "llmath.h"             // ll_round
#include "llui.h"               // LLUI::getScaleFactor (neutral)
#include "lluicolortable.h"     // LLUIColor / LLUIColorTable (neutral)
#include "llview.h"             // LLView
#include "llpanel.h"            // LLPanel (background state)
#include "llfloater.h"          // LLFloater (opaque chrome + shadow state)
#include "llbutton.h"           // LLButton (state images)
#include "lltabcontainer.h"     // GL-free tab layout preparation
#include "llscrollcontainer.h"  // GL-free scrollbar layout preparation
#include "llscrollbar.h"       // scrollbar track/thumb state
#include "llcombobox.h"         // editable-combo layout reconciliation
#include "lliconctrl.h"         // LLIconCtrl (icons)
#include "lllineeditor.h"       // LLLineEditor (field backgrounds)
#include "llsearcheditor.h"     // composite search-field preparation
#include "lltextbase.h"         // LLTextBase (computed text-line layout)
#include "llviewborder.h"       // LLViewBorder (bevel lines)
#include "llmenugl.h"           // LLMenuGL (menu bar strip + drop shadow)
#include "lllayoutstack.h"      // LLLayoutStack::updateLayout (GL-free rect math)
#include "llfocusmgr.h"         // gFocusMgr (focus border color)
#include "lluictrl.h"           // DROP_SHADOW_FLOATER
#include "lluicolor.h"          // LLUIColor
#include "llvkcontext.h"
#include "llvkui2d.h"
#include "llvkuiimage.h"        // LLVKUIImage registry (GL-free)
#include "llvktext.h"           // independent FreeType/Vulkan text atlas
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
    // the sink's top-left-origin coordinate space. Normalizes so top <= bottom
    // (the GL->top-left conversion can produce inverted or off-window rects for
    // some widgets; draw9Slice's band mapping assumes a sane top<bottom rect).
    void toSinkRect(const RenderCtx& rc, const LLRect& gl_rect,
                    float& left, float& top, float& right, float& bottom)
    {
        const F32 ui_h = (F32)rc.dev_h / rc.ui_scale_y;
        left   = (F32)gl_rect.mLeft;
        right  = (F32)gl_rect.mRight;
        top    = ui_h - (F32)gl_rect.mTop;
        bottom = ui_h - (F32)gl_rect.mBottom;
        if (left > right) std::swap(left, right);
        if (top > bottom) std::swap(top, bottom);
    }

    // <VulkanStorm> Registered per-class hooks (newview-side classes).
    std::map<const std::type_info*, LLVKUIRender::ViewHook> s_hooks;
    std::map<const std::type_info*, LLVKUIRender::ViewPrepareHook> s_prepare_hooks;
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

        // A floater is a separate in-viewer window. Its image/color can retain
        // alpha for edge decoration and inactive-state tinting, but its body
        // must first occlude the scene and any CEF surface below it. OpenGL
        // obtains that composition from the floater pass; Vulkan needs the
        // equivalent opaque underlay explicitly.
        if (dynamic_cast<const LLFloater*>(panel))
        {
            LLColor4 base = panel->isBackgroundOpaque()
                                ? panel->getBackgroundColor()
                                : panel->getTransparentColor();
            base.mV[VALPHA] = 1.f;
            // Keep the opaque body inside the skinned image's antialiased
            // corner pixels. A full-rect underlay made Vulkan floaters square.
            LLRect body = screen;
            body.stretch(-2);
            LLVKUIRender::emitScreenRect(body, rc.dev_h, rc.ui_scale_y, base);
        }

        if (panel->isBackgroundOpaque())
        {
            const std::string image_name = panel->getBackgroundImageVkName();
            if (!image_name.empty() && LLVKUIImage::ready())
            {
                // getBackgroundImageOverlay() is non-const; read-only in effect.
                const LLColor4& ov = const_cast<LLPanel*>(panel)->getBackgroundImageOverlay();
                LLColor4 c = LLColor4(ov.mV[0] * rc.parent_alpha, ov.mV[1] * rc.parent_alpha,
                                      ov.mV[2] * rc.parent_alpha, ov.mV[3] * rc.parent_alpha);
                float l, t, r, b; toSinkRect(rc, screen, l, t, r, b);
                LLVKUIImage::draw(image_name, l, t, r, b, c);
                return;
            }
        }
        else
        {
            const std::string image_name = panel->getTransparentImageVkName();
            if (!image_name.empty() && LLVKUIImage::ready())
            {
                const LLColor4& ov = const_cast<LLPanel*>(panel)->getTransparentImageOverlay();
                LLColor4 c = LLColor4(ov.mV[0] * rc.parent_alpha, ov.mV[1] * rc.parent_alpha,
                                      ov.mV[2] * rc.parent_alpha, ov.mV[3] * rc.parent_alpha);
                float l, t, r, b; toSinkRect(rc, screen, l, t, r, b);
                LLVKUIImage::draw(image_name, l, t, r, b, c);
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
        // LLMenuGL::draw() performs lazy layout before drawing.  The Vulkan
        // walker deliberately bypasses draw(), so reproduce that GL-free
        // preparation here.  This is also required for interaction: without
        // it newly-visible entries (notably the login Debug menu) retain an
        // empty hit rectangle even if their text is emitted.
        const_cast<LLMenuGL*>(menu)->arrangeAndClear();
        const LLRect screen = menu->calcScreenRect();
        if (menu->getVkDropShadow())
        {
            static LLUIColor color_drop_shadow = LLUIColorTable::instance().getColor("ColorDropShadow");
            emitDropShadow(rc, screen, color_drop_shadow.get(), DROP_SHADOW_FLOATER);
            rc.emitted++;
        }
        if (menu->getVkBgVisible())
        {
            LLColor4 background = menu->getVkBgColor();
            // Popup menus must occlude web/media content. Blending the menu
            // directly over CEF makes Vulkan menus visibly glassy rather than
            // matching the composed OpenGL result.
            background.mV[VALPHA] = 1.f;
            LLVKUIRender::emitScreenRect(screen, rc.dev_h, rc.ui_scale_y,
                                         background);
            rc.emitted++;
        }
    }

    void renderMenuItem(RenderCtx& rc, LLMenuItemGL* item)
    {
        if (!item || !LLVKText::ready()) return;
        const LLMenuItemGL::VkDrawState state = item->getVkDrawState(rc.parent_alpha);
        const LLRect screen = item->calcScreenRect();

        const bool draw_highlight = state.highlight &&
            (state.menu_bar || (state.enabled && !state.brief));
        if (draw_highlight)
        {
            LLVKUIRender::emitScreenRect(screen, rc.dev_h, rc.ui_scale_y,
                                         state.highlight_background);
            rc.emitted++;
        }

        if (state.kind == LLMenuItemGL::VkDrawState::Kind::Separator)
        {
            const S32 y = screen.mBottom + screen.getHeight() / 2;
            emitBorderLine(rc, screen.mLeft + 6, y, screen.mRight - 6, y,
                           state.foreground);
            rc.emitted++;
            return;
        }
        if (state.kind == LLMenuItemGL::VkDrawState::Kind::TearOff)
        {
            const S32 y = screen.getHeight() / 3;
            emitBorderLine(rc, screen.mLeft + 6, screen.mBottom + y,
                           screen.mRight - 6, screen.mBottom + y,
                           state.foreground);
            emitBorderLine(rc, screen.mLeft + 6, screen.mBottom + y * 2,
                           screen.mRight - 6, screen.mBottom + y * 2,
                           state.foreground);
            rc.emitted++;
            return;
        }
        if (!state.font) return;

        if (state.menu_bar)
        {
            LLVKText::render(state.font, state.label,
                             (F32)screen.getCenterX(), (F32)(screen.mBottom + 1),
                             state.foreground, LLFontGL::HCENTER,
                             LLFontGL::BOTTOM, screen.getWidth());
        }
        else if (state.brief)
        {
            LLVKText::render(state.font, state.label,
                             (F32)(screen.mLeft + 1), (F32)screen.mBottom,
                             state.foreground, LLFontGL::LEFT,
                             LLFontGL::BOTTOM, screen.getWidth() - 2);
        }
        else
        {
            const F32 baseline = (F32)(screen.mBottom + 2);
            if (!state.bool_label.empty())
            {
                LLVKText::render(state.font, state.bool_label,
                                 (F32)(screen.mLeft + 3), baseline,
                                 state.foreground, LLFontGL::LEFT,
                                 LLFontGL::BOTTOM, 15);
            }
            LLVKText::render(state.font, state.label,
                             (F32)(screen.mLeft + 18), baseline,
                             state.foreground, LLFontGL::LEFT,
                             LLFontGL::BOTTOM,
                             llmax(0, screen.getWidth() - 40));
            if (!state.accel_label.empty())
            {
                LLVKText::render(state.font, state.accel_label,
                                 (F32)(screen.mRight - 22), baseline,
                                 state.foreground, LLFontGL::RIGHT,
                                 LLFontGL::BOTTOM, screen.getWidth());
            }
            if (!state.branch_label.empty())
            {
                LLVKText::render(state.font, state.branch_label,
                                 (F32)(screen.mRight - 7), baseline,
                                 state.foreground, LLFontGL::RIGHT,
                                 LLFontGL::BOTTOM, 15);
            }
        }
        rc.emitted++;
    }

    void prepareView(LLVKContext* context, const LLView* view)
    {
        if (!context || !view || !view->getVisible()) return;
        auto hook = s_prepare_hooks.find(&typeid(*view));
        if (hook != s_prepare_hooks.end()) hook->second(view, context);
        for (LLView::child_list_const_iter_t it = view->getChildList()->begin();
             it != view->getChildList()->end(); ++it)
        {
            prepareView(context, *it);
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

        // <VulkanStorm> Layout reconciliation: LLLayoutStack::draw() calls
        // updateLayout() to position its panels. The greenfield walk never runs
        // draw(), so layout stacks would otherwise report stale rects (the
        // "wrong location" gap). updateLayout() is pure rect math (no GL), so
        // calling it here makes the read positions match the GL result.
        if (LLLayoutStack* stack = dynamic_cast<LLLayoutStack*>(const_cast<LLView*>(view)))
        {
            stack->updateLayout();
        }
        if (LLFloater* floater = dynamic_cast<LLFloater*>(const_cast<LLView*>(view)))
        {
            floater->prepareVkDraw();
        }
        if (LLTabContainer* tabs = dynamic_cast<LLTabContainer*>(const_cast<LLView*>(view)))
        {
            tabs->prepareVkDraw();
        }
        if (LLScrollContainer* scroller =
                dynamic_cast<LLScrollContainer*>(const_cast<LLView*>(view)))
        {
            scroller->prepareVkDraw();
        }
        if (LLSearchEditor* search =
                dynamic_cast<LLSearchEditor*>(const_cast<LLView*>(view)))
        {
            search->prepareVkDraw();
        }

        // </VulkanStorm>

        // Widget-specific chrome. (v1: panels/floaters backgrounds; borders +
        // images + text land next.)
        const LLPanel* panel = dynamic_cast<const LLPanel*>(view);
        if (panel)
        {
            rc.panels++;
            if (const LLFloater* floater = dynamic_cast<const LLFloater*>(panel))
            {
                if (floater->isBackgroundVisible() && floater->getVkDropShadow())
                {
                    LLColor4 shadow = LLUIColorTable::instance()
                                          .getColor("ColorDropShadow").get();
                    emitDropShadow(rc, floater->calcScreenRect(), shadow,
                                   DROP_SHADOW_FLOATER);
                    rc.emitted++;
                }
            }
            size_t vbefore = LLVKUI2DSink::get().pendingVerts();
            renderPanelBackground(rc, panel);
            if (LLVKUI2DSink::get().pendingVerts() > vbefore) rc.emitted++;
        }

        if (const LLScrollbar* scrollbar = dynamic_cast<const LLScrollbar*>(view))
        {
            const LLScrollbar::VkDrawState state =
                scrollbar->getVkDrawState(rc.parent_alpha);
            if (state.bg_visible)
            {
                LLVKUIRender::emitScreenRect(view->calcScreenRect(), rc.dev_h,
                                             rc.ui_scale_y, state.bg_color);
            }
            if (LLVKUIImage::ready())
            {
                LLVKUIRender::emitScreenRect(state.track_rect, rc.dev_h,
                                             rc.ui_scale_y, state.track_image,
                                             state.track_color);
                LLVKUIRender::emitScreenRect(state.thumb_rect, rc.dev_h,
                                             rc.ui_scale_y, state.thumb_image,
                                             state.thumb_color);
            }
            else
            {
                LLVKUIRender::emitScreenRect(state.track_rect, rc.dev_h,
                                             rc.ui_scale_y, state.track_color);
                LLVKUIRender::emitScreenRect(state.thumb_rect, rc.dev_h,
                                             rc.ui_scale_y, state.thumb_color);
            }
            rc.emitted += 2;
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
                const LLComboBox* parent_combo =
                    dynamic_cast<const LLComboBox*>(view->getParent());
                const bool is_combo_button = parent_combo != nullptr;
                if (is_combo_button)
                {
                    int image_w = 0, image_h = 0;
                    if (LLVKUIImage::getSize(imgname, image_w, image_h))
                    {
                        // One state update supplies the missing intrinsic
                        // width; subsequent calls are a no-op and resizes use
                        // LLComboBox's normal layout path.
                        const_cast<LLComboBox*>(parent_combo)
                            ->setVkArrowImageWidth(image_w);
                    }
                }
                const LLRect button_screen = view->calcScreenRect();
                float button_l, button_t, button_r, button_b;
                toSinkRect(rc, button_screen,
                           button_l, button_t, button_r, button_b);
                if (is_combo_button)
                {
                    int image_w = 0, image_h = 0;
                    if (LLVKUIImage::getSize(imgname, image_w, image_h))
                    {
                        // LLComboBox::createLineEditor() uses the source image
                        // width plus both BTN_DROP_SHADOW margins. Its GL image
                        // pointer is null on Vulkan, so correct only the visual
                        // bounds; never mutate the live control tree here.
                        button_l = button_r - (float)(llmax(8, image_w) +
                                                      2 * BTN_DROP_SHADOW);
                    }
                }
                const bool floater_button = view->getName().find("llfloater_") == 0;
                if (s_dbg && (s_dbg_n < 12 || floater_button))
                {
                    ++s_dbg_n;
                    LL_INFOS("Vulkan") << "VKBUTTON '" << view->getName() << "' img='" << imgname << "'"
                                       << " empty=" << (imgname.empty() ? 1 : 0) << LL_ENDL;
                }
                if (!imgname.empty())
                {
                    float l = button_l, t = button_t;
                    float r = button_r, b = button_b;
                    if (!button->getScaleImage() && !is_combo_button)
                    {
                        int image_w = 0, image_h = 0;
                        if (LLVKUIImage::getSize(imgname, image_w, image_h))
                        {
                            // LLButton::draw() places an unscaled image at the
                            // local top-left corner.
                            r = l + (F32)image_w;
                            b = t + (F32)image_h;
                        }
                    }
                    LLVKUIImage::draw(imgname, l, t, r, b, imgc);
                    rc.emitted++;

                    const F32 glow = const_cast<LLButton*>(button)
                                         ->updateVkGlowStrength();
                    if (glow > 0.01f)
                    {
                        LLVKUI2DSink::get().setBlend(LLVKBlend::AddWithAlpha);
                        LLVKUIImage::draw(imgname, l, t, r, b,
                                          LLColor4(1.f, 1.f, 1.f,
                                                   glow * rc.parent_alpha));
                        LLVKUI2DSink::get().setBlend(LLVKBlend::Alpha);
                        rc.emitted++;
                    }
                }

                // LLButton::draw() composites image_overlay after the state
                // image. Login combos use this distinct layer for the arrow.
                const LLButton::VkOverlayState overlay =
                    button->getVkOverlayState(rc.parent_alpha);
                if (!overlay.name.empty())
                {
                    int ow = 0, oh = 0;
                    if (LLVKUIImage::getSize(overlay.name, ow, oh) && ow > 0 && oh > 0)
                    {
                        const float l = button_l, t = button_t;
                        const float r = button_r, b = button_b;
                        const float button_w = r - l;
                        const float button_h = b - t;
                        const float scale = llmin(llmin(button_w / (float)ow,
                                                       button_h / (float)oh), 1.f);
                        const float overlay_w = (float)ll_round((float)ow * scale);
                        const float overlay_h = (float)ll_round((float)oh * scale);
                        float overlay_l = l + (button_w - overlay_w) * 0.5f;
                        if (overlay.right_delta > 0)
                        {
                            overlay_l = r - overlay_w - (float)overlay.right_delta;
                        }
                        else if (overlay.alignment == LLFontGL::LEFT)
                        {
                            overlay_l = l + (float)overlay.left_pad;
                        }
                        else if (overlay.alignment == LLFontGL::RIGHT)
                        {
                            overlay_l = r - (float)overlay.right_pad - overlay_w;
                        }
                        const float center_adjust =
                            (float)(overlay.bottom_pad - overlay.top_pad);
                        const float overlay_t = t + (button_h - overlay_h) * 0.5f
                                                - center_adjust;
                        LLVKUIImage::draw(overlay.name, overlay_l, overlay_t,
                                          overlay_l + overlay_w,
                                          overlay_t + overlay_h, overlay.color);
                        rc.emitted++;
                    }
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

        // Vulkan-native text. LLTextBase supplies its already-reflowed line
        // rectangles; buttons and editors expose the final baseline/alignment
        // inputs consumed by their OpenGL draw methods.
        if (LLVKText::ready())
        {
            if (const LLTextBase* text = dynamic_cast<const LLTextBase*>(view))
            {
                std::vector<LLTextBase::VkTextRun> runs;
                const_cast<LLTextBase*>(text)->getVkTextRuns(rc.parent_alpha, runs);
                for (const LLTextBase::VkTextRun& run : runs)
                {
                    if (rc.dump)
                    {
                        LL_INFOS("Vulkan") << "VULKTEXT '" << view->getName()
                                           << "' rect=" << run.screen_rect.mLeft << ","
                                           << run.screen_rect.mBottom << "-"
                                           << run.screen_rect.mRight << ","
                                           << run.screen_rect.mTop << " text='"
                                           << wstring_to_utf8str(run.text) << "'" << LL_ENDL;
                    }
                    F32 y = (F32)run.screen_rect.mBottom;
                    if (run.valign == LLFontGL::TOP) y = (F32)run.screen_rect.mTop;
                    else if (run.valign == LLFontGL::VCENTER) y = (F32)run.screen_rect.getCenterY();

                    if (run.clip)
                    {
                        const S32 sx = llclamp(ll_round((F32)run.clip_rect.mLeft * rc.ui_scale_x),
                                               0, (S32)rc.dev_w);
                        const S32 sy = llclamp(ll_round((F32)(rc.dev_h / rc.ui_scale_y -
                                                              run.clip_rect.mTop) * rc.ui_scale_y),
                                               0, (S32)rc.dev_h);
                        const S32 sr = llclamp(ll_round((F32)run.clip_rect.mRight * rc.ui_scale_x),
                                               sx, (S32)rc.dev_w);
                        const S32 sb = llclamp(ll_round((F32)(rc.dev_h / rc.ui_scale_y -
                                                              run.clip_rect.mBottom) * rc.ui_scale_y),
                                               sy, (S32)rc.dev_h);
                        LLVKUI2DSink::get().setScissor(sx, sy, sr - sx, sb - sy);
                    }
                    else
                    {
                        LLVKUI2DSink::get().clearScissor();
                    }
                    LLVKText::render(run.font, run.text, (F32)run.screen_rect.mLeft, y,
                                     run.color, LLFontGL::LEFT, run.valign,
                                     run.screen_rect.getWidth(), run.ellipses,
                                     run.shadow);
                    rc.emitted++;
                }
                LLVKUI2DSink::get().clearScissor();
            }
            else if (line_editor)
            {
                const LLLineEditor::VkTextState state = line_editor->getVkTextState(rc.parent_alpha);
                LLVKText::render(state.font, state.text, state.screen_x, state.screen_baseline,
                                 state.color, LLFontGL::LEFT, LLFontGL::BOTTOM,
                                 state.max_pixels);
                rc.emitted++;
                if (state.caret_visible)
                {
                    LLVKUIRender::emitScreenRect(state.caret_rect, rc.dev_h,
                                                 rc.ui_scale_y,
                                                 state.caret_color);
                    rc.emitted++;
                }
            }
            else if (const LLButton* button = dynamic_cast<const LLButton*>(view))
            {
                const LLButton::VkLabelState state = button->getVkLabelState(rc.parent_alpha);
                LLVKText::render(state.font, state.text, state.screen_x, state.screen_baseline,
                                 state.color, state.halign, LLFontGL::VCENTER,
                                 state.max_pixels, state.ellipses,
                                 state.soft_shadow ? LLFontGL::DROP_SHADOW_SOFT : LLFontGL::NO_SHADOW);
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
        if (LLMenuItemGL* menu_item =
                dynamic_cast<LLMenuItemGL*>(const_cast<LLView*>(view)))
        {
            renderMenuItem(rc, menu_item);
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

    void registerViewPrepareHook(const std::type_info& type, ViewPrepareHook hook)
    {
        if (hook) s_prepare_hooks[&type] = hook;
    }

    void prepareFrame(LLVKContext* context, LLView* root)
    {
        prepareView(context, root);
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

    void emitScreenRect(const LLRect& gl_rect, unsigned device_height,
                        float ui_scale_y, const std::string& image_name,
                        const LLColor4& color)
    {
        if (gl_rect.isEmpty() || image_name.empty() || !LLVKUIImage::ready()) return;
        const F32 ui_h = (F32)device_height / ui_scale_y;
        const F32 left   = (F32)gl_rect.mLeft;
        const F32 right  = (F32)gl_rect.mRight;
        const F32 top    = ui_h - (F32)gl_rect.mTop;
        const F32 bottom = ui_h - (F32)gl_rect.mBottom;
        LLVKUIImage::draw(image_name, left, top, right, bottom, color);
    }

    void emitDropShadow(const LLRect& gl_rect, unsigned device_height,
                        float ui_scale_y, const LLColor4& color, S32 lines)
    {
        RenderCtx rc;
        rc.dev_h = device_height;
        rc.ui_scale_y = ui_scale_y;
        ::emitDropShadow(rc, gl_rect, color, lines);
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
