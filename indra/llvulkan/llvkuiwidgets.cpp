/**
 * @file llvkuiwidgets.cpp
 * @brief Greenfield Vulkan passes for the remaining llui chrome widgets.
 *
 * @details
 * Per-widget passes for the widgets whose GL draw() emits chrome the walker
 * core (llvkuirender.cpp) does not cover: progress bars, loading indicators,
 * resize handles, badges, multi-sliders, accordion chrome, text-editor IME
 * markers, modal-dialog shadows, plus the GL-free draw()-time preparation for
 * tooltips, flyout buttons, drag handles, chat entry expansion, scroll-list
 * column sort arrows, toolbars, window shades, and accordion auto-scroll.
 *
 * Every pass reads widget state via the widgets' public Vk* accessors and
 * emits into the LLVKUI2D sink. GL-free: no gGL / LLRender / LLImageGL.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, The Vulkanstorm Project
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "llvkuiwidgets.h"

#include <cmath>                // sinf (progress bar pulse)
#include <vector>

#include "llvkuirender.h"
#include "llvkuirenderinternal.h"
#include "llvkuiimage.h"
#include "llvklegacytext.h"

#include "llview.h"
#include "llprogressbar.h"
#include "lltooltip.h"
#include "llresizehandle.h"
#include "llloadingindicator.h"
#include "llbadge.h"
#include "llmultislider.h"
#include "llaccordionctrl.h"
#include "llaccordionctrltab.h"
#include "llwindowshade.h"
#include "llflyoutbutton.h"
#include "lldraghandle.h"
#include "lltexteditor.h"
#include "llchatentry.h"
#include "llscrolllistcolumn.h"
#include "llmodaldialog.h"
#include "llfloater.h"
#include "lltoolbar.h"
#include "llstatbar.h"
#include "llstatgraph.h"
#include "llvirtualtrackball.h"
#include "llxyvector.h"
#include "llconsole.h"
#include "lldockablefloater.h"
#include "lldockcontrol.h"
#include "llfocusmgr.h"
#include "lluicolortable.h"
#include "lluictrl.h"           // DROP_SHADOW_FLOATER
#include "lltimer.h"            // LLTimer (progress bar pulse)
#include "llframetimer.h"

using namespace LLVKUIRenderInternal;

namespace
{
    // Mirror LLProgressBar::draw(): background image, then the fill image
    // clipped to the completed fraction with the sine-pulse alpha.
    void renderProgressBar(RenderCtx& rc, const LLProgressBar* bar)
    {
        const LLProgressBar::VkDrawState state = bar->getVkDrawState(rc.parent_alpha);
        const LLRect screen = bar->calcScreenRect();
        if (screen.isEmpty()) return;

        float l, t, r, b;
        toSinkRect(rc, screen, l, t, r, b);

        if (!state.bar_image.empty() && LLVKUIImage::ready())
        {
            LLVKUIImage::draw(state.bar_image, l, t, r, b, state.bar_color);
            rc.emitted++;
        }
        if (!state.fill_image.empty() && state.percent > 0.f && LLVKUIImage::ready())
        {
            static LLTimer timer;   // same shared cadence as draw()'s static
            const F32 pulse = 0.5f + 0.5f * 0.5f * (1.f + (F32)sin(3.f * timer.getElapsedTimeF32()));
            LLColor4 fill = state.fill_color;
            fill.mV[VALPHA] *= pulse;
            LLRect fill_screen(screen);
            fill_screen.mRight = screen.mLeft +
                ll_round(screen.getWidth() * (state.percent / 100.f));
            float fl, ft, fr, fb;
            toSinkRect(rc, fill_screen, fl, ft, fr, fb);
            LLVKUIImage::draw(state.fill_image, fl, ft, fr, fb, fill);
            rc.emitted++;
        }
    }

    // Mirror LLResizeHandle::draw(): the corner image, unscaled, bottom-left.
    void renderResizeHandle(RenderCtx& rc, const LLResizeHandle* handle)
    {
        const LLResizeHandle::VkDrawState state = handle->getVkDrawState();
        if (!state.draw_image || state.image.empty() || !LLVKUIImage::ready()) return;

        int w = 0, h = 0;
        LLVKUIImage::getSize(state.image, w, h);
        if (w <= 0 || h <= 0) return;

        const LLRect screen = handle->calcScreenRect();
        const LLRect image_screen(screen.mLeft, screen.mBottom + h,
                                  screen.mLeft + w, screen.mBottom);
        float l, t, r, b;
        toSinkRect(rc, image_screen, l, t, r, b);
        LLVKUIImage::draw(state.image, l, t, r, b,
                          LLColor4::white % rc.parent_alpha);
        rc.emitted++;
    }

    // Mirror LLLoadingIndicator::draw(): the current animation frame stretched
    // over the widget rect (prepareVkDraw advanced the frame index).
    void renderLoadingIndicator(RenderCtx& rc, const LLLoadingIndicator* indicator)
    {
        const std::string image = indicator->getVkImageName();
        if (image.empty() || !LLVKUIImage::ready()) return;

        float l, t, r, b;
        toSinkRect(rc, indicator->calcScreenRect(), l, t, r, b);
        LLVKUIImage::draw(image, l, t, r, b,
                          LLColor4::white % rc.parent_alpha);
        rc.emitted++;
    }

    // Mirror LLBadge::draw(): badge image (+ optional border image), then the
    // centered label with a soft drop shadow. Geometry arrives badge-local;
    // convert through the badge's own frame.
    void renderBadge(RenderCtx& rc, const LLBadge* badge)
    {
        LLBadge::VkDrawState state;
        badge->getVkDrawState(rc.parent_alpha, state);
        if (!state.visible) return;

        LLRect badge_screen;
        badge->localRectToScreen(state.badge_rect, &badge_screen);
        float l, t, r, b;
        toSinkRect(rc, badge_screen, l, t, r, b);

        if (!state.image.empty() && LLVKUIImage::ready())
        {
            // LLUIImage::drawSolid semantics: texture ignored, tinted fill.
            LLVKUIImage::drawSolid(state.image, l, t, r, b, state.image_color);
            rc.emitted++;
            if (!state.border_image.empty())
            {
                LLVKUIImage::drawSolid(state.border_image, l, t, r, b,
                                       state.border_color);
                rc.emitted++;
            }
        }
        else
        {
            // draw()'s no-image fallback: a plain REPLACE-mode rect.
            LLVKUI2DSink::get().setBlend(LLVKBlend::Replace);
            LLVKUIRender::emitScreenRect(badge_screen, rc.dev_h, rc.ui_scale_y,
                                         state.image_color);
            LLVKUI2DSink::get().setBlend(LLVKBlend::Alpha);
            rc.emitted++;
        }

        if (LLVKText::ready() && state.font && !state.label.empty())
        {
            const LLRect widget_screen = badge->calcScreenRect();
            // Badge-local -> GL screen (badge-local y is bottom-up).
            const F32 text_x = (F32)widget_screen.mLeft + state.label_x;
            const F32 text_y = (F32)widget_screen.mBottom + state.label_y;
            LLVKText::render(state.font, state.label, text_x, text_y,
                             state.label_color, LLFontGL::HCENTER,
                             LLFontGL::VCENTER, S32_MAX, false,
                             LLFontGL::DROP_SHADOW_SOFT);
            rc.emitted++;
        }
    }

    // Emit a gl_triangle_2d equivalent (screen GL coords -> sink).
    void emitScreenTriangle(RenderCtx& rc, S32 x1, S32 y1, S32 x2, S32 y2,
                            S32 x3, S32 y3, const LLColor4& c)
    {
        const F32 ui_h = (F32)rc.dev_h / rc.ui_scale_y;
        const float xy[6] = { (F32)x1, ui_h - (F32)y1,
                              (F32)x2, ui_h - (F32)y2,
                              (F32)x3, ui_h - (F32)y3 };
        const float rgba[12] = { c.mV[VRED], c.mV[VGREEN], c.mV[VBLUE], c.mV[VALPHA],
                                 c.mV[VRED], c.mV[VGREEN], c.mV[VBLUE], c.mV[VALPHA],
                                 c.mV[VRED], c.mV[VGREEN], c.mV[VBLUE], c.mV[VALPHA] };
        LLVKUI2DSink::get().rawTris(xy, rgba, 3);
    }

    // Mirror LLMultiSlider::draw(): track, then thumbs (triangle / solid /
    // image modes), ghost, focus and hover highlights, cur/hover last.
    void renderMultiSlider(RenderCtx& rc, const LLMultiSlider* slider)
    {
        static LLUICachedControl<S32> extra_triangle_height("UIExtraTriangleHeight", 0);
        static LLUICachedControl<S32> extra_triangle_width("UIExtraTriangleWidth", 0);

        const LLMultiSlider::VkDrawState state = slider->getVkDrawState(rc.parent_alpha);
        const F32 opacity = state.enabled ? 1.f : 0.3f;
        const S32 flash_width = gFocusMgr.getFocusFlashWidth();

        const bool image_mode = !state.thumb_image.empty()
                                || !state.rounded_square_image.empty();

        if (state.draw_track && !state.rounded_square_image.empty() && LLVKUIImage::ready())
        {
            float l, t, r, b;
            toSinkRect(rc, state.track_rect, l, t, r, b);
            LLVKUIImage::draw(state.rounded_square_image, l, t, r, b,
                              state.track_color % opacity);
            rc.emitted++;
        }

        const auto draw_thumb_image = [&](const LLMultiSlider::VkThumbState& thumb,
                                          const LLColor4& color, bool solid)
        {
            float l, t, r, b;
            toSinkRect(rc, thumb.screen_rect, l, t, r, b);
            if (!state.thumb_image.empty() && !solid)
            {
                LLVKUIImage::draw(state.thumb_image, l, t, r, b, color);
            }
            else if (!state.rounded_square_image.empty())
            {
                LLVKUIImage::drawSolid(state.rounded_square_image, l, t, r, b, color);
            }
            else
            {
                LLVKUIRender::emitScreenRect(thumb.screen_rect, rc.dev_h,
                                             rc.ui_scale_y, color);
            }
            rc.emitted++;
        };

        if (state.use_triangle)
        {
            for (const LLMultiSlider::VkThumbState& thumb : state.thumbs)
            {
                const LLRect& tr = thumb.screen_rect;
                emitScreenTriangle(rc,
                                   tr.mLeft - extra_triangle_width,
                                   tr.mTop + extra_triangle_height,
                                   tr.mRight + extra_triangle_width,
                                   tr.mTop + extra_triangle_height,
                                   tr.mLeft + tr.getWidth() / 2,
                                   tr.mBottom - extra_triangle_height,
                                   state.triangle_color % opacity);
                rc.emitted++;
            }
            return;
        }

        if (!image_mode)
        {
            // Solid-rect mode: plain thumbs, then current, then drag-start
            // outline (capture) or hover.
            for (const LLMultiSlider::VkThumbState& thumb : state.thumbs)
            {
                if (thumb.name == state.cur_slider) continue;
                if (thumb.name == state.hover_slider && state.enabled && !state.mouse_capture) continue;
                LLVKUIRender::emitScreenRect(thumb.screen_rect, rc.dev_h,
                                             rc.ui_scale_y, state.thumb_center_color);
                rc.emitted++;
            }
            for (const LLMultiSlider::VkThumbState& thumb : state.thumbs)
            {
                if (thumb.name == state.cur_slider)
                {
                    LLVKUIRender::emitScreenRect(thumb.screen_rect, rc.dev_h,
                                                 rc.ui_scale_y,
                                                 state.thumb_center_selected_color);
                    rc.emitted++;
                }
            }
            if (state.mouse_capture)
            {
                // Unfilled drag-start outline.
                const LLRect& gr = state.drag_start_thumb_rect;
                const LLColor4 c = state.thumb_center_color % opacity;
                emitBorderLine(rc, gr.mLeft, gr.mBottom, gr.mLeft, gr.mTop, c);
                emitBorderLine(rc, gr.mLeft, gr.mTop, gr.mRight, gr.mTop, c);
                emitBorderLine(rc, gr.mRight, gr.mTop, gr.mRight, gr.mBottom, c);
                emitBorderLine(rc, gr.mLeft, gr.mBottom, gr.mRight, gr.mBottom, c);
                rc.emitted++;
            }
            else
            {
                for (const LLMultiSlider::VkThumbState& thumb : state.thumbs)
                {
                    if (thumb.name == state.hover_slider && state.enabled)
                    {
                        LLVKUIRender::emitScreenRect(thumb.screen_rect, rc.dev_h,
                                                     rc.ui_scale_y,
                                                     state.thumb_center_selected_color);
                        rc.emitted++;
                    }
                }
            }
            return;
        }

        // Image mode.
        if (state.mouse_capture)
        {
            // Drag-start ghost.
            LLMultiSlider::VkThumbState ghost;
            ghost.screen_rect = state.drag_start_thumb_rect;
            draw_thumb_image(ghost, state.thumb_center_color % 0.3f, false);
        }

        // Focus/hover highlight borders.
        if (state.has_focus && !state.cur_slider.empty())
        {
            for (const LLMultiSlider::VkThumbState& thumb : state.thumbs)
            {
                if (thumb.name != state.cur_slider) continue;
                float l, t, r, b;
                toSinkRect(rc, thumb.screen_rect, l, t, r, b);
                const LLColor4 border_color = !state.thumb_image.empty()
                    ? state.thumb_highlight_color : gFocusMgr.getFocusColor();
                LLVKUIImage::drawBorder(
                    !state.thumb_image.empty() ? state.thumb_image
                                               : state.rounded_square_image,
                    l, t, r, b, border_color, flash_width);
                rc.emitted++;
            }
        }
        if (!state.hover_slider.empty())
        {
            for (const LLMultiSlider::VkThumbState& thumb : state.thumbs)
            {
                if (thumb.name != state.hover_slider) continue;
                float l, t, r, b;
                toSinkRect(rc, thumb.screen_rect, l, t, r, b);
                const LLColor4 border_color = !state.thumb_image.empty()
                    ? state.thumb_highlight_color : gFocusMgr.getFocusColor();
                LLVKUIImage::drawBorder(
                    !state.thumb_image.empty() ? state.thumb_image
                                               : state.rounded_square_image,
                    l, t, r, b, border_color, flash_width);
                rc.emitted++;
            }
        }

        // Thumbs; current and hover deferred to the end (painter's order).
        for (const LLMultiSlider::VkThumbState& thumb : state.thumbs)
        {
            if (thumb.name == state.cur_slider) continue;
            if (thumb.name == state.hover_slider && state.enabled && !state.mouse_capture) continue;
            if (!state.thumb_image.empty())
            {
                draw_thumb_image(thumb, state.enabled
                                     ? LLColor4::white
                                     : LLColor4::grey % 0.8f, false);
            }
            else
            {
                draw_thumb_image(thumb, state.mouse_capture
                                     ? state.thumb_center_color
                                     : state.thumb_center_color % opacity, true);
            }
        }
        for (const LLMultiSlider::VkThumbState& thumb : state.thumbs)
        {
            if (thumb.name != state.cur_slider) continue;
            if (!state.thumb_image.empty())
            {
                draw_thumb_image(thumb, state.enabled
                                     ? LLColor4::white
                                     : LLColor4::grey % 0.8f, false);
            }
            else
            {
                draw_thumb_image(thumb, state.mouse_capture
                                     ? state.thumb_center_selected_color
                                     : state.thumb_center_selected_color % opacity, true);
            }
        }
        if (state.mouse_capture == false)
        {
            for (const LLMultiSlider::VkThumbState& thumb : state.thumbs)
            {
                if (thumb.name != state.hover_slider) continue;
                draw_thumb_image(thumb, state.thumb_center_selected_color,
                                 state.thumb_image.empty());
            }
        }
    }

    // Mirror LLAccordionCtrlTabHeader::draw(): bg fill, header image, hover
    // overlay, expand/collapse arrow (text arrives via the child textbox).
    void renderAccordionHeader(RenderCtx& rc, const LLView* view)
    {
        LLAccordionCtrlTab::VkHeaderState hs;
        if (!LLAccordionCtrlTab::getVkHeaderState(view, rc.parent_alpha, hs)) return;

        const LLRect screen = view->calcScreenRect();
        if (screen.isEmpty()) return;

        LLRect bg_rect(screen);
        bg_rect.mRight -= 1;
        bg_rect.mTop -= 1;
        LLVKUIRender::emitScreenRect(bg_rect, rc.dev_h, rc.ui_scale_y,
                                     hs.bg_color);
        rc.emitted++;

        if (!LLVKUIImage::ready()) return;

        float l, t, r, b;
        toSinkRect(rc, screen, l, t, r, b);
        if (!hs.header_image.empty())
        {
            LLVKUIImage::draw(hs.header_image, l, t, r, b,
                              LLColor4::white % rc.parent_alpha);
            rc.emitted++;
        }
        if (!hs.header_over_image.empty())
        {
            LLVKUIImage::draw(hs.header_over_image, l, t, r, b,
                              LLColor4::white % rc.parent_alpha);
            rc.emitted++;
        }
        if (!hs.arrow_image.empty())
        {
            int arrow_w = 0, arrow_h = 0;
            LLVKUIImage::getSize(hs.arrow_image, arrow_w, arrow_h);
            if (arrow_w > 0 && arrow_h > 0)
            {
                const S32 HEADER_IMAGE_LEFT_OFFSET = 5;
                const LLRect arrow_screen(
                    screen.mLeft + HEADER_IMAGE_LEFT_OFFSET,
                    screen.mBottom + (screen.getHeight() + arrow_h) / 2,
                    screen.mLeft + HEADER_IMAGE_LEFT_OFFSET + arrow_w,
                    screen.mBottom + (screen.getHeight() - arrow_h) / 2);
                float al, at, ar, ab;
                toSinkRect(rc, arrow_screen, al, at, ar, ab);
                LLVKUIImage::draw(hs.arrow_image, al, at, ar, ab,
                                  LLColor4::white % rc.parent_alpha);
                rc.emitted++;
            }
        }
    }

    // Mirror LLTextEditor::drawPreeditMarker(): IME underline rects.
    void renderPreeditMarkers(RenderCtx& rc, const LLTextEditor* editor)
    {
        std::vector<LLTextEditor::VkPreeditMarker> markers;
        editor->getVkPreeditMarkers(markers);
        for (const LLTextEditor::VkPreeditMarker& marker : markers)
        {
            LLRect screen;
            editor->localRectToScreen(marker.local_rect, &screen);
            LLVKUIRender::emitScreenRect(screen, rc.dev_h, rc.ui_scale_y,
                                         marker.color);
            rc.emitted++;
        }
    }

    // Mirror LLStatBar::draw(): ticks behind, bg, min..max band, history or
    // current marker, mean bar, then label + value text (monospace, white).
    void renderStatBar(RenderCtx& rc, const LLStatBar* bar)
    {
        const LLStatBar::VkDrawState& state = bar->getVkDrawState();
        if (!state.valid) return;

        const LLRect widget_screen = bar->calcScreenRect();

        const auto local_rect_to_screen = [&](const LLRect& local, LLRect& screen)
        {
            bar->localRectToScreen(local, &screen);
        };

        // Ticks (behind the bars).
        for (const LLStatBar::VkTick& tick : state.ticks)
        {
            LLRect tick_screen;
            local_rect_to_screen(tick.rect, tick_screen);
            LLVKUIRender::emitScreenRect(tick_screen, rc.dev_h, rc.ui_scale_y,
                                         tick.labeled ? LLColor4(1.f, 1.f, 1.f, 0.25f)
                                                      : LLColor4(1.f, 1.f, 1.f, 0.1f));
            if (tick.labeled && LLVKText::ready())
            {
                LLVKText::render(LLFontGL::getFontMonospace(), tick.label,
                                 (F32)widget_screen.mLeft + tick.label_x,
                                 (F32)widget_screen.mBottom + tick.label_y,
                                 LLColor4(1.f, 1.f, 1.f, 0.5f),
                                 LLFontGL::LEFT, tick.label_valign, S32_MAX);
            }
        }

        if (state.bar_visible)
        {
            LLRect screen;
            // Background bar.
            local_rect_to_screen(state.bar_rect, screen);
            LLVKUIRender::emitScreenRect(screen, rc.dev_h, rc.ui_scale_y,
                                         LLColor4(0.f, 0.f, 0.f, 0.25f));
            rc.emitted++;
            // Min..max band.
            if (state.band_valid)
            {
                local_rect_to_screen(state.band_rect, screen);
                LLVKUIRender::emitScreenRect(screen, rc.dev_h, rc.ui_scale_y,
                                             LLColor4(1.f, 0.f, 0.f, 0.25f));
                rc.emitted++;
            }
            // History samples or the current-value marker.
            if (state.history_mode)
            {
                for (const LLRect& quad : state.hist_quads)
                {
                    local_rect_to_screen(quad, screen);
                    LLVKUIRender::emitScreenRect(screen, rc.dev_h, rc.ui_scale_y,
                                                 LLColor4(1.f, 0.f, 0.f, 1.f));
                }
                rc.emitted++;
            }
            else if (state.cur_valid)
            {
                local_rect_to_screen(state.cur_rect, screen);
                LLVKUIRender::emitScreenRect(screen, rc.dev_h, rc.ui_scale_y,
                                             LLColor4(1.f, 0.f, 0.f, 1.f));
                rc.emitted++;
            }
            // Mean bar.
            local_rect_to_screen(state.mean_rect, screen);
            LLVKUIRender::emitScreenRect(screen, rc.dev_h, rc.ui_scale_y,
                                         LLColor4(0.f, 1.f, 0.f, 1.f));
            rc.emitted++;
        }

        // Label (top-left) + value (top-right of the bar).
        if (LLVKText::ready())
        {
            LLVKText::render(LLFontGL::getFontMonospace(), state.label,
                             (F32)widget_screen.mLeft, (F32)widget_screen.mTop,
                             LLColor4::white, LLFontGL::LEFT, LLFontGL::TOP,
                             S32_MAX);
            LLRect bar_screen;
            local_rect_to_screen(state.bar_rect, bar_screen);
            LLVKText::render(LLFontGL::getFontMonospace(),
                             utf8str_to_wstring(state.value_text),
                             (F32)bar_screen.mRight, (F32)widget_screen.mTop,
                             LLColor4::white, LLFontGL::RIGHT, LLFontGL::TOP,
                             S32_MAX);
            rc.emitted++;
        }
    }

    // Mirror LLStatGraph::draw(): bg fill, black outline, threshold-colored
    // value bar.
    void renderStatGraph(RenderCtx& rc, const LLStatGraph* graph)
    {
        const LLStatGraph::VkDrawState state = graph->getVkDrawState();
        if (!state.valid) return;

        const LLRect screen = graph->calcScreenRect();
        LLVKUIRender::emitScreenRect(screen, rc.dev_h, rc.ui_scale_y,
                                     state.bg_color);
        LLRect bar_screen;
        graph->localRectToScreen(state.bar_rect, &bar_screen);
        LLVKUIRender::emitScreenRect(bar_screen, rc.dev_h, rc.ui_scale_y,
                                     state.bar_color);
        // 1px black outline.
        emitBorderLine(rc, screen.mLeft, screen.mBottom, screen.mLeft, screen.mTop, state.border_color);
        emitBorderLine(rc, screen.mLeft, screen.mTop, screen.mRight, screen.mTop, state.border_color);
        emitBorderLine(rc, screen.mRight, screen.mTop, screen.mRight, screen.mBottom, state.border_color);
        emitBorderLine(rc, screen.mLeft, screen.mBottom, screen.mRight, screen.mBottom, state.border_color);
        rc.emitted += 2;
    }

    // Mirror LLVirtualTrackball::draw(): sphere image + sun/moon thumb.
    void renderVirtualTrackball(RenderCtx& rc, const LLVirtualTrackball* trackball)
    {
        LLVirtualTrackball::VkDrawState state;
        trackball->getVkDrawState(rc.parent_alpha, state);

        if (!state.sphere_image.empty() && LLVKUIImage::ready())
        {
            float l, t, r, b;
            toSinkRect(rc, state.touch_rect, l, t, r, b);
            LLVKUIImage::draw(state.sphere_image, l, t, r, b, state.sphere_color);
            rc.emitted++;
        }
        if (!state.thumb_image.empty() && LLVKUIImage::ready())
        {
            int w = 0, h = 0;
            LLVKUIImage::getSize(state.thumb_image, w, h);
            if (w > 0 && h > 0)
            {
                // drawThumb(): intrinsic size, centered on the point.
                const LLRect thumb_screen(state.thumb_x - w / 2, state.thumb_y + h / 2,
                                          state.thumb_x + w / 2, state.thumb_y - h / 2);
                float l, t, r, b;
                toSinkRect(rc, thumb_screen, l, t, r, b);
                LLVKUIImage::draw(state.thumb_image, l, t, r, b,
                                  LLColor4::white % rc.parent_alpha);
                rc.emitted++;
            }
        }
    }

    // Mirror drawArrow() (llxyvector.cpp): shaft line + filled head triangle.
    void emitArrow(RenderCtx& rc, S32 tail_x, S32 tail_y, S32 tip_x, S32 tip_y,
                   const LLColor4& color)
    {
        constexpr S32 ARROW_ANGLE = 30;
        constexpr S32 ARROW_LENGTH_LONG = 10;
        constexpr S32 ARROW_LENGTH_SHORT = 6;

        emitBorderLine(rc, tail_x, tail_y, tip_x, tip_y, color);

        const S32 dx = tip_x - tail_x;
        const S32 dy = tip_y - tail_y;
        const S32 arrow_length = (llabs(dx) < ARROW_LENGTH_LONG && llabs(dy) < ARROW_LENGTH_LONG)
            ? ARROW_LENGTH_SHORT : ARROW_LENGTH_LONG;

        const F32 theta = (F32)std::atan2((F32)dy, (F32)dx);
        const F32 rad = (F32)(ARROW_ANGLE * std::atan(1.0) * 4 / 180);
        const F32 x = tip_x - arrow_length * (F32)cos(theta + rad);
        const F32 y = tip_y - arrow_length * (F32)sin(theta + rad);
        const F32 rad2 = (F32)(-1 * ARROW_ANGLE * std::atan(1.0) * 4 / 180);
        const F32 x2 = tip_x - arrow_length * (F32)cos(theta + rad2);
        const F32 y2 = tip_y - arrow_length * (F32)sin(theta + rad2);
        emitScreenTriangle(rc, tip_x, tip_y, (S32)x, (S32)y, (S32)x2, (S32)y2, color);
    }

    // Mirror LLXYVector::draw(): area fill, crosshair grid, ghost/value
    // arrows, center circle.
    void renderXYVector(RenderCtx& rc, const LLXYVector* vector)
    {
        LLXYVector::VkDrawState state;
        vector->getVkDrawState(state);

        LLRect touch_screen;
        vector->localRectToScreen(state.touch_rect, &touch_screen);
        LLVKUIRender::emitScreenRect(touch_screen, rc.dev_h, rc.ui_scale_y,
                                     state.area_color % rc.parent_alpha);
        rc.emitted++;

        // Grid crosshair (local coords -> screen offsets).
        const S32 ox = touch_screen.mLeft;
        const S32 oy = touch_screen.mBottom;
        const S32 center_x = ox + state.center_x - state.touch_rect.mLeft;
        const S32 center_y = oy + state.center_y - state.touch_rect.mBottom;
        emitBorderLine(rc, center_x, touch_screen.mBottom, center_x, touch_screen.mTop,
                       state.grid_color % rc.parent_alpha);
        emitBorderLine(rc, touch_screen.mLeft, center_y, touch_screen.mRight, center_y,
                       state.grid_color % rc.parent_alpha);

        if (state.draw_ghost)
        {
            emitArrow(rc, center_x, center_y,
                      ox + state.ghost_x - state.touch_rect.mLeft,
                      oy + state.ghost_y - state.touch_rect.mBottom,
                      state.ghost_color % rc.parent_alpha);
        }
        if (state.draw_arrow)
        {
            emitArrow(rc, center_x, center_y,
                      ox + state.point_x - state.touch_rect.mLeft,
                      oy + state.point_y - state.touch_rect.mBottom,
                      state.arrow_color % rc.parent_alpha);
        }
        // Center circle (arrow color in both draw() branches).
        emitCircle(rc, (F32)center_x, (F32)center_y, state.circle_radius,
                   state.arrow_color % rc.parent_alpha, true, 12);
        rc.emitted++;
    }

    // Mirror LLConsole::draw(): background + colored paragraph segments.
    void renderConsole(RenderCtx& rc, const LLConsole* console)
    {
        LLConsole::VkDrawState state;
        console->getVkDrawState(state);
        if (!state.visible) return;

        for (const LLRect& local : state.bg_rects)
        {
            LLRect screen;
            console->localRectToScreen(local, &screen);
            float l, t, r, b;
            toSinkRect(rc, screen, l, t, r, b);
            if (!state.bg_image.empty() && LLVKUIImage::ready())
            {
                LLVKUIImage::drawSolid(state.bg_image, l, t, r, b, state.bg_color);
            }
            else
            {
                LLVKUIRender::emitScreenRect(screen, rc.dev_h, rc.ui_scale_y,
                                             state.bg_color);
            }
        }
        rc.emitted++;

        if (LLVKText::ready() && state.font)
        {
            const LLRect widget_screen = console->calcScreenRect();
            for (const LLConsole::VkTextRun& run : state.runs)
            {
                LLVKText::render(state.font, run.text,
                                 (F32)widget_screen.mLeft + run.x,
                                 (F32)widget_screen.mBottom + run.y,
                                 run.color, LLFontGL::LEFT, LLFontGL::BASELINE,
                                 run.max_pixels, false, LLFontGL::DROP_SHADOW);
            }
            rc.emitted++;
        }
    }

    // Mirror LLDockableFloater::draw()'s dock tongue (position is floater-
    // local in GL, computed from screen rects — reproduce exactly).
    void renderDockTongue(RenderCtx& rc, const LLDockableFloater* floater)
    {
        if (!floater->isDocked()) return;
        LLDockControl* dock_control = const_cast<LLDockableFloater*>(floater)->getDockControl();
        if (!dock_control) return;

        std::string image;
        S32 x = 0, y = 0;
        if (!dock_control->getVkTongueState(image, x, y) || image.empty()) return;
        if (!LLVKUIImage::ready()) return;

        int w = 0, h = 0;
        LLVKUIImage::getSize(image, w, h);
        if (w > 0 && h > 0)
        {
            dock_control->setVkTongueSize(w, h);
        }

        // drawToungue() draws at (mDockTongueX, mDockTongueY) in the
        // floater's local frame.
        const LLRect floater_screen = floater->calcScreenRect();
        const LLRect tongue_screen(floater_screen.mLeft + x,
                                   floater_screen.mBottom + y + h,
                                   floater_screen.mLeft + x + w,
                                   floater_screen.mBottom + y);
        float l, t, r, b;
        toSinkRect(rc, tongue_screen, l, t, r, b);
        LLVKUIImage::draw(image, l, t, r, b, LLColor4::white % rc.parent_alpha);
        rc.emitted++;
    }
}

namespace LLVKUIWidgets
{
    void prepareView(LLVKContext* context, const LLView* view)
    {
        if (!view || !view->getVisible()) return;
        LLView* mutable_view = const_cast<LLView*>(view);

        // The draw()-time layout/state reconciliation of the covered widgets,
        // in the same order draw() performs it.
        if (LLAccordionCtrl* accordion = dynamic_cast<LLAccordionCtrl*>(mutable_view))
        {
            accordion->prepareVkDraw();
        }
        if (LLWindowShade* shade = dynamic_cast<LLWindowShade*>(mutable_view))
        {
            shade->prepareVkDraw();
        }
        if (LLFlyoutButton* flyout = dynamic_cast<LLFlyoutButton*>(mutable_view))
        {
            flyout->prepareVkDraw();
        }
        if (LLDragHandle* handle = dynamic_cast<LLDragHandle*>(mutable_view))
        {
            handle->prepareVkDraw();
        }
        if (LLChatEntry* chat_entry = dynamic_cast<LLChatEntry*>(mutable_view))
        {
            chat_entry->prepareVkDraw();
        }
        if (LLTextEditor* editor = dynamic_cast<LLTextEditor*>(mutable_view))
        {
            editor->prepareVkDraw();
        }
        if (LLScrollColumnHeader* header = dynamic_cast<LLScrollColumnHeader*>(mutable_view))
        {
            header->prepareVkDraw();
        }
        if (LLToolBar* toolbar = dynamic_cast<LLToolBar*>(mutable_view))
        {
            toolbar->prepareVkDraw();
        }
        if (LLLoadingIndicator* indicator = dynamic_cast<LLLoadingIndicator*>(mutable_view))
        {
            indicator->prepareVkDraw();
        }
        if (LLToolTip* tooltip = dynamic_cast<LLToolTip*>(mutable_view))
        {
            tooltip->prepareVkDraw();
        }
        if (LLStatBar* stat_bar = dynamic_cast<LLStatBar*>(mutable_view))
        {
            stat_bar->prepareVkDraw();
        }
        if (LLStatGraph* stat_graph = dynamic_cast<LLStatGraph*>(mutable_view))
        {
            stat_graph->prepareVkDraw();
        }
        if (LLXYVector* xy_vector = dynamic_cast<LLXYVector*>(mutable_view))
        {
            xy_vector->prepareVkDraw();
        }
        if (LLConsole* console = dynamic_cast<LLConsole*>(mutable_view))
        {
            console->prepareVkDraw();
        }
        if (LLDockableFloater* dockable = dynamic_cast<LLDockableFloater*>(mutable_view))
        {
            dockable->prepareVkDraw();
        }
        if (LLVirtualTrackball* trackball = dynamic_cast<LLVirtualTrackball*>(mutable_view))
        {
            // Feed the Vulkan-decoded sphere size (the GL image is null here).
            int sphere_w = 0, sphere_h = 0;
            LLVKUIImage::getSize(trackball->getVkSphereImageName(), sphere_w, sphere_h);
            trackball->prepareVkDraw(sphere_w, sphere_h);
        }

        // Glyph preparation for widget-owned text (badges render their label
        // outside the text-tree passes).
        if (LLVKText::ready())
        {
            if (const LLBadge* badge = dynamic_cast<const LLBadge*>(view))
            {
                LLBadge::VkDrawState state;
                badge->getVkDrawState(1.f, state);
                if (state.visible && state.font)
                {
                    LLVKText::prepare(state.font, state.label);
                }
            }
            if (const LLStatBar* stat_bar = dynamic_cast<const LLStatBar*>(view))
            {
                const LLStatBar::VkDrawState& state = stat_bar->getVkDrawState();
                LLVKText::prepare(LLFontGL::getFontMonospace(), state.label);
                LLVKText::prepare(LLFontGL::getFontMonospace(),
                                  utf8str_to_wstring(state.value_text));
                for (const LLStatBar::VkTick& tick : state.ticks)
                {
                    if (tick.labeled)
                    {
                        LLVKText::prepare(LLFontGL::getFontMonospace(), tick.label);
                    }
                }
            }
            if (const LLConsole* console = dynamic_cast<const LLConsole*>(view))
            {
                LLConsole::VkDrawState state;
                console->getVkDrawState(state);
                if (state.visible && state.font)
                {
                    for (const LLConsole::VkTextRun& run : state.runs)
                    {
                        LLVKText::prepare(state.font, run.text);
                    }
                }
            }
        }
    }

    void renderChrome(RenderCtx& rc, const LLView* view)
    {
        if (const LLProgressBar* bar = dynamic_cast<const LLProgressBar*>(view))
        {
            renderProgressBar(rc, bar);
        }
        if (const LLResizeHandle* handle = dynamic_cast<const LLResizeHandle*>(view))
        {
            renderResizeHandle(rc, handle);
        }
        if (const LLLoadingIndicator* indicator = dynamic_cast<const LLLoadingIndicator*>(view))
        {
            renderLoadingIndicator(rc, indicator);
        }
        if (const LLBadge* badge = dynamic_cast<const LLBadge*>(view))
        {
            renderBadge(rc, badge);
        }
        if (const LLMultiSlider* slider = dynamic_cast<const LLMultiSlider*>(view))
        {
            renderMultiSlider(rc, slider);
        }
        renderAccordionHeader(rc, view);
        if (const LLTextEditor* editor = dynamic_cast<const LLTextEditor*>(view))
        {
            renderPreeditMarkers(rc, editor);
        }
        if (const LLStatBar* stat_bar = dynamic_cast<const LLStatBar*>(view))
        {
            renderStatBar(rc, stat_bar);
        }
        if (const LLStatGraph* stat_graph = dynamic_cast<const LLStatGraph*>(view))
        {
            renderStatGraph(rc, stat_graph);
        }
        if (const LLVirtualTrackball* trackball = dynamic_cast<const LLVirtualTrackball*>(view))
        {
            renderVirtualTrackball(rc, trackball);
        }
        if (const LLXYVector* xy_vector = dynamic_cast<const LLXYVector*>(view))
        {
            renderXYVector(rc, xy_vector);
        }
        if (const LLConsole* console = dynamic_cast<const LLConsole*>(view))
        {
            renderConsole(rc, console);
        }
        if (const LLDockableFloater* dockable = dynamic_cast<const LLDockableFloater*>(view))
        {
            renderDockTongue(rc, dockable);
        }

        // LLModalDialog::draw() always emits the floater drop shadow, even
        // when the XUI drop_shadow flag (which the floater pass keys on) is
        // not set.
        if (const LLModalDialog* dialog = dynamic_cast<const LLModalDialog*>(view))
        {
            if (dialog->getVkModalShadow() && !dialog->getVkDropShadow())
            {
                static LLUIColor shadow_color =
                    LLUIColorTable::instance().getColor("ColorDropShadow");
                emitDropShadow(rc, dialog->calcScreenRect(), shadow_color.get(),
                               DROP_SHADOW_FLOATER);
                rc.emitted++;
            }
        }
    }

    float subtreeAlpha(const LLView* view)
    {
        // LLToolTip::draw() pushes its fade alpha over its whole subtree.
        if (const LLToolTip* tooltip = dynamic_cast<const LLToolTip*>(view))
        {
            return tooltip->getVkDrawAlpha();
        }
        return 1.f;
    }

    bool subtreeClip(const LLView* view, LLRect& gl_screen_rect)
    {
        // LLAccordionCtrl::draw() clips its whole subtree to its rect.
        if (dynamic_cast<const LLAccordionCtrl*>(view))
        {
            gl_screen_rect = view->calcScreenRect();
            return gl_screen_rect.notEmpty();
        }
        // LLStatBar::draw() clips to its local rect (LLLocalClipRect).
        if (dynamic_cast<const LLStatBar*>(view))
        {
            gl_screen_rect = view->calcScreenRect();
            return gl_screen_rect.notEmpty();
        }
        return false;
    }

    bool childClip(const LLView* view, const LLView* child, LLRect& gl_screen_rect)
    {
        // LLAccordionCtrlTab::draw() clips only the container panel (not the
        // header/scrollbar) in non-fit mode.
        if (const LLAccordionCtrlTab* tab = dynamic_cast<const LLAccordionCtrlTab*>(view))
        {
            if (child == tab->getVkContainerPanel())
            {
                return tab->getVkContainerClipRect(gl_screen_rect);
            }
        }
        return false;
    }
}
