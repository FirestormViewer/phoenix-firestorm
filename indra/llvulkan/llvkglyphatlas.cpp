#include "llvkglyphatlas.h"

#include <algorithm>
#include <map>

std::optional<LLVKGlyphAtlas> LLVKGlyphAtlas::prepare(const LLVKFont::LineLayout& layout,
                                                   std::uint32_t pageSize, std::size_t byteBudget,
                                                   std::string& error)
{
    error.clear();
    if (pageSize < 4 || pageSize > 4096 || byteBudget > 64 * 1024 * 1024)
    {
        error = "Invalid native glyph atlas size or budget";
        return std::nullopt;
    }
    const std::size_t pageBytes = std::size_t(pageSize) * pageSize * 4;
    struct Cursor { std::uint32_t x = 1; std::uint32_t y = 1; std::uint32_t rowHeight = 0; };
    struct Location { std::size_t page; std::uint32_t x; std::uint32_t y; };
    std::map<const LLVKFont::Glyph*, Location> locations;
    std::vector<Cursor> cursors;
    LLVKGlyphAtlas atlas;
    atlas.mPageSize = pageSize;
    for (const auto& draw : layout.glyphs)
    {
        if (!draw.glyph)
        {
            error = "Native glyph placement has no raster owner";
            return std::nullopt;
        }
        const auto& glyph = draw.glyph->raster;
        Placement placement;
        placement.draw = draw;
        if (!glyph.width || !glyph.height)
        {
            if (!glyph.bottomUpPixels.empty())
            {
                error = "Empty native glyph has a nonempty payload";
                return std::nullopt;
            }
            atlas.mPlacements.push_back(std::move(placement));
            continue;
        }
        std::size_t channels;
        switch (glyph.encoding)
        {
            case LLVKFontFace::PixelEncoding::Coverage8: channels = 1; break;
            case LLVKFontFace::PixelEncoding::PremultipliedSrgbRgba8: channels = 4; break;
            default:
                error = "Invalid native glyph pixel encoding";
                return std::nullopt;
        }
        if (glyph.width > pageSize - 2 || glyph.height > pageSize - 2 ||
            glyph.bottomUpPixels.size() != std::size_t(glyph.width) * glyph.height * channels)
        {
            error = "Native glyph does not fit the atlas or has an invalid payload";
            return std::nullopt;
        }
        auto found = locations.find(draw.glyph.get());
        if (found == locations.end())
        {
            std::size_t selected = atlas.mPages.size();
            Cursor selectedCursor;
            for (std::size_t index = 0; index < atlas.mPages.size(); ++index)
            {
                if (atlas.mPages[index].encoding != glyph.encoding) continue;
                Cursor candidate = cursors[index];
                if (candidate.x + glyph.width + 1 > pageSize)
                {
                    candidate.x = 1;
                    candidate.y += candidate.rowHeight + 1;
                    candidate.rowHeight = 0;
                }
                if (candidate.y + glyph.height + 1 <= pageSize)
                {
                    selected = index;
                    selectedCursor = candidate;
                    break;
                }
            }
            if (selected == atlas.mPages.size())
            {
                if (atlas.mPages.size() >= byteBudget / pageBytes)
                {
                    error = "Native glyph atlas budget exhausted";
                    return std::nullopt;
                }
                Page page;
                page.encoding = glyph.encoding;
                page.rgba.resize(pageBytes, 0);
                if (channels == 1)
                {
                    for (std::size_t offset = 0; offset < pageBytes; offset += 4)
                        page.rgba[offset] = page.rgba[offset + 1] = page.rgba[offset + 2] = 255;
                }
                atlas.mPages.push_back(std::move(page));
                cursors.emplace_back();
            }
            auto& page = atlas.mPages[selected];
            for (std::uint32_t row = 0; row < glyph.height; ++row)
            {
                for (std::uint32_t column = 0; column < glyph.width; ++column)
                {
                    const auto destination = ((std::size_t(selectedCursor.y) + row) * pageSize + selectedCursor.x + column) * 4;
                    const auto source = (std::size_t(row) * glyph.width + column) * channels;
                    if (channels == 1) page.rgba[destination + 3] = glyph.bottomUpPixels[source];
                    else std::copy_n(glyph.bottomUpPixels.data() + source, 4, page.rgba.data() + destination);
                }
            }
            const Location location{selected, selectedCursor.x, selectedCursor.y};
            selectedCursor.x += glyph.width + 1;
            selectedCursor.rowHeight = std::max(selectedCursor.rowHeight, glyph.height);
            cursors[selected] = selectedCursor;
            found = locations.emplace(draw.glyph.get(), location).first;
        }
        const auto& location = found->second;
        placement.page = location.page;
        placement.leftU = static_cast<float>(location.x) / pageSize;
        placement.rightU = static_cast<float>(location.x + glyph.width) / pageSize;
        placement.bottomV = (static_cast<float>(location.y) - 0.5f) / pageSize;
        placement.topV = (static_cast<float>(location.y + glyph.height) + 0.5f) / pageSize;
        atlas.mPlacements.push_back(std::move(placement));
    }
    return atlas;
}