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
    // v0 = texture v at the quad's TOP screen edge, v1 = at the BOTTOM edge.
    // The sink rect may arrive with the edges in either order (the GL->top-left
    // conversion can yield top>bottom for some widgets), so normalize x/y to
    // min..max while keeping the texture upright: v0 stays at the visual top.
    void emitQuad(float x0, float y0, float x1, float y1,
                  float u0, float v0, float u1, float v1, const float c[4])
    {
        // Normalize the screen rect (min..max) so a rect passed with swapped
        // edges still rasterizes upright instead of inverting the texture.
        if (x0 > x1) { std::swap(x0, x1); }
        if (y0 > y1) { std::swap(y0, y1); }
        float xy[12]; float uv[12]; float rgba[24];
        const float vx[6] = { x0, x1, x1, x0, x1, x0 };
        const float vy[6] = { y0, y0, y1, y0, y1, y1 };
        const float tu[6] = { u0, u1, u1, u0, u1, u0 };
        const float tv[6] = { v0, v0, v1, v0, v1, v1 };
        // <VulkanStorm> orientation diagnostic (VULKANSTORM_UI_DEBUG=1): log the
        // quad's screen rect + texture v at its top/bottom edges.
        static bool s_dbg = getenv("VULKANSTORM_UI_DEBUG") != nullptr;
        static int  s_n = 0;
        if (s_dbg && s_n < 24)
        {
            ++s_n;
            LL_INFOS("Vulkan") << "VKQUAD rect=" << (int)x0 << "," << (int)y0 << "-" << (int)x1 << "," << (int)y1
                               << " vTop=" << v0 << " vBot=" << v1 << LL_ENDL;
        }
        // </VulkanStorm>
        for (int i = 0; i < 6; ++i)
        {
            xy[i * 2] = vx[i]; xy[i * 2 + 1] = vy[i];
            uv[i * 2] = tu[i]; uv[i * 2 + 1] = tv[i];
            rgba[i * 4 + 0] = c[0]; rgba[i * 4 + 1] = c[1];
            rgba[i * 4 + 2] = c[2]; rgba[i * 4 + 3] = c[3];
        }
        LLVKUI2DSink::get().texturedBatchPreTransformed(xy, uv, rgba, 6);
    }

    // Faithful port of gl_draw_scaled_image_with_border (llrender2dutils.cpp).
    // Same center-region math + same 9-quad decomposition; only the coordinate
    // frame is converted at emission: GL bottom-left -> sink top-left
    // (sink_top = total_height - gl_top). v0/v1 in emitQuad are the quad's
    // top/bottom texture rows (v grows downward on screen).
    void draw9Slice(const ImageRec& rec,
                    float x0, float y0, float x1, float y1,
                    const LLColor4& color)
    {
        // Normalize the target rect to top<bottom/left<right. The GL->top-left
        // conversion can hand us an inverted or off-window rect (e.g. the login
        // connect button); the slice math below assumes a sane ordering.
        if (x0 > x1) std::swap(x0, x1);
        if (y0 > y1) std::swap(y0, y1);
        const float width  = x1 - x0;   // target rect size
        const float height = y1 - y0;
        if (width <= 0.f || height <= 0.f) return;

        const LLRectf& uv_outer = rec.clip;
        const LLRectf& ctr      = rec.scale;
        const bool scale_inner  = (rec.style == LLVKUIImage::ScaleStyle::Inner);

        float c[4];
        const LLColor4 white(1.f,1.f,1.f,1.f);
        modulate(white, color, c);
        LLVKUI2DSink::get().setTexture(rec.tex.descriptor);

        // Degenerate (full-region) case == gl_draw_scaled_image. Same
        // convention as the 9-slice bands: the quad's TOP screen edge samples
        // the image's top (the HIGHER texture v, since the PNG decode stores
        // the image bottom-origin => v=1 is the image top), and the BOTTOM
        // screen edge samples the lower v.
        if (ctr.mLeft == 0.f && ctr.mRight == 1.f && ctr.mBottom == 0.f && ctr.mTop == 1.f)
        {
            emitQuad(x0, y0, x1, y1, uv_outer.mLeft, uv_outer.mTop, uv_outer.mRight, uv_outer.mBottom, c);
            return;
        }

        const float image_width  = (float)rec.w;
        const float image_height = (float)rec.h;
        const float uv_width  = uv_outer.getWidth();
        const float uv_height = uv_outer.getHeight();

        // uv_center_rect (mirrors GL exactly; note mBottom edge uses ctr.mTop).
        LLRectf uv_center(uv_outer.mLeft   + ctr.mLeft  * uv_width,
                          uv_outer.mBottom + ctr.mTop   * uv_height,
                          uv_outer.mLeft   + ctr.mRight * uv_width,
                          uv_outer.mBottom + ctr.mBottom* uv_height);

        const float img_nat_w = (float)ll_round(image_width  * uv_width);
        const float img_nat_h = (float)ll_round(image_height * uv_height);

        // draw_center_rect in the GL frame (local, y-up; mBottom=min, mTop=max),
        // then converted to sink top-left when emitting.
        LLRectf draw_center(uv_center.mLeft  * image_width,
                            uv_center.mTop   * image_height,
                            uv_center.mRight * image_width,
                            uv_center.mBottom* image_height);

        if (scale_inner)
        {
            draw_center.mRight += width  - img_nat_w;
            draw_center.mTop   += height - img_nat_h;

            const float shrink_w = llmax(0.f, draw_center.mLeft - draw_center.mRight);
            const float shrink_h = llmax(0.f, draw_center.mBottom - draw_center.mTop);
            const float w_ratio = ctr.getWidth()  == 1.f ? 0.f : shrink_w / (img_nat_w * (1.f - ctr.getWidth()));
            const float h_ratio = ctr.getHeight() == 1.f ? 0.f : shrink_h / (img_nat_h * (1.f - ctr.getHeight()));
            const float shrink_scale = 1.f - llmax(w_ratio, h_ratio);
            draw_center.mLeft   *= shrink_scale;
            draw_center.mTop     = lerp(height, draw_center.mTop, shrink_scale);
            draw_center.mRight   = lerp(width,  draw_center.mRight, shrink_scale);
            draw_center.mBottom *= shrink_scale;
        }
        else
        {
            const float scale_factor = llmin(llmin(width / draw_center.getWidth(), height / draw_center.getHeight()), 1.f);
            draw_center.setCenterAndSize(uv_center.getCenterX() * width, uv_center.getCenterY() * height,
                                         draw_center.getWidth() * scale_factor, draw_center.getHeight() * scale_factor);
        }

        // draw_center is local GL-frame (y-up, origin at the rect's bottom-left).
        // Convert each GL y to sink top-left within the target rect:
        //   sink_y = y0 + (height - gl_y)
        // and translate x by x0.
        auto X = [&](float gx) { return x0 + gx; };
        auto Y = [&](float gy) { return y0 + (height - gy); };

        // Sink-space edges. GL mBottom(min y) -> larger sink y; mTop -> smaller.
        const float oL = x0, oR = x1;                       // outer left/right
        const float oT = y0, oB = y1;                       // outer top/bottom (sink)
        const float cL = X(draw_center.mLeft);
        const float cR = X(draw_center.mRight);
        const float cTop_sink    = Y(draw_center.mTop);     // center top (sink, smaller)
        const float cBottom_sink = Y(draw_center.mBottom);  // center bottom (sink, larger)

        // UV edges (GL texture frame: mBottom=min row, mTop=max row). The PNG
        // decoder stores the image BOTTOM-origin in texture memory (row 0 =
        // image bottom), so a GL v already indexes the texture directly — v=1
        // is the image top, v=0 the bottom, exactly as gl_draw_scaled_image
        // samples it. emitQuad takes v0 = the quad's TOP screen edge row, so
        // a band whose GL texture rows run glVlower..glVupper maps its TOP
        // screen edge to glVupper and its BOTTOM edge to glVlower (no extra
        // 1-x flip; emitQuad supplies the screen-vs-texture orientation).
        const float uL = uv_outer.mLeft, uC0 = uv_center.mLeft, uC1 = uv_center.mRight, uR = uv_outer.mRight;
        const float vB = uv_outer.mBottom, vC0 = uv_center.mBottom, vC1 = uv_center.mTop, vT = uv_outer.mTop;

        auto band = [&](float sTop, float sBot, float glVlower, float glVupper)
        {
            emitQuad(oL, sTop, cL, sBot, uL,  glVupper, uC0, glVlower, c);  // left column
            emitQuad(cL, sTop, cR, sBot, uC0, glVupper, uC1, glVlower, c);  // center
            emitQuad(cR, sTop, oR, sBot, uC1, glVupper, uR,  glVlower, c);  // right column
        };
        // GL rows: bottom band [vB..vC0], middle [vC0..vC1], top [vC1..vT].
        // Sink rows (top..bottom): top band = GL top, then middle, then bottom.
        band(cBottom_sink, oB,          vB,  vC0);  // GL bottom band -> sink bottom
        band(cTop_sink,    cBottom_sink, vC0, vC1); // middle
        band(oT,           cTop_sink,    vC1, vT);  // GL top band -> sink top
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
                // <VulkanStorm> GL samples UI textures LINEAR (no mips); use the
                // GL-matched LINEAR sampler so stretched/9-slice images sample
                // identically (NEAREST diverged on any non-1:1 stretch).
                if (!ctx->createTexture2D(rgba.data(), (uint32_t)w, (uint32_t)h, rec.tex, error, /*linear=*/true))
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

    bool getSize(const std::string& name, int& width, int& height)
    {
        auto it = s_images.find(name);
        if (it == s_images.end() || !it->second.ok) return false;
        width = it->second.w;
        height = it->second.h;
        return width > 0 && height > 0;
    }

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
