#include "llvkstyledtext.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include "llsd.h"
#include "llstring.h"
#include "lluri.h"
#include "lluriparser.h"
#include <boost/regex.hpp>

std::optional<LLVKWebText> LLVKWebText::parse(std::string_view markup, std::string& error)
{
    error.clear();
    if (markup.size() > 65536 || markup.find('\0') != markup.npos)
    { error = "Native web text exceeds its input budget or contains NUL"; return std::nullopt; }
    static const boost::regex pattern(
        "<nolink>.*?</nolink>|\\[(?:https?|ftp|secondlife|hop)://[^\\s]+[ \\t]+[^\\]]+\\]|(?:https?|ftp)://([^\\s/?\\.#]+\\.?)+\\.\\w+(:\\d+)?(/[^\\s]*)?",
        boost::regex::perl|boost::regex::icase);
    static const boost::regex webLabel("(?:https?|ftp)://|www\\.",boost::regex::perl|boost::regex::icase);
    static const std::string allowed = []
    {
        std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~!$?&()*+,@:;=/%#";
        std::sort(chars.begin(),chars.end());
        return chars;
    }();
    LLVKWebText result;
    const auto append = [&](std::string_view value,const std::string& target = {},bool query = false)
    {
        const auto wide = utf8str_to_wstring(std::string(value));
        const auto begin = result.text.size();
        result.text.append(wide.begin(),wide.end());
        if (!target.empty() && begin != result.text.size()) result.links.push_back({begin,result.text.size(),target,query});
    };
    const std::string source(markup);
    auto begin = source.cbegin();
    const auto end = source.cend();
    boost::match_results<std::string::const_iterator> match;
    try
    {
        while (boost::regex_search(begin,end,match,pattern))
        {
            if (result.links.size() >= 1024) { error = "Native web text link budget exceeded"; return std::nullopt; }
            append(std::string(begin,match[0].first));
            std::string found(match[0].first,match[0].second);
            begin = match[0].second;
            if (found.front() == '<') { append(std::string_view(found).substr(8,found.size()-17)); continue; }
            if (found.front() == '[')
            {
                const auto split = found.find_first_of(" \t");
                const auto labelBegin = found.find_first_not_of(" \t",split);
                const auto target = LLURI::escape(found.substr(1,split-1),allowed,true);
                auto label = LLURI::unescape(found.substr(labelBegin,found.size()-labelBegin-1));
                if (boost::regex_search(label,webLabel)) label = target;
                append(label,target);
                continue;
            }
            std::string trailing;
            while (!found.empty() && (found.back()=='.' || found.back()==','))
            { trailing.insert(trailing.begin(),found.back()); found.pop_back(); }
            const auto target = LLURI::escape(found,allowed,true);
            LLUriParser normalized(target);
            if (!normalized.normalize()) { append(found); append(trailing); continue; }
            normalized.extractParts();
            std::string label;
            normalized.glueFirst(label);
            LLUriParser original(target);
            original.extractParts();
            std::string host;
            original.glueFirst(host,false);
            const auto offset = target.find(host);
            append(LLURI::unescape(label),target);
            if (offset != std::string::npos) append(LLURI::unescape(target.substr(offset+host.size())),target,true);
            append(trailing);
        }
        append(std::string(begin,end));
    }
    catch (const std::runtime_error& exception)
    { error = "Native web text parsing failed: "+std::string(exception.what()); return std::nullopt; }
    return result;
}

