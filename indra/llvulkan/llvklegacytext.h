#ifndef LLVKLEGACYTEXT_H
#define LLVKLEGACYTEXT_H

#include "llfontgl.h"
#include "llstring.h"
#include "v4color.h"

class LLVKContext;

namespace LLVKText
{
    void init(LLVKContext* context);
    void shutdown();
    bool ready();
    void prepare(const LLFontGL* font, const LLWString& text);
    void flushPrepared();
    S32 debugGlyphCount(const LLFontGL* font);
    F32 debugMeasureAdvance(const LLFontGL* font, const LLWString& text);
    S32 render(const LLFontGL* font, const LLWString& text,
               F32 x, F32 y, const LLColor4& color,
               LLFontGL::HAlign halign, LLFontGL::VAlign valign,
               S32 max_pixels, bool ellipses = false,
               LLFontGL::ShadowType shadow = LLFontGL::NO_SHADOW);
}

#endif