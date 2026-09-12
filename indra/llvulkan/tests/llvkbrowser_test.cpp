#include "linden_common.h"
#include "llvkbrowser.h"
#include "lltut.h"
#include <windows.h>
#include <chrono>

namespace tut
{
    struct browser_data {};
    typedef test_group<browser_data> browser_group;
    typedef browser_group::object browser_object;
    browser_group browser_tests("llvkbrowser");

    template<> template<> void browser_object::test<1>()
    {
        set_test_name("native Dullahan renders input and resized frames without viewer GL owners");
        wchar_t executable[32768]{};
        ensure("test executable path",GetModuleFileNameW(nullptr,executable,32768) != 0);
        const auto directory = std::filesystem::path(executable).parent_path();
        struct Profile
        {
            std::filesystem::path path = std::filesystem::temp_directory_path()/
                ("vulkan-browser-test-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
            ~Profile() { std::error_code ignored; std::filesystem::remove_all(path,ignored); }
        } profile;
        LLVKBrowser browser;
        LLVKBrowser::Configuration configuration;
        configuration.helperDirectory = directory;
        configuration.localesDirectory = directory/"locales";
        configuration.cacheDirectory = profile.path;
        configuration.width = configuration.height = 64;
        std::string error;
        const bool started = browser.start(configuration,error);
        ensure(error,started);
        ensure("no viewer OpenGL library initialized",GetModuleHandleW(L"opengl32.dll") == nullptr);
        const bool navigated = browser.navigate(
            "data:text/html,<html><body style='margin:0;background:rgb(0,0,255)'>"
            "<div id='top' style='height:50vh;background:rgb(255,0,0)' "
            "onclick='this.style.background=\"rgb(0,255,0)\"'></div></body></html>",error);
        ensure(error,navigated);
        const auto pumpUntil = [&](const auto& condition)
        {
            const auto deadline = std::chrono::steady_clock::now()+std::chrono::seconds(20);
            while (std::chrono::steady_clock::now() < deadline)
            {
                MSG message;
                while (PeekMessageW(&message,nullptr,0,0,PM_REMOVE))
                { TranslateMessage(&message); DispatchMessageW(&message); }
                const bool updated = browser.update(error);
                ensure(error,updated);
                browser.takeEvents();
                if (condition()) return true;
                MsgWaitForMultipleObjectsEx(0,nullptr,10,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
            }
            return false;
        };
        const auto matches = [&](std::uint32_t width,std::uint32_t height,bool green)
        {
            const auto frame = browser.surface().frame();
            if (!frame || frame->width() != width || frame->height() != height) return false;
            const auto pixels = frame->bottomUpRgba();
            const auto bottom = (std::size_t(4)*width+4)*4;
            const auto top = (std::size_t(height-5)*width+4)*4;
            return pixels[bottom] == 0 && pixels[bottom+1] == 0 && pixels[bottom+2] == 255 && pixels[bottom+3] == 255 &&
                pixels[top] == (green ? 0 : 255) && pixels[top+1] == (green ? 255 : 0) && pixels[top+2] == 0 && pixels[top+3] == 255;
        };
        const bool firstPixels = pumpUntil([&] { return matches(64,64,false); });
        ensure("actual red top and blue bottom browser pixels",firstPixels);
        const auto first = browser.surface().frame();
        ensure("browser pointer down",browser.pointer(8,8,0,true,error));
        ensure("browser pointer up",browser.pointer(8,8,0,false,error));
        ensure("mouse input changes browser pixels",pumpUntil([&] { return matches(64,64,true); }));
        ensure("old browser frame retained",first->bottomUpRgba()[(std::size_t(59)*64+4)*4] == 255);
        ensure("browser resize",browser.resize(80,48,error));
        ensure("resized browser pixels",pumpUntil([&] { return matches(80,48,true); }));
        LLVKBrowser second;
        auto conflicting = configuration;
        conflicting.cookiesEnabled = !configuration.cookiesEnabled;
        ensure("conflicting process settings rejected",!second.start(conflicting,error));
        ensure("independent second browser starts",second.start(configuration,error));
        ensure("second page navigation",second.navigate(
            "data:text/html,<body style='margin:0;background:rgb(255,255,0)' "
            "onclick='this.style.background=\"rgb(0,255,255)\"'>second</body>",error));
        const auto secondMatches = [&](bool clicked)
        {
            const auto frame = second.surface().frame();
            if (!frame) return false;
            const auto pixels = frame->bottomUpRgba();
            return pixels[0] == (clicked ? 0 : 255) && pixels[1] == 255 && pixels[2] == (clicked ? 255 : 0);
        };
        ensure("independent second page pixels",pumpUntil([&] { return secondMatches(false); }));
        ensure("first page remains unchanged",matches(80,48,true));
        ensure("second history page",second.navigate("data:text/html,<body style='background:rgb(0,0,0)'>history</body>",error));
        ensure("second history navigation completes",pumpUntil([&]
        {
            const auto state=second.navigation(error);
            return state && state->back && !state->loading && !secondMatches(false);
        }));
        ensure("native back command",second.command(LLVKBrowser::Command::Back,error));
        ensure("back restores second page",pumpUntil([&] { return secondMatches(false); }));
        ensure("forward is available",second.navigation(error)->forward);
        ensure("reload command",second.command(LLVKBrowser::Command::Reload,error));
        ensure("reload completes",pumpUntil([&] { return !second.navigation(error)->loading && secondMatches(false); }));
        ensure("request asynchronous close",browser.requestClose(error));
        ensure("close completes before owner release",pumpUntil([&] { return browser.state() == LLVKBrowser::State::Closed; }));
        const auto pumpSecond = [&](const auto& condition)
        {
            const auto deadline = std::chrono::steady_clock::now()+std::chrono::seconds(20);
            while (std::chrono::steady_clock::now() < deadline)
            {
                MSG message;
                while (PeekMessageW(&message,nullptr,0,0,PM_REMOVE))
                { TranslateMessage(&message); DispatchMessageW(&message); }
                ensure("second runtime pump",second.update(error));
                if (condition()) return true;
                MsgWaitForMultipleObjectsEx(0,nullptr,10,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
            }
            return false;
        };
        ensure("second pointer down after first close",second.pointer(8,8,0,true,error));
        ensure("second pointer up after first close",second.pointer(8,8,0,false,error));
        ensure("second remains interactive after initial owner closes",pumpSecond([&] { return secondMatches(true); }));
        LLVKBrowser reopened;
        ensure("new view after original owner closes",reopened.start(configuration,error));
        ensure("new view close",reopened.requestClose(error));
        ensure("new view retires independently",pumpSecond([&]
        {
            ensure("new view update",reopened.update(error));
            return reopened.state() == LLVKBrowser::State::Closed;
        }));
        ensure("final browser close",second.requestClose(error));
        ensure("final runtime retirement",pumpSecond([&] { return second.state() == LLVKBrowser::State::Closed; }));
        LLVKBrowser afterShutdown;
        ensure("CEF restart remains forbidden",!afterShutdown.start(configuration,error));
        ensure("multiple views did not load OpenGL",GetModuleHandleW(L"opengl32.dll") == nullptr);
        std::cout << "Native browser: independent views, pixels, input, resize and last-view shutdown verified\n";
    }
}