#ifndef LLVKBROWSERSURFACE_H
#define LLVKBROWSERSURFACE_H

#include "llvkwidgetimage.h"
#include <algorithm>

class LLVKBrowserSurface final
{
public:
    static bool compositePopup(std::span<std::uint8_t> view,int width,int height,
        std::span<const std::uint8_t> popup,int popupWidth,int popupHeight,int left,int top,bool flipped)
    {
        if (width<=0 || height<=0 || popupWidth<=0 || popupHeight<=0 ||
            view.size()!=std::uint64_t(width)*height*4 || popup.size()!=std::uint64_t(popupWidth)*popupHeight*4) return false;
        const auto firstColumn=std::max<std::int64_t>(0,-std::int64_t(left));
        const auto lastColumn=std::min<std::int64_t>(popupWidth,std::int64_t(width)-left);
        if (lastColumn<=firstColumn) return true;
        for (int row=0; row<popupHeight; ++row)
        {
            const auto screenRow=std::int64_t(top)-1+row;
            if (screenRow<0 || screenRow>=height) continue;
            const auto destinationRow=flipped ? height-1-screenRow : screenRow;
            const auto source=(std::size_t(row)*popupWidth+firstColumn)*4;
            const auto destination=(std::size_t(destinationRow)*width+left+firstColumn)*4;
            std::copy_n(popup.begin()+source,static_cast<std::size_t>(lastColumn-firstColumn)*4,view.begin()+destination);
        }
        return true;
    }
    static LLVKWidgetImage::Rect displayRect(int width,int height,int mediaWidth,int mediaHeight);
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