/**
 * @file llvkui2d.h
 * @brief The independent Vulkan 2D/UI emission sink (Phase 3b v2).
 *
 * @details
 * LLVKUI2D is the backend-dispatch sink the UI funnel primitives route to when
 * the Vulkan backend owns the frame. It reproduces the OUTPUT of the GL
 * immediate-mode UI pipe without running gGL: it tracks the pending render
 * state (blend mode, bound texture, scissor, UI transform), accumulates
 * geometry per state-run, and flushes to the LLVKContext 2D pipeline in
 * submission (painter) order with flush-before-mutate discipline.
 *
 * The contract mirrors LLRender::flush(): positions/per-vertex color/uv are
 * baked at emit time (UI transform applied); blend/texture/scissor/program are
 * read at flush time. A state change flushes the pending run first.
 *
 * Frame bracketing: LLVKSession begins the 2D render pass and hands the command
 * buffer to LLVKUI2D::begin(); the funnels emit; LLVKUI2D::end() flushes and
 * the session presents.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, The Vulkanstorm Project
 * $/LicenseInfo$
 */

#ifndef LLVKUI2D_H
#define LLVKUI2D_H

#include "volk/volk.h"
#include "vma/vk_mem_alloc.h"

#include <cstdint>
#include <vector>

class LLVKContext;

// The 2D blend modes the UI uses (subset of LLRender::eBlendType). Mirrors the
// GL factors: ALPHA = SRC_ALPHA/ONE_MINUS_SRC_ALPHA, REPLACE = ONE/ZERO,
// ADD_WITH_ALPHA = SRC_ALPHA/ONE, ADD = ONE/ONE.
enum class LLVKBlend : uint8_t { Alpha, Replace, AddWithAlpha, Add };

class LLVKUI2D
{
public:
    // One interleaved vertex: pixel pos (top-left origin), uv, RGBA (matches
    // ui2d.vert).
    struct Vert { float x, y, u, v, r, g, b, a; };

    // Begin a UI frame: remember the context + live command buffer, reset the
    // batch and pending state.
    void begin(LLVKContext* ctx, VkCommandBuffer cmd);
    // Flush any pending run and end the frame.
    void end();

    bool isActive() const { return mCmd != VK_NULL_HANDLE; }

    // --- State (read at flush; a change flushes the pending run first) ------
    void setBlend(LLVKBlend blend);
    void setTexture(VkDescriptorSet descriptor); // VK_NULL_HANDLE = solid/white
    void setScissor(int x, int y, int w, int h); // device pixels; empty = off
    void clearScissor();
    // UI transform: the current offset+scale applied to emitted positions.
    void setTransform(float off_x, float off_y, float scale_x, float scale_y);

    // --- Geometry emission --------------------------------------------------
    // Solid-color filled rect (BT_ALPHA handled by state). Positions are local
    // UI coords; the current transform is applied at emit.
    void rect(float left, float top, float right, float bottom,
              float r, float g, float b, float a);
    // Textured quad with uv + tint. uv in 0..1 over the bound texture.
    void texturedQuad(float left, float top, float right, float bottom,
                      float u0, float v0, float u1, float v1,
                      float r, float g, float b, float a);
    // Line strip (outline). n points.
    void lineStrip(const float* xy, int count, float r, float g, float b, float a);
    // Raw triangles with PER-VERTEX color (e.g. gl_drop_shadow gradients). xy =
    // 2 floats per vertex (local UI coords, transform applied at emit), rgba =
    // 4 floats per vertex. count = number of vertices (must be a multiple of 3).
    void rawTris(const float* xy, const float* rgba, int count);
    // Pre-transformed textured triangles (M2 image path: gl_draw_scaled_rotated_image
    // bakes the UI translation into the verts itself). xy = 2 floats/vert
    // (already screen-space — NO transform applied), uv = 2 floats/vert, rgba =
    // 4 floats/vert (per-vertex tint, usually uniform). count = vertices (mult of 3).
    void texturedBatchPreTransformed(const float* xy, const float* uv, const float* rgba, int count);

private:
    void flushRun();

    LLVKContext*  mCtx = nullptr;
    VkCommandBuffer mCmd = VK_NULL_HANDLE;

    std::vector<Vert> mVerts;      // pending run
    VkPrimitiveTopology mTopo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // Pending state for the current run.
    LLVKBlend      mBlend = LLVKBlend::Alpha;
    VkDescriptorSet mTexture = VK_NULL_HANDLE;
    bool           mScissorOn = false;
    int            mSx = 0, mSy = 0, mSw = 0, mSh = 0;
    float          mOffX = 0.f, mOffY = 0.f, mScaleX = 1.f, mScaleY = 1.f;

    // GPU vertex buffer (grows as needed).
    VkBuffer       mVBuf = VK_NULL_HANDLE;
    VmaAllocation  mVBufAlloc = VK_NULL_HANDLE;
    VkDeviceSize   mVBufCapacity = 0;
    // Retired buffers from mid-stream grows; destroyed at end() after the
    // frame's draws are submitted (they may still be referenced by the
    // in-flight command buffer, so destruction must not wait-idle mid-pass).
    std::vector<std::pair<VkBuffer, VmaAllocation>> mRetiredBufs;
    // Per-frame draw stats (diagnostic).
    int    mFrameFlushes = 0;
    size_t mFrameVerts = 0;
    // Running vertex offset into mVBuf for this frame (appended per flush so
    // deferred draws don't overwrite each other).
    size_t mFrameVertOffset = 0;
};

// The process-wide UI sink. The funnel primitives route here when the Vulkan
// backend owns the frame.
namespace LLVKUI2DSink
{
    LLVKUI2D& get();
}

#endif // LLVKUI2D_H
