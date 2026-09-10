#ifndef LLVKPLAINTEXTLAYOUT_H
#define LLVKPLAINTEXTLAYOUT_H

#include "llvkfont.h"

class LLVKPlainTextLayout final
{
public:
    struct Options
    {
        std::int32_t width = 0;
        std::int32_t horizontalPadding = 0;
        std::int32_t spacingPixels = 0;
        std::int32_t fontSpacingAdjustment = 0;
        float spacingMultiple = 1.f;
        float scaleX = 1.f;
        float scaleY = 1.f;
        bool wrap = false;
        bool tabularNumbers = false;
        LLVKFont::HorizontalAlign alignment = LLVKFont::HorizontalAlign::Left;
    };
    struct Line
    {
        std::size_t begin = 0;
        std::size_t end = 0;
        std::size_t paragraph = 0;
        std::int32_t left = 0;
        std::int32_t top = 0;
        std::int32_t right = 0;
        std::int32_t bottom = 0;
    };
    static std::optional<std::vector<Line>> plain(std::u32string_view text, LLVKFont& font,
                                                const Options& options, std::string& error);
    struct Rect
    {
        std::int32_t left = 0, bottom = 0, right = 0, top = 0;
        auto operator<=>(const Rect&) const = default;
    };
    struct Document
    {
        std::vector<Line> lines;
        Rect bounds;
        Rect rectangle;
        std::int32_t fitWidth = 0;
        std::int32_t fitHeight = 0;
    };
    static std::optional<Document> document(std::u32string_view text, LLVKFont& font,
        const Options& options, std::int32_t height, std::int32_t verticalPadding,
        LLVKFont::VerticalAlign alignment, std::string& error);
};

#endif