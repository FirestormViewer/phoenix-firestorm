#ifndef LLVKFONTFACE_H
#define LLVKFONTFACE_H

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

class LLVKFontFace final
{
public:
    enum class Hinting { Default, ForceAutohint, DisableAutohint };
    enum class PixelEncoding { Coverage8, PremultipliedSrgbRgba8 };

    struct Options
    {
        float pointSize = 12.f;
        float horizontalDpi = 96.f;
        float verticalDpi = 96.f;
        int weight = -1;
        bool descriptorBold = false;
        Hinting hinting = Hinting::ForceAutohint;
    };

    struct Metrics
    {
        float ascender = 0.f;
        float descender = 0.f;
        float lineHeight = 0.f;
        bool bold = false;
        bool italic = false;
        bool weightApplied = false;
    };

    struct Glyph
    {
        char32_t codepoint = 0;
        std::uint32_t requestedIndex = 0;
        std::uint32_t renderedIndex = 0;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        int bearingX = 0;
        int bearingY = 0;
        std::int32_t lsbDelta = 0;
        std::int32_t rsbDelta = 0;
        float advanceX = 0.f;
        float advanceY = 0.f;
        bool retried = false;
        PixelEncoding encoding = PixelEncoding::Coverage8;
        std::vector<std::uint8_t> bottomUpPixels;
    };

    static std::unique_ptr<LLVKFontFace> create(std::span<const std::uint8_t> bytes,
                                               const Options& options,
                                               std::string& error);
    ~LLVKFontFace();
    LLVKFontFace(const LLVKFontFace&) = delete;
    LLVKFontFace& operator=(const LLVKFontFace&) = delete;

    const Metrics& metrics() const noexcept;
    std::uint32_t glyphIndex(char32_t codepoint) const;
    std::optional<Glyph> rasterize(char32_t codepoint, bool requestColor, std::string& error);
    std::optional<float> kerning(std::uint32_t leftIndex, std::uint32_t rightIndex,
                                 std::int32_t leftRsbDelta, std::int32_t rightLsbDelta,
                                 std::string& error) const;

private:
    struct Impl;
    explicit LLVKFontFace(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> mImpl;
};

#endif