/**
 * @file llvktext.cpp
 * @brief Independent Vulkan text rasterizer and atlas.
 */
#include "linden_common.h"

#include "llvklegacytext.h"

#include "llerror.h"
#include "llmath.h"
#include "llvkcontext.h"
#include "llvkui2d.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MULTIPLE_MASTERS_H

#include <algorithm>
#include <map>
#include <memory>
#include <vector>

namespace
{
    constexpr U32 ATLAS_SIZE = 512;
    constexpr U32 ATLAS_GAP = 1;

    struct Glyph
    {
        S32 width = 0, height = 0;
        S32 bearing_x = 0, bearing_y = 0;
        F32 advance = 0.f;
        S32 lsb_delta = 0, rsb_delta = 0;
        U32 glyph_index = 0;
        F32 u0 = 0.f, v0 = 0.f, u1 = 0.f, v1 = 0.f;
    };

    struct Font
    {
        FT_Face face = nullptr;
        // <VulkanStorm> Fallback face for glyphs the primary face cannot
        // rasterize (color-emoji/bitmap fonts like Twitter Color Emoji have no
        // outline ASCII glyphs). Loaded lazily from the default UI font.
        FT_Face fallback_face = nullptr;
        std::string fallback_filename;
        FT_F26Dot6 fallback_size = 0;
        // </VulkanStorm>
        std::map<llwchar, Glyph> glyphs;
        std::vector<U8> pixels;
        U32 pen_x = 1, pen_y = 1, row_h = 0;
        bool dirty = false;
        LLVKContext::Texture2D texture;
    };

    LLVKContext* s_context = nullptr;
    FT_Library s_library = nullptr;
    std::map<const LLFontGL*, std::unique_ptr<Font>> s_fonts;

    void setWeight(FT_Face face, S32 weight)
    {
        if (!face || weight < 0 || !FT_HAS_MULTIPLE_MASTERS(face)) return;
        FT_MM_Var* mm = nullptr;
        if (FT_Get_MM_Var(face, &mm) || !mm) return;
        std::vector<FT_Fixed> coords(mm->num_axis);
        if (FT_Get_Var_Design_Coordinates(face, mm->num_axis, coords.data()))
        {
            for (FT_UInt i = 0; i < mm->num_axis; ++i) coords[i] = mm->axis[i].def;
        }
        for (FT_UInt i = 0; i < mm->num_axis; ++i)
        {
            if (mm->axis[i].tag == FT_MAKE_TAG('w','g','h','t'))
                coords[i] = (FT_Fixed)weight << 16;
        }
        FT_Set_Var_Design_Coordinates(face, mm->num_axis, coords.data());
        FT_Done_MM_Var(s_library, mm);
    }

    Font* getFont(const LLFontGL* font)
    {
        if (!font || !s_library) return nullptr;
        auto found = s_fonts.find(font);
        if (found != s_fonts.end()) return found->second.get();

        LLFontGL::VkFaceInfo info;
        if (!font->getVkFaceInfo(info)) return nullptr;
        std::unique_ptr<Font> created(new Font());
        if (FT_New_Face(s_library, info.filename.c_str(), 0, &created->face))
        {
            LL_WARNS("Vulkan") << "LLVKText: cannot load " << info.filename << LL_ENDL;
            return nullptr;
        }
        setWeight(created->face, info.weight);
        FT_Set_Char_Size(created->face, 0, (FT_F26Dot6)ll_round(info.point_size * 64.f),
                        (FT_UInt)LLFontGL::sHorizDPI, (FT_UInt)LLFontGL::sVertDPI);
        created->pixels.assign(ATLAS_SIZE * ATLAS_SIZE * 4, 0);
        Font* result = created.get();
        s_fonts[font] = std::move(created);
        // <VulkanStorm> prepare the fallback face (default UI font) so
        // bitmap/emoji-only fonts can render plain text glyphs. The default
        // SansSerif face is the same source the working menu text uses.
        LLFontGL* fallback_gl = LLFontGL::getFontSansSerif();
        if (fallback_gl && fallback_gl != font)
        {
            LLFontGL::VkFaceInfo fb;
            if (fallback_gl->getVkFaceInfo(fb) && fb.filename != info.filename)
            {
                result->fallback_filename = fb.filename;
                result->fallback_size = (FT_F26Dot6)ll_round(fb.point_size * 64.f);
            }
        }
        // </VulkanStorm>
        return result;
    }

