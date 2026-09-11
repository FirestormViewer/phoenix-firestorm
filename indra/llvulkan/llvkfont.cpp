#include "llvkfont.h"

#include <algorithm>
#include <cmath>
#include <cwctype>

namespace
{
    bool digitCharacter(char32_t codepoint) { return codepoint >= U'0' && codepoint <= U'9'; }
    bool individualCharacter(char32_t codepoint)
    {
        return (codepoint >= 0x2e80 && codepoint <= 0x9fff) ||
               (codepoint >= 0xac00 && codepoint <= 0xd7a0) ||
               (codepoint >= 0xf900 && codepoint <= 0xfa60);
    }
    bool validTextExtent(float maxPixels, float scaleX)
    {
        return std::isfinite(maxPixels) && maxPixels >= 0.f &&
               std::isfinite(scaleX) && scaleX > 0.f;
    }
}

std::unique_ptr<LLVKFont> LLVKFont::create(const FaceSource& primary,
                                         const std::vector<FallbackSource>& fallbacks,
                                         bool monochromeEmoji, std::string& error)
{
    error.clear();
    auto font = std::unique_ptr<LLVKFont>(new LLVKFont);
    auto primaryFace = LLVKFontFace::create(primary.bytes, primary.options, error);
    if (!primaryFace) return nullptr;
    font->mFaces.push_back(std::move(primaryFace));
    font->mMonochromeEmoji = monochromeEmoji;
    font->mWeight = primary.options.weight;
    for (const auto& fallback : fallbacks)
    {
        switch (fallback.policy)
        {
            case FallbackPolicy::Unrestricted:
            case FallbackPolicy::Emoji:
            case FallbackPolicy::ColorEmoji:
            case FallbackPolicy::MonochromeEmoji: break;
            default:
                error = "Invalid native font fallback policy";
                return nullptr;
        }
        auto face = LLVKFontFace::create(fallback.source.bytes, fallback.source.options, error);
        if (!face) return nullptr;
        font->mFaces.push_back(std::move(face));
        font->mPolicies.push_back(fallback.policy);
    }
    auto missing = font->mFaces.front()->rasterize(0, false, error);
    if (!missing) return nullptr;
    font->mGlyphs.emplace(std::make_pair(char32_t(0), false),
                         std::make_shared<const Glyph>(Glyph{0, std::move(*missing)}));
    for (char32_t codepoint = 32; codepoint < 127; ++codepoint)
    {
        if (!font->glyph(codepoint, false, error)) return nullptr;
    }
    return font;
}

const LLVKFontFace::Metrics& LLVKFont::metrics() const noexcept
{
    return mFaces.front()->metrics();
}

std::size_t LLVKFont::cachedGlyphCount() const
{
    std::lock_guard lock(mMutex);
    return mGlyphs.size();
}

std::shared_ptr<const LLVKFont::Glyph> LLVKFont::glyph(char32_t codepoint,
                                                      bool requestColor, std::string& error)
{
    error.clear();
    std::lock_guard lock(mMutex);
    const auto key = std::make_pair(codepoint, requestColor);
    if (const auto found = mGlyphs.find(key); found != mGlyphs.end()) return found->second;
    std::size_t selected = 0;
    if (mFaces.front()->glyphIndex(codepoint) == 0)
    {
        if (codepoint >= 0x1f000 && codepoint < 0x20000)
        {
            for (std::size_t index = 0; index < mPolicies.size(); ++index)
            {
                const auto policy = mPolicies[index];
                const bool eligible = policy == FallbackPolicy::Emoji ||
                                      (policy == FallbackPolicy::ColorEmoji && !mMonochromeEmoji) ||
                                      (policy == FallbackPolicy::MonochromeEmoji && mMonochromeEmoji);
                if (eligible && mFaces[index + 1]->glyphIndex(codepoint))
                {
                    selected = index + 1;
                    break;
                }
            }
        }
        if (!selected)
        {
            for (std::size_t index = 0; index < mPolicies.size(); ++index)
            {
                if (mPolicies[index] == FallbackPolicy::Unrestricted && mFaces[index + 1]->glyphIndex(codepoint))
                {
                    selected = index + 1;
                    break;
                }
            }
        }
        if (!selected)
        {
            for (std::size_t index = 0; index < mPolicies.size(); ++index)
            {
                if (mPolicies[index] != FallbackPolicy::Unrestricted && mFaces[index + 1]->glyphIndex(codepoint))
                {
                    selected = index + 1;
                    break;
                }
            }
        }
    }
    auto raster = mFaces[selected]->rasterize(codepoint, requestColor, error);
    if (!raster) return nullptr;
    auto result = std::make_shared<const Glyph>(Glyph{selected, std::move(*raster)});
    mGlyphs.emplace(key, result);
    return result;
}

