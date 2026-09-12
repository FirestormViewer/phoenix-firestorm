#include "llvkfontface.h"
#include "llvkfontsvg.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MULTIPLE_MASTERS_H
#include FT_MODULE_H

#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>
#include <utility>
#include <vector>

struct LLVKFontFace::Impl
{
    std::vector<std::uint8_t> bytes;
    FT_Library library = nullptr;
    FT_Face face = nullptr;
    Metrics metrics;
    FT_Int32 loadFlags = FT_LOAD_FORCE_AUTOHINT;
    mutable std::mutex mutex;

    ~Impl()
    {
        if (face) FT_Done_Face(face);
        if (library) FT_Done_FreeType(library);
    }

    bool setAxis(FT_ULong tag, float value, bool& applied, std::string& error)
    {
        applied = false;
        if (!FT_HAS_MULTIPLE_MASTERS(face)) return true;
        FT_MM_Var* axes = nullptr;
        FT_Error result = FT_Get_MM_Var(face, &axes);
        if (result)
        {
            error = "FT_Get_MM_Var failed: " + std::to_string(result);
            return false;
        }
        auto release = [this](FT_MM_Var* pointer) { FT_Done_MM_Var(library, pointer); };
        std::unique_ptr<FT_MM_Var, decltype(release)> ownedAxes(axes, release);
        for (FT_UInt index = 0; index < axes->num_axis; ++index)
        {
            if (axes->axis[index].tag != tag) continue;
            std::vector<FT_Fixed> coordinates(axes->num_axis);
            result = FT_Get_Var_Design_Coordinates(face, axes->num_axis, coordinates.data());
            if (result)
            {
                error = "FT_Get_Var_Design_Coordinates failed: " + std::to_string(result);
                return false;
            }
            value = std::clamp(value, axes->axis[index].minimum / 65536.f,
                              axes->axis[index].maximum / 65536.f);
            coordinates[index] = static_cast<FT_Fixed>(value * 65536.f);
            result = FT_Set_Var_Design_Coordinates(face, axes->num_axis, coordinates.data());
            if (result)
            {
                error = "FT_Set_Var_Design_Coordinates failed: " + std::to_string(result);
                return false;
            }
            applied = true;
            return true;
        }
        return true;
    }
};

LLVKFontFace::LLVKFontFace(std::unique_ptr<Impl> impl) : mImpl(std::move(impl)) {}
LLVKFontFace::~LLVKFontFace() = default;

const LLVKFontFace::Metrics& LLVKFontFace::metrics() const noexcept
{
    return mImpl->metrics;
}

