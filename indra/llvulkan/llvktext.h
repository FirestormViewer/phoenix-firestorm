#ifndef LLVKTEXT_H
#define LLVKTEXT_H

#include "llvkfont.h"
#include "llvkwidgetimage.h"

#include <cstddef>
#include <compare>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

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
	static std::optional<Document> document(std::vector<Line> lines, const Options& options,
		std::int32_t height, std::int32_t verticalPadding, LLVKFont::VerticalAlign alignment, std::string& error);
};

struct LLVKWebText
{
	struct Icon
	{
		std::size_t position = 0;
		std::string name;
	};
	struct Link
	{
		std::size_t begin = 0, end = 0;
		std::string target;
		bool query = false;
	};
	std::u32string text;
	std::vector<Link> links;
    std::vector<Icon> icons;
	static std::optional<LLVKWebText> parse(std::string_view markup, std::string& error);
};

class LLVKStyledTextSegment final
{
public:
	enum class Kind { Normal, LineBreak, Image, InlineWidget };
	struct Params
	{
		std::shared_ptr<LLVKFont> font;
		std::shared_ptr<const LLVKWidgetImage> image;
		std::size_t begin = 0, end = 0;
		Kind kind = Kind::Normal;
		float scaleX = 1.f, scaleY = 1.f;
		bool tabularNumbers = false;
		bool highlightBackground = false;
		std::uint64_t widget = 0;
		std::int32_t widgetWidth = 0, widgetHeight = 0;
		std::int32_t leftPad = 0, rightPad = 0, topPad = 0, bottomPad = 0;
		bool forceNewLine = false;
	};
	struct Dimensions { float width = 0.f; std::int32_t height = 0; bool lineBreak = false; };
	static std::optional<LLVKStyledTextSegment> create(const Params& params, std::u32string_view text, std::string& error);
	std::optional<Dimensions> measure(std::u32string_view text, std::size_t offset, std::size_t count, std::string& error) const;
	std::optional<std::size_t> fit(std::u32string_view text, std::int32_t pixels, std::size_t offset,
		std::size_t lineOffset, std::size_t maximum, std::string& error, std::int64_t lineIndex = 0) const;
	std::optional<std::size_t> hit(std::u32string_view text, std::int32_t pixels, std::size_t offset,
		std::size_t count, bool nearest, std::string& error) const;
	const Params& params() const noexcept { return mParams; }
	bool editable() const noexcept { return mParams.kind == Kind::Normal && !mParams.highlightBackground; }
	bool permitsEmoji() const noexcept
	{ return mParams.kind != Kind::InlineWidget && (mParams.kind != Kind::Normal || !mParams.highlightBackground); }
	static std::optional<std::vector<LLVKPlainTextLayout::Line>> reflow(std::u32string_view text,
		std::span<const LLVKStyledTextSegment> segments, const LLVKPlainTextLayout::Options& options, std::string& error);
private:
	bool validRange(std::u32string_view text, std::size_t offset, std::size_t count, std::string& error) const;
	Params mParams;
	std::int32_t mHeight = 0;
};

class LLVKStyledTextDocument final
{
public:
	static std::optional<LLVKStyledTextDocument> create(std::u32string text,
		LLVKStyledTextSegment::Params defaults, std::string& error);
	bool overlay(const LLVKStyledTextSegment::Params& params, std::string& error);
	bool resetSegments(std::string& error);
	struct Edit { std::size_t position = 0, removed = 0, inserted = 0; };
	std::optional<Edit> insert(std::size_t position, std::u32string_view text, std::string& error);
	std::optional<Edit> erase(std::size_t position, std::size_t count, std::string& error);
	std::optional<bool> truncate(std::size_t maximumBytes, std::string& error);
	bool appendPlain(std::u32string_view text, const LLVKStyledTextSegment::Params& style,
		bool prependNewline, std::string& error);
	std::size_t editableIndex(std::size_t index, bool forward) const;
	std::optional<std::size_t> editableSegment(std::size_t index) const;
	bool reflow(const LLVKPlainTextLayout::Options& options, std::string& error);
	const std::u32string& text() const noexcept { return mText; }
	const std::vector<LLVKStyledTextSegment>& segments() const noexcept { return mSegments; }
	const std::optional<std::vector<LLVKPlainTextLayout::Line>>& lines() const noexcept { return mLines; }
	std::optional<std::size_t> reflowIndex() const noexcept { return mReflowIndex; }
private:
	bool publishEdit(std::u32string text, const std::vector<LLVKStyledTextSegment::Params>& params,
		std::size_t position, std::string& error);
	std::u32string mText;
	LLVKStyledTextSegment::Params mDefaults;
	std::vector<LLVKStyledTextSegment> mSegments;
	std::optional<std::vector<LLVKPlainTextLayout::Line>> mLines;
	std::optional<std::size_t> mReflowIndex;
};

#endif
