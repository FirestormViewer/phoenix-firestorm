#include "llvkbrowsersurface.h"
#include "llvkwidgettree.h"
#include <atomic>

std::optional<LLVKWidgetTree::Id> LLVKWidgetTree::createBrowser(const Params& view, const LLVKControl::Params& control,
    const LLVKPanel::Params& panel, const Node::Browser& browser, Id parent, std::string& error)
{
    if (browser.textureWidth <= 0 || browser.textureHeight <= 0 || browser.textureWidth > 8192 || browser.textureHeight > 8192 ||
        std::int64_t(browser.textureWidth)*browser.textureHeight > 16*1024*1024)
    { error = "Native browser declaration exceeds texture dimensions"; return std::nullopt; }
    LLVKPanel panelState;
    panelState.params = panel;
    return createControlImpl(view,control,{},parent,error,{},{},{},{},{},std::move(panelState),{},{},{},{},browser);
}

bool LLVKBrowserSurface::resize(std::uint32_t width, std::uint32_t height, std::string& error)
{
    error.clear();
    if (!width || !height || width > 8192 || height > 8192 || std::uint64_t(width)*height > 16*1024*1024)
    { error = "Native browser surface exceeds supported dimensions"; return false; }
    if (mWidth == width && mHeight == height) return true;
    mWidth = width;
    mHeight = height;
    mFrame.reset();
    static std::atomic<std::uint64_t> nextEpoch{0};
    mEpoch = ++nextEpoch;
    ++mGeneration;
    return true;
}

bool LLVKBrowserSurface::publish(std::uint32_t width, std::uint32_t height,
    std::span<const std::uint8_t> topDownBgra, std::string& error)
{
    error.clear();
    if (width != mWidth || height != mHeight) return false;
    auto frame = LLVKWidgetImage::browserFrame(width,height,topDownBgra,error);
    if (!frame) return false;
    mFrame = std::move(frame);
    ++mGeneration;
    return true;
}