    const Glyph* ensureGlyph(Font& font, llwchar ch)
    {
        auto old = font.glyphs.find(ch);
        if (old != font.glyphs.end()) return &old->second;
        FT_UInt index = FT_Get_Char_Index(font.face, (FT_ULong)ch);

        FT_Int32 load_flags = FT_LOAD_FORCE_AUTOHINT;
        if (FT_Load_Glyph(font.face, index, load_flags) ||
            FT_Render_Glyph(font.face->glyph, FT_RENDER_MODE_NORMAL))
            return nullptr;
        FT_GlyphSlot slot = font.face->glyph;
        // The primary face cannot rasterize this glyph (color/bitmap emoji
        // fonts have no outline ASCII). Fall back to the default UI font face.
        if (slot->advance.x == 0 && slot->bitmap.width == 0 &&
            !font.fallback_filename.empty())
        {
            if (!font.fallback_face)
            {
                if (FT_New_Face(s_library, font.fallback_filename.c_str(), 0,
                                &font.fallback_face))
                {
                    font.fallback_face = nullptr;
                }
                else
                {
                    FT_Set_Char_Size(font.fallback_face, 0, font.fallback_size,
                                     (FT_UInt)LLFontGL::sHorizDPI, (FT_UInt)LLFontGL::sVertDPI);
                }
            }
            if (font.fallback_face)
            {
                FT_UInt fb_index = FT_Get_Char_Index(font.fallback_face, (FT_ULong)ch);
                if (FT_Load_Glyph(font.fallback_face, fb_index, FT_LOAD_FORCE_AUTOHINT) ||
                    FT_Render_Glyph(font.fallback_face->glyph, FT_RENDER_MODE_NORMAL))
                    return nullptr;
                slot = font.fallback_face->glyph;
            }
            else
            {
                return nullptr;
            }
        }
        // </VulkanStorm>

        const U32 width = slot->bitmap.width;
        const U32 height = slot->bitmap.rows;
        if (font.pen_x + width + ATLAS_GAP >= ATLAS_SIZE)
        {
            font.pen_x = 1;
            font.pen_y += font.row_h + ATLAS_GAP;
            font.row_h = 0;
        }
        if (font.pen_y + height + ATLAS_GAP >= ATLAS_SIZE)
        {
            LL_WARNS("Vulkan") << "LLVKText: atlas full" << LL_ENDL;
            return nullptr;
        }

        Glyph glyph;
        glyph.width = (S32)width;
        glyph.height = (S32)height;
        glyph.bearing_x = slot->bitmap_left;
        glyph.bearing_y = slot->bitmap_top;
        glyph.advance = (F32)slot->advance.x / 64.f;
        glyph.lsb_delta = slot->lsb_delta;
        glyph.rsb_delta = slot->rsb_delta;
        glyph.glyph_index = index;
        // <VulkanStorm> trace zero-advance / empty glyphs (dropdown smear).
        static const bool s_dbg_adv = getenv("VULKANSTORM_ADV_DEBUG") != nullptr;
        if (s_dbg_adv && (slot->advance.x == 0 || width == 0))
        {
            LL_INFOS("Vulkan") << "VKGLYPH ch='" << (char)ch
                               << "' adv_x=" << slot->advance.x
                               << " w=" << width << " h=" << height
                               << " size_metrics.x_ppem=" << font.face->size->metrics.x_ppem
                               << " face=" << (font.face->family_name ? font.face->family_name : "?") << LL_ENDL;
        }
        // </VulkanStorm>
        glyph.u0 = (F32)font.pen_x / ATLAS_SIZE;
        glyph.v0 = (F32)font.pen_y / ATLAS_SIZE;
        glyph.u1 = (F32)(font.pen_x + width) / ATLAS_SIZE;
        glyph.v1 = (F32)(font.pen_y + height) / ATLAS_SIZE;

        for (U32 row = 0; row < height; ++row)
        {
            const U8* src = slot->bitmap.buffer + row * slot->bitmap.pitch;
            for (U32 col = 0; col < width; ++col)
            {
                U8* dst = &font.pixels[((font.pen_y + row) * ATLAS_SIZE + font.pen_x + col) * 4];
                dst[0] = dst[1] = dst[2] = 255;
                dst[3] = src[col];
            }
        }
        font.pen_x += width + ATLAS_GAP;
        font.row_h = llmax(font.row_h, height);
        font.dirty = true;
        return &font.glyphs.emplace(ch, glyph).first->second;
    }