std::optional<LLVKStyledTextSegment> LLVKStyledTextSegment::create(const Params& params,
    std::u32string_view text, std::string& error)
{
    error.clear();
    if (!params.font || text.size() > 1024*1024 || params.begin > text.size() || params.end <= params.begin ||
        params.end > text.size()+1 || !std::isfinite(params.scaleX) || params.scaleX <= 0 ||
        !std::isfinite(params.scaleY) || params.scaleY <= 0 ||
        (params.kind != Kind::Normal && params.kind != Kind::LineBreak && params.kind != Kind::Image && params.kind != Kind::InlineWidget) ||
        text.find(U'\0') != std::u32string_view::npos)
    { error = "Invalid native styled text segment"; return std::nullopt; }
    if (params.kind == Kind::LineBreak &&
        (params.end != params.begin+1 || params.begin == text.size() || text[params.begin] != U'\n'))
    { error = "Native line-break segment requires one newline"; return std::nullopt; }
    if (params.kind == Kind::Image && (params.end != params.begin+1 || params.end > text.size()))
    { error = "Native image segment requires one placeholder character"; return std::nullopt; }
    if (params.kind == Kind::InlineWidget)
    {
        const auto width = std::int64_t(params.widgetWidth)+params.leftPad+params.rightPad;
        const auto height = std::int64_t(params.widgetHeight)+params.bottomPad+params.topPad;
        if (!params.widget || params.widgetWidth < 0 || params.widgetHeight < 0 ||
            width < 0 || width > INT32_MAX || height < 0 || height > INT32_MAX || params.end > text.size())
        { error = "Invalid native inline widget identity, extent or padding"; return std::nullopt; }
    }
    const auto& metrics = params.font->metrics();
    const double height = std::ceil(metrics.ascender/params.scaleY)+std::ceil(metrics.descender/params.scaleY);
    if (!std::isfinite(height) || height < 0 || height > INT32_MAX)
    { error = "Native styled text height overflows"; return std::nullopt; }
    LLVKStyledTextSegment segment;
    segment.mParams = params;
    segment.mHeight = static_cast<std::int32_t>(height);
    return segment;
}

bool LLVKStyledTextSegment::validRange(std::u32string_view text, std::size_t offset, std::size_t count, std::string& error) const
{
    error.clear();
    if (text.size() > 1024*1024 || mParams.end > text.size()+1 || offset > mParams.end-mParams.begin ||
        count > mParams.end-mParams.begin-offset || mParams.begin+offset > text.size())
    { error = "Native styled text range is invalid or stale"; return false; }
    return true;
}

std::optional<LLVKStyledTextSegment::Dimensions> LLVKStyledTextSegment::measure(std::u32string_view text,
    std::size_t offset, std::size_t count, std::string& error) const
{
    if (!validRange(text,offset,count,error)) return std::nullopt;
    if (mParams.kind == Kind::LineBreak) return Dimensions{0.f,mHeight,true};
    if (mParams.kind == Kind::Image)
    {
        if (!count || !mParams.image) return Dimensions{0.f,mHeight,false};
        return Dimensions{float(mParams.image->width()+3),std::max(mHeight,static_cast<std::int32_t>(mParams.image->height()+3)),false};
    }
    if (mParams.kind == Kind::InlineWidget)
    {
        if (!offset && !count) return Dimensions{0.f,mParams.forceNewLine ? mHeight : 0,mParams.forceNewLine};
        return Dimensions{float(std::int64_t(mParams.widgetWidth)+mParams.leftPad+mParams.rightPad),
            static_cast<std::int32_t>(std::int64_t(mParams.widgetHeight)+mParams.topPad+mParams.bottomPad),false};
    }
    if (!count) return Dimensions{};
    const auto width = mParams.font->measureRun(text,mParams.begin+offset,count,mParams.scaleX,true,mParams.tabularNumbers,error);
    if (!width) return std::nullopt;
    return Dimensions{width->width,mHeight,false};
}

std::optional<std::size_t> LLVKStyledTextSegment::fit(std::u32string_view text, std::int32_t pixels,
    std::size_t offset, std::size_t lineOffset, std::size_t maximum, std::string& error, std::int64_t lineIndex) const
{
    if (!validRange(text,offset,0,error)) return std::nullopt;
    if (mParams.kind == Kind::LineBreak) return 1;
    if (mParams.kind == Kind::Image)
        return !mParams.image || !lineOffset || std::int64_t(pixels) > std::int64_t(mParams.image->width())+3 ? 1 : 0;
    if (mParams.kind == Kind::InlineWidget)
    {
        if (mParams.forceNewLine && !lineIndex) return 0;
        const auto width = std::int64_t(mParams.widgetWidth)+mParams.leftPad+mParams.rightPad;
        return lineOffset && pixels < width ? 0 : mParams.end-mParams.begin;
    }
    const auto begin = mParams.begin+offset;
    maximum = std::min(maximum,mParams.end-begin);
    const auto available = std::max<std::int64_t>(0,std::int64_t(pixels)-(mParams.image ? mParams.image->width() : 0));
    const auto fitted = mParams.font->fitCharacters(text.substr(begin),static_cast<float>(available),maximum,mParams.scaleX,
        lineOffset ? LLVKFont::Wrap::WordsOnly : LLVKFont::Wrap::WordsWhenPossible,mParams.tabularNumbers,error);
    if (!fitted) return std::nullopt;
    auto count = *fitted;
    if (!count && !lineOffset && maximum) count = 1;
    if (begin+count < mParams.end && begin+count >= text.size()) ++count;
    return count;
}