std::optional<LLVKFont::MeasuredRun> LLVKFont::measureRun(std::u32string_view text,
                                                        std::size_t begin, std::size_t count,
                                                        float scaleX, bool includePadding,
                                                        bool tabularNumbers, std::string& error)
{
    error.clear();
    if (begin > text.size() || !std::isfinite(scaleX) || scaleX <= 0.f)
    {
        error = "Invalid native text range or horizontal scale";
        return std::nullopt;
    }
    const std::size_t end = begin + std::min(count, text.size() - begin);
    MeasuredRun run;
    float digitAdvance = 0.f;
    if (tabularNumbers && mWeight > 0)
    {
        for (char32_t codepoint = U'0'; codepoint <= U'9'; ++codepoint)
        {
            const auto digit = glyph(codepoint, false, error);
            if (!digit) return std::nullopt;
            digitAdvance = std::max(digitAdvance, digit->raster.advanceX);
        }
    }
    const auto isDigit = [](char32_t codepoint) { return codepoint >= U'0' && codepoint <= U'9'; };
    for (std::size_t index = begin; index < end && text[index] != 0; ++index)
    {
        const auto current = glyph(text[index], false, error);
        if (!current) return std::nullopt;
        const auto& raster = current->raster;
        const bool tabular = digitAdvance > 0.f && isDigit(text[index]);
        const float advance = tabular ? digitAdvance : raster.advanceX;
        run.glyphs.push_back({index, run.advancePixels,
                             tabular ? (digitAdvance - raster.advanceX) * 0.5f : 0.f, current});
        if (includePadding)
        {
            run.trailingPaddingPixels = std::max({0.f, run.trailingPaddingPixels - advance,
                static_cast<float>(raster.width) + raster.bearingX - advance});
        }
        run.advancePixels += advance;
        if (index + 1 < end && text[index + 1] && text[index + 1] < 255 &&
            !(tabularNumbers && mWeight > 0 && (isDigit(text[index]) || isDigit(text[index + 1]))))
        {
            const auto next = glyph(text[index + 1], false, error);
            if (!next) return std::nullopt;
            const auto kern = pairKerning(*current, *next, tabularNumbers, error);
            if (!kern) return std::nullopt;
            run.advancePixels += *kern;
        }
        run.advancePixels = std::floor(run.advancePixels + 0.5f);
    }
    run.width = (run.advancePixels + run.trailingPaddingPixels) / scaleX;
    if (!std::isfinite(run.width))
    {
        error = "Native text measurement overflow";
        return std::nullopt;
    }
    return run;
}