    bool upload(Font& font)
    {
        if (!font.dirty) return font.texture.descriptor != VK_NULL_HANDLE;
        std::string error;
        if (font.texture.descriptor == VK_NULL_HANDLE)
        {
            if (!s_context->createTexture2D(font.pixels.data(), ATLAS_SIZE,
                                             ATLAS_SIZE, font.texture, error,
                                             true))
            {
                LL_WARNS("Vulkan") << "LLVKText: atlas upload failed: "
                                    << error << LL_ENDL;
                return false;
            }
        }
        else if (!s_context->updateTexture2D(font.pixels.data(), ATLAS_SIZE,
                                              ATLAS_SIZE, font.texture, error))
        {
            LL_WARNS("Vulkan") << "LLVKText: atlas update failed: "
                                << error << LL_ENDL;
            return false;
        }
        font.dirty = false;
        return true;
    }

    F32 kern(Font& font, const Glyph* left, const Glyph* right)
    {
        if (!left || !right) return 0.f;
        FT_Vector delta{0, 0};
        if (FT_HAS_KERNING(font.face))
            FT_Get_Kerning(font.face, left->glyph_index, right->glyph_index, FT_KERNING_UNFITTED, &delta);
        F32 result = (F32)delta.x / 64.f;
        if (left->rsb_delta - right->lsb_delta >= 32) result -= 1.f;
        else if (left->rsb_delta - right->lsb_delta < -32) result += 1.f;
        return result;
    }

    F32 measure(Font& font, const LLWString& text)
    {
        F32 x = 0.f;
        const Glyph* previous = nullptr;
        for (llwchar ch : text)
        {
            const Glyph* glyph = ensureGlyph(font, ch);
            if (!glyph) continue;
            if (previous) x += kern(font, previous, glyph);
            x += glyph->advance;
            x = (F32)ll_round(x);
            previous = glyph;
        }
        return x;
    }

    void appendQuad(std::vector<F32>& xy, std::vector<F32>& uv, std::vector<F32>& rgba,
                    F32 l, F32 t, F32 r, F32 b, const Glyph& g, const LLColor4& color)
    {
        const F32 vx[6] = {l,r,r,l,r,l};
        const F32 vy[6] = {t,t,b,t,b,b};
        const F32 tu[6] = {g.u0,g.u1,g.u1,g.u0,g.u1,g.u0};
        const F32 tv[6] = {g.v0,g.v0,g.v1,g.v0,g.v1,g.v1};
        for (S32 i = 0; i < 6; ++i)
        {
            xy.push_back(vx[i]); xy.push_back(vy[i]);
            uv.push_back(tu[i]); uv.push_back(tv[i]);
            for (S32 c = 0; c < 4; ++c) rgba.push_back(color.mV[c]);
        }
    }
}

namespace LLVKText
{
    void init(LLVKContext* context)
    {
        if (s_context == context && s_library) return;
        shutdown();
        s_context = context;
        if (FT_Init_FreeType(&s_library))
        {
            s_library = nullptr;
            s_context = nullptr;
            LL_WARNS("Vulkan") << "LLVKText: FreeType initialization failed" << LL_ENDL;
        }
    }

    void shutdown()
    {
        if (s_context)
        {
            for (auto& pair : s_fonts)
            {
                Font& font = *pair.second;
                s_context->destroyTexture2D(font.texture);
            }
        }
        for (auto& pair : s_fonts)
        {
            if (pair.second->face) FT_Done_Face(pair.second->face);
            if (pair.second->fallback_face) FT_Done_Face(pair.second->fallback_face); // <VulkanStorm/>
        }
        s_fonts.clear();
        if (s_library) FT_Done_FreeType(s_library);
        s_library = nullptr;
        s_context = nullptr;
    }

    bool ready() { return s_context && s_library; }

    void prepare(const LLFontGL* fontp, const LLWString& text)
    {
        if (!ready() || !fontp || text.empty() || !LLFontGL::sDisplayFont)
            return;
        Font* font = getFont(fontp);
        if (!font) return;
        measure(*font, text);
    }

    void flushPrepared()
    {
        if (!ready()) return;
        for (auto& pair : s_fonts)
        {
            Font& font = *pair.second;
            if (font.dirty) upload(font);
        }
    }

    // <VulkanStorm>
    S32 debugGlyphCount(const LLFontGL* fontp)
    {
        if (!ready() || !fontp) return -1;
        auto found = s_fonts.find(fontp);
        if (found == s_fonts.end()) return -1;
        return (S32)found->second->glyphs.size();
    }

    F32 debugMeasureAdvance(const LLFontGL* fontp, const LLWString& text)
    {
        if (!ready() || !fontp) return -1.f;
        Font* font = getFont(fontp);
        if (!font) return -1.f;
        return measure(*font, text);
    }
    // </VulkanStorm>

