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
#include "llslider.h"          // slider track/thumb state
#include "llscrolllistctrl.h"  // popup/dropdown row layout and text state
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
#include "llvklegacytext.h"
#include "lluiimage.h"          // LLUIImage (regions)
#include "llvkuirenderinternal.h" // shared walker context for chrome passes
#include "llvkuiwidgets.h"      // additional per-widget chrome passes
#include "llvkuifolder.h"       // folder-view (inventory tree) chrome passes

using LLVKUIRenderInternal::RenderCtx;
using LLVKUIRenderInternal::toSinkRect;
using LLVKUIRenderInternal::pushClip;
using LLVKUIRenderInternal::popClip;
using LLVKUIRenderInternal::emitBorderLine;
using LLVKUIRenderInternal::emitDropShadow;

namespace LLVKUIRenderInternal
{
    void applyClip(const RenderCtx& rc)
    {
        // <VulkanStorm> diagnostic: VULKANSTORM_NO_CLIPSTACK=1 disables the
        // whole clip stack (isolates scissor regressions from other causes).
        static const bool s_no_clipstack = getenv("VULKANSTORM_NO_CLIPSTACK") != nullptr;
        if (s_no_clipstack)
        {
            LLVKUI2DSink::get().clearScissor();
            return;
        }
        if (rc.clip_stack.empty())
        {
            LLVKUI2DSink::get().clearScissor();
            return;
        }
        LLRect clip = rc.clip_stack.front();
        for (size_t i = 1; i < rc.clip_stack.size(); ++i)
        {
            clip.intersectWith(rc.clip_stack[i]);
        }
        const S32 sx = llclamp(ll_round((F32)clip.mLeft * rc.ui_scale_x),
                               0, (S32)rc.dev_w);
        const S32 sy = llclamp(ll_round((F32)(rc.dev_h / rc.ui_scale_y -
                                              clip.mTop) * rc.ui_scale_y),
                               0, (S32)rc.dev_h);
        const S32 sr = llclamp(ll_round((F32)clip.mRight * rc.ui_scale_x),
                               sx, (S32)rc.dev_w);
        const S32 sb = llclamp(ll_round((F32)(rc.dev_h / rc.ui_scale_y -
                                              clip.mBottom) * rc.ui_scale_y),
                               sy, (S32)rc.dev_h);
        LLVKUI2DSink::get().setScissor(sx, sy, sr - sx, sb - sy);
    }

    void pushClip(RenderCtx& rc, const LLRect& gl_screen_rect)
    {
        rc.clip_stack.push_back(gl_screen_rect);
        applyClip(rc);
    }

    void popClip(RenderCtx& rc)
    {
        if (!rc.clip_stack.empty()) rc.clip_stack.pop_back();
        applyClip(rc);
    }

