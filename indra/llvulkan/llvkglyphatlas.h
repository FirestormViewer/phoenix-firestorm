#ifndef LLVKGLYPHATLAS_H
#define LLVKGLYPHATLAS_H

#include "llvkfont.h"

class LLVKGlyphAtlas final
{
public:
    struct Page
    {
        LLVKFontFace::PixelEncoding encoding = LLVKFontFace::PixelEncoding::Coverage8;
        std::vector<std::uint8_t> rgba;
    };
    struct Placement
    {
        LLVKFont::DrawGlyph draw;
        std::optional<std::size_t> page;
        float leftU = 0.f;
        float rightU = 0.f;
        float bottomV = 0.f;
        float topV = 0.f;
    };
    static std::optional<LLVKGlyphAtlas> prepare(const LLVKFont::LineLayout& layout,
                                                std::uint32_t pageSize, std::size_t byteBudget,
                                                std::string& error);
    std::uint32_t pageSize() const noexcept { return mPageSize; }
    std::span<const Page> pages() const noexcept { return mPages; }
    std::span<const Placement> placements() const noexcept { return mPlacements; }
    bool updateLayout(const LLVKFont::LineLayout& layout);
private:
    std::uint32_t mPageSize = 0;
    std::vector<Page> mPages;
    std::vector<Placement> mPlacements;
};

#endif