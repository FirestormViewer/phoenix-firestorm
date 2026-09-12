/**
 * @file llvkuifolder.cpp
 * @brief Greenfield Vulkan passes for the folder-view (inventory tree) widgets.
 *
 * @details
 * Implements the passes declared in llvkuifolder.h. The GL path draws the
 * inventory tree chrome (selection/hover/drag highlights, disclosure arrows,
 * item icons, link overlays, favorite stars, filter-match boxes, label text)
 * inside LLFolderViewItem::draw()/LLFolderViewFolder::draw()/LLFolderView::draw().
 * This module reproduces that output by reading the items' GL-free Vk* state
 * (LLFolderViewItem::getVkDrawState) and emitting into the LLVKUI2D sink. It
 * never calls widget draw() and never touches gGL.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, The Vulkanstorm Project
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "llvkuifolder.h"

#include "llfolderview.h"          // LLFolderView
#include "llfolderviewitem.h"      // LLFolderViewItem / LLFolderViewFolder
#include "llview.h"                // LLView

#include "llvkuirender.h"          // LLVKUIRender::emitScreenRect
#include "llvkuirenderinternal.h"  // RenderCtx, emitBorderLine
#include "llvkuiimage.h"           // LLVKUIImage::getSize
#include "llvklegacytext.h"

using LLVKUIRenderInternal::RenderCtx;
using LLVKUIRenderInternal::emitBorderLine;

namespace
{
    // Mirror a gl_rect_2d(..., filled=false) outline as four 1px segments.
    void emitOutline(const RenderCtx& rc, const LLRect& gl_rect,
                     const LLColor4& color)
    {
        emitBorderLine(rc, gl_rect.mLeft, gl_rect.mBottom, gl_rect.mLeft, gl_rect.mTop, color);
        emitBorderLine(rc, gl_rect.mLeft, gl_rect.mTop, gl_rect.mRight, gl_rect.mTop, color);
        emitBorderLine(rc, gl_rect.mRight, gl_rect.mTop, gl_rect.mRight, gl_rect.mBottom, color);
        emitBorderLine(rc, gl_rect.mLeft, gl_rect.mBottom, gl_rect.mRight, gl_rect.mBottom, color);
    }
}

void LLVKUIFolder::prepareView(LLVKContext* context, const LLView* view)
{
    (void)context; // no queue work here; glyph uploads happen in flushPrepared()

    LLFolderViewItem* item = dynamic_cast<LLFolderViewItem*>(const_cast<LLView*>(view));
    if (!item)
    {
        return;
    }

    // Draw-time mutations the GL path performs inside draw() (virtual: the
    // root LLFolderView reconciles view-level state, folders advance the
    // arrow animation and hide collapsed children, items refresh the model
    // filter state).
    item->prepareVkDraw();

    // The root folder draws no row chrome for itself (LLFolderView::draw
    // skips straight to LLView::draw), so no icon metrics or text needed.
    LLFolderView* root = item->getRoot();
    if (!root || root == item)
    {
        return;
    }

    // Icon geometry comes from the GL-free registry; the GL LLUIImage/texture
    // may not exist on this path, and llui cannot query LLVKUIImage itself.
    if (LLVKUIImage::ready())
    {
        LLFolderViewItem::VkIconMetrics metrics;
        int w = 0, h = 0;
        if (LLVKUIImage::getSize(item->getVkIconName(), w, h))
        {
            metrics.icon_w = w;
            metrics.icon_h = h;
        }
        w = h = 0;
        if (LLVKUIImage::getSize(item->getVkIconOpenName(), w, h))
        {
            metrics.open_w = w;
            metrics.open_h = h;
        }
        w = h = 0;
        if (LLVKUIImage::getSize(item->getVkIconOverlayName(), w, h))
        {
            metrics.overlay_w = w;
            metrics.overlay_h = h;
        }
        item->setVkIconMetrics(metrics);
    }

    // Rasterize label glyphs before the swapchain render pass begins (same
    // pattern as the LLTextBase/LLScrollListCtrl prepares in the walker).
    if (LLVKText::ready())
    {
        LLFolderViewItem::VkDrawState state;
        item->getVkDrawState(1.f, state);
        for (const LLFolderViewItem::VkTextRun& run : state.text_runs)
        {
            if (run.font && !run.text.empty())
            {
                LLVKText::prepare(run.font, run.text);
            }
        }
    }
}

void LLVKUIFolder::renderChrome(RenderCtx& rc, const LLView* view)
{
    const LLFolderViewItem* item = dynamic_cast<const LLFolderViewItem*>(view);
    if (!item)
    {
        return;
    }
    // The root LLFolderView has no label/arrow/icon chrome of its own.
    if (dynamic_cast<const LLFolderView*>(view))
    {
        return;
    }

    LLFolderViewItem* mutable_item = const_cast<LLFolderViewItem*>(item);
    LLFolderViewItem::VkDrawState state;
    mutable_item->getVkDrawState((F32)rc.parent_alpha, state);

    // GL draw order: highlight first, then arrow, then icon (+overlay), then
    // label text with the filter highlight under it.

    // 1. Selection / hover / drag-target highlight rects (drawHighlight()).
    for (const LLFolderViewItem::VkDrawState::RectOp& op : state.highlight_ops)
    {
        if (op.filled)
        {
            LLVKUIRender::emitScreenRect(op.rect, rc.dev_h, rc.ui_scale_y, op.color);
        }
        else
        {
            emitOutline(rc, op.rect, op.color);
        }
        rc.emitted++;
    }

    // 2. Disclosure arrow (drawOpenFolderArrow()). NOTE: GL rotates this image
    // by mControlLabelRotation; the 2D sink has no rotated-quad helper, so
    // the arrow is emitted axis-aligned (state.arrow_rotation is informational).
    if (state.arrow_visible)
    {
        LLVKUIRender::emitScreenRect(state.arrow_rect, rc.dev_h, rc.ui_scale_y,
                                     state.arrow_image, state.arrow_color);
        rc.emitted++;
    }

    // 3. Favorite star (drawFavoriteIcon()).
    if (state.favorite_visible)
    {
        LLVKUIRender::emitScreenRect(state.favorite_rect, rc.dev_h, rc.ui_scale_y,
                                     state.favorite_image, state.favorite_color);
        rc.emitted++;
    }

    // 4. Item icon (+ link overlay).
    if (state.icon_visible)
    {
        LLVKUIRender::emitScreenRect(state.icon_rect, rc.dev_h, rc.ui_scale_y,
                                     state.icon_image,
                                     LLColor4(1.f, 1.f, 1.f, (F32)rc.parent_alpha));
        rc.emitted++;
    }
    if (state.overlay_visible)
    {
        LLVKUIRender::emitScreenRect(state.overlay_rect, rc.dev_h, rc.ui_scale_y,
                                     state.overlay_image,
                                     LLColor4(1.f, 1.f, 1.f, (F32)rc.parent_alpha));
        rc.emitted++;
    }

    // 5. Filter-match background boxes (under the label text).
    for (const LLRect& box : state.filter_boxes)
    {
        LLVKUIRender::emitScreenRect(box, rc.dev_h, rc.ui_scale_y,
                                     state.selection_image, state.filter_bg_color);
        rc.emitted++;
    }

    // 6. Label text: label, locked/protected markers, suffix, filter-match
    // substrings (already in GL emission order).
    if (LLVKText::ready())
    {
        for (const LLFolderViewItem::VkTextRun& run : state.text_runs)
        {
            if (run.font && !run.text.empty())
            {
                LLVKText::render(run.font, run.text, run.x, run.y, run.color,
                                 LLFontGL::LEFT, LLFontGL::BOTTOM,
                                 run.max_pixels, run.ellipses);
            }
        }
    }

    // draw()/drawHighlight() consume the per-frame flags after rendering.
    mutable_item->vkPostDraw();
}