std::optional<std::size_t> LLVKStyledTextSegment::hit(std::u32string_view text, std::int32_t pixels,
    std::size_t offset, std::size_t count, bool nearest, std::string& error) const
{
    if (!validRange(text,offset,count,error)) return std::nullopt;
    if (mParams.kind != Kind::Normal) { error = "Native line-break hit testing requires line placement"; return std::nullopt; }
    return mParams.font->hitTest(text,mParams.begin+offset,float(pixels),std::numeric_limits<float>::max()/mParams.scaleX,
        count,mParams.scaleX,nearest,mParams.tabularNumbers,error);
}

std::optional<std::vector<LLVKPlainTextLayout::Line>> LLVKStyledTextSegment::reflow(std::u32string_view text,
    std::span<const LLVKStyledTextSegment> segments, const LLVKPlainTextLayout::Options& options, std::string& error)
{
    error.clear();
    if (text.size() > 1024*1024 || segments.empty() || segments.size() > 10000 || options.width < 0 ||
        !std::isfinite(options.spacingMultiple) || options.spacingMultiple < 0.f ||
        (options.alignment != LLVKFont::HorizontalAlign::Left && options.alignment != LLVKFont::HorizontalAlign::Center &&
         options.alignment != LLVKFont::HorizontalAlign::Right))
    { error = "Invalid native styled reflow inputs"; return std::nullopt; }
    std::size_t covered = 0;
    for (const auto& segment : segments)
    {
        if (segment.mParams.begin != covered || !segment.validRange(text,0,segment.mParams.end-segment.mParams.begin,error))
        { error = "Native styled segments must cover text contiguously"; return std::nullopt; }
        covered = segment.mParams.end;
    }
    if (covered != text.size()+1 || segments.back().mParams.kind != Kind::Normal)
    { error = "Native styled document requires a terminal normal EOF segment"; return std::nullopt; }
    const auto availableValue = std::int64_t(options.width)-options.horizontalPadding;
    if (availableValue < INT32_MIN || availableValue > INT32_MAX)
    { error = "Native styled reflow width overflows"; return std::nullopt; }
    const float available = static_cast<float>(availableValue);
    float remaining = available;
    std::int64_t top = 0, paragraph = 0, segmentLine = 1;
    std::int32_t height = 0;
    std::size_t segmentIndex = 0, offset = 0, lineBegin = 0, iterations = 0;
    std::vector<LLVKPlainTextLayout::Line> lines;
    while (segmentIndex < segments.size())
    {
        if (++iterations > 4*(text.size()+segments.size()+1))
        { error = "Native styled reflow failed to make bounded progress"; return std::nullopt; }
        const auto& segment = segments[segmentIndex];
        const auto current = segment.mParams.begin+offset;
        const double rounded = options.wrap ? std::max(0.0,std::floor(double(remaining)+0.5)) : double(INT32_MAX);
        if (!std::isfinite(rounded) || rounded > INT32_MAX)
        { error = "Native styled reflow remaining width overflows"; return std::nullopt; }
        const auto count = segment.fit(text,static_cast<std::int32_t>(rounded),offset,current-lineBegin,
            segment.mParams.end-current,error,paragraph-segmentLine);
        if (!count) return std::nullopt;
        const auto dimensions = segment.measure(text,offset,*count,error);
        if (!dimensions) return std::nullopt;
        height = std::max(height,dimensions->height);
        remaining -= dimensions->width;
        offset += *count;
        const auto end = segment.mParams.begin+offset;
        const bool partial = end < segment.mParams.end;
        const bool last = segmentIndex+1 == segments.size();
        if (partial || last || dimensions->lineBreak)
        {
            const double measured = std::ceil(available-remaining);
            if (!std::isfinite(measured) || measured < 0 || measured > INT32_MAX)
            { error = "Native styled line width overflows"; return std::nullopt; }
            const auto width = static_cast<std::int64_t>(measured);
            std::int64_t left = options.horizontalPadding;
            if (options.alignment == LLVKFont::HorizontalAlign::Center)
                left += std::max<std::int64_t>(0,(std::int64_t(options.width)-width-options.horizontalPadding)/2);
            else if (options.alignment == LLVKFont::HorizontalAlign::Right)
                left = std::max<std::int64_t>(left,std::int64_t(options.width)-width-1);
            for (const auto coordinate : {left,left+width,top,top-height})
                if (coordinate < INT32_MIN || coordinate > INT32_MAX)
                { error = "Native styled line rectangle overflows"; return std::nullopt; }
            lines.push_back({lineBegin,end,static_cast<std::size_t>(paragraph),static_cast<std::int32_t>(left),
                static_cast<std::int32_t>(top),static_cast<std::int32_t>(left+width),static_cast<std::int32_t>(top-height)});
            if (!partial && last) break;
            lineBegin = end;
            const double step = std::floor(float(height)*options.spacingMultiple+0.5f)+
                double(options.spacingPixels)+options.fontSpacingAdjustment;
            if (!std::isfinite(step) || step < INT32_MIN || step > INT32_MAX)
            { error = "Native styled line step overflows"; return std::nullopt; }
            top -= static_cast<std::int64_t>(step);
            remaining = available;
            height = 0;
        }
        if (!partial)
        {
            ++segmentIndex;
            offset = 0;
            segmentLine = dimensions->lineBreak ? paragraph+1 : paragraph;
        }
        if (dimensions->lineBreak) ++paragraph;
    }
    return lines;
}

