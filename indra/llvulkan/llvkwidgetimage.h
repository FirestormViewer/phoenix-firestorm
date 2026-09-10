#ifndef LLVKWIDGETIMAGE_H
#define LLVKWIDGETIMAGE_H

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

class LLVKWidgetImage final
{
public:
    static std::shared_ptr<const LLVKWidgetImage> decodePng(std::string name,
        std::span<const std::uint8_t> encoded, std::string& error);
    std::uint32_t width() const noexcept { return mWidth; }
    std::uint32_t height() const noexcept { return mHeight; }
    const std::string& name() const noexcept { return mName; }
    std::span<const std::uint8_t> bottomUpRgba() const noexcept { return mPixels; }
private:
    LLVKWidgetImage() = default;
    std::string mName;
    std::uint32_t mWidth = 0;
    std::uint32_t mHeight = 0;
    std::vector<std::uint8_t> mPixels;
};

#endif