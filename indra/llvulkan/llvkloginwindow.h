#ifndef LLVKLOGINWINDOW_H
#define LLVKLOGINWINDOW_H

#include "llvkloginui.h"
#include "llvkbrowser.h"

struct LLVKLoginWindow
{
    struct Configuration
    {
        LLVKLoginUi::Configuration ui;
        LLVKBrowser::Configuration browser;
        std::string loginPage;
        std::filesystem::path soundCacheDirectory;
        bool validation = true;
        std::uint32_t stopAfterFrames = 0;
        std::function<void(LLVKLoginUi&)> bindServices;
    };
    static bool run(const Configuration& configuration, std::string& error);
};

#endif