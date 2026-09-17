#ifndef LLVKFONT_H
#define LLVKFONT_H

#include "llvkfontface.h"

#include <map>
#include <mutex>
#include <utility>
#include <string_view>

class LLVKFont final
{
public:
    enum class FallbackPolicy { Unrestricted, Emoji, ColorEmoji, MonochromeEmoji };

    struct FaceSource
    {
        std::vector<std::uint8_t> bytes;
        LLVKFontFace::Options options;
    };

    struct FallbackSource
    {
        FaceSource source;
        FallbackPolicy policy = FallbackPolicy::Unrestricted;
    };

    struct Glyph
    {
        std::size_t faceIndex = 0;
        LLVKFontFace::Glyph raster;
    };

    struct PositionedGlyph
    {
        std::size_t sourceIndex = 0;
        float penX = 0.f;
        float digitOffsetX = 0.f;
        std::shared_ptr<const Glyph> glyph;
    };

    struct MeasuredRun
    {
        std::vector<PositionedGlyph> glyphs;
        float advancePixels = 0.f;
        float trailingPaddingPixels = 0.f;
        float width = 0.f;
    };

    enum class HorizontalAlign { Left, Center, Right };
    enum class VerticalAlign { Baseline, Top, Center, Bottom };
    enum class Wrap { Anywhere, WordsOnly, WordsWhenPossible };

    std::optional<std::size_t> fitCharacters(std::u32string_view text, float maxPixels,
        std::size_t count, float scaleX, Wrap wrap, bool tabularNumbers, std::string& error);
    std::optional<std::size_t> hitTest(std::u32string_view text, std::size_t begin, float targetX,
        float maxPixels, std::size_t count, float scaleX, bool nearest, bool tabularNumbers,
        std::string& error);
    std::optional<std::size_t> firstVisible(std::u32string_view text, std::size_t start,
        float maxPixels, std::size_t count, float scaleX, bool tabularNumbers, std::string& error);

    struct LineOptions
    {
        float x = 0.f;
        float y = 0.f;
        float originX = 0.f;
        float originY = 0.f;
        float scaleX = 1.f;
        float scaleY = 1.f;
        std::int32_t maxPixels = INT32_MAX;
        HorizontalAlign horizontal = HorizontalAlign::Left;
        VerticalAlign vertical = VerticalAlign::Baseline;
        bool ellipses = false;
        bool requestColor = false;
        bool tabularNumbers = false;
    };

    struct DrawGlyph
    {
        std::size_t sourceIndex = 0;
        bool ellipsis = false;
        float left = 0.f;
        float top = 0.f;
        float right = 0.f;
        float bottom = 0.f;
        std::shared_ptr<const Glyph> glyph;
    };

    struct LineLayout
    {
        float displayScale = 1.f;
        std::vector<DrawGlyph> glyphs;
        std::size_t sourceCharacters = 0;
        float startPixelX = 0.f;
        float baselinePixelY = 0.f;
        float endPixelX = 0.f;
        float endPixelY = 0.f;
        float rightX = 0.f;
        bool truncated = false;
        bool ellipsized = false;
    };

    std::optional<LineLayout> layoutLine(std::u32string_view text, std::size_t begin,
                                        std::size_t count, const LineOptions& options,
                                        std::string& error);

    std::optional<MeasuredRun> measureRun(std::u32string_view text, std::size_t begin,
                                         std::size_t count, float scaleX, bool includePadding,
                                         bool tabularNumbers, std::string& error);

    static std::unique_ptr<LLVKFont> create(const FaceSource& primary,
                                           const std::vector<FallbackSource>& fallbacks,
                                           bool monochromeEmoji, std::string& error, float displayScale = 1.f);
    const LLVKFontFace::Metrics& metrics() const noexcept;
    float displayScale() const noexcept { return mDisplayScale; }
    std::shared_ptr<const Glyph> glyph(char32_t codepoint, bool requestColor, std::string& error);
    std::size_t cachedGlyphCount() const;

private:
    friend class LLVKFontRegistry;
    void replaceRasterState(LLVKFont& replacement);
    LLVKFont() = default;
    std::optional<MeasuredRun> measureDeviceRun(std::u32string_view text, std::size_t begin,
        std::size_t count, float scaleX, bool includePadding, bool tabularNumbers, std::string& error);
    std::optional<LineLayout> layoutDeviceLine(std::u32string_view text, std::size_t begin,
        std::size_t count, const LineOptions& options, std::string& error);
    float mDisplayScale = 1.f;
    LLVKFontFace::Metrics mLogicalMetrics;
    std::optional<float> digitWidth(bool tabularNumbers, std::string& error);
    std::optional<float> pairKerning(const Glyph& left, const Glyph& right,
                                     bool tabularNumbers, std::string& error);
    std::vector<std::unique_ptr<LLVKFontFace>> mFaces;
    std::vector<FallbackPolicy> mPolicies;
    bool mMonochromeEmoji = false;
    int mWeight = -1;
    std::map<std::pair<char32_t, bool>, std::shared_ptr<const Glyph>> mGlyphs;
    mutable std::mutex mMutex;
};

#endif