    void emitBorderLine(const RenderCtx& rc, S32 x1, S32 y1, S32 x2, S32 y2,
                        const LLColor4& c)
    {
        const F32 ui_h = (F32)rc.dev_h / rc.ui_scale_y;
        const float xy[4] = { (F32)x1, ui_h - (F32)y1, (F32)x2, ui_h - (F32)y2 };
        LLVKUI2DSink::get().lineStrip(xy, 2, c.mV[VRED], c.mV[VGREEN], c.mV[VBLUE], c.mV[VALPHA]);
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

    void emitCircle(const RenderCtx& rc, F32 gl_center_x, F32 gl_center_y,
                    F32 radius, const LLColor4& color, bool filled,
                    S32 segments)
    {
        // CPU-triangulated gl_circle_2d: GL bottom-left -> sink top-left.
        const F32 ui_h = (F32)rc.dev_h / rc.ui_scale_y;
        const F32 cy = ui_h - gl_center_y;
        segments = llmax(segments, 8);
        if (filled)
        {
            std::vector<float> xy((segments + 2) * 3 * 2);
            std::vector<float> rgba((segments + 2) * 3 * 4);
            int n = 0;
            for (S32 i = 0; i < segments; ++i)
            {
                const F32 a0 = (F32)i / (F32)segments * (F32)F_TWO_PI;
                const F32 a1 = (F32)(i + 1) / (F32)segments * (F32)F_TWO_PI;
                const F32 vx[3] = { gl_center_x,
                                    gl_center_x + radius * cosf(a0),
                                    gl_center_x + radius * cosf(a1) };
                const F32 vy[3] = { cy,
                                    cy - radius * sinf(a0),
                                    cy - radius * sinf(a1) };
                for (int k = 0; k < 3; ++k)
                {
                    xy[(n + k) * 2]     = vx[k];
                    xy[(n + k) * 2 + 1] = vy[k];
                    rgba[(n + k) * 4]     = color.mV[VRED];
                    rgba[(n + k) * 4 + 1] = color.mV[VGREEN];
                    rgba[(n + k) * 4 + 2] = color.mV[VBLUE];
                    rgba[(n + k) * 4 + 3] = color.mV[VALPHA];
                }
                n += 3;
            }
            LLVKUI2DSink::get().rawTris(xy.data(), rgba.data(), n);
        }
        else
        {
            std::vector<float> xy((segments + 1) * 2);
            for (S32 i = 0; i <= segments; ++i)
            {
                const F32 a = (F32)i / (F32)segments * (F32)F_TWO_PI;
                xy[i * 2]     = gl_center_x + radius * cosf(a);
                xy[i * 2 + 1] = cy - radius * sinf(a);
            }
            LLVKUI2DSink::get().lineStrip(xy.data(), segments + 1,
                                          color.mV[VRED], color.mV[VGREEN],
                                          color.mV[VBLUE], color.mV[VALPHA]);
        }
    }
}

namespace
{
    // <VulkanStorm> Registered per-class hooks (newview-side classes).
    std::map<const std::type_info*, LLVKUIRender::ViewHook> s_hooks;
    std::map<const std::type_info*, LLVKUIRender::ViewPrepareHook> s_prepare_hooks;
    std::map<const std::type_info*, LLVKUIRender::ViewClipHook> s_clip_hooks;
    std::map<const std::type_info*, LLVKUIRender::ViewSubtreeAlphaHook> s_alpha_hooks;
    RenderCtx* s_active_render_ctx = nullptr;
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

        // <VulkanStorm> diagnostic: which panel paints over the top menu strip?
        static bool s_dbg_menu = getenv("VULKANSTORM_MENU_DEBUG") != nullptr;
        static int  s_dbg_panel_n = 0;
        if (s_dbg_menu && screen.mTop > 1340 && screen.getHeight() > 10 && s_dbg_panel_n < 20)
        {
            ++s_dbg_panel_n;
            const LLColor4 bg = panel->isBackgroundOpaque() ? panel->getBackgroundColor() : panel->getTransparentColor();
            LL_INFOS("Vulkan") << "VKPANEL-TOPSTRIP '" << panel->getName()
                               << "' (" << typeid(*panel).name() << ")"
                               << " rect=" << screen.mLeft << "," << screen.mBottom
                               << "-" << screen.mRight << "," << screen.mTop
                               << " opaque=" << (panel->isBackgroundOpaque() ? 1 : 0)
                               << " bg=" << bg.mV[0] << "," << bg.mV[1] << "," << bg.mV[2] << "," << bg.mV[3]
                               << LL_ENDL;
        }
        // </VulkanStorm>

        // A floater is a separate in-viewer window. Its image/color can retain
        // alpha for edge decoration and inactive-state tinting, but its body
        // must first occlude the scene and any CEF surface below it. OpenGL
        // obtains that composition from the floater pass; Vulkan needs the
        // equivalent opaque underlay explicitly.
        //
        // <VulkanStorm> Only emit the underlay when the floater actually paints
        // a background. In GL, LLFloater::draw() -> LLPanel::draw() emits
        // NOTHING when mBgVisible is false. Toasts (LLToast) run with an
        // invisible background so the world/menu shows through; the previous
        // unconditional underlay painted an opaque strip over the menu bar
        // and clipped dropdown popups. Keying on isBackgroundVisible() matches
        // GL exactly.
        if (panel->isBackgroundVisible())
        {
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
    }

    // <VulkanStorm> M3: non-text chrome helpers.

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

