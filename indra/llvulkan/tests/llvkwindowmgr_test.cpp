#include "linden_common.h"
#include "llvkwindowmgr.h"
#include "llvksettingsmgr.h"
#include "llvkaudio.h"
#include "llvkjoystick.h"
#include "llvktexturecache.h"
#include "llvktexturepreview.h"
#include "llvkstartupstatus.h"
#include "lltut.h"
#include <fstream>
#include <windows.h>

namespace tut
{
    struct loginwindow_data {};
    typedef test_group<loginwindow_data> loginwindow_group;
    typedef loginwindow_group::object loginwindow_object;
    loginwindow_group loginwindow_tests("llvkwindowmgr");

    template<> template<> void loginwindow_object::test<7>()
    {
        set_test_name("native startup and shutdown present the original localized status resource");
        LLVKSkinFiles::Configuration skin;
        skin.skinBaseDirectory=std::filesystem::path(LLVK_LOGIN_SOURCE)/"skins";
        skin.language="en";
        std::string error;
        const auto title="Native cache status fixture "+std::to_string(GetCurrentProcessId());
        const auto wide=std::wstring(title.begin(),title.end());
        {
            LLVKStartupStatus status;
            ensure("load original startup strings",status.load(skin,title,error));
            ensure("show initialization phase",status.show("StartupInitializingTextureCache",error));
            const auto window=FindWindowW(L"#32770",wide.c_str());
            ensure("actual startup dialog is visible",window && IsWindowVisible(window));
            wchar_t text[256]{};
            GetDlgItemTextW(window,666,text,256);
            ensure("original initialization text",std::wstring(text)==L"Initializing texture cache...");
            ensure("show clearing phase",status.show("StartupClearingTextureCache",error));
            GetDlgItemTextW(window,666,text,256);
            ensure("original clearing text",std::wstring(text)==L"Clearing texture cache...");
            ensure("unknown phase fails explicitly",!status.show("UnknownPhase",error));
            status.hide();
            ensure("startup presenter closes before viewer use",FindWindowW(L"#32770",wide.c_str())==nullptr);
            const auto root=std::filesystem::temp_directory_path()/("native-shutdown-status-"+LLUUID::generateNewID().asString());
            struct Cleanup { std::filesystem::path root; ~Cleanup() { std::error_code ignored; std::filesystem::remove_all(root,ignored); } } cleanup{root};
            LLVKTextureCache cache;
            LLVKTextureCache::Configuration configuration;
            configuration.directory=root/"cache"; configuration.localAssets=root/"assets";
            configuration.bytes=256ull*1024*1024;
            const bool started=cache.start(configuration,error); ensure(error,started);
            auto write=cache.write(LLUUID::generateNewID(),std::vector<std::uint8_t>(2048,42),2048,error);
            ensure("pending shutdown work accepted",write.valid());
            ensure("reopen presenter for shutdown",status.show("ShuttingDown",error));
            const auto shutdownWindow=FindWindowW(L"#32770",wide.c_str());
            ensure("shutdown dialog visible",shutdownWindow && IsWindowVisible(shutdownWindow));
            GetDlgItemTextW(shutdownWindow,666,text,256);
            ensure("original shutdown message",std::wstring(text)==L"Shutting down...");
            const bool stopped=cache.stop(error); ensure(error,stopped);
            ensure("shutdown drains accepted write",write.get().success);
            ensure("status remains visible through cache retirement",IsWindowVisible(shutdownWindow)!=FALSE);
            status.hide();
            status.hide();
            ensure("explicit shutdown hide is idempotent",FindWindowW(L"#32770",wide.c_str())==nullptr);
            ensure("shutdown can reopen after hide",status.show("ShuttingDown",error));
        }
        ensure("status closes on scope exit",FindWindowW(L"#32770",wide.c_str())==nullptr);
    }

