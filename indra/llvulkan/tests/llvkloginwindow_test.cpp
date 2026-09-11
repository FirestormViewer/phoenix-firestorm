#include "linden_common.h"
#include "llvkloginwindow.h"
#include "llvkstartupsettings.h"
#include "lltut.h"
#include <windows.h>

namespace tut
{
    struct loginwindow_data {};
    typedef test_group<loginwindow_data> loginwindow_group;
    typedef loginwindow_group::object loginwindow_object;
    loginwindow_group loginwindow_tests("llvkloginwindow");

    template<> template<> void loginwindow_object::test<1>()
    {
        set_test_name("actual native login tree browser and Vulkan presentation share one window lifecycle");
        wchar_t executable[32768]{};
        ensure("executable path",GetModuleFileNameW(nullptr,executable,32768) != 0);
        const auto directory = std::filesystem::path(executable).parent_path();
        struct Profile
        {
            std::filesystem::path path = std::filesystem::temp_directory_path()/
                ("vulkan-login-test-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
            ~Profile() { std::error_code ignored; std::filesystem::remove_all(path,ignored); }
        } profile;
        const auto viewer = std::filesystem::path(LLVK_LOGIN_SOURCE);
        LLVKStartupSettings settings;
        std::string error;
        const bool loaded = settings.loadFile(viewer/"app_settings"/"settings.xml",true,true,true,error);
        ensure(error,loaded);
        LLVKLoginWindow::Configuration configuration;
        configuration.ui.skin.skinBaseDirectory = viewer/"skins";
        configuration.ui.skin.userAppDirectory = profile.path;
        configuration.ui.fontDescription = viewer/"fonts"/"fonts.xml";
        configuration.ui.fonts.platform = "Windows";
        configuration.ui.fonts.searchDirectories = {viewer/"fonts",std::filesystem::path(LLVK_LOGIN_PACKAGED_FONTS)};
        configuration.ui.settings = settings.values();
        configuration.browser.helperDirectory = directory;
        configuration.browser.localesDirectory = directory/"locales";
        configuration.browser.cacheDirectory = profile.path/"browser";
        configuration.loginPage = "data:text/html,<html><body style='margin:0;background:rgb(45,90,120)'><h1>Native browser validation</h1></body></html>";
        configuration.stopAfterFrames = 6;
        const bool ran = LLVKLoginWindow::run(configuration,error);
        ensure(error,ran);
        ensure("no OpenGL parent module",GetModuleHandleW(L"opengl32.dll") == nullptr);
        std::cout << "Native login window presented six frames with real widgets, fonts, skin and browser\n";
    }
}