std::optional<LLVKStyledTextDocument> LLVKStyledTextDocument::create(std::u32string text,
    LLVKStyledTextSegment::Params defaults, std::string& error)
{
    error.clear();
    if (text.size() > 1024*1024 || std::any_of(text.begin(),text.end(),[](char32_t character)
        { return !character || character > 0x10ffff || (character >= 0xd800 && character <= 0xdfff); }))
    { error = "Native styled document requires bounded Unicode scalar text"; return std::nullopt; }
    LLVKStyledTextDocument document;
    document.mText = std::move(text);
    document.mDefaults = std::move(defaults);
    if (!document.resetSegments(error)) return std::nullopt;
    return document;
}

bool LLVKStyledTextDocument::resetSegments(std::string& error)
{
    auto defaults = mDefaults;
    defaults.begin = 0;
    defaults.end = mText.size()+1;
    defaults.kind = LLVKStyledTextSegment::Kind::Normal;
    const auto segment = LLVKStyledTextSegment::create(defaults,mText,error);
    if (!segment) return false;
    std::vector<LLVKStyledTextSegment> segments{*segment};
    mSegments = std::move(segments);
    mLines.reset();
    mReflowIndex = 0;
    return true;
}

bool LLVKStyledTextDocument::overlay(const LLVKStyledTextSegment::Params& params, std::string& error)
{
    error.clear();
    const auto incoming = LLVKStyledTextSegment::create(params,mText,error);
    if (!incoming) return false;
    if (params.kind != LLVKStyledTextSegment::Kind::Normal && params.end > mText.size())
    { error = "Native special segment cannot replace EOF"; return false; }
    std::vector<LLVKStyledTextSegment> segments;
    segments.reserve(mSegments.size()+2);
    bool inserted = false;
    auto reflow = params.begin;
    const auto append = [&](const LLVKStyledTextSegment::Params& value)
    {
        auto segment = LLVKStyledTextSegment::create(value,mText,error);
        if (!segment) return false;
        segments.push_back(std::move(*segment));
        return true;
    };
    for (const auto& current : mSegments)
    {
        const auto& range = current.params();
        if (range.end <= params.begin || range.begin >= params.end)
        {
            if (!inserted && range.begin >= params.end) { segments.push_back(*incoming); inserted = true; }
            segments.push_back(current);
            continue;
        }
        reflow = std::min(reflow,range.begin);
        if (range.begin < params.begin)
        {
            auto prefix = range;
            prefix.end = params.begin;
            if (!append(prefix)) return false;
        }
        if (!inserted) { segments.push_back(*incoming); inserted = true; }
        if (range.end > params.end)
        {
            auto suffix = range;
            suffix.begin = params.end;
            if (range.begin < params.begin) suffix.kind = LLVKStyledTextSegment::Kind::Normal;
            if (!append(suffix)) return false;
        }
    }
    if (!inserted || segments.size() > 10000)
    { error = "Native segment overlay exceeds document coverage or segment budget"; return false; }
    mSegments = std::move(segments);
    mLines.reset();
    mReflowIndex = std::min(mReflowIndex.value_or(reflow),reflow);
    return true;
}

