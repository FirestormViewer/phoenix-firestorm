#ifndef LLVKBROWSER_H
#define LLVKBROWSER_H

#include "llvkbrowsersurface.h"
#include <filesystem>
#include <thread>

class dullahan;

class LLVKBrowser final
{
public:
    struct Configuration
    {
        std::filesystem::path helperDirectory, localesDirectory, cacheDirectory;
        std::uint32_t width = 1024, height = 598;
        std::string language = "en", userAgent;
    };
    enum class State { Fresh, Running, Closing, Closed, Failed };
    enum class EventKind { Address, LoadStart, LoadEnd, LoadError, Popup, CustomScheme, Cursor, Status, Title, Tooltip };
    struct Event
    {
        EventKind kind;
        std::string text, detail;
        int code = 0;
        bool userGesture = false, redirect = false;
    };
    LLVKBrowser();
    ~LLVKBrowser();
    LLVKBrowser(const LLVKBrowser&) = delete;
    LLVKBrowser& operator=(const LLVKBrowser&) = delete;
    bool start(const Configuration& configuration, std::string& error);
    bool update(std::string& error);
    bool navigate(const std::string& url, std::string& error);
    bool resize(std::uint32_t width, std::uint32_t height, std::string& error);
    bool requestClose(std::string& error);
    bool pointer(int x, int y, int button, bool down, std::string& error);
    bool hover(int x, int y, std::string& error);
    bool wheel(int x, int y, int horizontal, int vertical, std::string& error);
    bool keyboard(std::uint32_t message, std::uint32_t parameter, std::uint64_t flags, std::string& error);
    std::vector<Event> takeEvents();
    State state() const noexcept { return mState; }
    const LLVKBrowserSurface& surface() const noexcept { return mSurface; }
private:
    bool onThread(std::string& error) const;
    bool running(std::string& error) const;
    void enqueue(Event event) noexcept;
    std::unique_ptr<dullahan> mEngine;
    std::thread::id mThread;
    LLVKBrowserSurface mSurface;
    std::vector<Event> mEvents;
    State mState = State::Fresh;
    bool mInitialized = false, mExitReady = false, mCallbackFailed = false;
};

#endif