std::optional<LLVKFont::LineLayout> LLVKFont::layoutLine(std::u32string_view text,
                                                       std::size_t begin, std::size_t count,
                                                       const LineOptions& options,
                                                       std::string& error)
{
    error.clear();
    const auto validCoordinate = [](float value)
    {
        return std::isfinite(value) && static_cast<double>(value) >= INT32_MIN &&
               static_cast<double>(value) <= INT32_MAX;
    };
    const auto roundPixel = [](float value) { return std::floor(value + 0.5f); };
    const auto isDigit = [](char32_t value) { return value >= U'0' && value <= U'9'; };
    if (begin > text.size() || options.maxPixels < 0 || !std::isfinite(options.scaleX) ||
        !std::isfinite(options.scaleY) || options.scaleX <= 0.f || options.scaleY <= 0.f)
    {
        error = "Invalid native line range, scale or width";
        return std::nullopt;
    }
    const float originX = std::floor(options.originX * options.scaleX);
    const float originY = std::floor(options.originY * options.scaleY);
    float penX = options.x * options.scaleX + originX;
    float penY = options.y * options.scaleY + originY;
    if (!validCoordinate(originX) || !validCoordinate(originY) || !validCoordinate(penX) || !validCoordinate(penY))
    {
        error = "Native line coordinates outside supported range";
        return std::nullopt;
    }
    std::int32_t available = INT32_MAX;
    if (options.maxPixels != INT32_MAX)
    {
        const float scaled = std::ceil(static_cast<float>(options.maxPixels) * options.scaleX);
        if (!validCoordinate(scaled))
        {
            error = "Native line width overflow";
            return std::nullopt;
        }
        available = static_cast<std::int32_t>(scaled);
    }
    const std::size_t length = std::min(count, text.size() - begin);
    LineLayout layout;
    switch (options.vertical)
    {
        case VerticalAlign::Baseline: break;
        case VerticalAlign::Top: penY -= std::ceil(metrics().ascender); break;
        case VerticalAlign::Bottom: penY += std::ceil(metrics().descender); break;
        case VerticalAlign::Center:
            penY -= std::ceil((std::ceil(metrics().ascender) - std::ceil(metrics().descender)) / 2.f);
            break;
        default:
            error = "Invalid vertical alignment";
            return std::nullopt;
    }
    if (options.horizontal != HorizontalAlign::Left || options.ellipses)
    {
        auto measured = measureRun(text, begin, length, options.scaleX, true, options.tabularNumbers, error);
        if (!measured) return std::nullopt;
        const float measuredPixels = roundPixel(measured->width * options.scaleX);
        if (!validCoordinate(measuredPixels))
        {
            error = "Native line measurement overflow";
            return std::nullopt;
        }
        const auto offset = std::min(available, static_cast<std::int32_t>(measuredPixels));
        switch (options.horizontal)
        {
            case HorizontalAlign::Left: break;
            case HorizontalAlign::Right: penX -= offset; break;
            case HorizontalAlign::Center: penX -= offset / 2; break;
            default:
                error = "Invalid horizontal alignment";
                return std::nullopt;
        }
        if (options.ellipses && measuredPixels > available)
        {
            auto dots = measureRun(U"....", 0, 4, options.scaleX, true, options.tabularNumbers, error);
            if (!dots) return std::nullopt;
            const float reservation = roundPixel(dots->width);
            if (!validCoordinate(reservation))
            {
                error = "Native ellipsis reservation overflow";
                return std::nullopt;
            }
            available = std::max(0, available - static_cast<std::int32_t>(reservation));
            layout.ellipsized = true;
        }
    }
    layout.startPixelX = roundPixel(penX);
    layout.baselinePixelY = penY;
    float digitAdvance = 0.f;
    if (options.tabularNumbers && mWeight > 0)
    {
        for (char32_t digit = U'0'; digit <= U'9'; ++digit)
        {
            auto value = glyph(digit, options.requestColor, error);
            if (!value) return std::nullopt;
            digitAdvance = std::max(digitAdvance, value->raster.advanceX);
        }
    }
    for (std::size_t index = begin; index < begin + length; ++index)
    {
        auto current = glyph(text[index], options.requestColor, error);
        if (!current) return std::nullopt;
        const auto& raster = current->raster;
        if (layout.startPixelX + available < penX + raster.bearingX + raster.width) break;
        const bool tabular = digitAdvance > 0.f && isDigit(text[index]);
        const float digitOffset = tabular ? (digitAdvance - raster.advanceX) * 0.5f : 0.f;
        const float left = roundPixel(penX + raster.bearingX + digitOffset);
        const float top = roundPixel(penY + raster.bearingY);
        if (!validCoordinate(left) || !validCoordinate(top) ||
            !validCoordinate(left + raster.width) || !validCoordinate(top - raster.height))
        {
            error = "Native glyph rectangle overflow";
            return std::nullopt;
        }
        layout.glyphs.push_back({index, false, left, top, left + raster.width, top - raster.height, current});
        ++layout.sourceCharacters;
        penX += tabular ? digitAdvance : raster.advanceX;
        penY += raster.advanceY;
        if (index + 1 < text.size() && text[index + 1] && text[index + 1] < 255 &&
            !(options.tabularNumbers && mWeight > 0 && (isDigit(text[index]) || isDigit(text[index + 1]))))
        {
            auto next = glyph(text[index + 1], options.requestColor, error);
            if (!next) return std::nullopt;
            auto kern = pairKerning(*current, *next, options.tabularNumbers, error);
            if (!kern) return std::nullopt;
            penX += *kern;
        }
        penX = roundPixel(penX);
    }
    if (!validCoordinate(penX) || !validCoordinate(penY))
    {
        error = "Native line advance overflow";
        return std::nullopt;
    }
    layout.truncated = layout.sourceCharacters < length;
    layout.endPixelX = penX;
    layout.endPixelY = penY;
    layout.rightX = (penX - originX) / options.scaleX;
    if (layout.ellipsized)
    {
        LineOptions suffixOptions = options;
        suffixOptions.x = layout.rightX;
        suffixOptions.horizontal = HorizontalAlign::Left;
        suffixOptions.ellipses = false;
        auto suffix = layoutLine(U"...", 0, 3, suffixOptions, error);
        if (!suffix) return std::nullopt;
        for (auto& placement : suffix->glyphs)
        {
            placement.sourceIndex = begin + layout.sourceCharacters;
            placement.ellipsis = true;
            layout.glyphs.push_back(std::move(placement));
        }
        layout.rightX = suffix->rightX;
    }
    return layout;
}

