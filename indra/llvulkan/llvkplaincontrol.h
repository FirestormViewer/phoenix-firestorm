#ifndef LLVKPLAINCONTROL_H
#define LLVKPLAINCONTROL_H

#include "llvklabel.h"
#include "llvkcolor.h"
#include "llvkplaintextlayout.h"

struct LLVKPlainControl
{
    struct Params
    {
        LLVKPlainTextLayout::Options layout;
        LLVKFont::VerticalAlign vertical = LLVKFont::VerticalAlign::Top;
        std::int32_t verticalPadding = 0;
        std::size_t maximumBytes = 4096;
        std::optional<bool> readOnly;
        bool trackEnd = false;
        LLVKColor textColor{1,1,1,1};
        LLVKColor readOnlyColor{1,1,1,1};
        std::function<void(std::uint64_t)> clicked;
        bool showHandCursor = true;
        bool parseUrls = false;
        LLVKColor tentativeColor{1,1,1,1};
        LLVKColor backgroundColor{0,0,0,1};
    };
    Params params;
    LLVKLabel source;
    std::u32string text;
    std::string value;
    std::uint64_t document = 0;
    std::size_t cursor = 0;
    std::size_t selectionStart = 0;
    std::size_t selectionEnd = 0;
    bool readOnly = false;
    bool selecting = false;
    std::optional<LLVKPlainTextLayout::Document> layout;
    std::uint64_t textGeneration = 0;
};

#endif