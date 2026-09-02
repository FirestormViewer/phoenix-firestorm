/**
 * @file llui2drouter_gl.cpp
 * @brief The GL backend for LLUI2DRouter — a zero-change pass-through over the
 *        existing gl_* / LLRender2D / scissor helpers, so the GL reference
 *        stays byte-stable. This file MAY touch gGL (it is the GL backend).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, The Vulkanstorm Project
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "llui2drouter.h"

#include "llrender.h"          // gGL, LLRender
#include "llrender2dutils.h"   // gl_rect_2d, gl_line_2d, LLRender2D

namespace
{
    // The GL backend: every call forwards to the exact helper the widget tree
    // uses today, in the same order, with the same arguments. Coordinates are
    // rounded to int to match the S32 gl_* signatures.
    class LLUI2DBackendGL : public LLUI2DBackend
    {
    public:
        bool isVulkan() const override { return false; }

        void pushTransform() override            { LLRender2D::pushMatrix(); }
        void popTransform() override             { LLRender2D::popMatrix(); }
        void translate(float x, float y) override { LLRender2D::translate(x, y); }
        void scale(float sx, float sy) override   { gGL.scaleUI(sx, sy, 1.f); }
        void loadIdentityTransform() override    { LLRender2D::loadIdentity(); }

        void setColor(float r, float g, float b, float a) override { gGL.color4f(r, g, b, a); }
        void setBlend(int blend_type) override
        {
            gGL.setSceneBlendType((LLRender::eBlendType)blend_type);
        }
        void setScissor(int x, int y, int w, int h) override
        {
            // Match LLScreenClipRect's GL path: scissor-test on + glScissor
            // (coords are GL bottom-left device pixels, as the clip stack
            // computes them). Flush deferred draws before changing the region.
            gGL.flush();
            if (w <= 0 || h <= 0)
            {
                glDisable(GL_SCISSOR_TEST);
                return;
            }
            glEnable(GL_SCISSOR_TEST);
            glScissor(x, y, w, h);
        }
        void clearScissor() override
        {
            gGL.flush();
            glDisable(GL_SCISSOR_TEST);
        }

        void rect(float left, float top, float right, float bottom) override
        {
            gl_rect_2d((S32)llround(left), (S32)llround(top), (S32)llround(right), (S32)llround(bottom), true);
        }
        void outlineRect(float left, float top, float right, float bottom) override
        {
            gl_rect_2d((S32)llround(left), (S32)llround(top), (S32)llround(right), (S32)llround(bottom), false);
        }
        void line(float x1, float y1, float x2, float y2) override
        {
            gl_line_2d((S32)llround(x1), (S32)llround(y1), (S32)llround(x2), (S32)llround(y2));
        }
        void rectColored(float left, float top, float right, float bottom,
                         float r, float g, float b, float a) override
        {
            gl_rect_2d((S32)llround(left), (S32)llround(top), (S32)llround(right), (S32)llround(bottom),
                       LLColor4(r, g, b, a), true);
        }
    };

    LLUI2DBackendGL s_gl_backend;
}

namespace LLUI2DRouter
{
    // Provided for the seam: bind the GL backend.
    LLUI2DBackend* getGLBackend() { return &s_gl_backend; }
}