std::optional<float> LLVKFont::digitWidth(bool tabularNumbers, std::string& error)
{
    float width = 0.f;
    if (tabularNumbers && mWeight > 0)
    {
        for (char32_t digit = U'0'; digit <= U'9'; ++digit)
        {
            auto value = glyph(digit, false, error);
            if (!value) return std::nullopt;
            width = std::max(width, value->raster.advanceX);
        }
    }
    return width;
}

std::optional<float> LLVKFont::pairKerning(const Glyph& left, const Glyph& right,
                                         bool tabularNumbers, std::string& error)
{
    error.clear();
    if (tabularNumbers && mWeight > 0 &&
        (digitCharacter(left.raster.codepoint) || digitCharacter(right.raster.codepoint))) return 0.f;
    if (left.faceIndex != right.faceIndex)
    {
        const auto difference = static_cast<std::int64_t>(left.raster.rsbDelta) - right.raster.lsbDelta;
        return difference > 32 ? -1.f : difference < -31 ? 1.f : 0.f;
    }
    return mFaces.at(left.faceIndex)->kerning(left.raster.requestedIndex, right.raster.requestedIndex,
                                   left.raster.rsbDelta, right.raster.lsbDelta, error);
}

std::optional<std::size_t> LLVKFont::fitCharacters(std::u32string_view text, float maxPixels,
    std::size_t count, float scaleX, Wrap wrap, bool tabularNumbers, std::string& error)
{
    error.clear();
    if (!validTextExtent(maxPixels, scaleX) ||
        (wrap != Wrap::Anywhere && wrap != Wrap::WordsOnly && wrap != Wrap::WordsWhenPossible))
    {
        error = "Invalid native wrapping extent or policy";
        return std::nullopt;
    }
    auto digits = digitWidth(tabularNumbers, error);
    if (!digits) return std::nullopt;
    const float limit = std::min(maxPixels * scaleX, static_cast<float>(INT32_MAX));
    const auto end = std::min(count, text.size());
    std::size_t wordStart = 0;
    bool inWord = false;
    float pen = 0.f;
    float padding = 0.f;
    for (std::size_t index = 0; index < end; ++index)
    {
        const auto codepoint = text[index];
        if (!codepoint) return index;
        const bool space = std::iswspace(static_cast<wint_t>(codepoint)) != 0;
        const bool individual = individualCharacter(codepoint);
        if (inWord)
        {
            if (space && codepoint != 0xa0) inWord = false;
            if (individual)
            {
                if (index + 1 < text.size() && std::iswpunct(static_cast<wint_t>(text[index + 1]))) inWord = true;
                else { inWord = false; wordStart = index; }
            }
        }
        else
        {
            wordStart = index;
            if (!space || !individual) inWord = true;
        }
        auto current = glyph(codepoint, false, error);
        if (!current) return std::nullopt;
        const auto& raster = current->raster;
        const float advance = *digits > 0.f && digitCharacter(codepoint) ? *digits : raster.advanceX;
        padding = std::max({0.f, padding - advance, static_cast<float>(raster.width) + raster.bearingX - advance});
        pen += advance;
        if (limit < pen + padding)
        {
            if (wrap == Wrap::WordsOnly || (wrap == Wrap::WordsWhenPossible && wordStart != 0)) return wordStart;
            return index;
        }
        if (index + 1 < end && text[index + 1] && text[index + 1] < 255)
        {
            auto next = glyph(text[index + 1], false, error);
            if (!next) return std::nullopt;
            auto kern = pairKerning(*current, *next, tabularNumbers, error);
            if (!kern) return std::nullopt;
            pen += *kern;
        }
        pen = std::floor(pen + 0.5f);
    }
    return end;
}

