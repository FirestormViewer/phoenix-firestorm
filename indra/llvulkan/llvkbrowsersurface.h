#ifndef LLVKBROWSERSURFACE_H
#define LLVKBROWSERSURFACE_H

#include "llvkwidgetimage.h"

class LLVKBrowserSurface final
{
public:
    bool resize(std::uint32_t width, std::uint32_t height, std::string& error);
    bool publish(std::uint32_t width, std::uint32_t height, std::span<const std::uint8_t> topDownBgra, std::string& error);
    std::shared_ptr<const LLVKWidgetImage> frame() const noexcept { return mFrame; }
    std::uint64_t generation() const noexcept { return mGeneration; }
    std::uint64_t epoch() const noexcept { return mEpoch; }
    std::uint32_t width() const noexcept { return mWidth; }
    std::uint32_t height() const noexcept { return mHeight; }
private:
    std::uint32_t mWidth = 0, mHeight = 0;
    std::uint64_t mGeneration = 0;
    std::uint64_t mEpoch = 0;
    std::shared_ptr<const LLVKWidgetImage> mFrame;
};

#endif