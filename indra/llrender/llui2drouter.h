/**
 * @file llui2drouter.h
 * @brief The neutral 2D/UI drawing router (Phase 3 v2, M0).
 *
 * @details
 * LLUI2DRouter is the boundary between the widget tree's 2D draw calls and the
 * active render backend. It is a ROUTER, not a renderer: it owns no drawing
 * logic itself, only forwards the tree's 2D calls to whichever backend is bound.
 *
 * Exactly one backend implementation is bound per session, selected once at the
 * frame seam (display_startup / render_ui_2d), not per call. The GL
 * implementation (llui2drouter_gl.cpp) is a zero-change pass-through over the
 * existing gl_* / LLRender2D / LLScreenClipRect helpers, so the GL reference
 * stays byte-stable. The Vulkan implementation is llvkrender, which never reads
 * gGL.
 *
 * Design: doc/vulkan/phase3_v2_ui_plan.md + phase3_v2_m0_design.md.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, The Vulkanstorm Project
 * $/LicenseInfo$
 */

#ifndef LLUI2DROUTER_H
#define LLUI2DROUTER_H

#include <cstdint>

// The 2D blend modes (mirror of LLRender::eBlendType values the UI uses). Kept
// as plain ints here so the header stays free of llrender/GL includes.
namespace LLUI2DBlend
{
    enum : int { Alpha = 0, Replace, AddWithAlpha, Add };
}

// The backend interface. One instance per backend; the router holds a pointer
// to the active one. Methods are intentionally immediate-style (the widget tree
// is immediate-style); the BACKEND is responsible for batching/accumulating so
// that the unit of GPU submission is the coalesced state-run, not the widget
// (see the submission-granularity invariant in the plan).
class LLUI2DBackend
{
public:
    virtual ~LLUI2DBackend() = default;

    // Backend identity: true for the Vulkan backend (llvkrender), false for GL.
    // Choke points use this to route to the GL reference body vs the Vulkan
    // path. (The GL backend's body IS the original code, so it needs no routing.)
    virtual bool isVulkan() const = 0;

    // --- transform traversal hooks (stack-based, relative) ------------------
    // The tree accumulates per-widget offsets via push/translate/pop. Each
    // backend keeps its OWN transform stack (GL -> gGL's UI matrix stack;
    // Vulkan -> llvkrender's own). Neither reads the other's.
    virtual void pushTransform() = 0;
    virtual void popTransform() = 0;
    virtual void translate(float x, float y) = 0;
    virtual void scale(float sx, float sy) = 0;
    virtual void loadIdentityTransform() = 0;

    // --- production state ----------------------------------------------------
    virtual void setColor(float r, float g, float b, float a) = 0;
    virtual void setBlend(int blend_type) = 0;   // LLUI2DBlend::*
    // Scissor in GL bottom-left-origin device pixels (as the clip stack
    // computes it). w/h <= 0 means "no clip".
    virtual void setScissor(int x, int y, int w, int h) = 0;
    virtual void clearScissor() = 0;

    // --- primitives (M0 chrome vocabulary; textures/text are M2/M3) ----------
    // Solid filled rect, current color.
    virtual void rect(float left, float top, float right, float bottom) = 0;
    // 1px outline rect (matches gl_rect_2d filled=false), current color.
    virtual void outlineRect(float left, float top, float right, float bottom) = 0;
    // 1px line, current color.
    virtual void line(float x1, float y1, float x2, float y2) = 0;
    // Filled rect with an explicit color (most callers pass a color).
    virtual void rectColored(float left, float top, float right, float bottom,
                             float r, float g, float b, float a) = 0;
};

namespace LLUI2DRouter
{
    // Bind the active backend. Called once per frame at the seam (or once at
    // backend selection). Pass nullptr to unbind (calls become no-ops).
    void bind(LLUI2DBackend* backend);
    // The currently bound backend, or nullptr.
    LLUI2DBackend* active();
    // True when a backend is bound (calls forward; otherwise they no-op).
    bool isBound();
    // True when the bound backend is the Vulkan one (llvkrender).
    bool activeIsVulkan();

    // --- forwarding entry points (what the tree / choke points call) --------
    void pushTransform();
    void popTransform();
    void translate(float x, float y);
    void scale(float sx, float sy);
    void loadIdentityTransform();
    void setColor(float r, float g, float b, float a);
    void setBlend(int blend_type);
    void setScissor(int x, int y, int w, int h);
    void clearScissor();
    void rect(float left, float top, float right, float bottom);
    void outlineRect(float left, float top, float right, float bottom);
    void line(float x1, float y1, float x2, float y2);
    void rectColored(float left, float top, float right, float bottom,
                     float r, float g, float b, float a);
}

#endif // LLUI2DROUTER_H