std::optional<std::size_t> LLVKStyledTextDocument::editableSegment(std::size_t index) const
{
    if (index > mText.size()) return std::nullopt;
    const auto found = std::upper_bound(mSegments.begin(),mSegments.end(),index,
        [](std::size_t position,const auto& segment) { return position < segment.params().end; });
    if (found == mSegments.end()) return std::nullopt;
    const auto offset = static_cast<std::size_t>(found-mSegments.begin());
    if (!found->editable() && found->params().begin == index && offset && mSegments[offset-1].editable()) return offset-1;
    return offset;
}

std::size_t LLVKStyledTextDocument::editableIndex(std::size_t index, bool forward) const
{
    if (index > mText.size()) return 0;
    const auto found = std::upper_bound(mSegments.begin(),mSegments.end(),index,
        [](std::size_t position,const auto& segment) { return position < segment.params().end; });
    if (found == mSegments.end()) return 0;
    const auto& params = found->params();
    if (!found->editable() && params.begin < index && index < params.end) return forward ? params.end : params.begin;
    return index;
}

bool LLVKStyledTextDocument::reflow(const LLVKPlainTextLayout::Options& options, std::string& error)
{
    auto lines = LLVKStyledTextSegment::reflow(mText,mSegments,options,error);
    if (!lines) return false;
    mLines = std::move(lines);
    mReflowIndex.reset();
    return true;
}

bool LLVKStyledTextDocument::publishEdit(std::u32string text, const std::vector<LLVKStyledTextSegment::Params>& params,
    std::size_t position, std::string& error)
{
    if (params.empty() || params.size() > 10000) { error = "Native edited segment count is invalid"; return false; }
    std::vector<LLVKStyledTextSegment> segments;
    segments.reserve(params.size());
    std::size_t covered = 0;
    for (const auto& input : params)
    {
        if (input.begin != covered) { error = "Native edited segments are not contiguous"; return false; }
        auto segment = LLVKStyledTextSegment::create(input,text,error);
        if (!segment) return false;
        covered = input.end;
        segments.push_back(std::move(*segment));
    }
    if (covered != text.size()+1 || params.back().kind != LLVKStyledTextSegment::Kind::Normal)
    { error = "Native edited segments lost EOF coverage"; return false; }
    mText = std::move(text);
    mSegments = std::move(segments);
    mLines.reset();
    mReflowIndex = std::min(mReflowIndex.value_or(position),position);
    return true;
}

std::optional<LLVKStyledTextDocument::Edit> LLVKStyledTextDocument::insert(std::size_t position,
    std::u32string_view inserted, std::string& error)
{
    error.clear();
    if (inserted.size() > 1024*1024-mText.size() || std::any_of(inserted.begin(),inserted.end(),[](char32_t character)
        { return !character || character > 0x10ffff || (character >= 0xd800 && character <= 0xdfff); }))
    { error = "Native styled insertion exceeds scalar budget or contains invalid text"; return std::nullopt; }
    position = editableIndex(std::min(position,mText.size()),true);
    const auto containing = editableSegment(position);
    if (!containing) { error = "Native styled insertion has no containing segment"; return std::nullopt; }
    if (inserted.empty()) return Edit{position,0,0};
    auto text = mText;
    text.insert(position,inserted);
    std::vector<LLVKStyledTextSegment::Params> segments;
    segments.reserve(mSegments.size()+1);
    for (std::size_t index = 0; index < mSegments.size(); ++index)
    {
        auto params = mSegments[index].params();
        if (index == *containing)
        {
            if (mSegments[index].editable()) params.end += inserted.size();
            else
            {
                auto defaults = mDefaults;
                defaults.kind = LLVKStyledTextSegment::Kind::Normal;
                defaults.begin = position;
                defaults.end = position+inserted.size();
                segments.push_back(std::move(defaults));
                params.begin += inserted.size();
                params.end += inserted.size();
            }
        }
        else if (index > *containing)
        {
            params.begin += inserted.size();
            params.end += inserted.size();
        }
        segments.push_back(std::move(params));
    }
    if (!publishEdit(std::move(text),segments,position,error)) return std::nullopt;
    return Edit{position,0,inserted.size()};
}

