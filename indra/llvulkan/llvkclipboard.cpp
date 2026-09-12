#include "llvkclipboard.h"
#include "llsd.h"
#include "llstring.h"

#include <algorithm>
#include <cstring>

#if defined(_WIN32)
#include <windows.h>

namespace
{
    struct ClipboardAccess
    {
        bool open;
        explicit ClipboardAccess(HWND window) : open(OpenClipboard(window) != FALSE) {}
        ~ClipboardAccess() { if (open) CloseClipboard(); }
        ClipboardAccess(const ClipboardAccess&) = delete;
        ClipboardAccess& operator=(const ClipboardAccess&) = delete;
    };
    struct ClipboardMemory
    {
        HGLOBAL handle;
        bool transferred = false;
        explicit ClipboardMemory(std::size_t size) : handle(GlobalAlloc(GMEM_MOVEABLE,size)) {}
        ~ClipboardMemory() { if (handle && !transferred) GlobalFree(handle); }
        ClipboardMemory(const ClipboardMemory&) = delete;
        ClipboardMemory& operator=(const ClipboardMemory&) = delete;
    };
    struct ClipboardLock
    {
        HGLOBAL handle;
        void* data;
        explicit ClipboardLock(HGLOBAL memory) : handle(memory), data(GlobalLock(memory)) {}
        ~ClipboardLock() { if (data) GlobalUnlock(handle); }
        ClipboardLock(const ClipboardLock&) = delete;
        ClipboardLock& operator=(const ClipboardLock&) = delete;
    };
    class WindowsClipboard final : public LLVKClipboard
    {
    public:
        explicit WindowsClipboard(HWND window) : mWindow(window), mThread(GetCurrentThreadId()) {}
        bool available(bool primary) const override
        {
            return !primary && usable() && IsClipboardFormatAvailable(CF_UNICODETEXT) != FALSE;
        }
        std::optional<std::u32string> read(bool primary, std::string& error) override
        {
            error.clear();
            if (primary) { error = "Windows has no native primary selection"; return std::nullopt; }
            if (!usable()) { error = "Native clipboard window/thread is no longer valid"; return std::nullopt; }
            if (!available(false)) { error = "Unicode clipboard text is unavailable"; return std::nullopt; }
            ClipboardAccess access(mWindow);
            if (!access.open) { error = "Could not open the Windows clipboard"; return std::nullopt; }
            const auto memory = GetClipboardData(CF_UNICODETEXT);
            const auto bytes = memory ? GlobalSize(memory) : 0;
            if (!bytes || bytes > 8*1024*1024 || bytes%sizeof(wchar_t))
            { error = "Native clipboard Unicode storage is invalid or exceeds budget"; return std::nullopt; }
            ClipboardLock lock(memory);
            if (!lock.data) { error = "Could not lock Unicode clipboard text"; return std::nullopt; }
            const auto* begin = static_cast<const wchar_t*>(lock.data);
            const auto* end = begin+bytes/sizeof(wchar_t);
            const auto* terminator = std::find(begin,end,L'\0');
            if (terminator == end) { error = "Unicode clipboard text is not terminated"; return std::nullopt; }
            return decodeWindows(std::wstring_view(begin,terminator-begin),error);
        }
        bool write(std::u32string_view text, bool primary, std::string& error) override
        {
            error.clear();
            if (primary) { error = "Windows has no native primary selection"; return false; }
            if (!usable()) { error = "Native clipboard window/thread is no longer valid"; return false; }
            const auto encoded = encodeWindows(text,error);
            if (!encoded) return false;
            const auto bytes = (encoded->size()+1)*sizeof(wchar_t);
            ClipboardMemory memory(bytes);
            if (!memory.handle) { error = "Could not allocate Unicode clipboard text"; return false; }
            {
                ClipboardLock lock(memory.handle);
                if (!lock.data) { error = "Could not lock native clipboard allocation"; return false; }
                std::memcpy(lock.data,encoded->c_str(),bytes);
            }
            ClipboardAccess access(mWindow);
            if (!access.open) { error = "Could not open the Windows clipboard"; return false; }
            if (!EmptyClipboard()) { error = "Could not clear the Windows clipboard"; return false; }
            if (!SetClipboardData(CF_UNICODETEXT,memory.handle))
            { error = "Could not publish Unicode clipboard text"; return false; }
            memory.transferred = true;
            return true;
        }
    private:
        bool usable() const
        {
            DWORD process = 0;
            return mThread == GetCurrentThreadId() && IsWindow(mWindow) &&
                GetWindowThreadProcessId(mWindow,&process) == mThread && process == GetCurrentProcessId();
        }
        HWND mWindow;
        DWORD mThread;
    };
}
#endif

std::shared_ptr<LLVKClipboard> LLVKClipboard::forWindow(void* window, std::string& error)
{
    error.clear();
#if defined(_WIN32)
    const auto handle = static_cast<HWND>(window);
    DWORD process = 0;
    if (!handle || !IsWindow(handle) || GetWindowThreadProcessId(handle,&process) != GetCurrentThreadId() ||
        process != GetCurrentProcessId())
    { error = "Native clipboard requires a live window on its owning thread"; return {}; }
    return std::make_shared<WindowsClipboard>(handle);
#else
    error = "Native clipboard transport is not implemented on this platform";
    return {};
#endif
}

std::optional<std::wstring> LLVKClipboard::encodeWindows(std::u32string_view text, std::string& error)
{
    error.clear();
    if (text.size() > 1024*1024 || std::any_of(text.begin(),text.end(),[](char32_t character)
        { return !character || character > 0x10ffff || (character >= 0xd800 && character <= 0xdfff); }))
    { error = "Native clipboard requires bounded Unicode scalar text without NUL"; return std::nullopt; }
    LLWString normalized;
    normalized.reserve(text.size());
    for (const auto character : text)
    {
        if (character == U'\n') normalized.push_back(U'\r');
        normalized.push_back(character);
    }
    return ll_convert<std::wstring>(normalized);
}

std::optional<std::u32string> LLVKClipboard::decodeWindows(std::wstring_view text, std::string& error)
{
    error.clear();
    if (text.size() > 2*1024*1024 || text.find(L'\0') != std::wstring_view::npos)
    { error = "Native clipboard text exceeds budget or contains NUL"; return std::nullopt; }
#if defined(_WIN32)
    for (std::size_t position = 0; position < text.size(); ++position)
    {
        const auto unit = text[position];
        if (unit >= 0xdc00 && unit <= 0xdfff)
        { error = "Unpaired UTF-16 low surrogate in clipboard text"; return std::nullopt; }
        if (unit >= 0xd800 && unit <= 0xdbff)
        {
            if (position+1 == text.size() || text[position+1] < 0xdc00 || text[position+1] > 0xdfff)
            { error = "Unpaired UTF-16 high surrogate in clipboard text"; return std::nullopt; }
            ++position;
        }
    }
#endif
    const auto decoded = ll_convert<LLWString>(std::wstring(text));
    if (decoded.size() > 1024*1024) { error = "Native clipboard text exceeds scalar budget"; return std::nullopt; }
    std::u32string normalized;
    normalized.reserve(decoded.size());
    for (std::size_t position = 0; position < decoded.size(); ++position)
    {
        const auto character = decoded[position];
        if (character == U'\r' && position+1 < decoded.size() && decoded[position+1] == U'\n') continue;
        normalized.push_back(character);
    }
    return normalized;
}