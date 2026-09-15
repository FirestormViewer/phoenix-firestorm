#include "llvkbrowser.h"
#include <dullahan.h>
#include <mutex>
#include <optional>
#include <chrono>
#include <cmath>
#include <windows.h>

namespace
{
    std::mutex runtimeMutex;
    std::optional<LLVKBrowser::Configuration> runtimeConfiguration;
    std::thread::id runtimeThread;
    std::size_t runtimeViews = 0;
    std::string utf8Path(const std::filesystem::path& path)
    {
        const auto bytes = path.u8string();
        return {reinterpret_cast<const char*>(bytes.data()),bytes.size()};
    }
}

LLVKBrowser::LLVKBrowser() : mThread(std::this_thread::get_id()) {}

LLVKBrowser::~LLVKBrowser()
{
    if (!mInitialized) return;
    if (std::this_thread::get_id() != mThread) std::terminate();
    std::string error;
    requestClose(error);
    const auto deadline = std::chrono::steady_clock::now()+std::chrono::seconds(15);
    while (mInitialized)
    {
        update(error);
        if (std::chrono::steady_clock::now() >= deadline) std::terminate();
        if (mInitialized) MsgWaitForMultipleObjectsEx(0,nullptr,10,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
    }
}

bool LLVKBrowser::onThread(std::string& error) const
{
    error.clear();
    if (std::this_thread::get_id() == mThread) return true;
    error = "Native browser must be used on its owning thread";
    return false;
}

bool LLVKBrowser::running(std::string& error) const
{
    if (!onThread(error)) return false;
    if (mState == State::Running && !mCallbackFailed) return true;
    error = "Native browser is not running";
    return false;
}

void LLVKBrowser::enqueue(Event event) noexcept
{
    try
    {
        if (mEvents.size() >= 1024 || event.text.size()+event.detail.size() > 1024*1024)
        { mCallbackFailed = true; return; }
        mEvents.push_back(std::move(event));
    }
    catch (...) { mCallbackFailed = true; }
}

bool LLVKBrowser::start(const Configuration& configuration, std::string& error)
{
    if (!onThread(error)) return false;
    if (mState != State::Fresh) { error = "Native browser cannot be restarted"; return false; }
    std::lock_guard runtimeLock(runtimeMutex);
    if (runtimeConfiguration)
    {
        const auto& active = *runtimeConfiguration;
        if (runtimeThread != mThread || !runtimeViews)
        { error = "Native CEF runtime cannot change threads or restart after final shutdown"; return false; }
        if (active.helperDirectory != configuration.helperDirectory || active.localesDirectory != configuration.localesDirectory ||
            active.cacheDirectory != configuration.cacheDirectory || active.language != configuration.language ||
            active.userAgent != configuration.userAgent || active.cookiesEnabled != configuration.cookiesEnabled ||
            active.proxy.type != configuration.proxy.type || active.proxy.host != configuration.proxy.host ||
            active.proxy.port != configuration.proxy.port)
        { error = "Native browser views must share CEF process configuration"; return false; }
    }
    if (!configuration.helperDirectory.is_absolute() || !configuration.localesDirectory.is_absolute() ||
        !configuration.cacheDirectory.is_absolute() ||
        !std::filesystem::is_regular_file(configuration.helperDirectory/"dullahan_host.exe") ||
        !std::filesystem::is_directory(configuration.localesDirectory))
    { error = "Native browser requires absolute helper, locale and private cache paths"; return false; }
    if (!mSurface.resize(std::min(configuration.width,2048u),std::min(configuration.height,2048u),error)) return false;
    std::error_code directoryError;
    std::filesystem::create_directories(configuration.cacheDirectory,directoryError);
    if (directoryError) { error = "Cannot create native browser cache directory: " + directoryError.message(); return false; }
    mEngine = std::make_unique<dullahan>();
    mEngine->setOnPageChangedCallback([this](const unsigned char* pixels,int x,int y,int width,int height)
    {
        try
        {
            if (!pixels || x || y || width <= 0 || height <= 0) { mCallbackFailed = true; return; }
            if (std::uint32_t(width) != mSurface.width() || std::uint32_t(height) != mSurface.height()) return;
            std::string problem;
            if (!mSurface.publish(width,height,{pixels,std::size_t(width)*height*4},problem)) mCallbackFailed = true;
        }
        catch (...) { mCallbackFailed = true; }
    });
    mEngine->setOnRequestExitCallback([this] { mExitReady = true; });
    mEngine->setOnAddressChangeCallback([this](const std::string url) { enqueue({EventKind::Address,url}); });
    mEngine->setOnLoadStartCallback([this] { enqueue({EventKind::LoadStart}); });
    mEngine->setOnLoadEndCallback([this](int status,const std::string url) { enqueue({EventKind::LoadEnd,url,{},status}); });
    mEngine->setOnLoadErrorCallback([this](int status,const std::string message,const std::string url)
        { enqueue({EventKind::LoadError,url,message,status}); });
    mEngine->setOnOpenPopupCallback([this](const std::string url,const std::string target) { enqueue({EventKind::Popup,url,target}); });
    mEngine->setOnCustomSchemeURLCallback([this](const std::string url,bool gesture,bool redirect)
        { enqueue({EventKind::CustomScheme,url,{},0,gesture,redirect}); });
    mEngine->setOnCursorChangedCallback([this](dullahan::ECursorType cursor) { enqueue({EventKind::Cursor,{},{},int(cursor)}); });
    mEngine->setOnStatusMessageCallback([this](const std::string text) { enqueue({EventKind::Status,text}); });
    mEngine->setOnTitleChangeCallback([this](const std::string text) { enqueue({EventKind::Title,text}); });
    mEngine->setOnTooltipCallback([this](const std::string text) { enqueue({EventKind::Tooltip,text}); });
    mEngine->setOnHTTPAuthCallback([](const std::string,const std::string,std::string&,std::string&) { return false; });
    mEngine->setOnFileDialogCallback([](dullahan::EFileDialogType,const std::string,const std::string,const std::string,bool& useDefault)
        { useDefault = false; return std::vector<std::string>{}; });
    mEngine->setOnJSDialogCallback([](const std::string,const std::string,const std::string) { return true; });
    mEngine->setOnJSBeforeUnloadCallback([] { return true; });
    mEngine->setCustomSchemes({"secondlife"});
    dullahan::dullahan_settings settings;
    settings.host_process_path = utf8Path(configuration.helperDirectory);
    settings.locales_dir_path = utf8Path(configuration.localesDirectory);
    settings.root_cache_path = utf8Path(configuration.cacheDirectory);
    settings.log_file = utf8Path(configuration.cacheDirectory/"browser.log");
    settings.initial_width = mSurface.width();
    settings.initial_height = mSurface.height();
    settings.accept_language_list = configuration.language;
    if (configuration.proxy.type!=LLVKProxy::Type::None)
    {
        if (configuration.proxy.type!=LLVKProxy::Type::Http || configuration.proxy.host.empty() ||
            configuration.proxy.port<1 || configuration.proxy.port>65535)
        { error="Invalid native browser HTTP proxy configuration"; return false; }
        const auto& host=configuration.proxy.host;
        settings.proxy_host_port=(host.find(':')!=std::string::npos && host.front()!='[' ? "["+host+"]" : host)+
            ":"+std::to_string(configuration.proxy.port);
    }
    settings.user_agent_substring = mEngine->makeCompatibleUserAgentString(configuration.userAgent);
    settings.disable_gpu = true;
    settings.webgl_enabled = false;
    settings.flip_pixels_y = false;
    settings.flip_mouse_y = false;
    settings.background_color = 0xffffffff;
    settings.flash_enabled = settings.java_enabled = settings.plugins_enabled = false;
    settings.media_stream_enabled = false;
    settings.javascript_enabled = configuration.javascriptEnabled;
    settings.cookies_enabled = configuration.cookiesEnabled;
    settings.disable_web_security = settings.file_access_from_file_urls = false;
    settings.frame_rate = 60;
    if (!runtimeConfiguration)
    {
        runtimeConfiguration = configuration;
        runtimeThread = mThread;
    }
    if (!mEngine->init(settings))
    { mState = State::Failed; error = "Native Dullahan initialization failed"; return false; }
    ++runtimeViews;
    mInitialized = true;
    mState = State::Running;
    return true;
}

bool LLVKBrowser::update(std::string& error)
{
    if (!onThread(error)) return false;
    if (!mInitialized) return mState == State::Closed;
    mEngine->update();
    if (mExitReady)
    {
        mEngine->shutdown();
        mInitialized = false;
        mEngine.reset();
        mState = State::Closed;
        std::lock_guard runtimeLock(runtimeMutex);
        --runtimeViews;
    }
    if (mCallbackFailed) { error = "Native browser callback publication failed or exceeded its budget"; return false; }
    return true;
}

bool LLVKBrowser::navigate(const std::string& url, std::string& error)
{
    if (!running(error)) return false;
    if (url.empty() || url.size() > 1024*1024 || url.find('\0') != url.npos)
    { error = "Invalid native browser navigation URL"; return false; }
    mEngine->navigate(url);
    return true;
}

bool LLVKBrowser::command(Command command,std::string& error)
{
    if (!running(error)) return false;
    switch (command)
    {
    case Command::Back: if (mEngine->canGoBack()) mEngine->goBack(); break;
    case Command::Forward: if (mEngine->canGoForward()) mEngine->goForward(); break;
    case Command::Reload: mEngine->reload(true); break;
    case Command::Stop: mEngine->stop(); break;
    default: error="Unknown native browser navigation command"; return false;
    }
    return true;
}

std::optional<LLVKBrowser::Navigation> LLVKBrowser::navigation(std::string& error) const
{
    if (!running(error)) return std::nullopt;
    return Navigation{mEngine->canGoBack(),mEngine->canGoForward(),mEngine->isLoading()};
}

bool LLVKBrowser::setPageScale(float scale,std::string& error)
{
    if (!running(error)) return false;
    if (!std::isfinite(scale) || scale<=0.f || scale>7.f) { error="Invalid native browser page scale"; return false; }
    if (scale!=mPageScale) { mEngine->setPageZoom(scale); mPageScale=scale; }
    return true;
}

bool LLVKBrowser::resize(std::uint32_t width,std::uint32_t height,std::string& error)
{
    width=std::min(width,2048u); height=std::min(height,2048u);
    if (mSurface.width()==width && mSurface.height()==height) return running(error);
    if (!running(error) || !mSurface.resize(width,height,error)) return false;
    mEngine->setSize(int(width),int(height));
    return true;
}

bool LLVKBrowser::requestClose(std::string& error)
{
    if (!onThread(error)) return false;
    if (!mInitialized || mState == State::Closing) return true;
    mState = State::Closing;
    mEngine->requestExit();
    return true;
}

bool LLVKBrowser::pointer(int x,int y,int button,bool down,std::string& error)
{
    if (!running(error)) return false;
    if (button < 0 || button > 2) { error = "Invalid native browser mouse button"; return false; }
    if (down) mEngine->setFocus();
    mEngine->mouseButton(static_cast<dullahan::EMouseButton>(button),down ? dullahan::ME_MOUSE_DOWN : dullahan::ME_MOUSE_UP,x,y);
    return true;
}

bool LLVKBrowser::hover(int x,int y,std::string& error)
{
    if (!running(error)) return false;
    mEngine->mouseMove(x,y);
    return true;
}

bool LLVKBrowser::wheel(int x,int y,int horizontal,int vertical,std::string& error)
{
    if (!running(error)) return false;
    mEngine->mouseWheel(x,y,horizontal,vertical);
    return true;
}

bool LLVKBrowser::keyboard(std::uint32_t message,std::uint32_t parameter,std::uint64_t flags,std::string& error)
{
    if (!running(error)) return false;
    mEngine->nativeKeyboardEventWin(message,parameter,flags);
    return true;
}

std::vector<LLVKBrowser::Event> LLVKBrowser::takeEvents()
{
    std::string error;
    if (!onThread(error)) return {};
    auto events = std::move(mEvents);
    mEvents.clear();
    return events;
}

std::string LLVKBrowser::versionInfo(std::string& error) const
{
    if (!running(error)) return {};
    return "Dullahan: "+mEngine->dullahan_version(false)+"\n  CEF: "+mEngine->dullahan_cef_version(false)+
        "\n  Chromium: "+mEngine->dullahan_chrome_version(false);
}