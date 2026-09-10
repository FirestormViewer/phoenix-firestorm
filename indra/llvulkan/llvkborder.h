#ifndef LLVKBORDER_H
#define LLVKBORDER_H

#include "llvkcolor.h"

struct LLVKBorder
{
    enum class Bevel { In, Out, Bright, None };
    enum class Style { Line, Texture };
    struct Params
    {
        std::int32_t thickness = 1;
        Bevel bevel = Bevel::Out;
        Style style = Style::Line;
        LLVKColor highlightLight{1,1,1,1}, highlightDark{1,1,1,1};
        LLVKColor shadowLight{0,0,0,1}, shadowDark{0,0,0,1};
    };
    Params params;
    bool keyboardFocus = false;
};

#endif