std::optional<LLVKStyledTextDocument::Edit> LLVKStyledTextDocument::erase(std::size_t position,
    std::size_t count, std::string& error)
{
    error.clear();
    if (position >= mText.size() || !count) return Edit{std::min(position,mText.size()),0,0};
    count = std::min(count,mText.size()-position);
    const auto end = position+count;
    auto text = mText;
    text.erase(position,count);
    std::vector<LLVKStyledTextSegment::Params> segments;
    segments.reserve(mSegments.size());
    for (const auto& segment : mSegments)
    {
        auto params = segment.params();
        if (params.end <= position) { segments.push_back(std::move(params)); continue; }
        if (params.begin < position) params.end = params.end > end ? params.end-count : position;
        else if (params.begin < end)
        {
            if (params.end <= end) continue;
            params.begin = position;
            params.end -= count;
        }
        else
        {
            params.begin -= count;
            params.end -= count;
        }
        segments.push_back(std::move(params));
    }
    if (!publishEdit(std::move(text),segments,position,error)) return std::nullopt;
    return Edit{position,count,0};
}

std::optional<bool> LLVKStyledTextDocument::truncate(std::size_t maximumBytes, std::string& error)
{
    error.clear();
    std::size_t bytes = 0, characters = 0;
    for (const auto character : mText)
    {
        const std::size_t length = character <= 0x7f ? 1 : character <= 0x7ff ? 2 : character <= 0xffff ? 3 : 4;
        if (length > maximumBytes-bytes) break;
        bytes += length;
        ++characters;
    }
    if (characters == mText.size()) return false;
    if (!erase(characters,mText.size()-characters,error)) return std::nullopt;
    return true;
}

bool LLVKStyledTextDocument::appendPlain(std::u32string_view appended, const LLVKStyledTextSegment::Params& style,
    bool prependNewline, std::string& error)
{
    error.clear();
    if (appended.empty()) return true;
    const auto extra = prependNewline ? 1u : 0u;
    if (appended.size() > 1024*1024-mText.size() || extra > 1024*1024-mText.size()-appended.size() ||
        std::any_of(appended.begin(),appended.end(),[](char32_t character)
        { return !character || character > 0x10ffff || (character >= 0xd800 && character <= 0xdfff); }))
    { error = "Native styled append exceeds scalar budget or contains invalid text"; return false; }
    auto text = mText;
    if (prependNewline) text.push_back(U'\n');
    text.append(appended);
    std::vector<LLVKStyledTextSegment::Params> segments;
    segments.reserve(std::min<std::size_t>(10000,mSegments.size()+8));
    for (const auto& segment : mSegments)
    {
        auto params = segment.params();
        params.end = std::min(params.end,mText.size());
        if (params.begin < params.end) segments.push_back(std::move(params));
    }
    std::size_t begin = mText.size();
    while (begin < text.size())
    {
        const auto newline = text.find(U'\n',begin);
        const auto end = newline == std::u32string::npos ? text.size() : newline;
        if (begin < end)
        {
            auto params = style;
            params.kind = LLVKStyledTextSegment::Kind::Normal;
            params.begin = begin; params.end = end;
            segments.push_back(std::move(params));
        }
        if (newline != std::u32string::npos)
        {
            auto params = style;
            params.kind = LLVKStyledTextSegment::Kind::LineBreak;
            params.begin = newline; params.end = newline+1;
            segments.push_back(std::move(params));
        }
        if (segments.size() >= 10000) { error = "Native styled append exceeds segment budget"; return false; }
        begin = newline == std::u32string::npos ? text.size() : newline+1;
    }
    auto eof = mSegments.back().params();
    eof.begin = text.size(); eof.end = text.size()+1;
    segments.push_back(std::move(eof));
    return publishEdit(std::move(text),segments,mText.size(),error);
}