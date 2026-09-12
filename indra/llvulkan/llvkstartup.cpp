#include "llvkstartup.h"
#include "llvkwindowmgr.h"
#include "llvksettingsmgr.h"
#include "llvkpreferencesbackup.h"
#include "llstring.h"
#include "llerror.h"
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

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
    const auto fail = [&](const std::string& problem) -> std::optional<int>
    {
        LL_WARNS("NativeStartup") << problem << LL_ENDL;
        MessageBoxW(nullptr,ll_convert<std::wstring>(problem).c_str(),L"Vulkanstorm native startup",MB_OK|MB_ICONERROR);
        return -1;
    };
    try
    {
    if (!defaults) return fail(error.empty() ? "Native default settings could not be loaded" : error);
    if (!modeError.empty()) return fail("Native settings mode could not be applied: "+modeError);
    if (unsupported) return fail("This native startup path does not yet support one or more supplied command-line options.");
    settings=LLVKSettingsMgr(globalGroup);
    if (!loadSettings()) return fail(error);
    const auto reset=LLVKSettingsMgr::consumeReset(profile,std::filesystem::path(std::u8string(settingsFile.begin(),settingsFile.end())),error);
    if (!reset) return fail(error);
    if (*reset)
    {
        if (!loadSettings()) return fail(error);
        if (!modeError.empty()) return fail("Native settings mode could not be applied after reset: "+modeError);
        if (!settings.set("RenderBackend",LLSD("Vulkan"),false,error)) return fail(error);
    }
    for (const auto& [name,value] : overrides) if (!settings.set(name,LLSD(value),false,error)) return fail(error);
    const auto values = settings.values();
    const auto stringValue = [&](const char* name,const std::string& fallback = {})
    { const auto found = values.find(name); return found == values.end() || found->second.asString().empty() ? fallback : found->second.asString(); };
    const auto browserDirectory = directory/"llplugin";
    if (!SetDllDirectoryW(browserDirectory.c_str())) return fail("Native browser DLL directory could not be selected.");
    struct DllDirectory { ~DllDirectory() { SetDllDirectoryW(nullptr); } } dllDirectory;
    LLVKWindowMgr::Configuration configuration;
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
    if (!warnings.loadFile(warningsFile,false,false,true,error)) return fail(error);
    configuration.ui.warningSettingsGroup=&warningGroup;
    configuration.ui.saveWarningPreferences=[&warnings,warningsFile](const auto& changes,std::string& problem)
    { return warnings.saveChanges(warningsFile,changes,problem); };
    configuration.ui.settingDefaults = settings.defaults();
    LLVKSettingsMgr accountSettings(accountGroup);
    if (!accountSettings.loadFile(directory/"app_settings"/"settings_per_account.xml",true,true,true,error)) return fail(error);
    configuration.ui.accountSettings=accountSettings.values();
    configuration.ui.accountDefaults=accountSettings.defaults();
    LLVKSettingsMgr crashSettings(crashGroup);
    const auto crashFile=userSettings/"settings_crash_behavior.xml";
    if (!crashSettings.loadFile(directory/"app_settings"/"settings_crash_behavior.xml",true,true,true,error) ||
        !crashSettings.loadFile(crashFile,false,false,true,error)) return fail(error);
    configuration.ui.crashSettings=crashSettings.values();
    configuration.ui.saveCrashPreferences=[&crashSettings,crashFile](const auto& changes,std::string& problem)
    { return crashSettings.saveChanges(crashFile,changes,problem); };
    configuration.ui.scheduleSettingsReset=[profile](std::string& problem)
    { return LLVKSettingsMgr::scheduleReset(profile,problem); };
#if LL_SEND_CRASH_REPORTS && defined(LL_BUGSPLAT)
    configuration.ui.crashSettingsRequireRestart=true;
#endif
    PWSTR local=nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&local))) return fail("Cannot resolve native cache directory");
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
        return fail("Native texture cache requires the viewer execution marker identity");
    LLVKTextureCache textureCache;
    auto cachePlan=LLVKTextureCache::planStartup(settings.values(),configuration.ui.defaultCacheDirectory,
        directory/"local_assets",profile/"logs"/executionMarkerName,false,error);
    if (!cachePlan) return fail(error);
    const auto& cacheConfiguration=cachePlan->configuration;
    if (!textureCache.start(cacheConfiguration,error)) return fail(error);
    cachePlan->metadata["CacheValidateCounter"]=LLSD(static_cast<int>(textureCache.validationIndex()));
    if (!settings.saveChanges(preferenceFile,cachePlan->metadata,error)) return fail(error);
    configuration.ui.cacheDirectory=cacheConfiguration.directory;
    if (soundCache.empty()) configuration.soundCacheDirectory=cacheConfiguration.directory;
    configuration.ui.settings=settings.values();
    configuration.textureCache=&textureCache;
    if (const auto requested=settings.find("FSStartupClearBrowserCache"); requested && requested->getValue().asBoolean())
    {
        if (!settings.consumeBrowserCacheClear(profile,preferenceFile,error)) return fail(error);
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
        { LL_WARNS("AutoReplace") << error << LL_ENDL; error.clear(); }
    }
    else if (!autoReplace.loadFile(directory/"app_settings"/"autoreplace.xml",error))
    {
        LL_WARNS("AutoReplace") << error << "; using example lists" << LL_ENDL;
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
    configuration.ui.skin.language = stringValue("Language","en");
    if (configuration.ui.skin.language == "default") configuration.ui.skin.language = "en";
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
    if (!LLVKWindowMgr::run(configuration,error)) return fail(error);
    if (!textureCache.stop(error)) return fail(error);
    LL_INFOS("NativeStartup") << "Goodbye!" << LL_ENDL;
    return 0;
    }
    catch (const std::exception& exception) { return fail(exception.what()); }
    catch (...) { return fail("Native viewer startup or shutdown failed with an unknown exception"); }
}