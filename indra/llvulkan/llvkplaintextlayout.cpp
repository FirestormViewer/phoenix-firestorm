#include "llvkplaintextlayout.h"

#include <algorithm>
#include <cmath>

std::optional<std::vector<LLVKPlainTextLayout::Line>> LLVKPlainTextLayout::plain(std::u32string_view text,
    LLVKFont& font, const Options& options, std::string& error)
{
    error.clear();
    if (text.size() > 1024 * 1024 || text.find(U'\0') != std::u32string_view::npos ||
        options.width < 0 || !std::isfinite(options.spacingMultiple) || options.spacingMultiple < 0.f ||
        !std::isfinite(options.scaleX) || options.scaleX <= 0.f ||
        !std::isfinite(options.scaleY) || options.scaleY <= 0.f)
    { error = "Invalid native plain text layout input"; return std::nullopt; }
    if (options.alignment != LLVKFont::HorizontalAlign::Left && options.alignment != LLVKFont::HorizontalAlign::Center &&
        options.alignment != LLVKFont::HorizontalAlign::Right)
    { error = "Invalid native text alignment"; return std::nullopt; }
    const double heightValue = std::ceil(font.metrics().ascender/options.scaleY) + std::ceil(font.metrics().descender/options.scaleY);
    if (!std::isfinite(heightValue) || heightValue < 0 || heightValue > INT32_MAX)
    { error = "Native text line height overflow"; return std::nullopt; }
    const auto height = static_cast<std::int32_t>(heightValue);
    const double stepValue = std::floor(static_cast<float>(height)*options.spacingMultiple + 0.5f) +
                             double(options.spacingPixels) + options.fontSpacingAdjustment;
    if (!std::isfinite(stepValue) || stepValue < INT32_MIN || stepValue > INT32_MAX)
    { error = "Native text line spacing overflow"; return std::nullopt; }
    const auto step = static_cast<std::int64_t>(stepValue);
    const auto available = std::int64_t(options.width)-options.horizontalPadding;
    if (available < INT32_MIN || available > INT32_MAX)
    { error = "Native text available width overflow"; return std::nullopt; }
    std::vector<Line> lines;
    std::size_t begin = 0;
    std::size_t paragraph = 0;
    std::int64_t top = 0;
    auto newline = text.find(U'\n');
    for (;;)
    {
        const auto segmentEnd = newline == std::u32string_view::npos ? text.size() : newline;
        const auto remaining = segmentEnd-begin;
        std::size_t count = remaining;
        if (options.wrap && remaining)
        {
            auto fitted = font.fitCharacters(text.substr(begin),static_cast<float>(std::max<std::int64_t>(0,available)),
                remaining,options.scaleX,LLVKFont::Wrap::WordsWhenPossible,options.tabularNumbers,error);
            if (!fitted) return std::nullopt;
            count = std::max<std::size_t>(1,*fitted);
        }
        auto measured = font.measureRun(text,begin,count,options.scaleX,true,options.tabularNumbers,error);
        if (!measured) return std::nullopt;
        const float remainingPixels = static_cast<float>(available)-measured->width;
        const double widthValue = std::ceil(static_cast<float>(available)-remainingPixels);
        if (!std::isfinite(widthValue) || widthValue < INT32_MIN || widthValue > INT32_MAX)
        { error = "Native text line width overflow"; return std::nullopt; }
        const auto width = static_cast<std::int64_t>(widthValue);
        std::int64_t left = options.horizontalPadding;
        if (options.alignment == LLVKFont::HorizontalAlign::Center)
            left += std::max<std::int64_t>(0,(std::int64_t(options.width)-width-options.horizontalPadding)/2);
        else if (options.alignment == LLVKFont::HorizontalAlign::Right)
            left = std::max<std::int64_t>(left,std::int64_t(options.width)-width-1);
        if (left < INT32_MIN || left > INT32_MAX || left+width < INT32_MIN || left+width > INT32_MAX ||
            top < INT32_MIN || top > INT32_MAX || top-height < INT32_MIN || top-height > INT32_MAX)
        { error = "Native text line rectangle overflow"; return std::nullopt; }
        const bool completeSegment = count == remaining;
        const auto end = begin+count+(completeSegment ? 1 : 0);
        lines.push_back({begin,end,paragraph,static_cast<std::int32_t>(left),static_cast<std::int32_t>(top),
                         static_cast<std::int32_t>(left+width),static_cast<std::int32_t>(top-height)});
        if (completeSegment && newline == std::u32string_view::npos) break;
        begin = end;
        if (completeSegment)
        {
            ++paragraph;
            newline = text.find(U'\n',begin);
        }
        top -= step;
    }
    return lines;
}

