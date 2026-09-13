#include "llvkerror.h"
#include <cstdio>
#include <windows.h>

namespace
{
    void unavailable() noexcept
    {
        constexpr auto message = "native-error: operating-system error presentation unavailable\n";
        OutputDebugStringA(message);
        std::fputs(message, stderr);
    }
}

bool llvkPresentErrorFallback(const LLVKError& error, void* owner) noexcept
{
    static thread_local bool presenting = false;
    if (presenting) { unavailable(); return false; }
    presenting = true;
    struct Reset
    {
        bool& flag;
        ~Reset() { flag = false; }
    } reset{presenting};
    auto window = static_cast<HWND>(owner);
    if (window && !IsWindow(window)) window = nullptr;
    try
    {
        const auto message = error.format();
        const std::wstring title(message.title.begin(), message.title.end());
        const std::wstring body(message.body.begin(), message.body.end());
        const auto icon = error.policy().severity == LLVKError::Severity::Warning ? MB_ICONWARNING : MB_ICONERROR;
        if (MessageBoxW(window, body.c_str(), title.c_str(), MB_OK | icon | MB_TASKMODAL) == IDOK) return true;
    }
    catch (...)
    {
        if (MessageBoxW(window, L"A native viewer error occurred. Acknowledge this message to return to the caller. No retry has been performed.",
            L"Vulkanstorm native error", MB_OK | MB_ICONERROR | MB_TASKMODAL) == IDOK) return true;
    }
    unavailable();
    return false;
}