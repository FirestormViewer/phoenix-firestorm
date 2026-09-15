#include "llvkstartup.h"
#include "llvkwindowmgr.h"
#include "llvksettingsmgr.h"
#include "llvkpreferencesbackup.h"
#include "llvkstartupstatus.h"
#include "llvkerror.h"
#include "llstring.h"
#include "llerror.h"
#include "llerrorcontrol.h"
#include <atomic>
#include <cstring>
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

struct LLVKFatalReporting::Impl
{
    std::filesystem::path record;
    Presenter presenter;
    std::atomic<std::shared_ptr<const LLVKError::Resolver>> resolver;
    std::atomic_flag reported=ATOMIC_FLAG_INIT;
    std::atomic<std::uint32_t> failure{0};
    LLError::RecorderPtr recorder;
    LLError::LLUserWarningMsg::Handler previousWarning;
    std::string previousOomTitle,previousOomMessage;
    void report(LLVKError::Code code=LLVKError::Code::Unexpected) noexcept
    {
        if (reported.test_and_set()) return;
        failure.store(static_cast<std::uint32_t>(code));
        const char* diagnostic=code==LLVKError::Code::OutOfMemory ?
            "native-error code=1014 operation=bootstrap generation=1 attempt=0 severity=2 recovery=0\n" :
            code==LLVKError::Code::MissingFiles ?
            "native-error code=1015 operation=bootstrap generation=1 attempt=0 severity=2 recovery=0\n" :
            "native-error code=1000 operation=bootstrap generation=1 attempt=0 severity=2 recovery=0\n";
        const auto bytes=static_cast<DWORD>(std::strlen(diagnostic));
        const auto file=CreateFileW(record.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if (file!=INVALID_HANDLE_VALUE)
        {
            DWORD written=0;
            if (!WriteFile(file,diagnostic,bytes,&written,nullptr) || written!=bytes || !FlushFileBuffers(file))
                OutputDebugStringA("native-error: fatal record write failed\n");
            CloseHandle(file);
        }
        else OutputDebugStringA("native-error: fatal record unavailable\n");
        try
        {
            const LLVKError failure{code,LLVKError::Operation::Bootstrap,1,0};
            if (presenter) presenter(failure);
            else
            {
                const auto catalog=resolver.load();
                llvkPresentErrorFallback(failure,nullptr,catalog ? *catalog : LLVKError::Resolver{});
            }
        }
        catch (...) { OutputDebugStringA("native-error: fatal presentation failed\n"); }
    }
};

LLVKFatalReporting::LLVKFatalReporting(const std::filesystem::path& record,Presenter presenter)
    : mImpl(std::make_unique<Impl>())
{
    mImpl->record=record;
    mImpl->presenter=std::move(presenter);
    std::error_code ignored;
    std::filesystem::create_directories(record.parent_path(),ignored);
    mImpl->previousWarning=LLError::LLUserWarningMsg::getHandler();
    LLError::LLUserWarningMsg::getOutOfMemoryStrings(mImpl->previousOomTitle,mImpl->previousOomMessage);
    try
    {
        LLError::LLUserWarningMsg::setOutOfMemoryStrings("Vulkanstorm native error","The viewer has run out of memory.");
        mImpl->recorder=LLError::addGenericRecorder([state=mImpl.get()](LLError::ELevel level,const std::string&)
        { if (level==LLError::LEVEL_ERROR) state->report(); });
        LLError::LLUserWarningMsg::setHandler([state=mImpl.get()](const std::string&,const std::string&,S32 code)
        {
            state->report(code==LLError::LLUserWarningMsg::ERROR_BAD_ALLOC ? LLVKError::Code::OutOfMemory :
                code==LLError::LLUserWarningMsg::ERROR_MISSING_FILES ? LLVKError::Code::MissingFiles : LLVKError::Code::Unexpected);
        });
    }
    catch (...)
    {
        LLError::removeRecorder(mImpl->recorder);
        LLError::LLUserWarningMsg::setOutOfMemoryStrings(mImpl->previousOomTitle,mImpl->previousOomMessage);
        throw;
    }
}

LLVKFatalReporting::~LLVKFatalReporting()
{
    LLError::LLUserWarningMsg::setHandler(mImpl->previousWarning);
    LLError::LLUserWarningMsg::setOutOfMemoryStrings(mImpl->previousOomTitle,mImpl->previousOomMessage);
    LLError::removeRecorder(mImpl->recorder);
}

void LLVKFatalReporting::setErrorResolver(LLVKError::Resolver resolver)
{
    mImpl->resolver.store(std::make_shared<const LLVKError::Resolver>(std::move(resolver)));
}

std::optional<LLVKError::Code> LLVKFatalReporting::failure() const noexcept
{
    const auto value=mImpl->failure.load();
    return value ? std::optional(static_cast<LLVKError::Code>(value)) : std::nullopt;
}

LLVKSessionOwner::Code LLVKApplicationCache::acquire(const LLVKSessionOwner::Context&)
{
    std::string error;
    return mCache.start(mConfiguration,error) ? LLVKSessionOwner::Code::Ok : LLVKSessionOwner::Code::ServiceFailed;
}

LLVKSessionOwner::Code LLVKApplicationCache::retire()
{
    std::string error;
    struct Hide
    {
        std::shared_ptr<LLVKStartupStatus> status;
        ~Hide() { if (status) status->hide(); }
    } hide{mStatus};
    if (mStatus)
    {
        try { mStatus->show("ShuttingDown",error); }
        catch (...) {}
    }
    return mCache.stop(error) ? LLVKSessionOwner::Code::Ok : LLVKSessionOwner::Code::CleanupFailed;
}

namespace
{
    struct ApplicationSession
    {
        std::unique_ptr<LLVKSessionOwner> owner=std::make_unique<LLVKSessionOwner>();
        LLVKError::Resolver errorResolver;
        ~ApplicationSession()
        {
            try
            {
                const auto snapshot=owner->snapshot();
                if (snapshot.state==LLVKSessionOwner::State::Stopped) return;
                if (snapshot.state!=LLVKSessionOwner::State::Disconnecting && owner->shutdown().ok()) return;
            }
            catch (...) {}
            owner.release();
            llvkPresentErrorFallback({LLVKError::Code::ShutdownFailed,LLVKError::Operation::Shutdown,1,0},nullptr,errorResolver);
        }
    };
}

std::optional<int> llvkStartup(const std::wstring& commandLine,const std::string& profileName,const std::string& shortVersion,
    LLControlGroup& globalGroup,LLControlGroup& accountGroup,LLControlGroup& crashGroup,LLControlGroup& warningGroup,
    const LLVKProxy::CredentialFactory& proxyCredentials,const std::function<void()>& clearSpamQueues,
    const std::string& executionMarkerName)
{
    int count = 0;
    auto arguments = CommandLineToArgvW((L"viewer "+commandLine).c_str(),&count);
    if (!arguments) return std::nullopt;
    struct Arguments { LPWSTR* value; ~Arguments() { LocalFree(value); } } ownedArguments{arguments};
    std::map<std::string,std::string> overrides;
    std::string settingsFile = "settings.xml", sessionFile;
    bool unsupported = false;
    for (int index = 1; index < count; ++index)
    {
        const std::wstring option = arguments[index];
        if (option == L"--set")
        {
            while (index+2 < count && !std::wstring_view(arguments[index+1]).starts_with(L"--"))
            {
                const auto name = ll_convert_wide_to_string(arguments[++index]);
                overrides[name] = ll_convert_wide_to_string(arguments[++index]);
            }
        }
        else if ((option == L"--settings" || option == L"--sessionsettings") && index+1 < count)
        {
            const auto value = ll_convert_wide_to_string(arguments[++index]);
            (option == L"--settings" ? settingsFile : sessionFile) = value;
        }
        else unsupported = true;
    }
    wchar_t executable[32768]{};
    if (!GetModuleFileNameW(nullptr,executable,32768)) return std::nullopt;
    const auto directory = std::filesystem::path(executable).parent_path();
    PWSTR roaming = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData,0,nullptr,&roaming))) return std::nullopt;
    const auto profile = std::filesystem::path(roaming)/std::filesystem::path(std::u8string(profileName.begin(),profileName.end()));
    CoTaskMemFree(roaming);
    const auto userSettings = profile/"user_settings";
    LLVKSettingsMgr settings;
    std::string error;
    std::string appliedSettingsMode, modeError;
    const auto requestedSessionFile=sessionFile;
    const auto loadSettings=[&]()
    {
        settings.group().cleanup();
        sessionFile=requestedSessionFile; appliedSettingsMode.clear(); modeError.clear();
        if (!settings.loadFile(directory/"app_settings"/"settings.xml",true,true,true,error)) return false;
        settings.loadFile(directory/"app_settings"/"settings_install.xml",false,true,true,error);
        settings.loadFile(userSettings/("fsdata_defaults."+shortVersion+".xml"),false,true,true,error);
        settings.loadFile(userSettings/std::filesystem::path(std::u8string(settingsFile.begin(),settingsFile.end())),false,false,true,error);
        if (sessionFile.empty())
        {
            const auto* session = settings.find("SessionSettingsFile");
            if (session) sessionFile = session->getValue().asString();
            const auto* first = settings.find("FirstRunThisInstall");
            if (sessionFile.empty() && first && first->getValue().asBoolean()) sessionFile = "settings_firestorm.xml";
        }
        if (!sessionFile.empty())
        {
            const auto modePath=directory/"app_settings"/sessionFile;
            if (settings.loadFile(modePath,true,true,false,modeError)) appliedSettingsMode=sessionFile;
        }
        settings.loadFile(userSettings/std::filesystem::path(std::u8string(settingsFile.begin(),settingsFile.end())),false,false,true,error);
        return true;
    };
    const bool defaults=loadSettings();
    const auto explicitBackend = overrides.find("RenderBackend");
    const auto* savedBackend = settings.find("RenderBackend");
    const auto backend = explicitBackend != overrides.end() ? explicitBackend->second : savedBackend ? savedBackend->getValue().asString() : std::string();
    if (backend != "Vulkan") return std::nullopt;
    using Code = LLVKError::Code;
    using Operation = LLVKError::Operation;
    LLVKErrorGate errorGate;
    LLVKError::Resolver errorResolver;
    LLVKFatalReporting fatalReporting(profile/"logs"/("native-fatal-"+std::to_string(GetCurrentProcessId())+".log"),{});
    const auto fail = [&](Code code, Operation operation = Operation::Bootstrap) -> std::optional<int>
    {
        if (fatalReporting.failure()) return -1;
        errorGate.begin(operation,1);
        const LLVKError failure{code,operation,1,1};
        if (errorGate.accept(failure))
        {
            try { LL_WARNS("NativeStartup") << failure.diagnostic() << LL_ENDL; }
            catch (...) {}
            llvkPresentErrorFallback(failure,nullptr,errorResolver);
        }
        return -1;
    };
    try
    {
    if (!defaults) return fail(Code::DefaultSettings,Operation::Settings);
    if (!modeError.empty()) return fail(Code::SettingsMode,Operation::Settings);
    if (unsupported) return fail(Code::UnsupportedArguments);
    settings=LLVKSettingsMgr(globalGroup);
    if (!loadSettings()) return fail(Code::DefaultSettings,Operation::Settings);
    const auto reset=LLVKSettingsMgr::consumeReset(profile,std::filesystem::path(std::u8string(settingsFile.begin(),settingsFile.end())),error);
    if (!reset) return fail(Code::SettingsWrite,Operation::Settings);
    if (*reset)
    {
        if (!loadSettings()) return fail(Code::DefaultSettings,Operation::Settings);
        if (!modeError.empty()) return fail(Code::SettingsMode,Operation::Settings);
        if (!settings.set("RenderBackend",LLSD("Vulkan"),false,error)) return fail(Code::SettingsRead,Operation::Settings);
    }
    for (const auto& [name,value] : overrides)
        if (!settings.set(name,LLSD(value),false,error)) return fail(Code::SettingsRead,Operation::Settings);
    auto values = settings.values();
    const auto language=LLVKViewerUi::uiLanguage(values);
    if (!settings.set("Language",values.at("Language"),false,error)) return fail(Code::SettingsRead,Operation::Settings);
    const auto stringValue = [&](const char* name,const std::string& fallback = {})
    { const auto found = values.find(name); return found == values.end() || found->second.asString().empty() ? fallback : found->second.asString(); };
    auto startupStatus=std::make_shared<LLVKStartupStatus>();
    LLVKSkinFiles::Configuration startupSkin;
    startupSkin.executableDirectory=directory;
    startupSkin.workingDirectory=std::filesystem::current_path();
    startupSkin.skinBaseDirectory=directory/"skins";
    startupSkin.userAppDirectory=profile;
    startupSkin.skin=stringValue("SkinCurrent","default");
    startupSkin.theme=stringValue("SkinCurrentTheme");
    startupSkin.language=language;
    if (!startupStatus->load(startupSkin,"Vulkanstorm",error)) return fail(Code::StartupResources);
    errorResolver=startupStatus->errorResolver();
    fatalReporting.setErrorResolver(errorResolver);
    const auto browserDirectory = directory/"llplugin";
    if (!SetDllDirectoryW(browserDirectory.c_str())) return fail(Code::BrowserUnavailable,Operation::Browser);
    struct DllDirectory { ~DllDirectory() { SetDllDirectoryW(nullptr); } } dllDirectory;
    LLVKWindowMgr::Configuration configuration;
    configuration.errorResolver=errorResolver;
    configuration.fatalError=[&fatalReporting] { return fatalReporting.failure(); };
    Code windowFailure = Code::WindowUnavailable;
    configuration.failureCode = &windowFailure;
    configuration.ui.clearSpamQueues=clearSpamQueues;
    if (proxyCredentials)
    {
        auto credentials=proxyCredentials(userSettings/"bin_conf.dat");
        configuration.ui.loadProxyCredentials=std::move(credentials.load);
        configuration.ui.saveProxyCredentials=std::move(credentials.save);
    }
    configuration.ui.settings = values;
    configuration.ui.userColorsFile=userSettings/"colors.xml";
    configuration.ui.settingsGroup=&globalGroup;
    configuration.ui.accountSettingsGroup=&accountGroup;
    LLVKSettingsMgr warnings(warningGroup);
    const auto warningsFile=userSettings/"ignorable_dialogs.xml";
    if (!warnings.loadFile(warningsFile,false,false,true,error)) return fail(Code::SettingsRead,Operation::Settings);
    configuration.ui.warningSettingsGroup=&warningGroup;
    configuration.ui.saveWarningPreferences=[&warnings,warningsFile](const auto& changes,std::string& problem)
    { return warnings.saveChanges(warningsFile,changes,problem); };
    configuration.ui.settingDefaults = settings.defaults();
    LLVKSettingsMgr accountSettings(accountGroup);
    if (!accountSettings.loadFile(directory/"app_settings"/"settings_per_account.xml",true,true,true,error)) return fail(Code::SettingsRead,Operation::Settings);
    configuration.ui.accountSettings=accountSettings.values();
    configuration.ui.accountDefaults=accountSettings.defaults();
    LLVKSettingsMgr crashSettings(crashGroup);
    const auto crashFile=userSettings/"settings_crash_behavior.xml";
    if (!crashSettings.loadFile(directory/"app_settings"/"settings_crash_behavior.xml",true,true,true,error) ||
        !crashSettings.loadFile(crashFile,false,false,true,error)) return fail(Code::SettingsRead,Operation::Settings);
    configuration.ui.crashSettings=crashSettings.values();
    configuration.ui.saveCrashPreferences=[&crashSettings,crashFile](const auto& changes,std::string& problem)
    { return crashSettings.saveChanges(crashFile,changes,problem); };
    configuration.ui.scheduleSettingsReset=[profile](std::string& problem)
    { return LLVKSettingsMgr::scheduleReset(profile,problem); };
#if LL_SEND_CRASH_REPORTS && defined(LL_BUGSPLAT)
    configuration.ui.crashSettingsRequireRestart=true;
#endif
    PWSTR local=nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&local))) return fail(Code::CacheUnavailable,Operation::Cache);
    configuration.ui.defaultCacheDirectory=std::filesystem::path(local)/std::filesystem::path(std::u8string(profileName.begin(),profileName.end()));
    CoTaskMemFree(local);
    const auto cache=stringValue("CacheLocation");
    configuration.ui.cacheDirectory=cache.empty() ? configuration.ui.defaultCacheDirectory :
        std::filesystem::path(std::u8string(cache.begin(),cache.end()));
    const auto soundCache=stringValue("FSSoundCacheLocation");
    configuration.soundCacheDirectory=soundCache.empty() ? configuration.ui.cacheDirectory :
        std::filesystem::path(std::u8string(soundCache.begin(),soundCache.end()));
    configuration.ui.appliedSettingsMode = appliedSettingsMode;
    const auto preferenceFile=userSettings/std::filesystem::path(std::u8string(settingsFile.begin(),settingsFile.end()));
    if (executionMarkerName.empty() || std::filesystem::path(executionMarkerName).filename()!=executionMarkerName)
        return fail(Code::CacheUnavailable,Operation::Cache);
    auto cachePlan=LLVKTextureCache::planStartup(settings.values(),configuration.ui.defaultCacheDirectory,
        directory/"local_assets",profile/"logs"/executionMarkerName,false,error);
    if (!cachePlan) return fail(Code::CacheUnavailable,Operation::Cache);
    const auto& cacheConfiguration=cachePlan->configuration;
    ApplicationSession session;
    session.errorResolver=errorResolver;
    auto cacheService=std::make_unique<LLVKApplicationCache>(cacheConfiguration,startupStatus);
    auto& textureCache=cacheService->cache();
    std::unique_ptr<LLVKSessionOwner::Service> applicationCache=std::move(cacheService);
    configuration.sessionOwner=session.owner.get();
    if (!startupStatus->show(cacheConfiguration.purge ? "StartupClearingTextureCache" : "StartupInitializingTextureCache",error))
        return fail(Code::StartupResources);
    if (!session.owner->install(LLVKSessionOwner::Lifetime::Application,1,applicationCache).ok())
    { startupStatus->hide(); return fail(Code::CacheUnavailable,Operation::Cache); }
    startupStatus->hide();
    cachePlan->metadata["CacheValidateCounter"]=LLSD(static_cast<int>(textureCache.validationIndex()));
    if (!settings.saveChanges(preferenceFile,cachePlan->metadata,error)) return fail(Code::SettingsWrite,Operation::Settings);
    configuration.ui.cacheDirectory=cacheConfiguration.directory;
    if (soundCache.empty()) configuration.soundCacheDirectory=cacheConfiguration.directory;
    configuration.ui.settings=settings.values();
    configuration.textureCache=&textureCache;
    if (const auto requested=settings.find("FSStartupClearBrowserCache"); requested && requested->getValue().asBoolean())
    {
        if (!settings.consumeBrowserCacheClear(profile,preferenceFile,error)) return fail(Code::SettingsWrite,Operation::Cache);
        configuration.ui.settings["FSStartupClearBrowserCache"]=false;
    }
    configuration.ui.savePreferences=[&settings,preferenceFile](const auto& changes,std::string& problem)
    { return settings.saveChanges(preferenceFile,changes,problem); };
    configuration.ui.backupHandler=[&settings,userSettings,directory,preferenceFile](const LLVKViewerUi::BackupRequest& request,std::string& problem)
    {
        if (request.accountSettings) { problem="Account backup requires an authenticated account directory"; return false; }
        if (request.restore)
        {
            std::map<std::string,LLSD> generated;
            if (request.globalSettings)
            {
                if (request.recommendedGraphics.empty()) { problem="Global settings restore requires native hardware recommendations"; return false; }
                if (preferenceFile.parent_path()!=userSettings) { problem="Restore requires a profile-local settings file"; return false; }
                const auto restored=LLVKPreferencesBackup::restoredSettings(directory/"app_settings"/"settings.xml",
                    request.directory/"settings.xml",request.recommendedGraphics,problem);
                if (!restored) return false;
                const auto filename=preferenceFile.filename().u8string();
                generated[std::string(filename.begin(),filename.end())]=*restored;
            }
            return LLVKPreferencesBackup::copy(request.directory,userSettings,request.globalFiles,request.folders,generated,problem);
        }
        return LLVKPreferencesBackup::copy(userSettings,request.directory,request.globalFiles,request.folders,
            {{"settings.xml",LLVKPreferencesBackup::settings(settings.group())}},problem);
    };
    configuration.ui.saveKeyBindings=[path=userSettings/"key_bindings.xml"](const LLVKKeyBindings& bindings,std::string& problem)
    { return bindings.saveFile(path,problem); };
    LLVKAutoReplaceSettings autoReplace;
    const auto autoReplaceFile=userSettings/"autoreplace.xml";
    if (std::filesystem::exists(autoReplaceFile))
    {
        if (!autoReplace.loadFile(autoReplaceFile,error))
        { LL_WARNS("AutoReplace") << (LLVKError{Code::OptionalSettings,Operation::Settings,1,1}).diagnostic() << LL_ENDL; error.clear(); }
    }
    else if (!autoReplace.loadFile(directory/"app_settings"/"autoreplace.xml",error))
    {
        LL_WARNS("AutoReplace") << (LLVKError{Code::OptionalSettings,Operation::Settings,1,1}).diagnostic() << LL_ENDL;
        error.clear();
        LLSD first; first["name"]="Example List 1"; first["replacements"]["keyword1"]="replacement string 1";
        first["replacements"]["keyword2"]="replacement string 2";
        LLSD second; second["name"]="Example List 2"; second["replacements"]["mistake1"]="correction 1";
        second["replacements"]["mistake2"]="correction 2";
        autoReplace.add(first); autoReplace.add(second);
    }
    configuration.ui.autoReplaceLists=autoReplace.lists();
    configuration.ui.saveAutoReplace=[autoReplaceFile](const LLSD& lists,std::string& problem)
    {
        LLVKAutoReplaceSettings pending;
        if (!pending.set(lists)) { problem="Invalid AutoReplace settings update"; return false; }
        return pending.saveFile(autoReplaceFile,problem);
    };
    configuration.ui.skin.executableDirectory = directory;
    configuration.ui.skin.workingDirectory = std::filesystem::current_path();
    configuration.ui.skin.skinBaseDirectory = directory/"skins";
    configuration.ui.skin.userAppDirectory = profile;
    configuration.ui.skin.skin = stringValue("SkinCurrent","default");
    configuration.ui.skin.language = language;
    configuration.ui.fonts.platform = "Windows";
    wchar_t windowsDirectory[32768]{};
    GetWindowsDirectoryW(windowsDirectory,32768);
    configuration.ui.fonts.searchDirectories = {directory/"fonts",userSettings/"fonts",std::filesystem::path(windowsDirectory)/"Fonts"};
    configuration.ui.fontPresetDirectories={directory/"fonts",userSettings/"fonts"};
    const auto descriptor = stringValue("FSFontSettingsFile","fonts.xml");
    if (std::filesystem::is_regular_file(directory/"fonts"/descriptor)) configuration.ui.fontDescription = directory/"fonts"/descriptor;
    else if (std::filesystem::is_regular_file(userSettings/"fonts"/descriptor)) configuration.ui.fontDescription = userSettings/"fonts"/descriptor;
    configuration.browser.helperDirectory = browserDirectory;
    configuration.browser.localesDirectory = browserDirectory/"locales";
    configuration.browser.cacheDirectory = profile/"native_browser";
    configuration.browser.language = configuration.ui.skin.language;
    configuration.browser.userAgent = "Vulkanstorm/"+shortVersion;
    LLVKViewerUi::Page page;
    page.url = stringValue("LoginPage",page.url);
    page.language = configuration.ui.skin.language;
    page.version = shortVersion;
    page.channel = "Vulkanstorm";
    page.grid = "agni";
    page.operatingSystem = "Win";
    page.skin = configuration.ui.skin.skin;
    page.theme = configuration.ui.skin.theme;
    page.settings = values;
    configuration.loginPage = LLVKViewerUi::pageUrl(page);
    if (!LLVKWindowMgr::run(configuration,error))
        return fail(windowFailure,windowFailure == Code::RendererUnavailable ? Operation::Renderer :
            windowFailure == Code::BrowserUnavailable ? Operation::Browser :
            windowFailure == Code::ShutdownFailed ? Operation::Shutdown : Operation::Window);
    if (session.owner->snapshot().state!=LLVKSessionOwner::State::Stopped)
        return fail(Code::ShutdownFailed,Operation::Shutdown);
    LL_INFOS("NativeStartup") << "Goodbye!" << LL_ENDL;
    return 0;
    }
    catch (const std::bad_alloc&) { return fail(Code::OutOfMemory); }
    catch (...) { return fail(Code::Unexpected); }
}