std::optional<LLVKPlainTextLayout::Document> LLVKPlainTextLayout::document(std::u32string_view text,
    LLVKFont& font, const Options& options, std::int32_t height, std::int32_t verticalPadding,
    LLVKFont::VerticalAlign alignment, std::string& error)
{
    error.clear();
    if (height < 0 || (alignment != LLVKFont::VerticalAlign::Top && alignment != LLVKFont::VerticalAlign::Center &&
        alignment != LLVKFont::VerticalAlign::Bottom && alignment != LLVKFont::VerticalAlign::Baseline))
    { error = "Invalid native text document dimensions or alignment"; return std::nullopt; }
    auto lines = plain(text,font,options,error);
    if (!lines) return std::nullopt;
    std::int64_t left = lines->front().left, right = lines->front().right;
    std::int64_t bottom = lines->front().bottom, top = lines->front().top;
    for (const auto& line : *lines)
    {
        left = std::min<std::int64_t>(left,line.left);
        right = std::max<std::int64_t>(right,line.right);
        bottom = std::min<std::int64_t>(bottom,line.bottom);
        top = std::max<std::int64_t>(top,line.top);
    }
    top += verticalPadding;
    std::int64_t translation = 0;
    if (alignment == LLVKFont::VerticalAlign::Top) translation = std::max(height-top,-bottom);
    else if (alignment == LLVKFont::VerticalAlign::Center) translation = (std::max(height-top,-bottom)-bottom)/2;
    else if (alignment == LLVKFont::VerticalAlign::Bottom) translation = -bottom;
    bottom += translation;
    top += translation;
    auto documentBottom = std::min<std::int64_t>(0,bottom);
    auto documentTop = std::max<std::int64_t>(height,top-bottom)+documentBottom;
    if (alignment == LLVKFont::VerticalAlign::Top)
    {
        documentBottom += height-documentTop;
        documentTop = height;
    }
    else if (alignment == LLVKFont::VerticalAlign::Center)
    {
        const auto shift = (height-documentTop)/2;
        documentTop += shift;
        documentBottom += shift;
    }
    const auto fitWidth = right-left+2*std::int64_t(options.horizontalPadding)+1;
    const auto fitHeight = top-bottom+2*std::int64_t(verticalPadding);
    for (const auto value : {left,right,bottom,top,documentBottom,documentTop,fitWidth,fitHeight})
        if (value < INT32_MIN || value > INT32_MAX)
        { error = "Native text document rectangle overflow"; return std::nullopt; }
    for (auto& line : *lines)
    {
        const auto lineTop = line.top+translation;
        const auto lineBottom = line.bottom+translation;
        if (lineTop < INT32_MIN || lineTop > INT32_MAX || lineBottom < INT32_MIN || lineBottom > INT32_MAX)
        { error = "Native text document line translation overflow"; return std::nullopt; }
        line.top = static_cast<std::int32_t>(lineTop);
        line.bottom = static_cast<std::int32_t>(lineBottom);
    }
    Document result;
    result.lines = std::move(*lines);
    result.bounds = {static_cast<std::int32_t>(left),static_cast<std::int32_t>(bottom),
                     static_cast<std::int32_t>(right),static_cast<std::int32_t>(top)};
    result.rectangle = {0,static_cast<std::int32_t>(documentBottom),options.width,static_cast<std::int32_t>(documentTop)};
    result.fitWidth = static_cast<std::int32_t>(fitWidth);
    result.fitHeight = static_cast<std::int32_t>(fitHeight);
    return result;
}