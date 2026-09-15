#ifndef LLVKWINDOWMGR_H
#define LLVKWINDOWMGR_H

#include "llvkviewerui.h"
#include "llvkbrowser.h"
#include "llvktexturecache.h"
#include "llvkerror.h"

struct LLVKWindowMgr
{
    static constexpr std::uint64_t applicationServiceId=2;
    struct Configuration
    {
        LLVKViewerUi::Configuration ui;
        LLVKBrowser::Configuration browser;
        std::string loginPage;
        std::filesystem::path soundCacheDirectory;
        bool validation = true;
        std::uint32_t stopAfterFrames = 0;
        std::function<void(LLVKViewerUi&)> bindServices;
        LLVKViewerUi::HelpContext helpContext;
        std::function<void(LLVKViewerUi&,const LLVKWidgetPaint::Input&)> presentedFrame;
        LLVKTextureCache* textureCache=nullptr;
        LLVKSessionOwner* sessionOwner=nullptr;
        LLVKError::Code* failureCode=nullptr;
        LLVKError::Resolver errorResolver;
        std::function<std::optional<LLVKError::Code>()> fatalError;
    };
    static bool run(const Configuration& configuration, std::string& error);
};

#endif