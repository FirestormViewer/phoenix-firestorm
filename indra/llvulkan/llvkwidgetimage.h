#ifndef LLVKWIDGETIMAGE_H
#define LLVKWIDGETIMAGE_H

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>
#include <optional>
#include <array>

class LLVKWidgetImage final
{
public:
    struct Rect
    {
        std::int32_t left = 0, bottom = 0, right = 0, top = 0;
        auto operator<=>(const Rect&) const = default;
    };
    struct Region
    {
        float left = 0.f, bottom = 0.f, right = 1.f, top = 1.f;
    };
    enum class Scale { Inner, Outer };
    struct Metadata
    {
        std::optional<Rect> clip, scale;
        Scale style = Scale::Inner;
    };
    struct Quad
    {
        Region position, uv;
    };
    struct Geometry
    {
        std::array<Quad,9> quads;
        std::size_t count = 0;
    };
    std::optional<Geometry> prepare(Rect target, float scaleX, float scaleY,
        float translateX, float translateY, std::string& error) const;
    static std::shared_ptr<const LLVKWidgetImage> decodePng(std::string name,
        std::span<const std::uint8_t> encoded, std::string& error);
    static std::shared_ptr<const LLVKWidgetImage> browserFrame(std::uint32_t width, std::uint32_t height,
        std::span<const std::uint8_t> topDownBgra, std::string& error);
    static std::shared_ptr<const LLVKWidgetImage> decodeSkinPng(std::string name,
        std::span<const std::uint8_t> encoded, const Metadata& metadata, std::string& error);
    static std::shared_ptr<const LLVKWidgetImage> decodeJpeg(std::string name,
        std::span<const std::uint8_t> encoded, std::string& error);
    static std::shared_ptr<const LLVKWidgetImage> decodeTga(std::string name,
        std::span<const std::uint8_t> encoded, std::string& error);
    static std::shared_ptr<const LLVKWidgetImage> decodeJ2c(std::string name,
        std::span<const std::uint8_t> encoded, std::string& error);
    static std::string j2cDecoderVersion();
    static std::shared_ptr<const LLVKWidgetImage> decodeSkin(std::string name,
        std::span<const std::uint8_t> encoded, const Metadata& metadata, std::string& error);
    static std::shared_ptr<const LLVKWidgetImage> skinView(std::string name,
        std::shared_ptr<const LLVKWidgetImage> pixels, const Metadata& metadata, std::string& error);
    std::uint32_t width() const noexcept { return mLogicalWidth; }
    std::uint32_t height() const noexcept { return mLogicalHeight; }
    std::uint32_t pixelWidth() const noexcept { return mWidth; }
    std::uint32_t pixelHeight() const noexcept { return mHeight; }
    const Region& clipRegion() const noexcept { return mClip; }
    const Region& scaleRegion() const noexcept { return mScale; }
    Scale scaleStyle() const noexcept { return mStyle; }
    const std::string& name() const noexcept { return mName; }
    std::span<const std::uint8_t> bottomUpRgba() const noexcept { return mPixelOwner ? mPixelOwner->bottomUpRgba() : std::span<const std::uint8_t>(mPixels); }
private:
    static std::shared_ptr<LLVKWidgetImage> decode(std::string name,
        std::span<const std::uint8_t> encoded, std::string& error);
    static std::shared_ptr<LLVKWidgetImage> decodeJpegPixels(std::string name,
        std::span<const std::uint8_t> encoded, std::string& error);
    static std::shared_ptr<LLVKWidgetImage> decodeTgaPixels(std::string name,
        std::span<const std::uint8_t> encoded, std::string& error);
    static std::shared_ptr<LLVKWidgetImage> decodeJ2cPixels(std::string name,
        std::span<const std::uint8_t> encoded, std::string& error);
    static std::shared_ptr<const LLVKWidgetImage> applySkin(std::shared_ptr<LLVKWidgetImage> image,
        const Metadata& metadata, std::string& error);
    LLVKWidgetImage() = default;
    std::string mName;
    std::uint32_t mWidth = 0;
    std::uint32_t mHeight = 0;
    std::uint32_t mLogicalWidth = 0, mLogicalHeight = 0;
    Region mClip, mScale;
    Scale mStyle = Scale::Inner;
    bool mHasAlpha = false;
    bool mSkinPixels = false;
    std::shared_ptr<const LLVKWidgetImage> mPixelOwner;
    std::vector<std::uint8_t> mPixels;
};

#endif