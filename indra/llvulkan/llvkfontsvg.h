#ifndef LL_LLVKFONTSVG_H
#define LL_LLVKFONTSVG_H

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OTSVG_H

namespace LLVKFontSvg
{
    const SVG_RendererHooks& hooks() noexcept;
}

FT_Error llvkInstallSvgHooks(FT_Library library);

#endif