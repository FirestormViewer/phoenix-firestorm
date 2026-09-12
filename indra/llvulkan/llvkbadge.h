#ifndef LLVKBADGE_H
#define LLVKBADGE_H

#include "llvkwidgetimage.h"
#include "llvklabel.h"
#include "llvkcolor.h"
#include <array>
#include <optional>
#include <string>

struct LLVKBadge
{
    enum Location : std::uint8_t
    {
        Center = 0, Left = 1, Right = 2, Top = 4, Bottom = 8,
        TopLeft = Top | Left, TopRight = Top | Right,
        BottomLeft = Bottom | Left, BottomRight = Bottom | Right
    };
    struct Params
    {
        std::shared_ptr<const LLVKWidgetImage> image, borderImage;
        LLVKColor imageColor{1,1,1,1}, borderColor{1,1,1,1}, labelColor{1,1,1,1};
        std::u32string label;
        std::int32_t labelOffsetHorizontal = 0;
        std::int32_t labelOffsetVertical = 0;
        Location location = TopLeft;
        std::optional<std::int32_t> offsetHorizontal;
        std::optional<std::int32_t> offsetVertical;
        std::uint32_t percentHorizontal = 0;
        std::uint32_t percentVertical = 0;
        float paddingHorizontal = 0.f;
        float paddingVertical = 0.f;
        bool equals(const Params& other) const noexcept;
    };
    Params params;
    LLVKLabel labelSource;
    std::uint64_t owner = 0;
    float horizontalCenter = 0.5f;
    float verticalCenter = 0.5f;
    bool drawAtParentTop = false;
};

#endif