std::optional<std::size_t> LLVKFont::hitTest(std::u32string_view text, std::size_t begin,
    float targetX, float maxPixels, std::size_t count, float scaleX, bool nearest,
    bool tabularNumbers, std::string& error)
{
    error.clear();
    if (!validTextExtent(maxPixels, scaleX) || begin > text.size() ||
        !std::isfinite(targetX) || !std::isfinite(targetX * scaleX))
    {
        error = "Invalid native text hit-test inputs";
        return std::nullopt;
    }
    if (!count || text.empty() || !text.front()) return 0;
    const auto end = begin + std::min(count - 1, text.size() - begin);
    auto digits = digitWidth(tabularNumbers, error);
    if (!digits) return std::nullopt;
    const float target = targetX * scaleX;
    const float limit = maxPixels * scaleX;
    float pen = 0.f;
    std::size_t index = begin;
    for (; index < end && text[index]; ++index)
    {
        auto current = glyph(text[index], false, error);
        if (!current) return std::nullopt;
        const float advance = *digits > 0.f && digitCharacter(text[index]) ? *digits : current->raster.advanceX;
        if (target < pen + advance * (nearest ? 0.5f : 1.f) || limit < pen + advance) break;
        pen += advance;
        if (index + 1 < end && text[index + 1])
        {
            auto next = glyph(text[index + 1], false, error);
            if (!next) return std::nullopt;
            auto kern = pairKerning(*current, *next, tabularNumbers, error);
            if (!kern) return std::nullopt;
            pen += *kern;
        }
        pen = std::floor(pen + 0.5f);
    }
    return index - begin;
}

std::optional<std::size_t> LLVKFont::firstVisible(std::u32string_view text, std::size_t start,
    float maxPixels, std::size_t count, float scaleX, bool tabularNumbers, std::string& error)
{
    error.clear();
    if (!validTextExtent(maxPixels, scaleX) || (!text.empty() && start >= text.size()))
    {
        error = "Invalid native backward text range";
        return std::nullopt;
    }
    if (!count || text.empty() || !text.front()) return 0;
    auto digits = digitWidth(tabularNumbers, error);
    if (!digits) return std::nullopt;
    const float limit = maxPixels * scaleX;
    float width = 0.f;
    std::size_t drawable = 0;
    for (std::size_t remaining = start + 1; remaining > 0; --remaining)
    {
        const auto index = remaining - 1;
        auto current = glyph(text[index], false, error);
        if (!current) return std::nullopt;
        const auto& raster = current->raster;
        const float advance = index == start ? static_cast<float>(raster.width) + raster.bearingX :
                              *digits > 0.f && digitCharacter(text[index]) ? *digits : raster.advanceX;
        if (limit < width + advance) break;
        width += advance;
        if (++drawable >= count) break;
        if (index > 0)
        {
            auto previous = glyph(text[index - 1], false, error);
            if (!previous) return std::nullopt;
            auto kern = pairKerning(*previous, *current, tabularNumbers, error);
            if (!kern) return std::nullopt;
            width += *kern;
        }
        width = std::floor(width + 0.5f);
    }
    return drawable ? start + 1 - drawable : start;
}