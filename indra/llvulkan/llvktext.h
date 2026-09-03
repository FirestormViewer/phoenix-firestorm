/**
 * @file llvktext.h
 * @brief GL-free FreeType atlas and text submission for the Vulkan UI path.
 */
#ifndef LLVKTEXT_H
#define LLVKTEXT_H

#include "llfontgl.h"
#include "llstring.h"
#include "v4color.h"

class LLVKContext;

namespace LLVKText
{
    void init(LLVKContext* context);
    void shutdown();
    bool ready();

    // Coordinates are viewer screen coordinates (GL-style bottom-left origin).
    // Raster dimensions are physical pixels; emission compensates for the
    // active UI scale because LLVKUI2D applies that scale at submission.
    S32 render(const LLFontGL* font, const LLWString& text,
               F32 x, F32 y, const LLColor4& color,
               LLFontGL::HAlign halign, LLFontGL::VAlign valign,
               S32 max_pixels, bool ellipses = false,
               LLFontGL::ShadowType shadow = LLFontGL::NO_SHADOW);
}

#endif
