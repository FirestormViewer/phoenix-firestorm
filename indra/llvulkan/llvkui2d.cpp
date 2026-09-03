/**
 * @file llvkui2d.cpp
 * @brief Implementation of the independent Vulkan 2D/UI emission sink
 *        (see llvkui2d.h).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, The Vulkanstorm Project
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "llvkui2d.h"

#include "llerror.h"
#include "llvkcontext.h"

#include <cstring>
#include <string>  // <VulkanStorm> UI debug dump
#include <cstdlib> // <VulkanStorm> getenv (UI debug counter)

namespace
{
    LLVKUI2D s_ui;

    // <VulkanStorm> Mirror LLRender::color4f's float->U8 TRUNCATION
    // (llrender.cpp: GL converts via (GLubyte)(clamp(c)*255)) so vertex colors
    // quantize byte-identically to the GL path on unorm8 targets — e.g. the
    // login panel gray 0.16f lands as 40, not 41.
    inline float qcol(float c)
    {
        c = c < 0.f ? 0.f : (c > 1.f ? 1.f : c);
        return (float)(U8)(c * 255.f) / 255.f;
    }
    // </VulkanStorm>

    LLVKContext::Blend2D toContextBlend(LLVKBlend b)
    {
        switch (b)
        {
        case LLVKBlend::Alpha:         return LLVKContext::Blend2D::Alpha;
        case LLVKBlend::Replace:       return LLVKContext::Blend2D::Replace;
        case LLVKBlend::AddWithAlpha:  return LLVKContext::Blend2D::AddWithAlpha;
        case LLVKBlend::Add:           return LLVKContext::Blend2D::Add;
        }
        return LLVKContext::Blend2D::Alpha;
    }
}

void LLVKUI2D::begin(LLVKContext* ctx, VkCommandBuffer cmd)
{
    mCtx = ctx;
    mCmd = cmd;
    mVerts.clear();
    mTopo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    mFrameVertOffset = 0; // <VulkanStorm> reset the per-frame append offset

    // <VulkanStorm> Clean up retired grow-buffers from prior frames. begin2DFrame
    // has already waited on the previous frame's fence, so they are safe to free.
    for (auto& rb : mRetiredBufs)
    {
        vmaDestroyBuffer(mCtx->allocator(), rb.first, rb.second);
    }
    mRetiredBufs.clear();
    // </VulkanStorm>

    // Reset pending state to the frame defaults (matches GL's UI-pass init:
    // BT_ALPHA, untextured=white, no scissor, identity transform).
    mBlend = LLVKBlend::Alpha;
    mTexture = VK_NULL_HANDLE;
    mScissorOn = false;
    mOffX = 0.f; mOffY = 0.f; mScaleX = 1.f; mScaleY = 1.f;

    // <VulkanStorm> Pre-allocate a generous vertex buffer BEFORE the frame's
    // draws. Growing it mid-frame (flushRun) would call vkDeviceWaitIdle inside
    // the render pass, disrupting the in-flight frame and dropping the draws
    // (this was the M1 "teal screen" blocker). 1M verts (32 MB) covers the
    // login UI with huge headroom; flushRun only grows if genuinely exceeded.
    if (mVBuf == VK_NULL_HANDLE)
    {
        const VkDeviceSize cap = (VkDeviceSize)1024 * 1024 * sizeof(Vert);
        VkBufferCreateInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bi.size = cap;
        bi.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VmaAllocationCreateInfo ai{};
        ai.usage = VMA_MEMORY_USAGE_AUTO;
        ai.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        VmaAllocationInfo vinfo{};
        if (vmaCreateBuffer(mCtx->allocator(), &bi, &ai, &mVBuf, &mVBufAlloc, &vinfo) == VK_SUCCESS)
        {
            mVBufCapacity = cap;
        }
        else
        {
            LL_WARNS("Vulkan") << "UI2D: vertex buffer pre-alloc failed" << LL_ENDL;
        }
    }
    // </VulkanStorm>
}

void LLVKUI2D::end()
{
    flushRun();
    // <VulkanStorm> M1 diagnostic: total verts drawn this frame (VULKANSTORM_UI_DEBUG=1).
    static bool s_dbg = getenv("VULKANSTORM_UI_DEBUG") != nullptr;
    if (s_dbg) { static int s_f = 0; if ((s_f++ % 120) == 0) { LL_INFOS("Vulkan") << "UI2D frame: flushes=" << mFrameFlushes << " vertsDrawn=" << mFrameVerts << LL_ENDL; } }
    mFrameFlushes = 0; mFrameVerts = 0;
    // </VulkanStorm>
    mCmd = VK_NULL_HANDLE;
    // Retired buffers from mid-stream grows: the frame's submits are queued but
    // not necessarily complete, so defer destruction until the device is idle.
    // end2DFrame presents right after this; the next begin() (next frame) idles
    // the device via begin2DFrame's fence wait, so destroying then is safe.
    // For simplicity and correctness, destroy them here only after a device
    // idle is NOT done (that was the bug); instead retire them for the next
    // begin() to clean up after the previous frame's fence is waited.
    mCtx = nullptr;
}

void LLVKUI2D::setBlend(LLVKBlend blend)
{
    if (blend == mBlend) return;
    flushRun();            // flush-before-mutate
    mBlend = blend;
}

void LLVKUI2D::setTexture(VkDescriptorSet descriptor)
{
    if (descriptor == mTexture) return;
    flushRun();            // flush-before-mutate
    mTexture = descriptor;
}

void LLVKUI2D::setScissor(int x, int y, int w, int h)
{
    if (mScissorOn && x == mSx && y == mSy && w == mSw && h == mSh) return;
    flushRun();
    mScissorOn = true;
    mSx = x; mSy = y; mSw = w; mSh = h;
}

void LLVKUI2D::clearScissor()
{
    if (!mScissorOn) return;
    flushRun();
    mScissorOn = false;
}

void LLVKUI2D::setTransform(float off_x, float off_y, float scale_x, float scale_y)
{
    mOffX = off_x; mOffY = off_y; mScaleX = scale_x; mScaleY = scale_y;
}

void LLVKUI2D::rect(float left, float top, float right, float bottom,
                    float r, float g, float b, float a)
{
    if (!isActive()) return;
    if (mTopo != VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST) { flushRun(); mTopo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; }

    // Bake the UI transform at emit time (matches LLRender::vertex3f).
    float x0 = (left + mOffX) * mScaleX,  y0 = (top + mOffY) * mScaleY;
    float x1 = (right + mOffX) * mScaleX, y1 = (bottom + mOffY) * mScaleY;
    // gl_rect_2d is called with LLRect semantics (mTop = larger y, GL bottom-
    // origin). Normalize to min..max so the quad covers the rect regardless of
    // the caller's y ordering (the harness and LLRect callers differ).
    if (x0 > x1) { float t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { float t = y0; y0 = y1; y1 = t; }

    // Two triangles matching the harness pushRect winding (proven byte-exact):
    // (x0,y0),(x1,y0),(x1,y1) / (x0,y0),(x1,y1),(x0,y1). Colors quantized like
    // LLRender::color4f (U8 truncation) for byte-exact unorm8 output.
    r = qcol(r); g = qcol(g); b = qcol(b); a = qcol(a);
    mVerts.push_back({ x0, y0, 0, 0, r, g, b, a });
    mVerts.push_back({ x1, y0, 1, 0, r, g, b, a });
    mVerts.push_back({ x1, y1, 1, 1, r, g, b, a });
    mVerts.push_back({ x0, y0, 0, 0, r, g, b, a });
    mVerts.push_back({ x1, y1, 1, 1, r, g, b, a });
    mVerts.push_back({ x0, y1, 0, 1, r, g, b, a });
}

void LLVKUI2D::texturedQuad(float left, float top, float right, float bottom,
                            float u0, float v0, float u1, float v1,
                            float r, float g, float b, float a)
{
    if (!isActive()) return;
    if (mTopo != VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST) { flushRun(); mTopo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; }

    const float x0 = (left + mOffX) * mScaleX,  y0 = (top + mOffY) * mScaleY;
    const float x1 = (right + mOffX) * mScaleX, y1 = (bottom + mOffY) * mScaleY;

    mVerts.push_back({ x0, y0, u0, v0, r, g, b, a });
    mVerts.push_back({ x0, y1, u0, v1, r, g, b, a });
    mVerts.push_back({ x1, y1, u1, v1, r, g, b, a });
    mVerts.push_back({ x0, y0, u0, v0, r, g, b, a });
    mVerts.push_back({ x1, y1, u1, v1, r, g, b, a });
    mVerts.push_back({ x1, y0, u1, v0, r, g, b, a });
}

void LLVKUI2D::lineStrip(const float* xy, int count, float r, float g, float b, float a)
{
    if (!isActive() || count < 2) return;
    // Each lineStrip is an INDEPENDENT polyline (GL draws each outline as its own
    // begin/end). A shared LINE_STRIP run would connect consecutive strips with a
    // spurious diagonal, so flush any pending work, emit this strip, and flush it
    // immediately as its own draw. (UI outlines are few; correctness first.)
    flushRun();
    if (mTopo != VK_PRIMITIVE_TOPOLOGY_LINE_STRIP) { mTopo = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP; }
    for (int i = 0; i < count; ++i)
    {
        const float x = (xy[i * 2] + mOffX) * mScaleX;
        const float y = (xy[i * 2 + 1] + mOffY) * mScaleY;
        mVerts.push_back({ x, y, 0, 0, r, g, b, a });
    }
    flushRun();
    mTopo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

void LLVKUI2D::rawTris(const float* xy, const float* rgba, int count)
{
    if (!isActive() || count < 3 || (count % 3) != 0) return;
    if (mTopo != VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST) { flushRun(); mTopo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; }
    for (int i = 0; i < count; ++i)
    {
        const float x = (xy[i * 2] + mOffX) * mScaleX;
        const float y = (xy[i * 2 + 1] + mOffY) * mScaleY;
        mVerts.push_back({ x, y, 0, 0,
                           rgba[i * 4 + 0], rgba[i * 4 + 1], rgba[i * 4 + 2], rgba[i * 4 + 3] });
    }
}

void LLVKUI2D::texturedBatchPreTransformed(const float* xy, const float* uv, const float* rgba, int count)
{
    if (!isActive() || count < 3 || (count % 3) != 0) return;
    if (mTopo != VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST) { flushRun(); mTopo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; }
    // Positions are already screen-space (the caller baked the UI transform), so
    // NO transform is applied here — matching LLRender::vertexBatchPreTransformed.
    for (int i = 0; i < count; ++i)
    {
        mVerts.push_back({ xy[i * 2], xy[i * 2 + 1],
                           uv[i * 2], uv[i * 2 + 1],
                           rgba[i * 4 + 0], rgba[i * 4 + 1], rgba[i * 4 + 2], rgba[i * 4 + 3] });
    }
}

void LLVKUI2D::flushRun()
{
    if (!isActive() || mVerts.empty() || !mCtx)
    {
        return;
    }

    const VkDeviceSize bytes = (VkDeviceSize)(mVerts.size() * sizeof(Vert));
    const VkDeviceSize need = (mFrameVertOffset * sizeof(Vert)) + bytes;

    if (need > mVBufCapacity)
    {
        // <VulkanStorm> Grow WITHOUT vkDeviceWaitIdle: the old buffer may be
        // referenced by an in-flight frame, and waiting mid-render-pass drops
        // the current frame's draws. Allocate a new buffer and defer destroying
        // the old one (leak-safe: destroyed at end() via the deferred list).
        // </VulkanStorm>
        VkBuffer oldBuf = mVBuf;
        VmaAllocation oldAlloc = mVBufAlloc;
        VkDeviceSize cap = need * 2;
        VkBufferCreateInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bi.size = cap;
        bi.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VmaAllocationCreateInfo ai{};
        ai.usage = VMA_MEMORY_USAGE_AUTO;
        ai.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        VmaAllocationInfo vinfo{};
        if (vmaCreateBuffer(mCtx->allocator(), &bi, &ai, &mVBuf, &mVBufAlloc, &vinfo) != VK_SUCCESS)
        {
            LL_WARNS("Vulkan") << "UI2D: vertex buffer alloc failed" << LL_ENDL;
            mVerts.clear();
            return;
        }
        mVBufCapacity = cap;
        // Defer the old buffer's destruction (it may still be in use).
        if (oldBuf != VK_NULL_HANDLE)
        {
            mRetiredBufs.push_back({ oldBuf, oldAlloc });
        }
    }

    VmaAllocationInfo vinfo{};
    vmaGetAllocationInfo(mCtx->allocator(), mVBufAlloc, &vinfo);
    // <VulkanStorm> Append this run at the frame's running vertex offset. The
    // draws are deferred (recorded, executed at submit), so each flush MUST
    // write to a distinct region — overwriting one buffer between flushes would
    // make every draw read only the final flush's data (the M1 teal-screen bug).
    uint8_t* dst = (uint8_t*)vinfo.pMappedData + (mFrameVertOffset * sizeof(Vert));
    memcpy(dst, mVerts.data(), (size_t)bytes);
    // Ensure the host writes are visible to the GPU before the draw reads them.
    // (No-op when the allocation is already HOST_COHERENT.)
    vmaFlushAllocation(mCtx->allocator(), mVBufAlloc, mFrameVertOffset * sizeof(Vert), bytes);
    const uint32_t firstVertex = (uint32_t)mFrameVertOffset;
    mFrameVertOffset += mVerts.size();
    // </VulkanStorm>

    // Ortho projection for TOP-LEFT-origin input: maps [0,W]x[0,H] to clip with
    // y inverted (screen-y down). Combined with a POSITIVE-height viewport, this
    // puts UI element (0,0) at the screen top-left with NO global flip — so both
    // geometry positions and textured content land upright.
    const float W = (float)mCtx->swapchainExtent().width;
    const float H = (float)mCtx->swapchainExtent().height;
    float ortho[16] = {
        2.f / W, 0.f,       0.f, 0.f,
        0.f,    -2.f / H,   0.f, 0.f,
        0.f,     0.f,      -1.f, 0.f,
       -1.f,     1.f,       0.f, 1.f
    };
    vkCmdPushConstants(mCmd, mCtx->pipelineLayout2D(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ortho), ortho);

    // <VulkanStorm> Positive-height viewport (no global flip): the top-left
    // origin comes from the ortho matrix above, not from a negative viewport.
    VkViewport viewport{ 0.f, 0.f, (float)mCtx->swapchainExtent().width, (float)mCtx->swapchainExtent().height, 0.f, 1.f };
    vkCmdSetViewport(mCmd, 0, 1, &viewport);
    // </VulkanStorm>

    // Scissor (read at flush).
    // <VulkanStorm> M1 diagnostic: VULKANSTORM_UI_DEBUG=nosci forces full-frame
    // scissor to isolate a bad scissor conversion.
    static bool s_nosci = getenv("VULKANSTORM_UI_DEBUG") && std::string(getenv("VULKANSTORM_UI_DEBUG")) == "nosci";
    VkRect2D scissor{ { 0, 0 }, mCtx->swapchainExtent() };
    if (mScissorOn && !s_nosci)
    {
        scissor.offset = { mSx, mSy };
        scissor.extent = { (uint32_t)mSw, (uint32_t)mSh };
    }
    vkCmdSetScissor(mCmd, 0, 1, &scissor);

    // Bind the pipeline for the current blend mode AND topology (lines use the
    // line-strip variant; topology is baked into the pipeline, not dynamic).
    const bool isLine = (mTopo == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP);
    vkCmdBindPipeline(mCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mCtx->pipeline2D(toContextBlend(mBlend), isLine));
    VkDescriptorSet tex = (mTexture != VK_NULL_HANDLE) ? mTexture : mCtx->whiteTextureDescriptor();
    mCtx->bindTexture2D(mCmd, tex);

    VkDeviceSize off = 0;
    vkCmdBindVertexBuffers(mCmd, 0, 1, &mVBuf, &off);
    vkCmdDraw(mCmd, (uint32_t)mVerts.size(), 1, firstVertex, 0);

    // <VulkanStorm> track per-frame draw stats (diagnostic).
    ++mFrameFlushes; mFrameVerts += mVerts.size();
    // </VulkanStorm>

    mVerts.clear();
}

// --- Global sink ------------------------------------------------------------

namespace LLVKUI2DSink
{
    LLVKUI2D& get() { return s_ui; }
}