        // <VulkanStorm> one-shot diagnostic: why is a menu item's text absent?
        static bool s_dbg_menu = getenv("VULKANSTORM_MENU_DEBUG") != nullptr;
        static int  s_dbg_menu_n = 0;
        if (s_dbg_menu && state.menu_bar && s_dbg_menu_n < 12)
        {
            ++s_dbg_menu_n;
            LL_INFOS("Vulkan") << "VKMENU '" << item->getName()
                               << "' vis=" << (item->getVisible() ? 1 : 0)
                               << " label_len=" << state.label.size()
                               << " font=" << (void*)state.font
                               << " color=" << state.foreground.mV[0] << "," << state.foreground.mV[1]
                               << "," << state.foreground.mV[2] << "," << state.foreground.mV[3]
                               << " rect=" << screen.mLeft << "," << screen.mBottom
                               << "-" << screen.mRight << "," << screen.mTop
                               << " clips=" << rc.clip_stack.size() << LL_ENDL;
        }
        // </VulkanStorm>

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
            S32 drawn = LLVKText::render(state.font, state.label,
                             (F32)screen.getCenterX(), (F32)(screen.mBottom + 1),
                             state.foreground, LLFontGL::HCENTER,
                             LLFontGL::BOTTOM, S32_MAX);
            // <VulkanStorm> diagnostic: confirm glyphs actually emitted.
            if (s_dbg_menu && s_dbg_menu_n <= 12)
            {
                LL_INFOS("Vulkan") << "VKMENU-RENDER '" << item->getName()
                                  << "' drawn=" << drawn
                                  << " cx=" << screen.getCenterX()
                                  << " basey=" << (screen.mBottom + 1) << LL_ENDL;
            }
            // </VulkanStorm>
        }
        else if (state.brief)
        {
            LLVKText::render(state.font, state.label,
                             (F32)(screen.mLeft + 1), (F32)screen.mBottom,
                             state.foreground, LLFontGL::LEFT,
                             LLFontGL::BOTTOM, S32_MAX);
        }
        else
        {
            const F32 baseline = (F32)(screen.mBottom + 2);
            if (!state.bool_label.empty())
            {
                LLVKText::render(state.font, state.bool_label,
                                 (F32)(screen.mLeft + 3), baseline,
                                 state.foreground, LLFontGL::LEFT,
                                 LLFontGL::BOTTOM, S32_MAX);
            }
            LLVKText::render(state.font, state.label,
                             (F32)(screen.mLeft + 18), baseline,
                             state.foreground, LLFontGL::LEFT,
                             LLFontGL::BOTTOM, S32_MAX);
            if (!state.accel_label.empty())
            {
                LLVKText::render(state.font, state.accel_label,
                                 (F32)(screen.mRight - 22), baseline,
                                 state.foreground, LLFontGL::RIGHT,
                                 LLFontGL::BOTTOM, S32_MAX);
            }
            if (!state.branch_label.empty())
            {
                LLVKText::render(state.font, state.branch_label,
                                 (F32)(screen.mRight - 7), baseline,
                                 state.foreground, LLFontGL::RIGHT,
                                 LLFontGL::BOTTOM, S32_MAX);
            }
        }
        rc.emitted++;
    }

    void prepareView(LLVKContext* context, const LLView* view)
    {
        if (!context || !view) return;

        // <VulkanStorm> A combo's dropdown list stays invisible until the combo
        // opens, so the visibility gate below would skip it and its row glyphs
        // would never be rasterized/uploaded (blank dropdown). Prepare the list
        // contents explicitly before the visibility check drops this subtree.
        if (LLComboBox* combo = dynamic_cast<LLComboBox*>(const_cast<LLView*>(view)))
        {
            if (LLScrollListCtrl* list = combo->getVkList())
            {
                list->prepareVkDraw();
                if (LLVKText::ready())
                {
                    LLScrollListCtrl::VkDrawState state;
                    list->getVkDrawState(1.f, state);
                    for (const LLScrollListCtrl::VkRowState& row : state.rows)
                        for (const LLScrollListCtrl::VkTextCellState& cell : row.cells)
                            LLVKText::prepare(cell.font, cell.text);
                }
            }
        }
        // </VulkanStorm>

        if (!view->getVisible()) return;

        // Several GL widgets finalize layout from draw(). Vulkan never calls
        // draw(), so make the same non-rendering updates before collecting
        // glyphs and before input dispatch uses their rectangles.
        if (LLLayoutStack* stack = dynamic_cast<LLLayoutStack*>(const_cast<LLView*>(view)))
            stack->updateLayout();
        if (LLFloater* floater = dynamic_cast<LLFloater*>(const_cast<LLView*>(view)))
            floater->prepareVkDraw();
        if (LLTabContainer* tabs = dynamic_cast<LLTabContainer*>(const_cast<LLView*>(view)))
            tabs->prepareVkDraw();
        if (LLScrollContainer* scroller = dynamic_cast<LLScrollContainer*>(const_cast<LLView*>(view)))
            scroller->prepareVkDraw();
        if (LLSlider* slider = dynamic_cast<LLSlider*>(const_cast<LLView*>(view)))
        {
            int thumb_width = 16;
            int thumb_height = 16;
            LLVKUIImage::getSize(slider->getVkThumbImageName(),
                                 thumb_width, thumb_height);
            slider->prepareVkDraw(thumb_width, thumb_height);
        }
        if (LLSearchEditor* search = dynamic_cast<LLSearchEditor*>(const_cast<LLView*>(view)))
            search->prepareVkDraw();
        if (LLScrollListCtrl* list = dynamic_cast<LLScrollListCtrl*>(const_cast<LLView*>(view)))
            list->prepareVkDraw();
        if (LLMenuGL* menu = dynamic_cast<LLMenuGL*>(const_cast<LLView*>(view)))
            menu->arrangeAndClear();

        auto hook = s_prepare_hooks.find(&typeid(*view));
        if (hook != s_prepare_hooks.end()) hook->second(view, context);

        // Additional widget preparation (llvkuiwidgets.cpp / llvkuifolder.cpp).
        static const bool s_no_widgets_prepare = getenv("VULKANSTORM_NO_WIDGETS") != nullptr;
        if (!s_no_widgets_prepare)
        {
            LLVKUIWidgets::prepareView(context, view);
            LLVKUIFolder::prepareView(context, view);
        }

        if (LLVKText::ready())
        {
            if (const LLTextBase* text = dynamic_cast<const LLTextBase*>(view))
            {
                std::vector<LLTextBase::VkTextRun> runs;
                const_cast<LLTextBase*>(text)->getVkTextRuns(1.f, runs);
                for (const LLTextBase::VkTextRun& run : runs)
                    LLVKText::prepare(run.font, run.text);
            }
            else if (const LLLineEditor* editor = dynamic_cast<const LLLineEditor*>(view))
            {
                const LLLineEditor::VkTextState state = editor->getVkTextState(1.f);
                LLVKText::prepare(state.font, state.text);
                LLVKText::prepare(state.font, state.selected_text);
                LLVKText::prepare(state.font, state.trailing_text);
            }
            else if (const LLButton* button = dynamic_cast<const LLButton*>(view))
            {
                const LLButton::VkLabelState state = button->getVkLabelState(1.f);
                LLVKText::prepare(state.font, state.text);
            }
            if (LLMenuItemGL* item = dynamic_cast<LLMenuItemGL*>(const_cast<LLView*>(view)))
            {
                const LLMenuItemGL::VkDrawState state = item->getVkDrawState(1.f);
                LLVKText::prepare(state.font, state.label);
                LLVKText::prepare(state.font, state.bool_label);
                LLVKText::prepare(state.font, state.accel_label);
                LLVKText::prepare(state.font, state.branch_label);
            }
            if (LLScrollListCtrl* list = dynamic_cast<LLScrollListCtrl*>(const_cast<LLView*>(view)))
            {
                LLScrollListCtrl::VkDrawState state;
                list->getVkDrawState(1.f, state);
                for (const LLScrollListCtrl::VkRowState& row : state.rows)
                    for (const LLScrollListCtrl::VkTextCellState& cell : row.cells)
                        LLVKText::prepare(cell.font, cell.text);
            }
        }
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

        // Scroll container opaque background (LLScrollContainer::draw()'s
        // mIsOpaque branch); the scrolled-children clip is applied at the
        // child recursion below.
        if (const LLScrollContainer* scroll_container =
                dynamic_cast<const LLScrollContainer*>(view))
        {
            const LLScrollContainer::VkBackground bg =
                scroll_container->getVkBackground(rc.parent_alpha);
            if (bg.bg_visible)
            {
                LLVKUIRender::emitScreenRect(bg.inner_rect, rc.dev_h,
                                             rc.ui_scale_y, bg.bg_color);
                rc.emitted++;
            }
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

        if (const LLSlider* slider = dynamic_cast<const LLSlider*>(view))
        {
            const LLSlider::VkDrawState state =
                slider->getVkDrawState(rc.parent_alpha);
            int track_width = 0;
            int track_height = 0;
            LLVKUIImage::getSize(state.track_image, track_width, track_height);
            if (track_width <= 0) track_width = state.horizontal ? 1 : 4;
            if (track_height <= 0) track_height = state.horizontal ? 4 : 1;

            LLRect track_rect;
            LLRect highlight_rect;
            if (state.horizontal)
            {
                const S32 cy = state.control_rect.getCenterY();
                track_rect.set(state.thumb_rect.getWidth() / 2 + state.control_rect.mLeft,
                               cy + track_height / 2,
                               state.control_rect.mRight - state.thumb_rect.getWidth() / 2,
                               cy - track_height / 2);
                highlight_rect.set(track_rect.mLeft, track_rect.mTop,
                                   state.thumb_rect.getCenterX(), track_rect.mBottom);
            }
            else
            {
                const S32 cx = state.control_rect.getCenterX();
                track_rect.set(cx - track_width / 2, state.control_rect.mTop,
                               cx + track_width / 2, state.control_rect.mBottom);
                highlight_rect = track_rect;
            }

            LLVKUIRender::emitScreenRect(track_rect, rc.dev_h, rc.ui_scale_y,
                                         state.track_image, state.track_color);
            LLVKUIRender::emitScreenRect(highlight_rect, rc.dev_h, rc.ui_scale_y,
                                         state.track_highlight_image,
                                         state.track_color);
            if (state.draw_focus)
            {
                const LLColor4 focus = gFocusMgr.getFocusColor() % rc.parent_alpha;
                float l, t, r, b;
                toSinkRect(rc, state.thumb_rect, l, t, r, b);
                LLVKUIImage::drawBorder(state.thumb_image, l, t, r, b,
                                        focus, gFocusMgr.getFocusFlashWidth());
            }
            if (state.draw_ghost)
            {
                LLVKUIRender::emitScreenRect(state.drag_start_thumb_rect,
                                             rc.dev_h, rc.ui_scale_y,
                                             slider->getVkThumbImageName(),
                                             state.ghost_color);
            }
            LLVKUIRender::emitScreenRect(state.thumb_rect, rc.dev_h,
                                         rc.ui_scale_y, state.thumb_image,
                                         state.thumb_color);
            rc.emitted += 3 + (state.draw_focus ? 1 : 0) +
                          (state.draw_ghost ? 1 : 0);
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
                // Editable combos use a narrow arrow button beside their
                // line editor. Non-editable combos use DropDown_* as the
                // stretchable background for the entire field.
                const bool is_combo_button = parent_combo != nullptr &&
                                             parent_combo->acceptsTextInput();
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

        // Scroll lists (including combo popup menus) draw their rows directly
        // rather than as child text views. Emit the same row geometry here;
        // prepareVkDraw() also keeps these rectangles current for hit testing.
        if (LLScrollListCtrl* list =
                dynamic_cast<LLScrollListCtrl*>(const_cast<LLView*>(view)))
        {
            LLScrollListCtrl::VkDrawState state;
            list->getVkDrawState(rc.parent_alpha, state);

            // <VulkanStorm> diagnostic: VULKANSTORM_LIST_DEBUG=1 logs the
            // row/cell census for scroll lists (catches empty dropdowns).
            static const bool s_dbg_list = getenv("VULKANSTORM_LIST_DEBUG") != nullptr;
            if (s_dbg_list)
            {
                S32 total_cells = 0;
                for (const auto& row : state.rows) total_cells += (S32)row.cells.size();
                LL_INFOS("Vulkan") << "VKLIST '" << view->getName()
                                   << "' rows=" << state.rows.size()
                                   << " cells=" << total_cells
                                   << " clip=" << state.clip_rect.mLeft << "," << state.clip_rect.mBottom
                                   << "-" << state.clip_rect.mRight << "," << state.clip_rect.mTop
                                   << " bg=" << (state.background_visible ? 1 : 0)
                                   << " bgcolor=" << state.background.mV[0] << "," << state.background.mV[1]
                                   << "," << state.background.mV[2] << "," << state.background.mV[3]
                                   << " vis=" << (view->getVisible() ? 1 : 0) << LL_ENDL;
                // Log the first few cells' text content + width to catch
                // empty/zero-width dropdown rows.
                int n = 0;
                for (const auto& row : state.rows)
                {
                    for (const auto& cell : row.cells)
                    {
                        if (n++ >= 6) break;
                        LL_INFOS("Vulkan") << "  VKCELL text='" << wstring_to_utf8str(cell.text)
                                           << "' max_px=" << cell.max_pixels
                                           << " color=" << cell.color.mV[0] << "," << cell.color.mV[1]
                                           << "," << cell.color.mV[2] << "," << cell.color.mV[3]
                                           << " rect=" << cell.screen_rect.mLeft << "," << cell.screen_rect.mBottom
                                           << "-" << cell.screen_rect.mRight << "," << cell.screen_rect.mTop
                                           << " font=" << (void*)cell.font
                                           << " x=" << cell.screen_x << LL_ENDL;
                    }
                }
            }
            // </VulkanStorm>

            if (state.background_visible)
            {
                LLVKUIRender::emitScreenRect(state.background_rect, rc.dev_h,
                                             rc.ui_scale_y, state.background);
                rc.emitted++;
            }
            pushClip(rc, state.clip_rect);
            for (const LLScrollListCtrl::VkRowState& row : state.rows)
            {
                if (row.background_visible)
                    LLVKUIRender::emitScreenRect(row.screen_rect, rc.dev_h,
                                                 rc.ui_scale_y, row.background);
                if (LLVKText::ready())
                {
                    for (const LLScrollListCtrl::VkTextCellState& cell : row.cells)
                    {
                        // Search/type-ahead highlight behind the matched
                        // substring (draw()'s mHighlightCount branch).
                        if (cell.highlight_visible)
                        {
                            float l, t, r, b;
                            toSinkRect(rc, cell.highlight_rect, l, t, r, b);
                            LLVKUIImage::draw("Rounded_Square", l, t, r, b,
                                              cell.highlight_color);
                        }
                        LLVKText::render(cell.font, cell.text, cell.screen_x,
                                         cell.screen_baseline, cell.color,
                                         cell.alignment, LLFontGL::BOTTOM,
                                         cell.max_pixels);
                        // <VulkanStorm> diagnostic: dump the scissor stack state
                        // at the moment a combo-popup row's text renders.
                        if (s_dbg_list)
                        {
                            const S32 glyphs = LLVKText::debugGlyphCount(cell.font);
                            const F32 adv = LLVKText::debugMeasureAdvance(cell.font, cell.text);
                            LL_INFOS("Vulkan") << "VKCELL-GLYPHS '" << wstring_to_utf8str(cell.text)
                                               << "' font_glyphs=" << glyphs
                                               << " advance=" << adv
                                               << " stack=" << rc.clip_stack.size() << LL_ENDL;
                        }
                        // </VulkanStorm>
                    }
                }
                // Icon cells (LLScrollListIcon / LLScrollListIconText).
                for (LLScrollListCtrl::VkIconCellState icon : row.icons)
                {
                    if (icon.image.empty() || !LLVKUIImage::ready()) continue;
                    if (icon.screen_rect.isEmpty())
                    {
                        // Intrinsic-sized icon whose dimensions were unknown
                        // to llui (no GL image); resolve via the registry and
                        // re-anchor at the cell's left/bottom.
                        int w = 0, h = 0;
                        LLVKUIImage::getSize(icon.image, w, h);
                        if (w <= 0 || h <= 0) continue;
                        icon.screen_rect.mRight = icon.screen_rect.mLeft + w;
                        icon.screen_rect.mTop = icon.screen_rect.mBottom + h;
                    }
                    float l, t, r, b;
                    toSinkRect(rc, icon.screen_rect, l, t, r, b);
                    LLVKUIImage::draw(icon.image, l, t, r, b, icon.color);
                }
                // Bar cells (LLScrollListBar).
                for (const LLScrollListCtrl::VkBarCellState& bar : row.bars)
                {
                    LLVKUIRender::emitScreenRect(bar.screen_rect, rc.dev_h,
                                                 rc.ui_scale_y, bar.color);
                }
                // Embedded checkbox cells (LLScrollListCheck).
                for (const LLScrollListCtrl::VkCheckCellState& check : row.checks)
                {
                    if (check.image.empty() || !LLVKUIImage::ready()) continue;
                    LLRect rect = check.screen_rect;
                    if (!check.scale_image)
                    {
                        int w = 0, h = 0;
                        if (LLVKUIImage::getSize(check.image, w, h) && w > 0 && h > 0)
                        {
                            // Unscaled image at the local top-left (LLButton).
                            rect.mRight = rect.mLeft + w;
                            rect.mBottom = rect.mTop - h;
                        }
                    }
                    float l, t, r, b;
                    toSinkRect(rc, rect, l, t, r, b);
                    LLVKUIImage::draw(check.image, l, t, r, b, check.color);
                }
                rc.emitted++;
            }
            popClip(rc);
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
                        pushClip(rc, run.clip_rect);
                    }
                    LLVKText::render(run.font, run.text, (F32)run.screen_rect.mLeft, y,
                                     run.color, LLFontGL::LEFT, run.valign,
                                     run.screen_rect.getWidth(), run.ellipses,
                                     run.shadow);
                    rc.emitted++;
                    if (run.clip)
                    {
                        popClip(rc);
                    }
                }
            }
            else if (line_editor)
            {
                const LLLineEditor::VkTextState state = line_editor->getVkTextState(rc.parent_alpha);
                if (state.selection_visible)
                {
                    LLVKUIRender::emitScreenRect(state.selection_rect, rc.dev_h,
                                                 rc.ui_scale_y,
                                                 state.selection_color);
                    rc.emitted++;
                }
                // IME preedit underlines paint before the text (GL order).
                std::vector<LLLineEditor::VkPreeditMarker> markers;
                line_editor->getVkPreeditMarkers(rc.parent_alpha, markers);
                for (const LLLineEditor::VkPreeditMarker& marker : markers)
                {
                    LLRect marker_screen;
                    line_editor->localRectToScreen(marker.local_rect, &marker_screen);
                    LLVKUIRender::emitScreenRect(marker_screen, rc.dev_h,
                                                 rc.ui_scale_y, marker.color);
                    rc.emitted++;
                }
                LLVKText::render(state.font, state.text, state.screen_x, state.screen_baseline,
                                 state.color, LLFontGL::LEFT, LLFontGL::BOTTOM,
                                 state.max_pixels);
                if (state.selection_visible)
                {
                    LLVKText::render(state.font, state.selected_text,
                                     state.selected_x, state.screen_baseline,
                                     state.selected_text_color, LLFontGL::LEFT,
                                     LLFontGL::BOTTOM, state.selected_max_pixels);
                    LLVKText::render(state.font, state.trailing_text,
                                     state.trailing_x, state.screen_baseline,
                                     state.color, LLFontGL::LEFT,
                                     LLFontGL::BOTTOM, state.trailing_max_pixels);
                }
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

        // </VulkanStorm> Registered per-class hooks (e.g. LLMediaCtrl's
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

        // Additional per-widget chrome passes (llvkuiwidgets.cpp /
        // llvkuifolder.cpp): progress bars, badges, multi-sliders, editor
        // extras, accordion chrome, folder-view rows, stat widgets, etc.
        // <VulkanStorm> diagnostic: VULKANSTORM_NO_WIDGETS=1 disables the
        // additional widget passes (isolates them from the core walker).
        static const bool s_no_widgets = getenv("VULKANSTORM_NO_WIDGETS") != nullptr;
        if (!s_no_widgets)
        {
            LLVKUIWidgets::renderChrome(rc, view);
            LLVKUIFolder::renderChrome(rc, view);
        }

        // Recurse children in painter's order. mChildList front = top-most, so
        // reverse iteration draws back-to-front (deepest first).
        //
        // Clipping: scroll containers clip only their scrolled content child
        // (the scrollbars/border stay outside the clip, as in
        // LLScrollContainer::draw()); registered clip hooks clip a view's
        // whole subtree (the LLScreenClipRect pattern in newview widgets).
        const LLScrollContainer* scroller =
            dynamic_cast<const LLScrollContainer*>(view);
        const LLView* scrolled_view =
            scroller ? scroller->getVkScrolledView() : nullptr;
        LLRect scroller_clip;
        const bool has_scroller_clip =
            scrolled_view && scroller->getVkScrolledClipRect(scroller_clip);

        LLRect subtree_clip;
        bool has_subtree_clip = false;
        if (!s_clip_hooks.empty())
        {
            auto clip_it = s_clip_hooks.find(&typeid(*view));
            if (clip_it != s_clip_hooks.end())
            {
                has_subtree_clip = clip_it->second(view, subtree_clip);
            }
        }
        if (!has_subtree_clip)
        {
            has_subtree_clip = LLVKUIWidgets::subtreeClip(view, subtree_clip);
        }

        F32 saved_alpha = rc.parent_alpha;
        if (!s_alpha_hooks.empty())
        {
            auto alpha_it = s_alpha_hooks.find(&typeid(*view));
            if (alpha_it != s_alpha_hooks.end())
            {
                rc.parent_alpha *= alpha_it->second(view);
            }
        }
        rc.parent_alpha *= LLVKUIWidgets::subtreeAlpha(view);

        if (has_subtree_clip)
        {
            pushClip(rc, subtree_clip);
        }
        rc.depth++;
        for (LLView::child_list_const_reverse_iter_t it = view->getChildList()->rbegin();
             it != view->getChildList()->rend(); ++it)
        {
            const LLView* child = *it;
            // <VulkanStorm> An open combo's dropdown list is a registered
            // popup: LLPopupView re-renders it above everything via
            // renderOverlaySubtree(). Skip the in-tree copy (which runs under
            // the combo's parent layout clip and fights the overlay on hover).
            if (!rc.in_overlay)
            {
                if (const LLComboBox* combo = dynamic_cast<const LLComboBox*>(view))
                {
                    const LLScrollListCtrl* open_list = combo->getVkList();
                    if (open_list && child == open_list && open_list->getVisible())
                    {
                        continue;
                    }
                }
            }
            // </VulkanStorm>
            const bool clip_child =
                has_scroller_clip && child == scrolled_view;
            if (clip_child)
            {
                pushClip(rc, scroller_clip);
            }
            LLRect widget_child_clip;
            const bool has_widget_child_clip =
                LLVKUIWidgets::childClip(view, child, widget_child_clip);
            if (has_widget_child_clip)
            {
                pushClip(rc, widget_child_clip);
            }
            // <VulkanStorm> LLLayoutStack::draw() clips each LLLayoutPanel
            // child to its layout rect (this keeps e.g. the login panel out
            // of the menu strip). Reproduce with the clip stack.
            LLRect layout_child_clip;
            const LLLayoutStack* layout_stack =
                dynamic_cast<const LLLayoutStack*>(view);
            const bool has_layout_clip =
                layout_stack &&
                layout_stack->getVkPanelClipRect(child, layout_child_clip);
            if (has_layout_clip)
            {
                pushClip(rc, layout_child_clip);
            }
            // </VulkanStorm>
            renderView(rc, child);
            if (has_layout_clip)
            {
                popClip(rc);
            }
            if (has_widget_child_clip)
            {
                popClip(rc);
            }
            if (clip_child)
            {
                popClip(rc);
            }
        }
        rc.depth--;
        if (has_subtree_clip)
        {
            popClip(rc);
        }
        rc.parent_alpha = saved_alpha;
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

    void registerViewClipHook(const std::type_info& type, ViewClipHook hook)
    {
        if (hook) s_clip_hooks[&type] = hook;
    }

    void registerViewSubtreeAlphaHook(const std::type_info& type, ViewSubtreeAlphaHook hook)
    {
        if (hook) s_alpha_hooks[&type] = hook;
    }

    void renderOverlaySubtree(const LLView* root)
    {
        if (!root || !s_active_render_ctx) return;

        // OpenGL draws a registered popup once in its ordinary hierarchy and
        // again from LLPopupView above the remaining UI. Share the enclosing
        // Vulkan context so the second traversal has identical state/order,
        // and flag it so popup-owned subtrees (a combo's open list) render
        // here rather than in the in-tree walk.
        RenderCtx& rc = *s_active_render_ctx;
        const bool saved = rc.in_overlay;
        rc.in_overlay = true;
        renderView(rc, root);
        rc.in_overlay = saved;
    }

    void prepareFrame(LLVKContext* context, LLView* root)
    {
        prepareView(context, root);
        LLVKText::flushPrepared();
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

        s_active_render_ctx = &rc;
        renderView(rc, root);
        s_active_render_ctx = nullptr;

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