std::unique_ptr<LLVKFontFace> LLVKFontFace::create(std::span<const std::uint8_t> bytes,
                                               const Options& options,
                                               std::string& error)
{
    error.clear();
    const auto validDpi = [](float dpi)
    {
        return std::isfinite(dpi) && dpi >= 1.f &&
               static_cast<double>(dpi) <= std::numeric_limits<FT_UInt>::max();
    };
    if (bytes.empty() || bytes.size() > static_cast<std::size_t>(std::numeric_limits<FT_Long>::max()) ||
        !std::isfinite(options.pointSize) || options.pointSize < 1.f / 64.f ||
        static_cast<double>(options.pointSize * 64.f) > std::numeric_limits<std::int32_t>::max() ||
        !validDpi(options.horizontalDpi) || !validDpi(options.verticalDpi))
    {
        error = "Invalid font bytes, point size or DPI";
        return nullptr;
    }
    auto impl = std::make_unique<Impl>();
    switch (options.hinting)
    {
        case Hinting::Default: impl->loadFlags = FT_LOAD_DEFAULT; break;
        case Hinting::ForceAutohint: impl->loadFlags = FT_LOAD_FORCE_AUTOHINT; break;
        case Hinting::DisableAutohint: impl->loadFlags = FT_LOAD_NO_AUTOHINT; break;
        default:
            error = "Invalid font hinting policy";
            return nullptr;
    }
    impl->bytes.assign(bytes.begin(), bytes.end());
    FT_Error result = FT_Init_FreeType(&impl->library);
    if (result)
    {
        error = "FT_Init_FreeType failed: " + std::to_string(result);
        return nullptr;
    }
    result = FT_New_Memory_Face(impl->library, impl->bytes.data(),
                               static_cast<FT_Long>(impl->bytes.size()), 0, &impl->face);
    if (result)
    {
        error = "FT_New_Memory_Face failed: " + std::to_string(result);
        return nullptr;
    }
    if (FT_HAS_SVG(impl->face))
    {
        result = llvkInstallSvgHooks(impl->library);
        if (result)
        {
            error = "Native SVG hook installation failed: " + std::to_string(result);
            return nullptr;
        }
    }
    if (options.weight >= 0)
    {
        bool opticalSizeApplied = false;
        if (!impl->setAxis(FT_MAKE_TAG('w', 'g', 'h', 't'), static_cast<float>(options.weight),
                           impl->metrics.weightApplied, error) ||
            !impl->setAxis(FT_MAKE_TAG('o', 'p', 's', 'z'), options.pointSize,
                           opticalSizeApplied, error)) return nullptr;
    }
    result = FT_Set_Char_Size(impl->face, 0, static_cast<FT_F26Dot6>(options.pointSize * 64.f),
                              static_cast<FT_UInt>(options.horizontalDpi),
                              static_cast<FT_UInt>(options.verticalDpi));
    if (result)
    {
        error = "FT_Set_Char_Size failed: " + std::to_string(result);
        return nullptr;
    }
    if (!FT_IS_SCALABLE(impl->face) || impl->face->units_per_EM == 0)
    {
        error = "Font has no scalable metrics; bitmap-strike sizing is not implemented";
        return nullptr;
    }
    if (!impl->face->charmap)
    {
        if (impl->face->num_charmaps == 0 || FT_Set_Charmap(impl->face, impl->face->charmaps[0]))
        {
            error = "Font has no usable character map";
            return nullptr;
        }
    }
    const float pixelsPerEm = (options.pointSize / 72.f) * options.verticalDpi;
    const float pixelsPerUnit = pixelsPerEm * (1.f / impl->face->units_per_EM);
    impl->metrics.ascender = impl->face->ascender * pixelsPerUnit;
    impl->metrics.descender = -impl->face->descender * pixelsPerUnit;
    impl->metrics.lineHeight = impl->face->height * pixelsPerUnit;
    impl->metrics.bold = (impl->face->style_flags & FT_STYLE_FLAG_BOLD) || options.descriptorBold ||
                         (options.weight >= 600 && impl->metrics.weightApplied);
    impl->metrics.italic = (impl->face->style_flags & FT_STYLE_FLAG_ITALIC) != 0;
    return std::unique_ptr<LLVKFontFace>(new LLVKFontFace(std::move(impl)));
}

std::uint32_t LLVKFontFace::glyphIndex(char32_t codepoint) const
{
    std::lock_guard lock(mImpl->mutex);
    return FT_Get_Char_Index(mImpl->face, static_cast<FT_ULong>(codepoint));
}