    template<> template<> void loginwindow_object::test<6>()
    {
        set_test_name("native cache startup policy separates writable migration from read-only inspection");
        LLVKSettingsMgr settings;
        std::string error;
        ensure("cache policy uses source declarations",settings.loadFile(std::filesystem::path(LLVK_LOGIN_SOURCE)/
            "app_settings"/"settings.xml",true,true,true,error));
        auto values=settings.values();
        const auto root=std::filesystem::temp_directory_path()/"native-cache-policy-fixture";
        const auto current=root/"current",next=root/"next";
        const auto text=[](const std::filesystem::path& path) { const auto bytes=path.u8string(); return std::string(bytes.begin(),bytes.end()); };
        values["CacheLocation"]=text(current); values["NewCacheLocation"]=text(next);
        values["CacheSize"]=-1; values["CacheValidateCounter"]=255;
        values["LocalCacheVersion"]=9; values["LastJ2CVersion"]=LLVKTextureCache::encoderVersion();
        values["PurgeCacheOnStartup"]=false; values["PurgeCacheOnNextStartup"]=true;
        const auto plan=[&](bool readOnly) { return LLVKTextureCache::planStartup(values,root/"default",root/"assets",root/"marker",readOnly,error); };
        auto writable=plan(false);
        ensure("writable selects pending relocation",writable && writable->configuration.directory==next);
        ensure_equals("negative size clamps before unsigned conversion",writable->configuration.bytes,std::uint64_t(256)*1024*1024);
        ensure("one-shot texture purge requested but global request retained",writable->configuration.purge &&
            !writable->metadata.contains("PurgeCacheOnNextStartup"));
        const auto readOnly=plan(true);
        ensure("read-only retains current location",readOnly && readOnly->configuration.directory==current);
        ensure("read-only suppresses purge and all metadata writes",!readOnly->configuration.purge && readOnly->metadata.empty());
        ensure("planning leaves settings unchanged",values.at("CacheLocation").asString()==text(current) && values.at("PurgeCacheOnNextStartup").asBoolean());
        values["CacheSize"]=200*1024; values["NewCacheLocation"]="";
        writable=plan(false);
        ensure("empty requested path restores default",writable && writable->configuration.directory==root/"default");
        ensure_equals("maximum capacity",writable->configuration.bytes,std::uint64_t(100)*1024*1024*1024);
        values["PurgeCacheOnNextStartup"]=false; values["LastJ2CVersion"]="other-codec";
        ensure("writable invalidates old codec",plan(false)->configuration.purge);
        ensure("read-only refuses old codec without purge",plan(true)->configuration.versionMismatch && !plan(true)->configuration.purge);
        values["CacheValidateCounter"]=256;
        ensure("invalid validation counter rejected",!plan(false));
        values["CacheValidateCounter"]=0; values["NewCacheLocation"]="relative";
        ensure("relative relocation rejected before IO",!plan(false));
        values.erase("CacheSize");
        ensure("missing declaration reports failure rather than dereference",!plan(false));
    }

