/**
 * @file llvkrender.cpp
 * @brief Implementation of llvkrender — the Vulkan LLUI2DBackend (see header).
 *
 * GL-free: this file must NOT include llrender/llgl headers or touch gGL.
 * The only viewer-neutral input it reads is LLUI::getScaleFactor() (UI scale),
 * which is backend-neutral CPU state, not GL.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, The Vulkanstorm Project
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "llvkrender.h"

#include "llerror.h"
#include "llvkcontext.h"
#include "llvkui2d.h"

namespace
{
    // Map the tree's blend-type int (LLRender::eBlendType values) onto the
    // sink's LLVKBlend. We use the LLRender::eBlendType integer values directly
    // (BT_ALPHA=0, BT_ADD=1, BT_ADD_WITH_ALPHA=2, BT_REPLACE=6) so the header
    // stays free of an llrender include; the mapping is explicit here.
    LLVKBlend toSinkBlend(int blend_type)
    {
        switch (blend_type)
        {
        case 0:  return LLVKBlend::Alpha;         // BT_ALPHA
        case 1:  return LLVKBlend::Add;           // BT_ADD
        case 2:  return LLVKBlend::AddWithAlpha;  // BT_ADD_WITH_ALPHA
        case 6:  return LLVKBlend::Replace;       // BT_REPLACE
        default:
            LL_WARNS("Vulkan") << "llvkrender: unsupported blend type " << blend_type
                               << " (using Alpha)" << LL_ENDL;
            return LLVKBlend::Alpha;
        }
    }
}

LLVKRender2D::LLVKRender2D()
{
    mStack.reserve(64);
}

void LLVKRender2D::beginFrame(LLVKContext* ctx, unsigned device_height, float ui_scale_x, float ui_scale_y)
{
    mCtx = ctx;
    mDeviceHeight = device_height;
    mUIScaleX = ui_scale_x;
    mUIScaleY = ui_scale_y;
    mStack.clear();
    // Base transform: identity offset, scale = the neutral UI scale factor
    // (supplied by the seam; llvulkan does not read llui).
    mCur = { 0.f, 0.f, mUIScaleX, mUIScaleY };
    mR = mG = mB = mA = 1.f;
    mInFrame = true;
    applyTransform();
}

void LLVKRender2D::endFrame()
{
    mInFrame = false;
    mCtx = nullptr;
}

void LLVKRender2D::applyTransform()
{
    LLVKUI2DSink::get().setTransform(mCur.off_x, mCur.off_y, mCur.scale_x, mCur.scale_y);
}

void LLVKRender2D::pushTransform()
{
    mStack.push_back(mCur);
}

void LLVKRender2D::popTransform()
{
    if (!mStack.empty())
    {
        mCur = mStack.back();
        mStack.pop_back();
        applyTransform();
    }
}

void LLVKRender2D::translate(float x, float y)
{
    // Accumulate the widget offset (matches the result of the GL UI matrix
    // stack: offsets sum down the tree; scale is applied at emit by the sink).
    mCur.off_x += x;
    mCur.off_y += y;
    applyTransform();
}

void LLVKRender2D::scale(float sx, float sy)
{
    // Accumulate a scale factor (matches gGL.scaleUI). Offsets are in the
    // pre-scale space; the sink applies (pos + off) * scale at emit.
    mCur.scale_x *= sx;
    mCur.scale_y *= sy;
    applyTransform();
}

void LLVKRender2D::loadIdentityTransform()
{
    mCur = { 0.f, 0.f, mUIScaleX, mUIScaleY };
    applyTransform();
}

void LLVKRender2D::setColor(float r, float g, float b, float a)
{
    mR = r; mG = g; mB = b; mA = a;
}

void LLVKRender2D::setBlend(int blend_type)
{
    LLVKUI2DSink::get().setBlend(toSinkBlend(blend_type));
}

void LLVKRender2D::setScissor(int x, int y, int w, int h)
{
    // Incoming rect is GL bottom-left-origin device pixels. Convert to the
    // sink's top-left-origin space (the negative-height viewport): the Vulkan
    // scissor y is measured from the top.
    if (w <= 0 || h <= 0 || mDeviceHeight == 0)
    {
        LLVKUI2DSink::get().clearScissor();
        return;
    }
    const int vk_y = (int)mDeviceHeight - (y + h);
    LLVKUI2DSink::get().setScissor(x, vk_y, w, h);
}

void LLVKRender2D::clearScissor()
{
    LLVKUI2DSink::get().clearScissor();
}

void LLVKRender2D::rect(float left, float top, float right, float bottom)
{
    if (!mInFrame) return;
    LLVKUI2DSink::get().rect(left, top, right, bottom, mR, mG, mB, mA);
}

void LLVKRender2D::outlineRect(float left, float top, float right, float bottom)
{
    if (!mInFrame) return;
    // Match gl_rect_2d's outline winding: inset top/right by 1px, closed strip.
    const float l = left, t = top - 1.f, r = right - 1.f, b = bottom;
    const float xy[10] = { l, t,  l, b,  r, b,  r, t,  l, t };
    LLVKUI2DSink::get().lineStrip(xy, 5, mR, mG, mB, mA);
}

void LLVKRender2D::line(float x1, float y1, float x2, float y2)
{
    if (!mInFrame) return;
    const float xy[4] = { x1, y1, x2, y2 };
    LLVKUI2DSink::get().lineStrip(xy, 2, mR, mG, mB, mA);
}

void LLVKRender2D::rectColored(float left, float top, float right, float bottom,
                               float r, float g, float b, float a)
{
    if (!mInFrame) return;
    LLVKUI2DSink::get().rect(left, top, right, bottom, r, g, b, a);
}

namespace LLVKRender
{
    LLVKRender2D& get()
    {
        static LLVKRender2D s_backend;
        return s_backend;
    }
}