std::optional<LLVKFontFace::Glyph> LLVKFontFace::rasterize(char32_t codepoint,
                                                        bool requestColor, std::string& error)
{
    error.clear();
    std::lock_guard lock(mImpl->mutex);
    Glyph glyph;
    glyph.codepoint = codepoint;
    glyph.requestedIndex = FT_Get_Char_Index(mImpl->face, static_cast<FT_ULong>(codepoint));
    glyph.renderedIndex = glyph.requestedIndex;
    const FT_Int32 flags = mImpl->loadFlags | (requestColor ? FT_LOAD_COLOR : 0);
    FT_Error result = FT_Load_Glyph(mImpl->face, glyph.renderedIndex, flags);
    if (result == FT_Err_Out_Of_Memory || result == FT_Err_Unimplemented_Feature)
    {
        error = "Glyph loading unavailable: " + std::to_string(result);
        return std::nullopt;
    }
    if (result)
    {
        glyph.retried = true;
        result = FT_Load_Glyph(mImpl->face, glyph.renderedIndex, flags ^ FT_LOAD_COLOR);
        if (result == FT_Err_Out_Of_Memory || result == FT_Err_Unimplemented_Feature)
        {
            error = "Glyph retry unavailable: " + std::to_string(result);
            return std::nullopt;
        }
        if (result == FT_Err_Invalid_Outline || result == FT_Err_Invalid_Composite ||
            (result && codepoint >= 0x1f000 && codepoint < 0x20000))
        {
            glyph.renderedIndex = 0;
            result = FT_Load_Glyph(mImpl->face, 0, FT_LOAD_FORCE_AUTOHINT);
            if (result)
            {
                error = "Missing-glyph load failed: " + std::to_string(result);
                return std::nullopt;
            }
        }
        if (result)
        {
            glyph.renderedIndex = FT_Get_Char_Index(mImpl->face, '?');
            result = FT_Load_Glyph(mImpl->face, glyph.renderedIndex, flags ^ FT_LOAD_COLOR);
        }
    }
    if (result)
    {
        error = "Glyph load failed: " + std::to_string(result);
        return std::nullopt;
    }
    result = FT_Render_Glyph(mImpl->face->glyph, FT_RENDER_MODE_NORMAL);
    if (result)
    {
        error = "Glyph rendering failed: " + std::to_string(result);
        return std::nullopt;
    }
    const auto slot = mImpl->face->glyph;
    const auto& bitmap = slot->bitmap;
    glyph.width = bitmap.width;
    glyph.height = bitmap.rows;
    glyph.bearingX = slot->bitmap_left;
    glyph.bearingY = slot->bitmap_top;
    glyph.lsbDelta = static_cast<std::int32_t>(slot->lsb_delta);
    glyph.rsbDelta = static_cast<std::int32_t>(slot->rsb_delta);
    glyph.advanceX = slot->advance.x / 64.f;
    glyph.advanceY = slot->advance.y / 64.f;
    if (!glyph.width || !glyph.height) return glyph;
    if (bitmap.pixel_mode != FT_PIXEL_MODE_MONO && bitmap.pixel_mode != FT_PIXEL_MODE_GRAY &&
        bitmap.pixel_mode != FT_PIXEL_MODE_BGRA)
    {
        error = "Unsupported glyph pixel mode: " + std::to_string(bitmap.pixel_mode);
        return std::nullopt;
    }
    const std::size_t channels = bitmap.pixel_mode == FT_PIXEL_MODE_BGRA ? 4 : 1;
    const std::size_t rowBytes = static_cast<std::size_t>(glyph.width) * channels;
    const std::size_t sourceBytes = bitmap.pixel_mode == FT_PIXEL_MODE_MONO ?
                                   (static_cast<std::size_t>(glyph.width) + 7) / 8 : rowBytes;
    const auto pitchMagnitude = bitmap.pitch < 0 ? -static_cast<std::int64_t>(bitmap.pitch) : bitmap.pitch;
    if (!bitmap.buffer || sourceBytes > static_cast<std::uint64_t>(pitchMagnitude) ||
        rowBytes > glyph.bottomUpPixels.max_size() / glyph.height ||
        static_cast<std::uint64_t>(pitchMagnitude) * (glyph.height - 1) >
            static_cast<std::uint64_t>(std::numeric_limits<std::ptrdiff_t>::max()))
    {
        error = "Invalid or oversized glyph bitmap";
        return std::nullopt;
    }
    glyph.encoding = channels == 4 ? PixelEncoding::PremultipliedSrgbRgba8 : PixelEncoding::Coverage8;
    glyph.bottomUpPixels.resize(rowBytes * glyph.height);
    for (std::uint32_t row = 0; row < glyph.height; ++row)
    {
        const auto* source = bitmap.buffer + static_cast<std::ptrdiff_t>(glyph.height - 1 - row) * bitmap.pitch;
        auto* destination = glyph.bottomUpPixels.data() + row * rowBytes;
        for (std::uint32_t column = 0; column < glyph.width; ++column)
        {
            if (channels == 4)
            {
                const auto offset = static_cast<std::size_t>(column) * 4;
                destination[offset] = source[offset + 2];
                destination[offset + 1] = source[offset + 1];
                destination[offset + 2] = source[offset];
                destination[offset + 3] = source[offset + 3];
            }
            else if (bitmap.pixel_mode == FT_PIXEL_MODE_MONO)
            {
                destination[column] = (source[column / 8] & (0x80 >> (column % 8))) ? 255 : 0;
            }
            else destination[column] = source[column];
        }
    }
    return glyph;
}

std::optional<float> LLVKFontFace::kerning(std::uint32_t leftIndex, std::uint32_t rightIndex,
                                         std::int32_t leftRsbDelta, std::int32_t rightLsbDelta,
                                         std::string& error) const
{
    error.clear();
    std::lock_guard lock(mImpl->mutex);
    if (leftIndex >= static_cast<std::uint64_t>(mImpl->face->num_glyphs) ||
        rightIndex >= static_cast<std::uint64_t>(mImpl->face->num_glyphs))
    {
        error = "Kerning glyph index outside this face";
        return std::nullopt;
    }
    FT_Vector delta{};
    const auto result = FT_Get_Kerning(mImpl->face, leftIndex, rightIndex, FT_KERNING_UNFITTED, &delta);
    if (result)
    {
        error = "Kerning failed: " + std::to_string(result);
        return std::nullopt;
    }
    const auto difference = static_cast<std::int64_t>(leftRsbDelta) - rightLsbDelta;
    return delta.x * (1.f / 64.f) + (difference > 32 ? -1.f : difference < -31 ? 1.f : 0.f);
}