    template<> template<> void loginwindow_object::test<5>()
    {
        set_test_name("shared persistent texture cache initializes drains and reopens without viewer owners");
        const auto root=std::filesystem::temp_directory_path()/
            ("native-texture-cache-"+LLUUID::generateNewID().asString());
        struct Cleanup { std::filesystem::path root; ~Cleanup() { std::error_code ignored; std::filesystem::remove_all(root,ignored); } } cleanup{root};
        LLVKTextureCache::Configuration configuration;
        configuration.directory=root/"cache"; configuration.localAssets=root/"assets";
        configuration.executionMarker=root/"logs"/"fixture.exec_marker";
        configuration.bytes=256ull*1024*1024;
        const auto id=LLUUID::generateNewID();
        std::vector<std::uint8_t> bytes(2048);
        for (std::size_t index=0; index<bytes.size(); ++index) bytes[index]=static_cast<std::uint8_t>(index%251);
        std::string error;
        LLVKTextureCache cache;
        ensure("initialize shared texture cache",cache.start(configuration,error));
        LLVKTextureCache conflict;
        ensure("second cache writer rejected",!conflict.start(configuration,error));
        auto written=cache.write(id,bytes,static_cast<int>(bytes.size()),error);
        ensure(error,written.valid());
        ensure("shutdown drains pending persistent write",cache.stop(error));
        ensure("encoded cache write completed",written.get().success);
        {
            std::ifstream fast(configuration.directory/"texturecache"/"FastCache.cache",std::ios::binary);
            std::int32_t header[4]{1,1,1,1};
            ensure("encoded write invalidates fast-cache header",bool(fast.read(reinterpret_cast<char*>(header),sizeof(header))) &&
                std::all_of(std::begin(header),std::end(header),[](auto value) { return value==0; }));
        }
        ensure("idempotent cache stop",cache.stop(error));
        ensure("read after stop rejected",!cache.read(id,0,2048,error).valid());
        LLVKTextureCache reopened;
        ensure("reopen same persistent cache",reopened.start(configuration,error));
        auto read=reopened.read(id,0,2048,error);
        auto missing=reopened.read(LLUUID::generateNewID(),0,2048,error);
        ensure("cache read requests accepted",read.valid() && missing.valid());
        ensure("shutdown drains reads",reopened.stop(error));
        const auto result=read.get();
        ensure("persistent bytes survive shutdown",result.success && result.bytes==bytes && result.imageSize==2048 && !result.local);
        ensure("cache miss remains a miss",!missing.get().success);
        const auto snapshot=[&]()
        {
            std::map<std::filesystem::path,std::vector<char>> files;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(configuration.directory))
                if (entry.is_regular_file())
                {
                    std::ifstream input(entry.path(),std::ios::binary);
                    files.emplace(entry.path().lexically_relative(configuration.directory),
                        std::vector<char>{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()});
                }
            return files;
        };
        const auto beforeReadOnly=snapshot();
        configuration.readOnly=true;
        configuration.purge=true;
        LLVKTextureCache reader,secondReader,blockedWriter;
        ensure("open nonmutating reader",reader.start(configuration,error) && reader.readOnly());
        ensure("readers share stable storage",secondReader.start(configuration,error));
        auto writerConfiguration=configuration; writerConfiguration.readOnly=false;
        ensure("writer cannot replace active readers",!blockedWriter.start(writerConfiguration,error));
        ensure("read-only write rejected",!reader.write(id,bytes,2048,error).valid());
        auto readOnlyBytes=reader.read(id,0,2048,error);
        ensure("read-only request accepted",readOnlyBytes.valid());
        ensure("read-only drain",reader.stop(error) && secondReader.stop(error));
        const auto readOnlyResult=readOnlyBytes.get();
        ensure("read-only ignores purge and retains bytes",readOnlyResult.success && readOnlyResult.bytes==bytes);
        ensure("read-only startup read and shutdown do not modify files",snapshot()==beforeReadOnly);
        auto missingConfiguration=configuration;
        missingConfiguration.directory=root/"missing-read-only";
        missingConfiguration.executionMarker.clear();
        LLVKTextureCache missingReader;
        ensure("missing read-only cache is not created",!missingReader.start(missingConfiguration,error) &&
            !std::filesystem::exists(missingConfiguration.directory));
        auto mismatchConfiguration=configuration; mismatchConfiguration.versionMismatch=true;
        LLVKTextureCache mismatchReader;
        ensure("read-only version mismatch rejected",!mismatchReader.start(mismatchConfiguration,error));
        ensure("rejected mismatch does not mutate storage",snapshot()==beforeReadOnly);
        const auto indexPath=configuration.directory/"texturecache"/"texture.entries";
        const auto originalIndex=beforeReadOnly.at(std::filesystem::path("texturecache")/"texture.entries");
        {
            std::ofstream truncated(indexPath,std::ios::binary|std::ios::trunc);
            truncated.write(originalIndex.data(),originalIndex.size()-1);
        }
        const auto corruptSnapshot=snapshot();
        LLVKTextureCache corruptReader;
        ensure("truncated index rejected without repair",!corruptReader.start(configuration,error));
        ensure("failed initialization cannot publish partial index",!corruptReader.read(id,0,2048,error).valid());
        ensure("failed reader cleanup",corruptReader.stop(error));
        ensure("failed reader leaves corrupt bytes unchanged",snapshot()==corruptSnapshot);
        {
            std::ofstream restoredIndex(indexPath,std::ios::binary|std::ios::trunc);
            restoredIndex.write(originalIndex.data(),originalIndex.size());
        }
        configuration.readOnly=false;
        LLVKTextureCache purged;
        configuration.purge=true;
        ensure("explicit cache purge initializes replacement",purged.start(configuration,error));
        auto afterPurge=purged.read(id,0,2048,error);
        ensure("purge miss request accepted",afterPurge.valid());
        ensure("purged cache stops",purged.stop(error));
        ensure("purge removes persistent entry",!afterPurge.get().success);
        configuration.purge=false;
        LLVKTextureCache seeded;
        ensure("seed cache for version migration",seeded.start(configuration,error));
        auto seed=seeded.write(id,bytes,2048,error);
        ensure("version fixture write accepted",seed.valid());
        ensure("version fixture drained",seeded.stop(error) && seed.get().success);
        configuration.versionMismatch=true;
        LLVKTextureCache migrated;
        ensure("version mismatch initializes cache",migrated.start(configuration,error));
        auto obsolete=migrated.read(id,0,2048,error);
        ensure("version-mismatch read accepted",obsolete.valid());
        ensure("version-mismatch drain",migrated.stop(error));
        ensure("old-version entries cannot publish",!obsolete.get().success);
        LLVKTextureCache invalid;
        configuration.directory="relative-cache";
        ensure("relative cache root rejected",!invalid.start(configuration,error));
        ensure("no OpenGL during disk cache use",GetModuleHandleW(L"opengl32.dll")==nullptr);
    }

    template<> template<> void loginwindow_object::test<4>()
    {
        set_test_name("native voice service shares WebRTC independently of any widget tree");
        LLVKVoice voice;
        std::string error;
        LLVKVoice::AudioConfig processing;
        processing.mEchoCancellation=false;
        processing.mAGC=false;
        processing.mNoiseSuppressionLevel=LLVKVoice::AudioConfig::NOISE_SUPPRESSION_LEVEL_LOW;
        ensure("configure without starting device engine",voice.configure(processing,error));
        ensure("configuration does not acquire shared engine",llwebrtc::getDeviceInterface()==nullptr);
        ensure("voice enumeration without UI",voice.refresh(error));
        const auto state = voice.state(error);
        ensure("voice snapshot without capture",state && !state->tuning && state->energy == 0.f);
        ensure("pending configuration submitted at startup",!state->audioConfig.mEchoCancellation && !state->audioConfig.mAGC &&
            state->audioConfig.mNoiseSuppressionLevel==processing.mNoiseSuppressionLevel);
        processing.mNoiseSuppressionLevel=static_cast<LLVKVoice::AudioConfig::ENoiseSuppressionLevel>(5);
        ensure("invalid processing rejected",!voice.configure(processing,error));
        ensure("invalid processing preserves prior request",voice.state(error)->audioConfig.mNoiseSuppressionLevel==
            LLVKVoice::AudioConfig::NOISE_SUPPRESSION_LEVEL_LOW);
        for (int level=0; level<=4; ++level)
        {
            processing.mNoiseSuppressionLevel=static_cast<LLVKVoice::AudioConfig::ENoiseSuppressionLevel>(level);
            ensure("live software processing update without capture",voice.configure(processing,error));
        }
        ensure("restore default processing after override",voice.configure(LLVKVoice::AudioConfig{},error));
        const auto restored=voice.state(error);
        ensure("default processing restored",restored && restored->audioConfig.mAGC && restored->audioConfig.mEchoCancellation &&
            restored->audioConfig.mNoiseSuppressionLevel==LLVKVoice::AudioConfig::NOISE_SUPPRESSION_LEVEL_VERY_HIGH);
        LLVKVoice conflict;
        ensure("second owner cannot steal shared engine",!conflict.refresh(error));
        ensure("conflicting owner shutdown leaves engine intact",conflict.stop(error) && voice.state(error).has_value());
        ensure("invalid tuning gain rejected",!voice.tune(false,-1.f,error));
        ensure("disabled tuning is valid",voice.tune(false,1.f,error));
        ensure("voice teardown drains callbacks",voice.stop(error));
        ensure("voice teardown idempotent",voice.stop(error));
        ensure("stopped owner cannot restart implicitly",!voice.refresh(error));
        ensure("stopped owner rejects configuration",!voice.configure(processing,error));
        LLVKVoice unused;
        ensure("never-started owner teardown",unused.stop(error));
        for (int iteration=0; iteration<8; ++iteration)
        {
            LLVKVoice restarted;
            ensure("new owner after prior shutdown",restarted.refresh(error));
            for (int request=0; request<4; ++request)
            {
                ensure("queue input deployment without capture",restarted.select(true,request%2 ? "Default" : "",error));
                ensure("queue output deployment without capture",restarted.select(false,request%2 ? "Default" : "",error));
                ensure("queue enumeration before immediate shutdown",restarted.refresh(error));
                ensure("tuning remains disabled",restarted.tune(false,1.f,error));
            }
            ensure("queued cross-thread work drains before owner destruction",restarted.stop(error));
        }
    }

    template<> template<> void loginwindow_object::test<3>()
    {
        set_test_name("native joystick owns DirectInput enumeration and bounded device state");
        const auto window=CreateWindowExW(0,L"STATIC",L"Native input test",WS_OVERLAPPED,0,0,32,32,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        ensure("input test window",window!=nullptr);
        struct Window { HWND handle; ~Window() { DestroyWindow(handle); } } cleanup{window};
        LLVKJoystick joystick;
        std::string error;
        const bool started=joystick.start(window,error);
        ensure(error,started);
        ensure("native joystick enumeration",joystick.enumerate(error));
        for (const auto& device : joystick.devices()) ensure("binary device identity",device.id.isBinary() && device.id.asBinary().size()==16);
        ensure("invalid identity rejected",!joystick.select(LLSD("not a GUID"),error));
        ensure("no fabricated selected device",!joystick.selected().isDefined());
        ensure("no-device polling succeeds",joystick.poll(error));
        ensure("no fabricated axes",joystick.state().axisCount==0 && !joystick.state().connected);
        ensure("explicit no device selection",joystick.select(LLSD(0),error));
        joystick.stop();
        ensure("stopped input cannot poll",!joystick.poll(error));
    }

    template<> template<> void loginwindow_object::test<2>()
    {
        set_test_name("native startup audio owns an actual driver and honors NoAudio");
        std::string error;
        LLVKAudio disabled;
        ensure("NoAudio accepted",disabled.start(true,error));
        ensure("NoAudio has no active device",!disabled.active());
        ensure_equals("inactive source report",disabled.driverName(),std::string("Undefined"));
        LLVKAudio audio;
        const auto started=audio.start(false,error);
        ensure(error,started);
        ensure("real audio context active",audio.active());
        ensure("real provider driver report",audio.driverName().find("OpenAL, version ")==0);
        LLVKAudio::Volume volume; volume.master=0.375f;
        ensure("live master gain",audio.setVolume(volume,error));
        const auto gain=audio.listenerGain(error);
        ensure(error,gain.has_value());
        ensure_equals("actual OpenAL listener gain",*gain,0.375f);
        volume.muted=true;
        ensure("mute audio",audio.setVolume(volume,error));
        ensure_equals("actual listener muted",audio.listenerGain(error).value_or(-1.f),0.f);
        volume.muted=false; volume.windowActive=false; volume.muteWhenInactive=true;
        ensure("inactive window mute",audio.setVolume(volume,error));
        ensure_equals("inactive listener muted",audio.listenerGain(error).value_or(-1.f),0.f);
        volume.windowActive=true;
        ensure("window reactivation restores gain",audio.setVolume(volume,error));
        ensure_equals("master gain retained",audio.listenerGain(error).value_or(-1.f),0.375f);
        volume.progressVisible=true;
        ensure("progress mutes effects",audio.setVolume(volume,error));
        ensure_equals("progress listener muted",audio.listenerGain(error).value_or(-1.f),0.f);
        volume.master=-1.f;
        ensure("invalid gain rejected",!audio.setVolume(volume,error));
        ensure_equals("rejected gain leaves listener unchanged",audio.listenerGain(error).value_or(-1.f),0.f);
        LLVKAudio conflict;
        ensure("cannot steal context",!conflict.start(false,error));
        ensure("original audio unaffected",audio.active());
        std::vector<std::uint8_t> silence(44+44100*2,0);
        const auto bytes=[&](std::size_t offset,std::string_view value)
        { std::copy(value.begin(),value.end(),silence.begin()+offset); };
        const auto little=[&](std::size_t offset,std::uint32_t value,int count)
        { for (int index=0; index<count; ++index) silence[offset+index]=static_cast<std::uint8_t>(value>>(index*8)); };
        bytes(0,"RIFF"); little(4,static_cast<std::uint32_t>(silence.size()-8),4); bytes(8,"WAVEfmt ");
        little(16,16,4); little(20,1,2); little(22,1,2); little(24,44100,4); little(28,88200,4);
        little(32,2,2); little(34,16,2); bytes(36,"data"); little(40,88200,4);
        ensure("native UI gain",audio.setUiGain(0.5f,false,error));
        ensure("decode and play silent WAV fixture",audio.playUiWav(silence,error));
        ensure_equals("source and buffer retained while playing",audio.activeUiSounds(),std::size_t(1));
        ensure("live UI mute",audio.setUiGain(0.5f,true,error));
        ensure("native audio pump",audio.update(error));
        ensure("clean audio shutdown",audio.stop(error));
        ensure_equals("shutdown retires UI audio resources",audio.activeUiSounds(),std::size_t(0));
        ensure("audio no longer active",!audio.active());
        ensure("idempotent shutdown",audio.stop(error));
    }

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
        LLVKSettingsMgr settings;
        std::string error;
        const bool loaded = settings.loadFile(viewer/"app_settings"/"settings.xml",true,true,true,error);
        ensure(error,loaded);
        LLVKWindowMgr::Configuration configuration;
        configuration.ui.skin.skinBaseDirectory = viewer/"skins";
        configuration.ui.skin.userAppDirectory = profile.path;
        configuration.ui.fontDescription = viewer/"fonts"/"fonts.xml";
        configuration.ui.fonts.platform = "Windows";
        configuration.ui.fonts.searchDirectories = {viewer/"fonts",std::filesystem::path(LLVK_LOGIN_PACKAGED_FONTS)};
        configuration.ui.settings = settings.values();
        configuration.ui.settingsGroup=&settings.group();
        configuration.ui.savePreferences=[&settings,path=profile.path/"settings.xml"](const auto& changes,std::string& problem)
        { return settings.saveChanges(path,changes,problem); };
        configuration.ui.settingDefaults = settings.defaults();
        LLVKSettingsMgr accountSettings;
        ensure("native account declaration load",accountSettings.loadFile(viewer/"app_settings"/"settings_per_account.xml",true,true,true,error));
        configuration.ui.accountSettings=accountSettings.values(); configuration.ui.accountDefaults=accountSettings.defaults();
        configuration.ui.accountSettingsGroup=&accountSettings.group();
        LLVKSettingsMgr crashSettings;
        ensure("native crash declaration load",crashSettings.loadFile(viewer/"app_settings"/"settings_crash_behavior.xml",true,true,true,error));
        configuration.ui.crashSettings=crashSettings.values();
        configuration.ui.dictionaryDirectory=std::filesystem::path(LLVK_LOGIN_PACKAGED_FONTS).parent_path()/"dictionaries";
        configuration.browser.helperDirectory = directory;
        configuration.browser.localesDirectory = directory/"locales";
        configuration.browser.cacheDirectory = profile.path/"browser";
        configuration.loginPage = "data:text/html,<html><body style='margin:0;background:rgb(45,90,120)'><h1>Native browser validation</h1></body></html>";
        configuration.stopAfterFrames = 6;
        LLVKTextureCache textureCache;
        LLVKTextureCache::Configuration cacheConfiguration;
        cacheConfiguration.directory=profile.path/"textures";
        cacheConfiguration.localAssets=profile.path/"assets";
        std::filesystem::create_directories(cacheConfiguration.localAssets);
        const auto previewAsset=LLUUID::generateNewID(),replacementAsset=LLUUID::generateNewID();
        for (const auto& [asset,red] : {std::pair{previewAsset,std::uint8_t(40)},std::pair{replacementAsset,std::uint8_t(180)}})
        {
            std::vector<std::uint8_t> tga(18+4,0);
            tga[2]=2; tga[12]=1; tga[14]=1; tga[16]=32; tga[17]=8;
            tga[18]=90; tga[19]=120; tga[20]=red; tga[21]=128;
            std::ofstream file(cacheConfiguration.localAssets/(asset.asString()+".tga"),std::ios::binary);
            file.write(reinterpret_cast<const char*>(tga.data()),tga.size());
        }
        ensure("cache starts before visual services",textureCache.start(cacheConfiguration,error));
        configuration.textureCache=&textureCache;
        auto rejected=configuration;
        rejected.browser.helperDirectory=profile.path/"missing-browser-helper";
        ensure("partial visual startup reports browser failure",!LLVKWindowMgr::run(rejected,error));
        ensure("partial startup preserves error",error.find("Native browser requires absolute helper")!=std::string::npos);
        ensure("partial startup destroys native window",FindWindowW(L"VulkanstormNativeLogin",nullptr)==nullptr);
        int expectedWidth=0,expectedHeight=0;
        configuration.bindServices=[&settings,&expectedWidth,&expectedHeight,&textureCache,previewAsset,replacementAsset](LLVKViewerUi& ui)
        {
            std::string problem;
            ensure("authoritative echo cancellation override",settings.set("VoiceEchoCancellation",LLSD(false),false,problem));
            ensure("authoritative automatic gain override",settings.set("VoiceAutomaticGainControl",LLSD(false),false,problem));
            ensure("authoritative noise suppression override",settings.set("VoiceNoiseSuppressionLevel",LLSD(2),false,problem));
            ensure("native Preferences in presentation",ui.showPreferences(problem));
            ensure("native About in presentation",ui.showAbout(problem));
            const auto tabs=ui.tree().get(ui.find("about_tab"));
            ensure("original About tabs",tabs && tabs->tabContainer && tabs->tabContainer->tabs.size()==4);
            ensure("native credits selected",ui.tree().commit(tabs->tabContainer->tabs[2].button));
            ensure(ui.dialogError(),ui.dialogError().empty());
            LLVKWidgetTree::Params view; view.rect={10,10,90,70};
            LLVKControl::Params control; control.font=ui.tree().get(ui.find("password_edit"))->control->params.font;
            LLVKWidgetTree::ColorSwatchParams color; color.color=LLVKColor{0.2f,0.6f,0.8f,1.f};
            const auto swatch=ui.tree().createColorSwatch(view,control,color,ui.root(),problem);
            ensure(problem,swatch.has_value());
            const bool opened=ui.showColorPicker(*swatch,true,problem);
            ensure(problem,opened);
            const auto general=ui.constructPreferencePanel("panel_preferences_general.xml",ui.root(),problem);
            ensure(problem,general.has_value());
            const auto colors=ui.constructPreferencePanel("panel_preferences_colors.xml",ui.root(),problem);
            ensure(problem,colors.has_value());
            ui.tree().setVisible(*general,false);
            ui.tree().setVisible(*colors,false);
            const auto chat=ui.constructPreferencePanel("panel_preferences_chat.xml",ui.root(),problem);
            ensure(problem,chat.has_value());
            std::vector<LLVKWidgetTree::Id> pending{*chat};
            for (std::size_t index=0; index<pending.size(); ++index)
            {
                const auto* node=ui.tree().get(pending[index]);
                pending.insert(pending.end(),node->children.begin(),node->children.end());
                if (!node->tabContainer) continue;
                const auto tabs=node->tabContainer->tabs;
                for (const auto& tab : tabs)
                {
                    ensure("original Chat subtab selects",ui.tree().selectTabPanel(pending[index],tab.panel,problem));
                    const auto paint=ui.preparePaint({},problem);
                    ensure(problem,paint.has_value());
                }
            }
            ui.tree().setVisible(*chat,false);
            const auto controls=ui.constructPreferencePanel("panel_preferences_controls.xml",ui.root(),problem);
            ensure(problem,controls.has_value());
            const auto list=ui.find("controls_list");
            auto rows=ui.tree().get(list)->scrollList->rows;
            for (auto& row : rows) { row.selected=row.value.asString()=="walk_to"; row.selectedCell=row.selected ? 1 : -1; }
            ensure("select integrated binding cell",ui.tree().setScrollListRows(list,std::move(rows),problem));
            ensure("open integrated key capture",ui.tree().commit(list));
            ensure("original key capture visible",ui.keyCaptureDialog()!=0);
            const auto window=FindWindowW(L"VulkanstormNativeLogin",nullptr);
            ensure("native capture window exists",window!=nullptr);
            RECT client{};
            ensure("read normal native client geometry",GetClientRect(window,&client)!=FALSE);
            expectedWidth=client.right-client.left; expectedHeight=client.bottom-client.top;
            SendMessageW(window,WM_KEYDOWN,'J',1);
            ensure("Windows input closes key capture",ui.keyCaptureDialog()==0);
            const auto& updated=ui.tree().get(list)->scrollList->rows;
            const auto walk=std::find_if(updated.begin(),updated.end(),[](const auto& row) { return row.value.asString()=="walk_to"; });
            ensure("Windows input updates binding table",walk!=updated.end() && walk->cells[1]=="J");
            ui.tree().setVisible(*controls,false);
            const bool joystickOpened=ui.showJoystick(problem);
            ensure(problem,joystickOpened);
            ensure("original joystick native service paint",ui.preparePaint({},problem).has_value());
            ensure("close integrated joystick",ui.closeFloater(problem));
            ensure("keep microphone capture disabled in integration test",ui.tree().updateSetting("EnableVoiceChat",LLSD(false)));
            const auto sound=ui.constructPreferencePanel("panel_preferences_sound.xml",ui.root(),problem);
            ensure(problem,sound.has_value());
            auto voiceAncestor=ui.tree().get(ui.find("voice_input_device"))->parent;
            while (voiceAncestor && voiceAncestor!=*sound)
            {
                const auto parent=ui.tree().get(voiceAncestor)->parent;
                if (parent && ui.tree().get(parent)->tabContainer)
                    ensure("select native voice device tab",ui.tree().selectTabPanel(parent,voiceAncestor,problem));
                voiceAncestor=parent;
            }
            ensure("show original voice device controls",ui.tree().updateSetting("ShowDeviceSettings",LLSD(true)));
            const auto voicePaint=ui.preparePaint({},problem);
            ensure(problem,voicePaint.has_value());
            ensure("voice remains disabled during real enumeration",!ui.tree().setting("EnableVoiceChat")->asBoolean());
            ui.tree().setVisible(*sound,false);
            const auto privacy=ui.constructPreferencePanel("panel_preferences_privacy.xml",ui.root(),problem);
            ensure(problem,privacy.has_value());
            ensure("Privacy native paint",ui.preparePaint({},problem).has_value());
            ui.tree().setVisible(*privacy,false);
            ensure("select standalone block-list policy",ui.tree().updateSetting("FSUseStandaloneBlocklistFloater",LLSD(true)));
            const bool blockOpened=ui.showBlockList(problem);
            ensure(problem,blockOpened);
            ensure("original Block List sort popup",ui.tree().buttonReturn(ui.find("view_btn"),0,false,problem));
            ensure("native Block List popup paint",ui.preparePaint({},problem).has_value());
            ui.menu().dismiss();
            ensure("close original Block List",ui.closeFloater(problem));
            ensure("original Spell Checker in presentation",ui.showSpellCheck(problem));
            ensure("actual dictionary service active",ui.spellCheck().active());
            ensure("original Translation Settings in presentation",ui.showTranslation(problem));
            const bool wasEnabled=ui.tree().setting("RestrainedLove")->asBoolean();
            ensure("RLVa original setting change",ui.tree().updateSetting("RestrainedLove",LLSD(!wasEnabled)));
            ensure("original AutoReplace in native presentation",ui.showAutoReplace(problem));
            ensure("native asynchronous XML picker starts",ui.tree().commit(ui.find("autoreplace_import_list")));
            ensure(ui.dialogError(),ui.dialogError().empty());
            ensure("bring live Preferences to front",ui.showPreferences(problem));
            const auto preferences=ui.activeFloater();
            const auto core=ui.find("pref core",preferences);
            ensure("original Preferences hierarchy presented",core && ui.tree().get(core)->tabContainer->tabs.size()>=15);
            ensure("present original Backup controls",ui.tree().selectTabPanel(core,ui.find("backup",core),problem));
            const auto preferencesPaint=ui.preparePaint({},problem);
            ensure(problem,preferencesPaint.has_value());
            const auto viewerPanel=ui.find("firestorm",preferences);
            ensure("open original beam color editor in native window",ui.showBeamColor(viewerPanel,problem));
            const auto beamPaint=ui.preparePaint({},problem);
            ensure(problem,beamPaint.has_value());
            ensure("beam hue image reaches native paint publication",std::any_of(beamPaint->commands.begin(),beamPaint->commands.end(),[](const auto& command)
            { return command.image && command.image->name()=="native-beam-color-strip"; }));
            const auto graphics=ui.find("display",preferences);
            ensure("open original graphics preset Save in native window",ui.showGraphicPreset(graphics,"PrefSave",problem));
            const auto presetPaint=ui.preparePaint({},problem);
            ensure(problem,presetPaint.has_value());
            ensure("original Save preset editor exists",ui.find("preset_combo",ui.activeFloater())!=0);
            ensure("open original beam shape editor in native window",ui.showBeamShape(viewerPanel,problem));
            const auto shapePaint=ui.preparePaint({},problem);
            ensure(problem,shapePaint.has_value());
            ensure("shape image reaches native publication",std::any_of(shapePaint->commands.begin(),shapePaint->commands.end(),[](const auto& command)
            { return command.image && command.image->name()=="native-beam-shape"; }));
            ui.takeNotices();
            LLVKTexturePreview previews(textureCache,ui.tree(),ui.root());
            LLVKWidgetTree::Params textureView; textureView.rect={10,10,90,90}; textureView.name="cache_preview_fixture";
            LLVKControl::Params textureControl; textureControl.font=control.font;
            LLVKTextureCtrl::Params textureParams; textureParams.initialAsset=previewAsset;
            textureParams.captionControl.font=control.font; textureParams.multipleControl.font=control.font;
            const auto texture=ui.tree().createTextureControl(textureView,textureControl,textureParams,ui.root(),problem);
            ensure(problem,texture.has_value());
            ensure("start async cached preview",previews.update(problem));
            ensure("change identity while request is pending",ui.tree().setTextureValue(*texture,LLSD(replacementAsset),problem));
            textureView.name="missing_cache_preview_fixture"; textureParams.initialAsset=LLUUID::generateNewID();
            const auto missing=ui.tree().createTextureControl(textureView,textureControl,textureParams,ui.root(),problem);
            ensure(problem,missing.has_value());
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
            while (previews.status(*texture)!=LLVKTexturePreview::Status::Ready || previews.status(*missing)!=LLVKTexturePreview::Status::Missing)
            {
                const bool updated=previews.update(problem); ensure(problem,updated);
                ensure("bounded preview completion: "+previews.failure(*texture),std::chrono::steady_clock::now()<deadline);
                std::this_thread::yield();
            }
            const auto& image=ui.tree().get(*texture)->textureControl->preview;
            ensure("replacement asset alone publishes",image && image->name()==replacementAsset.asString());
            const auto pixels=image->bottomUpRgba();
            ensure("native decoder preserves RGBA fixture",pixels.size()==4 && pixels[0]==180 && pixels[1]==120 && pixels[2]==90 && pixels[3]==128);
            ensure("preview alpha classification retained",ui.tree().get(*texture)->textureControl->previewHasAlpha);
            ensure("missing asset has no fabricated image",!ui.tree().get(*missing)->textureControl->preview);
            const auto previewPaint=ui.preparePaint({},problem);
            ensure(problem,previewPaint.has_value());
            ensure("cache-backed decoded preview enters native paint",std::any_of(previewPaint->commands.begin(),previewPaint->commands.end(),[&](const auto& command)
            { return command.image==image; }));
            const auto missingAsset=ui.tree().get(*missing)->textureControl->current.asset;
            std::vector<std::uint8_t> incompleteBytes(700,0);
            auto incompleteWrite=textureCache.write(missingAsset,incompleteBytes,1400,problem);
            ensure("partial asset bytes accepted by cache",incompleteWrite.valid());
            const auto retryDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
            while (previews.status(*missing)!=LLVKTexturePreview::Status::Failed)
            {
                const bool updated=previews.update(problem); ensure(problem,updated);
                ensure("bounded retry after cache publication",std::chrono::steady_clock::now()<retryDeadline);
                std::this_thread::yield();
            }
            ensure("cache miss retried after new bytes arrive",incompleteWrite.get().success);
            ensure("incomplete payload reports missing fetch responsibility",previews.failure(*missing).find("incomplete")!=std::string::npos);
            ensure("partial payload never becomes preview",!ui.tree().get(*missing)->textureControl->preview);
            auto corruptWrite=textureCache.write(missingAsset,std::vector<std::uint8_t>(1400,0),1400,problem);
            ensure("malformed complete fixture accepted as encoded bytes",corruptWrite.valid());
            const auto corruptDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
            do
            {
                const bool updated=previews.update(problem); ensure(problem,updated);
                ensure("bounded malformed decode rejection",std::chrono::steady_clock::now()<corruptDeadline);
                std::this_thread::yield();
            }
            while (previews.status(*missing)!=LLVKTexturePreview::Status::Failed || previews.failure(*missing).find("incomplete")!=std::string::npos);
            ensure("malformed bytes stored without fake success",corruptWrite.get().success && !previews.failure(*missing).empty() &&
                !ui.tree().get(*missing)->textureControl->preview);
        };
        const bool ran = LLVKWindowMgr::run(configuration,error);
        ensure(error,ran);
        const bool cacheStopped=textureCache.stop(error);
        ensure(error,cacheStopped);
        ensure("orderly quit destroys HWND",FindWindowW(L"VulkanstormNativeLogin",nullptr)==nullptr);
        ensure_equals("native shutdown persists client width",settings.find("WindowWidth")->getSaveValue().asInteger(),expectedWidth);
        ensure_equals("native shutdown persists client height",settings.find("WindowHeight")->getSaveValue().asInteger(),expectedHeight);
        ensure("no OpenGL parent module",GetModuleHandleW(L"opengl32.dll") == nullptr);
        std::cout << "Native login window presented six frames with real widgets, fonts, skin and browser\n";
    }
}