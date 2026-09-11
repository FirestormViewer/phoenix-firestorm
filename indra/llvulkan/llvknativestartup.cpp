#include "llvknativestartup.h"
#include "llvkloginwindow.h"
#include "llvkstartupsettings.h"
#include "llstring.h"
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

std::optional<int> llvkNativeStartup(const std::wstring& commandLine,const std::string& profileName,const std::string& shortVersion)
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
    LLVKStartupSettings settings;
    std::string error;
    std::string appliedSettingsMode, modeError;
    const bool defaults = settings.loadFile(directory/"app_settings"/"settings.xml",true,true,true,error);
    if (defaults)
    {
        settings.loadFile(directory/"app_settings"/"settings_install.xml",false,true,true,error);
        settings.loadFile(userSettings/("fsdata_defaults."+shortVersion+".xml"),false,true,true,error);
        settings.loadFile(userSettings/std::filesystem::path(std::u8string(settingsFile.begin(),settingsFile.end())),false,false,true,error);
        if (sessionFile.empty())
        {
            const auto* session = settings.find("SessionSettingsFile");
            if (session) sessionFile = session->value().asString();
            const auto* first = settings.find("FirstRunThisInstall");
            if (sessionFile.empty() && first && first->value().asBoolean()) sessionFile = "settings_firestorm.xml";
        }
        if (!sessionFile.empty())
        {
            const auto modePath=directory/"app_settings"/sessionFile;
            if (settings.loadFile(modePath,true,true,false,modeError)) appliedSettingsMode=sessionFile;
        }
        settings.loadFile(userSettings/std::filesystem::path(std::u8string(settingsFile.begin(),settingsFile.end())),false,false,true,error);
    }
    const auto explicitBackend = overrides.find("RenderBackend");
    const auto* savedBackend = settings.find("RenderBackend");
    const auto backend = explicitBackend != overrides.end() ? explicitBackend->second : savedBackend ? savedBackend->value().asString() : std::string();
    if (backend != "Vulkan") return std::nullopt;
    const auto fail = [&](const std::string& problem) -> std::optional<int>
    {
        MessageBoxW(nullptr,ll_convert<std::wstring>(problem).c_str(),L"Vulkanstorm native startup",MB_OK|MB_ICONERROR);
        return -1;
    };
    if (!defaults) return fail(error.empty() ? "Native default settings could not be loaded" : error);
    if (!modeError.empty()) return fail("Native settings mode could not be applied: "+modeError);
    if (unsupported) return fail("This native startup path does not yet support one or more supplied command-line options.");
    for (const auto& [name,value] : overrides) if (!settings.set(name,LLSD(value),false,error)) return fail(error);
    const auto values = settings.values();
    const auto stringValue = [&](const char* name,const std::string& fallback = {})
    { const auto found = values.find(name); return found == values.end() || found->second.asString().empty() ? fallback : found->second.asString(); };
    const auto browserDirectory = directory/"llplugin";
    if (!SetDllDirectoryW(browserDirectory.c_str())) return fail("Native browser DLL directory could not be selected.");
    struct DllDirectory { ~DllDirectory() { SetDllDirectoryW(nullptr); } } dllDirectory;
    LLVKLoginWindow::Configuration configuration;
    configuration.ui.settings = values;
    configuration.ui.settingDefaults = settings.defaults();
    configuration.ui.appliedSettingsMode = appliedSettingsMode;
    const auto preferenceFile=userSettings/std::filesystem::path(std::u8string(settingsFile.begin(),settingsFile.end()));
    configuration.ui.savePreferences=[&settings,preferenceFile](const auto& changes,std::string& problem)
    { return settings.saveChanges(preferenceFile,changes,problem); };
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
    const auto descriptor = stringValue("FSFontSettingsFile","fonts.xml");
    if (std::filesystem::is_regular_file(directory/"fonts"/descriptor)) configuration.ui.fontDescription = directory/"fonts"/descriptor;
    else if (std::filesystem::is_regular_file(userSettings/"fonts"/descriptor)) configuration.ui.fontDescription = userSettings/"fonts"/descriptor;
    configuration.browser.helperDirectory = browserDirectory;
    configuration.browser.localesDirectory = browserDirectory/"locales";
    configuration.browser.cacheDirectory = profile/"native_browser";
    configuration.browser.language = configuration.ui.skin.language;
    configuration.browser.userAgent = "Vulkanstorm/"+shortVersion;
    LLVKLoginUi::Page page;
    page.url = stringValue("LoginPage",page.url);
    page.language = configuration.ui.skin.language;
    page.version = shortVersion;
    page.channel = "Vulkanstorm";
    page.grid = "agni";
    page.operatingSystem = "Win";
    page.skin = configuration.ui.skin.skin;
    page.theme = configuration.ui.skin.theme;
    page.settings = values;
    configuration.loginPage = LLVKLoginUi::pageUrl(page);
    try
    {
        if (!LLVKLoginWindow::run(configuration,error)) return fail(error);
    }
    catch (const std::exception& exception) { return fail(exception.what()); }
    return 0;
}