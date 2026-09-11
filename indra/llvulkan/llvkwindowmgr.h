#ifndef LLVKWINDOWMGR_H
#define LLVKWINDOWMGR_H

#include "llvkviewerui.h"
#include "llvkbrowser.h"

struct LLVKWindowMgr
{
    struct Configuration
    {
        LLVKViewerUi::Configuration ui;
        LLVKBrowser::Configuration browser;
        std::string loginPage;
        std::filesystem::path soundCacheDirectory;
        bool validation = true;
        std::uint32_t stopAfterFrames = 0;
        std::function<void(LLVKViewerUi&)> bindServices;
    };
    static bool run(const Configuration& configuration, std::string& error);
};

#endif