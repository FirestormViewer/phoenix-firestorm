/**
 * @file llvkrender.h
 * @brief llvkrender — the Vulkan backend for LLUI2DRouter (Phase 3 v2, M0).
 *
 * @details
 * The independent Vulkan 2D render layer. It implements the LLUI2DBackend
 * surface over the batched LLVKUI2D sink, holding its OWN production state
 * (color, transform stack, scissor, blend) and NEVER reading gGL. This is the
 * fully-parallel counterpart to llrender's 2D role (see
 * doc/vulkan/phase3_v2_ui_plan.md + phase3_v2_m0_design.md).
 *
 * Transform: the tree accumulates per-widget offsets via push/translate/pop.
 * llvkrender keeps its own offset/scale stack (mirroring the result of the GL
 * UI matrix stack) and bakes it into each emitted vertex via the sink's
 * setTransform. Scale comes from the neutral LLUI::getScaleFactor() (never gGL).
 *
 * Scissor: the clip stack computes GL bottom-left device rects; llvkrender
 * converts to the sink's top-left space (the negative-height viewport) via a
 * Y-flip using the swapchain/device height.
 *
 * GL-free: no gGL / LLRender / LLImageGL / LLVertexBuffer.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, The Vulkanstorm Project
 * $/LicenseInfo$
 */

#ifndef LLVKRENDER_H
#define LLVKRENDER_H

#include "llui2drouter.h"

#include <vector>

class LLVKContext;

// The Vulkan 2D backend. One instance, bound to the router on the Vulkan path.
class LLVKRender2D : public LLUI2DBackend
{
public:
    LLVKRender2D();

    // Bind the Vulkan 2D context + the frame's live command buffer for the
    // frame. Called by the session seam at beginUIFrame. The backend emits to
    // the sink (which the session has begun). device_height is the swapchain
    // height (scissor Y-flip); ui_scale_x/y are the neutral UI scale factor
    // (the seam reads LLUI::getScaleFactor() and passes it in, so llvulkan
    // does not depend on llui).
    void beginFrame(LLVKContext* ctx, unsigned device_height, float ui_scale_x, float ui_scale_y);
    void endFrame();

    // --- LLUI2DBackend ---
    bool isVulkan() const override { return true; }
    void pushTransform() override;
    void popTransform() override;
    void translate(float x, float y) override;
    void scale(float sx, float sy) override;
    void loadIdentityTransform() override;

    void setColor(float r, float g, float b, float a) override;
    void setBlend(int blend_type) override;   // LLRender::eBlendType int
    void setScissor(int x, int y, int w, int h) override;
    void clearScissor() override;

    void rect(float left, float top, float right, float bottom) override;
    void outlineRect(float left, float top, float right, float bottom) override;
    void line(float x1, float y1, float x2, float y2) override;
    void rectColored(float left, float top, float right, float bottom,
                     float r, float g, float b, float a) override;

private:
    // Push the current accumulated transform (offset + scale) to the sink.
    void applyTransform();

    struct Transform { float off_x, off_y, scale_x, scale_y; };

    LLVKContext* mCtx = nullptr;
    unsigned     mDeviceHeight = 0;
    float        mUIScaleX = 1.f;   // neutral UI scale, supplied at beginFrame
    float        mUIScaleY = 1.f;

    // Own production state (never from gGL).
    float mR = 1.f, mG = 1.f, mB = 1.f, mA = 1.f;
    std::vector<Transform> mStack;   // transform stack (push/pop)
    Transform mCur{ 0.f, 0.f, 1.f, 1.f };
    bool mInFrame = false;
};

// Process-wide Vulkan 2D backend instance (bound to the router on Vulkan).
namespace LLVKRender
{
    LLVKRender2D& get();
}

#endif // LLVKRENDER_H