    S32 render(const LLFontGL* fontp, const LLWString& source,
               F32 x, F32 y, const LLColor4& color,
               LLFontGL::HAlign halign, LLFontGL::VAlign valign,
               S32 max_pixels, bool ellipses, LLFontGL::ShadowType shadow)
    {
        if (!ready() || !fontp || source.empty() || !LLFontGL::sDisplayFont) return 0;
        Font* font = getFont(fontp);
        if (!font) return 0;

        LLWString text = source;
        const F32 sx = LLFontGL::sScaleX, sy = LLFontGL::sScaleY;
        const F32 physical_limit = max_pixels == S32_MAX ? F32_MAX : (F32)max_pixels * sx;
        F32 width = measure(*font, text);
        if (ellipses && width > physical_limit)
        {
            const LLWString dots(3, L'.');
            const F32 dot_width = measure(*font, dots);
            while (!text.empty() && measure(*font, text) + dot_width > physical_limit) text.pop_back();
            text += dots;
            width = measure(*font, text);
        }
        // Glyph discovery/upload belongs to prepareFrame(), before dynamic
        // rendering starts. Queue submission here would occur inside the
        // swapchain render pass and can make later UI text disappear.
        if (font->dirty || font->texture.descriptor == VK_NULL_HANDLE)
        {
            // <VulkanStorm> diagnostic: VULKANSTORM_TEXT_DEBUG=1 logs text
            // that is dropped because the atlas isn't uploaded/ready.
            static const bool s_dbg = getenv("VULKANSTORM_TEXT_DEBUG") != nullptr;
            static int s_dbg_n = 0;
            if (s_dbg && s_dbg_n < 24)
            {
                ++s_dbg_n;
                LL_INFOS("Vulkan") << "VKTEXT-DROP dirty=" << (font->dirty ? 1 : 0)
                                   << " desc=" << (font->texture.descriptor != VK_NULL_HANDLE ? 1 : 0)
                                   << " x=" << x << " y=" << y
                                   << " text='" << wstring_to_utf8str(text) << "'" << LL_ENDL;
            }
            // </VulkanStorm>
            return 0;
        }

        F32 px = x * sx;
        F32 py = y * sy;
        const F32 asc = (F32)font->face->size->metrics.ascender / 64.f;
        const F32 desc = -(F32)font->face->size->metrics.descender / 64.f;
        if (valign == LLFontGL::TOP) py -= ceilf(asc);
        else if (valign == LLFontGL::BOTTOM) py += ceilf(desc);
        else if (valign == LLFontGL::VCENTER) py -= ceilf((ceilf(asc) - ceilf(desc)) * .5f);
        if (halign == LLFontGL::RIGHT) px -= llmin(width, physical_limit);
        else if (halign == LLFontGL::HCENTER) px -= llmin(width, physical_limit) * .5f;

        std::vector<F32> xy, uv, rgba;
        xy.reserve(text.size() * 12); uv.reserve(text.size() * 12); rgba.reserve(text.size() * 24);
        const F32 device_h = (F32)s_context->swapchainExtent().height;
        const Glyph* previous = nullptr;
        S32 drawn = 0;
        for (llwchar ch : text)
        {
            const Glyph* glyph = ensureGlyph(*font, ch);
            if (!glyph) continue;
            if (previous) px += kern(*font, previous, glyph);
            if (px + glyph->bearing_x + glyph->width > x * sx + physical_limit) break;
            const F32 left = (F32)ll_round(px + glyph->bearing_x);
            const F32 top = (F32)ll_round(py + glyph->bearing_y);
            LLColor4 glyph_color = color;
            const F32 l = left / sx, r = (left + glyph->width) / sx;
            const F32 t = (device_h - top) / sy, b = (device_h - (top - glyph->height)) / sy;
            if (shadow != LLFontGL::NO_SHADOW)
            {
                LLColor4 sc = LLFontGL::sShadowColor;
                sc.mV[VALPHA] *= color.mV[VALPHA];
                appendQuad(xy, uv, rgba, l + 1.f / sx, t + 1.f / sy,
                           r + 1.f / sx, b + 1.f / sy, *glyph, sc);
            }
            appendQuad(xy, uv, rgba, l, t, r, b, *glyph, glyph_color);
            px += glyph->advance;
            px = (F32)ll_round(px);
            previous = glyph;
            ++drawn;
        }
        if (!xy.empty())
        {
            LLVKUI2DSink::get().setTexture(font->texture.descriptor);
            LLVKUI2DSink::get().texturedBatchPreTransformed(xy.data(), uv.data(), rgba.data(), (S32)(xy.size() / 2));
        }
        return drawn;
    }
}
