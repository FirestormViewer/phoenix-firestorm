/**
 * @file llvkuiimage.cpp
 * @brief Implementation of LLVKUIImage — a GL-free, Vulkan-native UI-image
 *        registry (Phase 3 v2 M2). See llvkuiimage.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, The Vulkanstorm Project
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "llvkuiimage.h"

#include "llerror.h"
#include "lldir.h"
#include "llimage.h"          // LLImageFormatted::createFromExtension, LLImageRaw
#include "llmath.h"           // ll_round, lerp, llclamp, llmin, llmax
#include "llrect.h"
#include "llvkcontext.h"
#include "llvkui2d.h"
#include "llxmlnode.h"        // LLXMLNode (GL-free XML parse)
#include "v4color.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

namespace
{
    // One decoded UI image: a Vulkan texture + the geometry state the GL path
    // derives from the (POT-padded) GL texture + the textures.xml clip/scale
    // rects. Width/height are the DECODED image dims (pre-power-of-2 padding);
    // the Vulkan texture is exactly w x h (no POT pad needed), so UVs map 1:1.
    struct ImageRec
    {
        LLVKContext::Texture2D tex;
        int      w = 0;
        int      h = 0;
        LLRectf  clip;                       // normalized UV bounds (GL bottom-origin)
        LLRectf  scale;                      // normalized 9-slice center region
        LLVKUIImage::ScaleStyle style = LLVKUIImage::ScaleStyle::Inner;
        bool     ok = false;                 // uploaded successfully
    };

    LLVKContext*            s_ctx = nullptr;
    bool                    s_ready = false;
    std::map<std::string, ImageRec> s_images;

    // ---- helpers ----------------------------------------------------------

    // Decode a local image file to RGBA8 (bottom-left origin, like GL). Uses
    // the GL-free llimage decoders. Returns false on failure.
    bool decodeFileRGBA(const std::string& path, std::vector<uint8_t>& out, int& w, int& h)
    {
        LLPointer<LLImageFormatted> fmt = LLImageFormatted::createFromExtension(path);
        if (fmt.isNull())
        {
            return false;
        }
        if (!fmt->load(path))
        {
            return false;
        }
        if (!fmt->updateData())
        {
            return false;
        }
        LLPointer<LLImageRaw> raw = new LLImageRaw();
        if (!fmt->decode(raw, 0.f))
        {
            return false;
        }
        w = raw->getWidth();
        h = raw->getHeight();
        const S8 comp = raw->getComponents();
        if (w <= 0 || h <= 0 || comp < 1)
        {
            return false;
        }
        const U8* src = raw->getData();
        out.assign((size_t)w * h * 4, 255);
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                const size_t s = (size_t)(y * w + x) * comp;
                const size_t d = (size_t)(y * w + x) * 4;
                uint8_t r = src[s], g = comp > 1 ? src[s + 1] : r, b = comp > 2 ? src[s + 2] : r;
                uint8_t a = comp > 3 ? src[s + 3] : 255;
                out[d + 0] = r; out[d + 1] = g; out[d + 2] = b; out[d + 3] = a;
            }
        }
        return true;
    }

    // LLColor4 operator% (per-component multiply) replicated locally to keep
    // this file free of extra llmath color deps beyond v4color.
    inline void modulate(const LLColor4& a, const LLColor4& b, float out[4])
    {
        out[0] = a.mV[0] * b.mV[0];
        out[1] = a.mV[1] * b.mV[1];
        out[2] = a.mV[2] * b.mV[2];
        out[3] = a.mV[3] * b.mV[3];
    }

    // Emit one textured quad (top-left sink coords, 2 tris) into the sink.
    void emitQuad(float x0, float y0, float x1, float y1,
                  float u0, float v0, float u1, float v1, const float c[4])
    {
        // winding matches the GL 9-slice emission (each quad: TL,TR,BR / TL,BR,BL
        // in top-left space). UVs map GL bottom-origin texture rows (v=0 bottom).
        float xy[12]; float uv[12]; float rgba[24];
        const float vx[6] = { x0, x1, x1, x0, x1, x0 };
        const float vy[6] = { y0, y0, y1, y0, y1, y1 };
        const float tu[6] = { u0, u1, u1, u0, u1, u0 };
        const float tv[6] = { v1, v1, v0, v1, v0, v0 };
        for (int i = 0; i < 6; ++i)
        {
            xy[i * 2] = vx[i]; xy[i * 2 + 1] = vy[i];
            uv[i * 2] = tu[i]; uv[i * 2 + 1] = tv[i];
            rgba[i * 4 + 0] = c[0]; rgba[i * 4 + 1] = c[1];
            rgba[i * 4 + 2] = c[2]; rgba[i * 4 + 3] = c[3];
        }
        LLVKUI2DSink::get().texturedBatchPreTransformed(xy, uv, rgba, 6);
    }

    // 9-slice draw across a top-left sink rect. The sink's negative-height
    // viewport already flips texture v, so a fragment's texture-row index is
    // rowTex = v * H where v grows DOWNWARD on screen. We therefore compute the
    // slice layout directly in TOP-LEFT sink space (no y-up detour): the slice
    // tops track the scale region's top edge, and UVs map top slice -> v_top.
    // x0/y0 = top-left, x1/y1 = bottom-right in sink (top-left-origin) space.
    void draw9Slice(const ImageRec& rec,
                    float x0, float y0, float x1, float y1,
                    const LLColor4& color)
    {
        const float width  = x1 - x0;
        const float height = y1 - y0;
        if (width <= 0.f || height <= 0.f) return;

        const LLRectf& uv_outer = rec.clip;
        const LLRectf& ctr      = rec.scale;   // normalized center region (top-left frame)
        const bool scale_inner  = (rec.style == LLVKUIImage::ScaleStyle::Inner);

        const float image_width  = (float)rec.w;
        const float image_height = (float)rec.h;
        const float uv_width  = uv_outer.getWidth();
        const float uv_height = uv_outer.getHeight();

        // Degenerate (full-region) case == plain gl_draw_scaled_image.
        const bool full = (ctr.mLeft == 0.f && ctr.mRight == 1.f &&
                           ctr.mBottom == 0.f && ctr.mTop == 1.f);

        float c[4];
        const LLColor4 white(1.f,1.f,1.f,1.f);
        modulate(white, color, c);
        LLVKUI2DSink::get().setTexture(rec.tex.descriptor);

        if (full)
        {
            // Single quad: top-left sink corner maps to the clip region's top,
            // bottom-right to its bottom (v grows downward through the texture).
            emitQuad(x0, y0, x1, y1, uv_outer.mLeft, uv_outer.mTop, uv_outer.mRight, uv_outer.mBottom, c);
            return;
        }

        // uv_center_rect (top-left frame: ctr.mTop is the region's TOP).
        LLRectf uv_center(uv_outer.mLeft + ctr.mLeft  * uv_width,
                          uv_outer.mTop  + ctr.mTop   * uv_height,
                          uv_outer.mLeft + ctr.mRight * uv_width,
                          uv_outer.mTop  + ctr.mBottom* uv_height);

        const float img_nat_w = (float)ll_round(image_width  * uv_width);
        const float img_nat_h = (float)ll_round(image_height * uv_height);

        // Center region edges in image-pixel space (top-left frame): distances
        // from the top/left edges.
        float cL = ctr.mLeft   * img_nat_w;   // left border width  (px)
        float cR = ctr.mRight  * img_nat_w;   // left border + center (px from left)
        float cT = ctr.mTop    * img_nat_h;   // top border height  (px)
        float cB = ctr.mBottom * img_nat_h;   // top border + center (px from top)

        // SCALE_INNER: stretch the center to fill, preserving border pixel size
        // (shrinking borders proportionally if the rect is smaller than the image).
        if (scale_inner)
        {
            cR += width  - img_nat_w;   // grow right edge by the extra width
            cB += height - img_nat_h;   // grow bottom edge by the extra height

            const float shrink_w = llmax(0.f, cL - cR);
            const float shrink_h = llmax(0.f, cT - cB);
            const float w_ratio = ctr.getWidth()  == 1.f ? 0.f : shrink_w / (img_nat_w * (1.f - ctr.getWidth()));
            const float h_ratio = ctr.getHeight() == 1.f ? 0.f : shrink_h / (img_nat_h * (1.f - ctr.getHeight()));
            const float shrink_scale = 1.f - llmax(w_ratio, h_ratio);
            cL *= shrink_scale;
            cT *= shrink_scale;
            cR = lerp(width,  cR, shrink_scale);
            cB = lerp(height, cB, shrink_scale);
        }
        else
        {
            // SCALE_OUTER: keep the center at a fixed scale, same relative spot.
            const float center_w = cR - cL, center_h = cB - cT;
            const float scale_factor = llmin(llmin(width / center_w, height / center_h), 1.f);
            const float cx = (cL + cR) * 0.5f, cy = (cT + cB) * 0.5f;
            const float sw = center_w * scale_factor, sh = center_h * scale_factor;
            cL = cx - sw * 0.5f; cR = cx + sw * 0.5f;
            cT = cy - sh * 0.5f; cB = cy + sh * 0.5f;
        }

        // Convert local (top-left) center edges to absolute sink coords.
        const float L  = x0 + cL;
        const float R  = x0 + cR;
        const float T  = y0 + cT;
        const float B  = y0 + cB;
        const float oL = x0, oR = x1, oT = y0, oB = y1;

        // UV edges (top-left frame: v grows downward through the texture).
        const float uL = uv_outer.mLeft, uC0 = uv_center.mLeft, uC1 = uv_center.mRight, uR = uv_outer.mRight;
        const float vT = uv_outer.mTop, vC0 = uv_center.mTop,  vC1 = uv_center.mBottom, vB = uv_outer.mBottom;

        // Row band helper: sink-y band [yTop..yBot] maps to texture v [vTop..vBot].
        auto band = [&](float yTop, float yBot, float vTop, float vBot)
        {
            emitQuad(oL, yTop, L,  yBot, uL,  vTop, uC0, vBot, c);
            emitQuad(L,  yTop, R,  yBot, uC0, vTop, uC1, vBot, c);
            emitQuad(R,  yTop, oR, yBot, uC1, vTop, uR,  vBot, c);
        };
        band(oT, T,  vT,  vC0);   // top row
        band(T,  B,  vC0, vC1);   // middle row
        band(B,  oB, vC1, vB);    // bottom row
    }
}

namespace LLVKUIImage
{
    void init(LLVKContext* ctx)
    {
        if (s_ready || !ctx) return;
        s_ctx = ctx;

        std::vector<std::string> paths =
            gDirUtilp->findSkinnedFilenames(LLDir::TEXTURES, "textures.xml", LLDir::ALL_SKINS);
        if (paths.empty())
        {
            LL_WARNS("Vulkan") << "LLVKUIImage: no textures.xml found" << LL_ENDL;
            s_ready = true; // avoid retry storms; draws fall back to solid
            return;
        }

        // First-wins merge by name (the most-specific skin path comes first in
        // the ALL_SKINS list). Track which path supplied each entry so the file
        // resolves from the SAME skin dir as its declaration.
        std::map<std::string, std::string> src_dir; // name -> skin textures dir
        for (const std::string& p : paths)
        {
            LLXMLNodePtr root;
            if (!LLXMLNode::parseFile(p, root, nullptr) || root.isNull()) continue;
            const std::string dir = gDirUtilp->getDirName(p);
            for (LLXMLNodePtr child = root->getFirstChild(); child.notNull(); child = child->getNextSibling())
            {
                if (!child->hasName("texture")) continue;
                std::string name, file_name;
                if (!child->getAttributeString("name", name) || name.empty()) continue;
                if (s_images.find(name) != s_images.end()) continue; // first wins
                if (!child->getAttributeString("file_name", file_name) || file_name.empty())
                {
                    file_name = name;
                }
                ImageRec rec;
                // clip/scale are pixel rects expressed as dotted sub-attributes
                // (clip.left/clip.top/clip.right/clip.bottom), NOT a single
                // string attribute. Read each sub-attribute as S32.
                std::string scale_type = "scale_inner";
                auto rd = [&](const char* base, LLRect& out)
                {
                    char a[64];
                    S32 l, t, r, b;
                    snprintf(a, sizeof(a), "%s.left",   base); if (!child->getAttributeS32(a, l)) return false;
                    snprintf(a, sizeof(a), "%s.top",    base); if (!child->getAttributeS32(a, t)) return false;
                    snprintf(a, sizeof(a), "%s.right",  base); if (!child->getAttributeS32(a, r)) return false;
                    snprintf(a, sizeof(a), "%s.bottom", base); if (!child->getAttributeS32(a, b)) return false;
                    out.mLeft = l; out.mTop = t; out.mRight = r; out.mBottom = b;
                    return true;
                };
                LLRect clip_px, scale_px;
                const bool has_clip  = rd("clip",  clip_px);
                const bool has_scale = rd("scale", scale_px);
                child->getAttributeString("scale_type", scale_type);
                rec.style = (scale_type == "scale_outer") ? ScaleStyle::Outer : ScaleStyle::Inner;

                // Resolve + decode from the SAME skin dir as the declaration.
                std::string full = dir;
                if (!full.empty() && full.back() != '/' && full.back() != '\\') full += gDirUtilp->getDirDelimiter();
                full += file_name;

                std::vector<uint8_t> rgba; int w = 0, h = 0;
                if (!decodeFileRGBA(full, rgba, w, h))
                {
                    LL_DEBUGS("Vulkan") << "LLVKUIImage: decode failed for " << name << " (" << full << ")" << LL_ENDL;
                    s_images[name] = rec; // not ok; draw() falls back to solid
                    continue;
                }
                rec.w = w; rec.h = h;
                // Clip region: GL defaults to full image (0..1); with an
                // explicit clip rect, normalize against the decoded dims.
                if (has_clip)
                {
                    rec.clip = LLRectf(
                        llclamp((F32)clip_px.mLeft   / (F32)w, 0.f, 1.f),
                        llclamp((F32)clip_px.mTop    / (F32)h, 0.f, 1.f),
                        llclamp((F32)clip_px.mRight  / (F32)w, 0.f, 1.f),
                        llclamp((F32)clip_px.mBottom / (F32)h, 0.f, 1.f));
                }
                else
                {
                    rec.clip = LLRectf(0.f, 1.f, 1.f, 0.f);
                }
                if (has_scale)
                {
                    rec.scale = LLRectf(
                        llclamp((F32)scale_px.mLeft   / (F32)w, 0.f, 1.f),
                        llclamp((F32)scale_px.mTop    / (F32)h, 0.f, 1.f),
                        llclamp((F32)scale_px.mRight  / (F32)w, 0.f, 1.f),
                        llclamp((F32)scale_px.mBottom / (F32)h, 0.f, 1.f));
                }
                else
                {
                    rec.scale = LLRectf(0.f, 1.f, 1.f, 0.f);
                }
                std::string error;
                if (!ctx->createTexture2D(rgba.data(), (uint32_t)w, (uint32_t)h, rec.tex, error, /*linear=*/false))
                {
                    LL_WARNS("Vulkan") << "LLVKUIImage: upload failed for " << name << ": " << error << LL_ENDL;
                    s_images[name] = rec;
                    continue;
                }
                rec.ok = true;
                s_images[name] = rec;
            }
        }
        LL_INFOS("Vulkan") << "LLVKUIImage: loaded " << s_images.size() << " UI images" << LL_ENDL;
        s_ready = true;
    }

    bool ready() { return s_ready; }

    void draw(const std::string& name, float left, float top, float right, float bottom, const LLColor4& color)
    {
        // <VulkanStorm> M2 diagnostic (VULKANSTORM_UI_DEBUG=1): log the first
        // few image draws with their resolution status.
        static bool s_dbg = getenv("VULKANSTORM_UI_DEBUG") != nullptr;
        static int  s_dbg_n = 0;
        auto it = s_images.find(name);
        const bool found = (it != s_images.end());
        const bool ok    = found && it->second.ok;
        if (s_dbg && s_dbg_n < 12)
        {
            ++s_dbg_n;
            LL_INFOS("Vulkan") << "VKIMG draw '" << name << "' found=" << (found?1:0)
                               << " ok=" << (ok?1:0)
                               << (ok ? (" " + std::to_string(it->second.w) + "x" + std::to_string(it->second.h)) : "")
                               << " rect=" << (int)left << "," << (int)top << "-" << (int)right << "," << (int)bottom << LL_ENDL;
        }
        if (!found || !ok)
        {
            // Unknown/unloaded: fill solid so the widget still shows *something*.
            LLVKUI2DSink::get().setTexture(VK_NULL_HANDLE);
            LLVKUI2DSink::get().rect(left, top, right, bottom, color.mV[0], color.mV[1], color.mV[2], color.mV[3]);
            return;
        }
        draw9Slice(it->second, left, top, right, bottom, color);
    }

    void drawBorder(const std::string& name, float left, float top, float right, float bottom, const LLColor4& color, int border_width)
    {
        auto it = s_images.find(name);
        if (it == s_images.end() || !it->second.ok || it->second.w <= 0 || it->second.h <= 0)
        {
            LLVKUI2DSink::get().setTexture(VK_NULL_HANDLE);
            LLVKUI2DSink::get().rect(left, top, right, bottom, color.mV[0], color.mV[1], color.mV[2], color.mV[3]);
            return;
        }
        const ImageRec& rec = it->second;
        const float bw_frac = (float)border_width / (float)rec.w;
        const float bh_frac = (float)border_width / (float)rec.h;
        LLRectf scale_rect(bw_frac, 1.f - bh_frac, 1.f - bw_frac, bh_frac);
        ImageRec tmp = rec;
        tmp.scale = scale_rect;
        draw9Slice(tmp, left, top, right, bottom, color);
    }

    void drawSolid(const std::string& name, float left, float top, float right, float bottom, const LLColor4& color)
    {
        LLVKUI2DSink::get().setTexture(VK_NULL_HANDLE);
        LLVKUI2DSink::get().rect(left, top, right, bottom, color.mV[0], color.mV[1], color.mV[2], color.mV[3]);
    }

    void shutdown()
    {
        if (s_ctx)
        {
            for (auto& kv : s_images)
            {
                if (kv.second.ok)
                {
                    s_ctx->destroyTexture2D(kv.second.tex);
                }
            }
        }
        s_images.clear();
        s_ctx = nullptr;
        s_ready = false;
    }
}
