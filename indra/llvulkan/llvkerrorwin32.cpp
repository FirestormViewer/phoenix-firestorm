#include "llvkerror.h"
#include <cstdio>
#include <windows.h>

namespace
{
    std::wstring unicode(const std::string& text)
    {
        const int size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0);
        if (!size) return {};
        std::wstring result(size,L'\0');
        if (!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),result.data(),size)) return {};
        return result;
    }

    void unavailable() noexcept
    {
        constexpr auto message = "native-error: operating-system error presentation unavailable\n";
        OutputDebugStringA(message);
        std::fputs(message, stderr);
    }
}

bool llvkPresentErrorFallback(const LLVKError& error, void* owner, const LLVKError::Resolver& resolver) noexcept
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
        const auto message = error.format(resolver);
        auto title = unicode(message.title);
        auto body = unicode(message.body);
        if (title.empty() || body.empty())
        {
            const auto fallback=error.format();
            title=unicode(fallback.title);
            body=unicode(fallback.body);
        }
        const auto icon = error.policy().severity == LLVKError::Severity::Warning ? MB_ICONWARNING : MB_ICONERROR;
        if (MessageBoxW(window, body.c_str(), title.c_str(), MB_OK | icon | MB_TASKMODAL) == IDOK) return true;
    }
    catch (...)
    {
        const auto body=error.code==LLVKError::Code::OutOfMemory ?
            L"The viewer has run out of memory and must close. Close other applications before restarting." :
            L"A native viewer error occurred. Acknowledge this message to return to the caller. No retry has been performed.";
        if (MessageBoxW(window, body,
            L"Vulkanstorm native error", MB_OK | MB_ICONERROR | MB_TASKMODAL) == IDOK) return true;
    }
    unavailable();
    return false;
}