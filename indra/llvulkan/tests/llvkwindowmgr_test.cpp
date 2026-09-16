#include "linden_common.h"
#include "llvkwindowmgr.h"
#include "llvksettingsmgr.h"
#include "llvkaudio.h"
#include "llvkjoystick.h"
#include "llvktexturecache.h"
#include "llvktexturepreview.h"
#include "llvkstartupstatus.h"
#include "llvkstartup.h"
#include "llerrorcontrol.h"
#include "llsdserialize.h"
#include "lltut.h"
#include <fstream>
#include <windows.h>
#include <tlhelp32.h>
#include <boost/asio.hpp>
#include "../../../tools/vulkan/diagnostic_replay_clock.h"

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
            LLVKSessionOwner session;
            LLVKTextureCache::Configuration configuration;
            configuration.directory=root/"cache"; configuration.localAssets=root/"assets";
            configuration.bytes=256ull*1024*1024;
            auto ownedCache=std::make_unique<LLVKApplicationCache>(configuration);
            auto& cache=ownedCache->cache();
            std::unique_ptr<LLVKSessionOwner::Service> service=std::move(ownedCache);
            const bool started=session.install(LLVKSessionOwner::Lifetime::Application,1,service).ok(); ensure(error,started);
            ensure("production cache is adopted",!service && session.snapshot().owned[0]==1);
            auto write=cache.write(LLUUID::generateNewID(),std::vector<std::uint8_t>(2048,42),2048,error);
            ensure("pending shutdown work accepted",write.valid());
            ensure("reopen presenter for shutdown",status.show("ShuttingDown",error));
            const auto shutdownWindow=FindWindowW(L"#32770",wide.c_str());
            ensure("shutdown dialog visible",shutdownWindow && IsWindowVisible(shutdownWindow));
            GetDlgItemTextW(shutdownWindow,666,text,256);
            ensure("original shutdown message",std::wstring(text)==L"Shutting down...");
            const bool stopped=session.shutdown().ok(); ensure(error,stopped);
            ensure("owner retires production cache",session.snapshot().state==LLVKSessionOwner::State::Stopped && session.snapshot().owned[0]==0);
            ensure("shutdown drains accepted write",write.get().success);
            ensure("status remains visible through cache retirement",IsWindowVisible(shutdownWindow)!=FALSE);
            status.hide();
            status.hide();
            ensure("explicit shutdown hide is idempotent",FindWindowW(L"#32770",wide.c_str())==nullptr);
            ensure("shutdown can reopen after hide",status.show("ShuttingDown",error));
            LLVKSessionOwner partial;
            configuration.directory="relative-cache";
            service=std::make_unique<LLVKApplicationCache>(configuration);
            ensure("failed acquisition is not successful startup",!partial.install(LLVKSessionOwner::Lifetime::Application,1,service).ok());
            ensure("partial cache adoption is fully retired",!service && partial.snapshot().state==LLVKSessionOwner::State::Stopped && partial.snapshot().owned[0]==0);
            status.hide();
            auto ownedStatus=std::make_shared<LLVKStartupStatus>();
            const auto ownedTitle=title+" owned";
            ensure("load service-owned shutdown presenter",ownedStatus->load(skin,ownedTitle,error));
            LLVKSessionOwner application;
            configuration.directory=root/"owned-cache";
            service=std::make_unique<LLVKApplicationCache>(configuration,ownedStatus);
            ensure("acquire cache with retained status",application.install(LLVKSessionOwner::Lifetime::Application,1,service).ok());
            ensure("service retains its presenter",ownedStatus.use_count()==2);
            ensure("retire cache and status together",application.shutdown().ok());
            ensure("status ownership released after retirement",ownedStatus.use_count()==1);
            const auto ownedWide=std::wstring(ownedTitle.begin(),ownedTitle.end());
            ensure("service shutdown hides status before recovery UI",FindWindowW(L"#32770",ownedWide.c_str())==nullptr);
        }
        ensure("status closes on scope exit",FindWindowW(L"#32770",wide.c_str())==nullptr);
        LLVKError::Resolver retained;
        {
            LLVKStartupStatus localized;
            skin.language="de";
            ensure("load localized error catalog without visual UI",localized.load(skin,title,error));
            retained=localized.errorResolver();
            ensure("German native error text",retained("NativeErrorRetryCleanup")=="Bereinigung wiederholen");
            skin.language="fr";
            ensure("reload distinct catalog",localized.load(skin,title,error));
            ensure("prior snapshot is immutable",retained("NativeErrorRetryCleanup")=="Bereinigung wiederholen");
            ensure("new snapshot uses new locale",localized.errorResolver()("NativeErrorRetryCleanup")!="Bereinigung wiederholen");
        }
        ensure("error catalog outlives loader and window",retained("NativeErrorRetryCleanup")=="Bereinigung wiederholen");
        ensure("missing catalog entry permits English fallback",retained("MissingNativeError").empty());
        const auto fatalRoot=std::filesystem::temp_directory_path()/("native-fatal-test-"+LLUUID::generateNewID().asString());
        struct Remove { std::filesystem::path path; ~Remove() { std::error_code ignored; std::filesystem::remove_all(path,ignored); } } remove{fatalRoot};
        unsigned reports=0;
        struct ExpectedFatal {};
        LLError::OverrideFatalFunction controlled([](const std::string&) { throw ExpectedFatal{}; });
        const auto emit=[]
        {
            try { LL_ERRS("NativeFatalFixture") << "synthetic-private-payload" << LL_ENDL; }
            catch (const ExpectedFatal&) { return; }
            ensure("fatal logger must retain termination contract",false);
        };
        {
            LLVKFatalReporting reporting(fatalRoot/"fatal.log",[&](const LLVKError& failure)
            { ++reports; ensure("fatal callback has stable code",failure.code==LLVKError::Code::Unexpected); });
            emit(); emit();
            ensure_equals("fatal reporting occurs only once",reports,1u);
            std::ifstream record(fatalRoot/"fatal.log");
            const std::string content((std::istreambuf_iterator<char>(record)),std::istreambuf_iterator<char>());
            ensure("fatal record contains safe structured facts",content.starts_with("native-error code=1000 ") &&
                content.find("synthetic-private-payload")==std::string::npos);
        }
        emit();
        ensure_equals("fatal callback removed on owner destruction",reports,1u);
        const auto priorWarning=LLError::LLUserWarningMsg::getHandler();
        std::string priorTitle,priorMessage;
        LLError::LLUserWarningMsg::getOutOfMemoryStrings(priorTitle,priorMessage);
        unsigned restoredWarnings=0;
        LLError::LLUserWarningMsg::setHandler([&](const std::string&,const std::string&,S32) { ++restoredWarnings; });
        struct RestoreWarning
        {
            LLError::LLUserWarningMsg::Handler handler;
            ~RestoreWarning() { LLError::LLUserWarningMsg::setHandler(handler); }
        } restoreWarning{priorWarning};
        for (const auto expected : {LLVKError::Code::OutOfMemory,LLVKError::Code::MissingFiles})
        {
            LLVKError::Code observed=LLVKError::Code::Unexpected;
            {
                LLVKFatalReporting reporting(fatalRoot/(expected==LLVKError::Code::OutOfMemory ? "oom.log" : "missing.log"),
                    [&](const LLVKError& failure) { observed=failure.code; });
                std::vector<std::thread> workers;
                for (unsigned worker=0; worker<4; ++worker) workers.emplace_back([expected]
                {
                    if (expected==LLVKError::Code::OutOfMemory) LLError::LLUserWarningMsg::showOutOfMemory();
                    else LLError::LLUserWarningMsg::showMissingFiles();
                });
                for (auto& worker : workers) worker.join();
                ensure("native warning retains typed cause",observed==expected);
                ensure("fatal warning tells native loop to stop",reporting.failure()==expected);
            }
            LLError::LLUserWarningMsg::show("restored warning fixture");
        }
        ensure_equals("warning handler restored after native scope",restoredWarnings,2u);
        std::string restoredTitle,restoredMessage;
        LLError::LLUserWarningMsg::getOutOfMemoryStrings(restoredTitle,restoredMessage);
        ensure("prior OOM strings restored",restoredTitle==priorTitle && restoredMessage==priorMessage);
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
        wchar_t windowsDirectory[MAX_PATH]{};
        const auto windowsLength=GetWindowsDirectoryW(windowsDirectory,MAX_PATH);
        ensure("native fixture resolves production system-font directory",windowsLength>0 && windowsLength<MAX_PATH);
        configuration.ui.fonts.searchDirectories.push_back(std::filesystem::path(windowsDirectory)/"Fonts");
        configuration.ui.settings = settings.values();
        configuration.ui.settingsGroup=&settings.group();
        configuration.ui.cacheDirectory=profile.path/"cache";
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
        struct GuidebookServer
        {
            boost::asio::io_context context;
            boost::asio::ip::tcp::acceptor acceptor{context,{boost::asio::ip::address_v4::loopback(),0}};
            std::atomic<bool> stopping{false};
            std::thread worker;
            GuidebookServer() : worker([this]
            {
                while (!stopping)
                {
                    boost::system::error_code problem;
                    boost::asio::ip::tcp::socket client(context);
                    acceptor.accept(client,problem);
                    if (problem || stopping) break;
                    const DWORD timeout=1000;
                    setsockopt(client.native_handle(),SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<const char*>(&timeout),sizeof(timeout));
                    boost::asio::streambuf request(16384);
                    boost::asio::read_until(client,request,"\r\n\r\n",problem);
                    if (problem) continue;
                    const std::string body="<html><body style='margin:0;background:rgb(255,255,0);height:2000px'>"
                        "<script>document.onclick=()=>document.body.style.background='rgb(0,255,255)';"
                        "document.onkeydown=()=>document.body.style.background='rgb(255,0,255)';"
                        "document.onwheel=()=>document.body.style.background='rgb(0,0,255)';</script></body></html>";
                    const auto response="HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\nContent-Length: "+
                        std::to_string(body.size())+"\r\n\r\n"+body;
                    boost::asio::write(client,boost::asio::buffer(response),problem);
                }
            }) {}
            ~GuidebookServer()
            {
                stopping=true;
                boost::system::error_code ignored;
                boost::asio::ip::tcp::socket wake(context);
                wake.connect(acceptor.local_endpoint(),ignored);
                worker.join();
            }
            std::string url() const { return "http://127.0.0.1:"+std::to_string(acceptor.local_endpoint().port())+"/guidebook"; }
        } guidebookServer;
        configuration.loginPage="data:text/html,<html><body style='margin:0;background:rgb(45,90,120)'>"
            "<a target='_blank' style='display:block;width:200px;height:48px' href='"+guidebookServer.url()+
            "'>Open login page link</a><a style='display:block;width:200px;height:48px' href='"+
            guidebookServer.url()+"'>Navigate login page</a></body></html>";
        ensure("loopback Guidebook URL",settings.set("GuidebookURL",LLSD(guidebookServer.url()),false,error));
        configuration.ui.settings["GuidebookURL"]=guidebookServer.url();
        int guidebookStage=0;
        int reportedGuidebookStage=-1;
        LLVKSessionOwner session;
        struct CleanupState { int attempts=0,frames=0; bool destroyed=false; };
        auto cleanupState=std::make_shared<CleanupState>();
        struct CleanupGate final : LLVKSessionOwner::Service
        {
            std::shared_ptr<CleanupState> state;
            explicit CleanupGate(std::shared_ptr<CleanupState> value) : state(std::move(value)) {}
            ~CleanupGate() override { state->destroyed=true; }
            LLVKSessionOwner::Code acquire(const LLVKSessionOwner::Context&) override { return LLVKSessionOwner::Code::Ok; }
            LLVKSessionOwner::Code retire() override
            {
                ++state->attempts;
                return state->attempts==1 ? LLVKSessionOwner::Code::Pending : state->attempts==2 ?
                    LLVKSessionOwner::Code::CleanupFailed : LLVKSessionOwner::Code::Ok;
            }
        };
        LLVKWidgetTree::Id originalGuidebook=0;
        LLVKWidgetTree::Id originalHelp=0;
        std::set<LLVKWidgetTree::Id> browsersBeforeLoginLink;
        const auto guidebookDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(90);
        configuration.presentedFrame=[&](LLVKViewerUi& ui,const LLVKWidgetPaint::Input& input)
        {
            if (reportedGuidebookStage!=guidebookStage)
            {
                LL_INFOS("Vulkan") << "Browser integration stage " << guidebookStage << LL_ENDL;
                reportedGuidebookStage=guidebookStage;
            }
            ensure("bounded integrated Guidebook completion at stage "+std::to_string(guidebookStage),
                std::chrono::steady_clock::now()<guidebookDeadline);
            const auto snapshot=session.snapshot();
            if (snapshot.state==LLVKSessionOwner::State::Disconnecting)
            {
                if (!cleanupState->attempts) return;
                ensure("application producers retire before cache dependency",snapshot.owned[0]==2 && input.browsers.empty());
                ensure("voice engine drained before dependency retry",llwebrtc::getDeviceInterface()==nullptr);
                ensure("owner snapshot reaches live recovery UI",ui.sessionSnapshot().state==LLVKSessionOwner::State::Disconnecting);
                ensure("retired audio callback is cleared",!ui.previewUiSound("UISndClick",error));
                ui.takeDialogError();
                if (snapshot.cleanup.action!=LLVKSessionOwner::Action::RetryCleanup) return;
                ensure_equals("failed retirement is never retried automatically",cleanupState->attempts,2);
                if (++cleanupState->frames<3) return;
                const auto retry=ui.find("retry_cleanup",ui.modalNotice());
                ensure("cleanup action presented in surviving visual host",retry!=0);
                ensure("retry through actual modal button",ui.tree().commit(retry));
                ensure("UI action completes owner shutdown",session.snapshot().state==LLVKSessionOwner::State::Stopped);
                return;
            }
            ensure("cache gate and real window services adopted",snapshot.owned[0]==3);
            if (guidebookStage==8)
            {
                const auto inspector=ui.find("overlap_panel");
                ensure("overlap diagnostic reaches presented frame",inspector && ui.tree().get(inspector)->params.visible &&
                    !ui.tree().get(inspector)->overlapPanel->elements.empty());
                std::string problem;
                ensure("configure local Help",ui.tree().updateSetting("HelpURLFormat",LLSD(guidebookServer.url())) &&
                    ui.tree().updateSetting("PreferredBrowserBehavior",LLSD(2)));
                const bool opened=ui.showHelp("fixture",problem); ensure(problem,opened);
                originalHelp=ui.helpBrowser();
                ++guidebookStage;
                return;
            }
            if (guidebookStage==9 || guidebookStage==10)
            {
                const auto frame=input.browsers.find(ui.helpBrowser());
                if (frame==input.browsers.end() || !frame->second) return;
                const auto pixels=frame->second->bottomUpRgba();
                if (pixels[0]!=255 || pixels[1]!=255 || pixels[2]!=0) return;
                ensure("Help remains prelogin",snapshot.state==LLVKSessionOwner::State::PreLogin);
                std::string problem;
                if (guidebookStage==9)
                {
                    ensure("live Help closes",ui.closeMenuWindow(problem));
                    const bool reopened=ui.showHelp("reopened",problem); ensure(problem,reopened);
                    ensure("live Help has fresh browser identity",ui.helpBrowser()!=originalHelp);
                }
                else
                {
                    ensure("close Help before login hyperlink",ui.closeMenuWindow(problem));
                    for (const auto& [id,frame] : input.browsers) browsersBeforeLoginLink.insert(id);
                    const auto window=FindWindowW(L"VulkanstormNativeLogin",nullptr);
                    const auto rectangle=ui.tree().screenRect(ui.find("login_html"),problem);
                    ensure("login hyperlink browser bounds",rectangle.has_value());
                    RECT client{}; GetClientRect(window,&client);
                    const auto point=MAKELPARAM(rectangle->left+20,client.bottom-rectangle->top+20);
                    SendMessageW(window,WM_LBUTTONDOWN,MK_LBUTTON,point);
                    SendMessageW(window,WM_LBUTTONUP,0,point);
                }
                ++guidebookStage;
                return;
            }
            if (guidebookStage==11)
            {
                const auto popup=ui.find("webbrowser",ui.activeFloater());
                if (browsersBeforeLoginLink.contains(popup)) return;
                const auto frame=input.browsers.find(popup);
                if (!popup || frame==input.browsers.end() || !frame->second) return;
                const auto pixels=frame->second->bottomUpRgba();
                if (pixels[0]!=255 || pixels[1]!=255 || pixels[2]!=0) return;
                ensure("login popup creates a distinct browser",popup!=ui.find("login_html") && popup!=originalGuidebook);
                ensure("login hyperlink stays prelogin",snapshot.state==LLVKSessionOwner::State::PreLogin);
                std::string problem;
                ensure("close login hyperlink popup",ui.closeMenuWindow(problem));
                const auto window=FindWindowW(L"VulkanstormNativeLogin",nullptr);
                const auto rectangle=ui.tree().screenRect(ui.find("login_html"),problem);
                ensure("same-window login link bounds",rectangle.has_value());
                RECT client{}; GetClientRect(window,&client);
                const auto point=MAKELPARAM(rectangle->left+20,client.bottom-rectangle->top+70);
                SendMessageW(window,WM_LBUTTONDOWN,MK_LBUTTON,point);
                SendMessageW(window,WM_LBUTTONUP,0,point);
                ++guidebookStage;
                return;
            }
            if (guidebookStage==12)
            {
                const auto frame=input.browsers.find(ui.find("login_html"));
                if (frame==input.browsers.end() || !frame->second) return;
                const auto pixels=frame->second->bottomUpRgba();
                if (pixels[0]!=255 || pixels[1]!=255 || pixels[2]!=0) return;
                ensure("same-window login navigation stays prelogin",snapshot.state==LLVKSessionOwner::State::PreLogin);
                ++guidebookStage;
                PostMessageW(FindWindowW(L"VulkanstormNativeLogin",nullptr),WM_CLOSE,0,0);
                return;
            }
            const auto id=ui.find("webbrowser",guidebookStage>=5 ? ui.activeFloater() : ui.guidebook());
            const auto found=input.browsers.find(id);
            if (found==input.browsers.end() || !found->second) return;
            const auto pixels=found->second->bottomUpRgba();
            const std::array<std::array<int,3>,8> expected{{{255,255,0},{0,255,255},{255,0,255},{0,0,255},{255,255,0},{255,255,0},{255,255,0},{255,255,0}}};
            if (guidebookStage>=8) return;
            const auto color=expected[guidebookStage];
            if (pixels[0]!=color[0] || pixels[1]!=color[1] || pixels[2]!=color[2]) return;
            std::string problem;
            const auto window=FindWindowW(L"VulkanstormNativeLogin",nullptr);
            const auto rect=ui.tree().screenRect(id,problem);
            ensure("visible browser bounds",rect.has_value());
            RECT client{}; GetClientRect(window,&client);
            const auto mouse=MAKELPARAM(rect->left+30,client.bottom-rect->top+30);
            if (guidebookStage==0)
            {
                const bool dumped=ui.dumpFontTextures(problem); ensure(problem,dumped);
                std::size_t pages=0;
                bool visibleGlyphs=false;
                for (const auto& file : std::filesystem::recursive_directory_iterator(profile.path/"logs"))
                {
                    if (!file.is_regular_file() || file.path().extension()!=".png") continue;
                    std::ifstream input(file.path(),std::ios::binary);
                    const std::vector<std::uint8_t> encoded{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
                    const auto image=LLVKWidgetImage::decodePng("dumped-atlas",encoded,problem); ensure(problem,image!=nullptr);
                    ensure("dumped native atlas dimensions",image->pixelWidth()==256 && image->pixelHeight()==256);
                    const auto rgba=image->bottomUpRgba();
                    for (std::size_t offset=3; offset<rgba.size(); offset+=4) visibleGlyphs|=rgba[offset]!=0;
                    ++pages;
                }
                ensure("native atlas diagnostic writes real glyph pages",pages>0 && visibleGlyphs);
                originalGuidebook=ui.guidebook();
                SendMessageW(window,WM_LBUTTONDOWN,MK_LBUTTON,mouse);
                SendMessageW(window,WM_LBUTTONUP,0,mouse);
                ensure_equals("Guidebook owns keyboard focus",ui.tree().keyboardFocus(),id);
            }
            else if (guidebookStage==1) SendMessageW(window,WM_KEYDOWN,'J',1);
            else if (guidebookStage==2)
            {
                POINT point{rect->left+30,client.bottom-rect->top+30}; ClientToScreen(window,&point);
                SendMessageW(window,WM_MOUSEWHEEL,MAKEWPARAM(0,-WHEEL_DELTA),MAKELPARAM(point.x,point.y));
            }
            else if (guidebookStage==3)
            {
                SendMessageW(window,WM_KEYDOWN,VK_F1,1);
                ensure("F1 closes embedded Guidebook",ui.guidebook()==0);
                ensure("immediate reopen while old view closes",ui.toggleGuidebook(problem));
                ensure("fresh Guidebook identity",ui.guidebook()!=originalGuidebook);
            }
            else if (guidebookStage==4)
            {
                const auto login=input.browsers.find(ui.find("login_html"));
                ensure("login browser retained",login!=input.browsers.end() && login->second);
                const auto loginPixels=login->second->bottomUpRgba();
                ensure("Guidebook input never navigates or repaints login",loginPixels[0]==45 && loginPixels[1]==90 && loginPixels[2]==120);
                const bool opened=ui.showMediaBrowser(guidebookServer.url(),problem); ensure(problem,opened);
            }
            else if (guidebookStage==5)
            {
                const auto address=ui.find("address",ui.activeFloater());
                const auto editor=ui.tree().get(address)->combo->editor;
                ensure("live Media Browser address edit",ui.tree().setValue(editor,LLSD(guidebookServer.url()+"?next")));
                ensure("live address navigation",ui.tree().commit(editor));
            }
            else if (guidebookStage==6)
            {
                const auto address=ui.find("address",ui.activeFloater());
                if (ui.tree().value(address).asString()!=guidebookServer.url()+"?next" ||
                    ui.tree().get(ui.find("stop",ui.activeFloater()))->params.visible) return;
                const auto back=ui.find("back",ui.activeFloater());
                if (!ui.tree().get(back)->params.enabled) return;
                ensure("live Back command",ui.tree().commit(back));
            }
            else
            {
                const auto address=ui.find("address",ui.activeFloater());
                if (ui.tree().value(address).asString()!=guidebookServer.url() ||
                    ui.tree().get(ui.find("stop",ui.activeFloater()))->params.visible) return;
                ensure("live Media Browser closes",ui.closeMenuWindow(problem));
                const bool opened=ui.showUiPreview(problem); ensure(problem,opened);
                const auto tool=ui.activeFloater();
                ensure("select integrated XUI preview",ui.tree().selectScrollListValue(ui.find("name_list",tool),LLSD("floater_test_textbox.xml"),true,problem));
                ensure("open integrated primary",ui.tree().commit(ui.find("display_floater",tool)));
                ensure(ui.dialogError(),ui.dialogError().empty());
                ensure("show overlap diagnostic",ui.tree().commit(ui.find("toggle_overlap_panel",tool)));
                const auto label=ui.find("left_aligned_text",ui.activeFloater());
                const auto labelRect=ui.tree().screenRect(label,problem); ensure(problem,labelRect.has_value());
                SendMessageW(window,WM_RBUTTONDOWN,MK_RBUTTON,
                    MAKELPARAM(labelRect->left+2,client.bottom-1-(labelRect->bottom+2)));
            }
            ++guidebookStage;
        };
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
        auto cacheService=std::make_unique<LLVKApplicationCache>(cacheConfiguration);
        auto& textureCache=cacheService->cache();
        std::unique_ptr<LLVKSessionOwner::Service> adoptedCache=std::move(cacheService);
        ensure("cache adopted before visual services",session.install(LLVKSessionOwner::Lifetime::Application,1,adoptedCache).ok());
        configuration.textureCache=&textureCache;
        auto rejected=configuration;
        auto failureCode=LLVKError::Code::Unexpected;
        rejected.failureCode=&failureCode;
        rejected.browser.helperDirectory=profile.path/"missing-browser-helper";
        rejected.bindServices={};
        bool launchNoticePresented=false;
        unsigned failureFrames=0;
        unsigned failurePhase=0;
        const auto captureDirectory=std::getenv("LLVK_NOTIFICATION_CAPTURE_DIR");
        const auto capturePage=std::getenv("LLVK_NOTIFICATION_CAPTURE_PAGE");
        LLSD captureRequest;
        captureRequest["name"]="MediaPluginFailed";
        captureRequest["substitutions"]["PLUGIN"]="media_plugin_cef";
        if (const auto requestPath=std::getenv("LLVK_NOTIFICATION_CAPTURE_REQUEST"))
        {
            ensure("custom notification capture requires a working local page",captureDirectory && capturePage);
            ensure("capture request is bounded",std::filesystem::file_size(requestPath)<=65536);
            std::ifstream requestFile(requestPath);
            ensure("capture request parses",LLSDSerialize::fromXML(captureRequest,requestFile)>0);
            ensure("capture request contains a name and substitutions",captureRequest["name"].isString() &&
                !captureRequest["name"].asString().empty() && captureRequest["substitutions"].isMap());
        }
        const auto captureName=captureRequest["name"].asString();
        if (captureRequest["helpBrowser"].asBoolean())
        {
            ensure("Help capture requires the local page",capturePage!=nullptr);
            ensure("Help capture URL",settings.set("HelpURLFormat",LLSD(capturePage),false,error));
            ensure("Help capture internal policy",settings.set("PreferredBrowserBehavior",LLSD(2),false,error));
            rejected.ui.settings=settings.values();
        }
        const bool browserSequence=captureRequest.has("sequence") &&
            (captureRequest["sequence"].asString()=="Browser" || captureRequest["sequence"].asString()=="Dialogs");
        bool sequenceActive=false;
        const bool liveScale=std::getenv("LLVK_CAPTURE_LIVE_SCALE")!=nullptr;
        const auto initialLiveScale=liveScale && captureRequest.has("display") &&
            captureRequest["display"]["UIScaleFactor"].asReal()==1.0 ? 1.25f : 1.f;
        if (captureRequest.has("display"))
        {
            const auto& display=captureRequest["display"];
            ensure("display overrides are a map",display.isMap());
            for (const auto& name : {"Language","WindowWidth","WindowHeight","UIScaleFactor","RenderAnisotropic"})
                if (display.has(name)) ensure("isolated display override",settings.set(name,display[name],false,error));
            rejected.ui.settings=settings.values();
            rejected.ui.skin.language=LLVKViewerUi::uiLanguage(rejected.ui.settings);
            ensure("isolated language fallback reset",settings.set("Language",rejected.ui.settings.at("Language"),false,error));
        }
        if (liveScale)
        {
            ensure("live scale fixture has a target",captureRequest["display"].has("UIScaleFactor"));
            ensure("live scale starts at a different value",settings.set("UIScaleFactor",LLSD(initialLiveScale),false,error));
            rejected.ui.settings=settings.values();
        }
        const bool buttonStates=std::getenv("LLVK_CAPTURE_LOGIN_BUTTON_STATES")!=nullptr;
        const bool pressedOnly=buttonStates && std::string_view(std::getenv("LLVK_CAPTURE_LOGIN_BUTTON_STATES"))=="pressed";
        const bool focusStates=buttonStates && std::string_view(std::getenv("LLVK_CAPTURE_LOGIN_BUTTON_STATES"))=="focus";
        struct CursorRestore
        {
            POINT point{};
            bool active=false;
            ~CursorRestore() { if (active) SetCursorPos(point.x,point.y); }
        } cursorRestore;
        unsigned buttonPhase=0;
        auto buttonSince=std::chrono::steady_clock::now();
        const std::array<const char*,5> buttonStateNames{"enabled","hover","pressed","focus-lost","focus-regained"};
        if (captureDirectory && capturePage)
        {
            rejected.browser.helperDirectory=configuration.browser.helperDirectory;
            rejected.loginPage=capturePage;
        }
        const bool captureMaximized=std::getenv("LLVK_NOTIFICATION_CAPTURE_MAXIMIZED")!=nullptr;
        if (captureDirectory)
            rejected.bindServices=[&](LLVKViewerUi& ui)
            {
                if (captureMaximized) ShowWindow(FindWindowW(L"VulkanstormNativeLogin",nullptr),SW_MAXIMIZE);
                if (capturePage)
                {
                    ensure("working-browser capture queues reference modal",ui.queueNotice(captureName,captureRequest["substitutions"],{},error));
                }
            };
        std::future<DWORD> captureResult;
        std::optional<double> diagnosticRequestedTime;
        std::optional<double> diagnosticBaseTime;
        if (captureDirectory && std::getenv("LL_DIAGNOSTIC_REPLAY_DIR"))
            rejected.diagnosticFrameTime=[&](double previous) -> std::optional<double>
            {
                if (diagnosticRequestedTime) return diagnosticRequestedTime;
                auto& clock=diagnostic_replay::Clock::instance();
                if (!clock.begin()) return std::nullopt;
                if (!diagnosticBaseTime) diagnosticBaseTime=previous;
                diagnosticRequestedTime=*diagnosticBaseTime+clock.microseconds(0,0)/1000000.;
                return diagnosticRequestedTime;
            };
        struct InactiveFocusOwner
        {
            HWND window=nullptr;
            ~InactiveFocusOwner() { if (window) DestroyWindow(window); }
        } inactiveFocusOwner;
        unsigned captures=0;
        std::map<std::string,std::vector<double>> timingSamples;
        if (captureDirectory)
            rejected.diagnosticTiming=[&](const char* stage,double milliseconds)
            {
                const std::string phase=failurePhase ? "shutdown/" : sequenceActive ? "interactive/" : "startup/";
                auto& samples=timingSamples[phase+stage];
                if (samples.size()<10000) samples.push_back(milliseconds);
            };
        rejected.presentedFrame=[&](LLVKViewerUi& ui,const LLVKWidgetPaint::Input& input)
        {
            if (rejected.diagnosticFrameTime)
            {
                diagnostic_replay::Clock::instance().finish();
                diagnosticRequestedTime.reset();
            }
            ++failureFrames;
            if (sequenceActive)
            {
                ensure("browser sequence stays prelogin",ui.sessionSnapshot().state==LLVKSessionOwner::State::PreLogin);
                ensure("browser sequence does not enable login",!ui.tree().get(ui.find("connect_btn"))->params.enabled);
                if (std::filesystem::exists(std::filesystem::path(captureDirectory)/"sequence-complete.json"))
                {
                    std::ofstream geometry(std::filesystem::path(captureDirectory)/"native-widget-geometry.txt");
                    std::vector<LLVKWidgetTree::Id> nodes{ui.root()};
                    for (std::size_t index=0; index<nodes.size(); ++index)
                    {
                        const auto* node=ui.tree().get(nodes[index]);
                        if (!node) continue;
                        nodes.insert(nodes.end(),node->children.begin(),node->children.end());
                        geometry<<nodes[index]<<" parent="<<node->parent<<" name="<<node->params.name
                            <<" rect="<<node->params.rect.left<<","<<node->params.rect.bottom<<","<<node->params.rect.right<<","<<node->params.rect.top;
                        if (node->control && node->control->params.font)
                        {
                            const auto& metrics=node->control->params.font->metrics();
                            geometry<<" ascent="<<metrics.ascender<<" descent="<<metrics.descender;
                        }
                        geometry<<"\n";
                    }
                    failurePhase=1;
                    PostMessageW(FindWindowW(L"VulkanstormNativeLogin",nullptr),WM_CLOSE,0,0);
                }
                return;
            }
            if (buttonStates && buttonPhase)
            {
                if (std::chrono::steady_clock::now()-buttonSince<std::chrono::seconds(2)) return;
                ensure("synthetic credentials enable login",ui.tree().get(ui.find("connect_btn"))->params.enabled);
                ensure("button state does not start authentication",ui.sessionSnapshot().state==LLVKSessionOwner::State::PreLogin);
                if (buttonPhase>=4) ensure("activation event reaches native paint",input.editor.applicationFocused==(buttonPhase==5));
                if (buttonPhase==3) ensure("pressed login owns pointer capture",ui.tree().mouseCapture()==ui.find("connect_btn"));
                if (buttonPhase==3)
                {
                    const auto rect=ui.tree().screenRect(ui.find("connect_btn"),error);
                    ensure("pressed pointer stays inside login",rect && input.button.mouseX>=rect->left && input.button.mouseX<rect->right &&
                        input.button.mouseY>=rect->bottom && input.button.mouseY<rect->top);
                }
            }
            if (!capturePage) ensure("failed login browser does not block native presentation",input.browsers.empty());
            else
            {
                const auto frame=input.browsers.find(ui.find("login_html"));
                if (frame==input.browsers.end() || !frame->second) return;
                if (captureMaximized && frame->second->width()!=2048) return;
                const auto pixels=frame->second->bottomUpRgba();
                const auto middle=4*(frame->second->pixelWidth()*(frame->second->pixelHeight()/2)+frame->second->pixelWidth()/2);
                if (pixels[middle]!=41 || pixels[middle+1]!=41 || pixels[middle+2]!=41) return;
            }
            if (!launchNoticePresented && !buttonPhase)
            {
                const auto modal=ui.modalNotice();
                ensure("actual browser launch failure presents reference notification",modal &&
                    ui.tree().get(modal)->params.name==captureName);
                if (captureName=="MediaPluginFailed") ensure("browser notification includes implementation name",
                    ui.tree().value(ui.find("Alert message",modal)).asString().find("media_plugin_cef")!=std::string::npos);
                ensure("login credential controls remain usable",ui.tree().get(ui.find("username_combo"))->params.enabled &&
                    ui.tree().get(ui.find("password_edit"))->params.enabled);
                ensure("empty credentials keep login disabled",!ui.tree().get(ui.find("connect_btn"))->params.enabled);
                launchNoticePresented=true;
                if (liveScale)
                {
                    ensure_equals("live fixture begins at source scale",ui.displayScale(),initialLiveScale);
                    ensure("live scale preference applied",settings.set("UIScaleFactor",captureRequest["display"]["UIScaleFactor"],false,error));
                    failureFrames=0;
                    return;
                }
                if (captureRequest["inactiveFocus"].asBoolean())
                {
                    inactiveFocusOwner.window=CreateWindowExW(WS_EX_TOOLWINDOW,L"STATIC",L"Capture focus owner",WS_POPUP,
                        -30000,-30000,1,1,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
                    ensure("inactive capture focus owner",inactiveFocusOwner.window!=nullptr);
                    ShowWindow(inactiveFocusOwner.window,SW_SHOW);
                    SetForegroundWindow(inactiveFocusOwner.window);
                    ensure("inactive window state applied",GetForegroundWindow()==inactiveFocusOwner.window);
                    SendMessageW(FindWindowW(L"VulkanstormNativeLogin",nullptr),WM_KILLFOCUS,0,0);
                    failureFrames=0;
                    return;
                }
                if (captureRequest.has("input"))
                {
                    const auto editor=ui.find("notification_input",modal);
                    ensure("notification input exists",editor!=0);
                    const auto value=captureRequest["input"].asString();
                    const auto window=FindWindowW(L"VulkanstormNativeLogin",nullptr);
                    for (const auto character : value) SendMessageW(window,WM_CHAR,static_cast<unsigned char>(character),1);
                    ensure_equals("notification text input applied",ui.tree().value(editor).asString(),value);
                    if (captureRequest["select"].asBoolean()) ensure("notification select-all applied",ui.tree().selectLineEditorAll(editor,error));
                }
                if (captureRequest["checkIgnore"].asBoolean())
                {
                    const auto check=ui.find("notification_ignore",modal);
                    const auto rect=ui.tree().screenRect(check,error);
                    ensure("notification ignore control exists",rect.has_value());
                    const auto window=FindWindowW(L"VulkanstormNativeLogin",nullptr);
                    RECT client{}; GetClientRect(window,&client);
                    const auto point=MAKELPARAM(rect->left+5,client.bottom-1-(rect->bottom+7));
                    SendMessageW(window,WM_LBUTTONDOWN,MK_LBUTTON,point);
                    SendMessageW(window,WM_LBUTTONUP,0,point);
                    ensure("notification ignore checked",ui.tree().value(check).asBoolean());
                }
                if (captureRequest.has("input") || captureRequest["checkIgnore"].asBoolean())
                { failureFrames=0; return; }
            }
            if (failureFrames<40) return;
            if (liveScale) ensure_equals("live scale reached presentation",ui.displayScale(),static_cast<float>(captureRequest["display"]["UIScaleFactor"].asReal()));
            if (captureRequest["inactiveFocus"].asBoolean())
            {
                ensure("inactive keyboard focus reaches painting",!input.editor.applicationFocused);
                ensure("inactive window stays nonforeground",GetForegroundWindow()!=FindWindowW(L"VulkanstormNativeLogin",nullptr));
            }
            if (captureDirectory && !failurePhase && captures<2 && !(pressedOnly && buttonPhase<3) && !(focusStates && buttonPhase<4))
            {
                if (!captureResult.valid())
                {
                    const auto directory=std::filesystem::path(captureDirectory);
                    std::filesystem::create_directories(directory);
                    const auto stateName=buttonPhase ? std::string("native-login-")+buttonStateNames[buttonPhase-1]+"-settled-" : "native-"+captureName+"-settled-";
                    const auto destination=directory/(stateName+std::to_string(captures)+".rgba");
                    const auto window=FindWindowW(L"VulkanstormNativeLogin",nullptr);
                    ensure("requested maximized native capture",!captureMaximized || IsZoomed(window));
                    std::wstring command=L"\""+std::filesystem::path(LLVK_NOTIFICATION_CAPTURE_EXE).wstring()+L"\" "+
                        std::to_wstring(reinterpret_cast<std::uintptr_t>(window))+L" \""+destination.wstring()+L"\"";
                    captureResult=std::async(std::launch::async,[command=std::move(command)]() mutable
                    {
                        STARTUPINFOW startup{}; startup.cb=sizeof(startup);
                        PROCESS_INFORMATION process{};
                        if (!CreateProcessW(nullptr,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process))
                            return GetLastError();
                        CloseHandle(process.hThread);
                        WaitForSingleObject(process.hProcess,INFINITE);
                        DWORD code=1; GetExitCodeProcess(process.hProcess,&code);
                        CloseHandle(process.hProcess);
                        return code;
                    });
                    std::ofstream metadata(directory/(stateName+std::to_string(captures)+".txt"));
                    const auto rect=ui.tree().get(buttonPhase ? ui.find("connect_btn") : ui.modalNotice())->params.rect;
                    metadata<<"backend=native\nnotification="<<captureName<<"\nstate=settled\n"
                        <<"capture=Windows.Graphics.Capture\nformat=RGBA8-top-origin-with-LE-width-height\n"
                        <<"maximized="<<(IsZoomed(window) ? "true" : "false")<<"\n"
                        <<"anisotropy="<<(ui.tree().setting("RenderAnisotropic").value_or(LLSD(false)).asBoolean() ? "on" : "off")<<"\n"
                        <<"modal="<<rect.left<<","<<rect.bottom<<","<<rect.right<<","<<rect.top<<"\n";
                    if (buttonPhase) metadata<<"buttonState="<<buttonStateNames[buttonPhase-1]<<"\n";
                    RECT captureClient{};
                    ensure("capture client geometry available",GetClientRect(window,&captureClient)!=FALSE);
                    const auto rootRect=ui.tree().get(ui.root())->params.rect;
                    metadata<<"uiScaleSetting="<<ui.tree().setting("UIScaleFactor").value_or(LLSD(1.0)).asReal()<<"\n"
                        <<"physicalClient="<<captureClient.right-captureClient.left<<","<<captureClient.bottom-captureClient.top<<"\n"
                        <<"uiRoot="<<rootRect.left<<","<<rootRect.bottom<<","<<rootRect.right<<","<<rootRect.top<<"\n"
                        <<"fontRegistryDpi="<<rejected.ui.fonts.horizontalDpi<<","<<rejected.ui.fonts.verticalDpi<<"\n";
                    if (capturePage)
                    {
                        struct Snapshot
                        {
                            HANDLE handle;
                            ~Snapshot() { if (handle!=INVALID_HANDLE_VALUE) CloseHandle(handle); }
                        } processes{CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0)};
                        ensure("CEF process snapshot available",processes.handle!=INVALID_HANDLE_VALUE);
                        std::map<DWORD,DWORD> parents;
                        PROCESSENTRY32W processEntry{}; processEntry.dwSize=sizeof(processEntry);
                        ensure("CEF process snapshot readable",Process32FirstW(processes.handle,&processEntry)!=FALSE);
                        do
                        {
                            if (_wcsicmp(processEntry.szExeFile,L"dullahan_host.exe")==0)
                                parents.emplace(processEntry.th32ProcessID,processEntry.th32ParentProcessID);
                        }
                        while (Process32NextW(processes.handle,&processEntry));
                        unsigned browserChildren=0,d3dChildren=0;
                        for (const auto& [processId,parentId] : parents)
                        {
                            if (parentId!=GetCurrentProcessId()) continue;
                            Snapshot modules{CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,processId)};
                            metadata<<"cefInspectProcess="<<processId<<" snapshotError="
                                <<(modules.handle==INVALID_HANDLE_VALUE ? GetLastError() : 0)<<"\n"<<std::flush;
                            ensure("browser child modules available",modules.handle!=INVALID_HANDLE_VALUE);
                            MODULEENTRY32W moduleEntry{}; moduleEntry.dwSize=sizeof(moduleEntry);
                            ensure("browser child modules readable",Module32FirstW(modules.handle,&moduleEntry)!=FALSE);
                            bool cef=false,d3d=false,openGl=false;
                            do
                            {
                                cef|=_wcsicmp(moduleEntry.szModule,L"libcef.dll")==0;
                                d3d|=_wcsicmp(moduleEntry.szModule,L"d3d11.dll")==0;
                                openGl|=_wcsicmp(moduleEntry.szModule,L"opengl32.dll")==0;
                            } while (Module32NextW(modules.handle,&moduleEntry));
                            metadata<<"cefProcess="<<processId<<" cef="<<cef<<" d3d11="<<d3d
                                <<" opengl32="<<openGl<<"\n"<<std::flush;
                            if (!cef) continue;
                            ensure("CEF child does not load OpenGL",!openGl);
                            ++browserChildren;
                            if (d3d) ++d3dChildren;
                        }
                        ensure("CEF children observed",browserChildren>0);
                        ensure("CEF D3D11 child observed",d3dChildren>0);
                        metadata<<"cefChildren="<<browserChildren<<"\ncefD3D11Children="<<d3dChildren
                            <<"\ncefOpenGLModules=0\n";
                    }
                    const auto modeLabel=ui.find("mode_selection_text");
                    const auto modeRect=ui.tree().screenRect(modeLabel,error);
                    if (modeRect && ui.tree().get(modeLabel)->plainText->layout)
                    {
                        metadata<<"modeLabel="<<modeRect->left<<","<<modeRect->bottom<<","<<modeRect->right<<","<<modeRect->top<<"\n";
                        for (const auto& line : ui.tree().get(modeLabel)->plainText->layout->lines)
                            metadata<<"modeLine="<<line.left<<","<<line.bottom<<","<<line.right<<","<<line.top<<"\n";
                    }
                }
                if (captureResult.wait_for(std::chrono::seconds(0))!=std::future_status::ready) return;
                ensure_equals("external native notification capture succeeds",captureResult.get(),DWORD{0});
                ++captures;
                return;
            }
            if (!buttonPhase)
            {
                ensure("browser notification acknowledgement",ui.noticeKey(true,false,error));
                ensure("acknowledgement does not close viewer",!ui.modalNotice());
            }
            if (buttonStates && capturePage)
            {
                const auto window=FindWindowW(L"VulkanstormNativeLogin",nullptr);
                RECT client{}; GetClientRect(window,&client);
                ensure("button fixture uses verified maximized extent",client.right==2560 && client.bottom==1369);
                const auto outside=MAKELPARAM(1280,900),buttonPoint=MAKELPARAM(1695,1266);
                if (!buttonPhase)
                {
                    for (const auto& field : {std::pair{MAKELPARAM(1000,1255),"fixture-user"},std::pair{MAKELPARAM(1250,1255),"fixture-only"}})
                    {
                        SendMessageW(window,WM_LBUTTONDOWN,MK_LBUTTON,field.first);
                        SendMessageW(window,WM_LBUTTONUP,0,field.first);
                        for (const char* character=field.second; *character; ++character) SendMessageW(window,WM_CHAR,*character,1);
                    }
                    SendMessageW(window,WM_LBUTTONDOWN,MK_LBUTTON,outside);
                    SendMessageW(window,WM_LBUTTONUP,0,outside);
                    SendMessageW(window,WM_MOUSEMOVE,0,outside);
                }
                else if (buttonPhase==1) SendMessageW(window,WM_MOUSEMOVE,0,buttonPoint);
                else if (buttonPhase==2)
                {
                    cursorRestore.active=GetCursorPos(&cursorRestore.point)!=FALSE;
                    POINT point{1695,1266}; ClientToScreen(window,&point);
                    SetCursorPos(point.x,point.y);
                    SendMessageW(window,WM_MOUSEMOVE,0,buttonPoint);
                    SendMessageW(window,WM_LBUTTONDOWN,MK_LBUTTON,buttonPoint);
                }
                else if (focusStates && buttonPhase==3)
                {
                    SendMessageW(window,WM_MOUSEMOVE,MK_LBUTTON,outside);
                    SendMessageW(window,WM_LBUTTONUP,0,outside);
                    SendMessageW(window,WM_KILLFOCUS,0,0);
                }
                else if (focusStates && buttonPhase==4) SendMessageW(window,WM_SETFOCUS,0,0);
                else
                {
                    SendMessageW(window,WM_MOUSEMOVE,MK_LBUTTON,outside);
                    SendMessageW(window,WM_LBUTTONUP,0,outside);
                    ensure("release outside does not authenticate",ui.sessionSnapshot().state==LLVKSessionOwner::State::PreLogin && !ui.modalNotice());
                    failurePhase=1;
                    PostMessageW(window,WM_CLOSE,0,0);
                    return;
                }
                ++buttonPhase;
                captures=0;
                buttonSince=std::chrono::steady_clock::now();
                return;
            }
            if (capturePage)
            {
                if (browserSequence)
                {
                    ensure("browser sequence capture configuration",captureDirectory && captureMaximized &&
                        (ui.displayScale()==1.f || captureRequest["sequence"].asString()=="Dialogs"));
                    sequenceActive=true;
                    std::ofstream(std::filesystem::path(captureDirectory)/"sequence-ready.txt")<<"modal dismissed\n";
                    return;
                }
                if (ui.displayScale()!=1.f)
                {
                    const auto window=FindWindowW(L"VulkanstormNativeLogin",nullptr);
                    const auto password=ui.find("password_edit");
                    const auto rectangle=ui.tree().screenRect(password,error);
                    ensure("scaled input rectangle",rectangle.has_value());
                    RECT client{}; GetClientRect(window,&client);
                    const auto horizontal=static_cast<int>(std::floor((rectangle->left+5)*ui.displayScale()+0.5f));
                    const auto vertical=client.bottom-1-static_cast<int>(std::floor((rectangle->bottom+10)*ui.displayScale()+0.5f));
                    SendMessageW(window,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(horizontal,vertical));
                    SendMessageW(window,WM_LBUTTONUP,0,MAKELPARAM(horizontal,vertical));
                    ensure_equals("scaled pointer focuses password",ui.tree().keyboardFocus(),password);
                    SendMessageW(window,WM_CHAR,'x',1);
                    ensure_equals("scaled field receives text",ui.tree().value(password).asString(),std::string("x"));
                    ensure("scaled input remains prelogin",ui.sessionSnapshot().state==LLVKSessionOwner::State::PreLogin);
                }
                failurePhase=1;
                PostMessageW(FindWindowW(L"VulkanstormNativeLogin",nullptr),WM_CLOSE,0,0);
                return;
            }
            if (!failurePhase)
            {
                ensure("failed auxiliary browser leaves floater usable",ui.showMediaBrowser("https://example.invalid/",error));
                const auto floater=ui.activeFloater();
                ensure("auxiliary failure shows existing failure text",ui.tree().get(ui.find("plugin_fail_text",floater))->params.visible);
                ensure("auxiliary unavailable surface does not block paint",!ui.tree().get(ui.find("webbrowser",floater))->params.visible);
                failureFrames=0;
                launchNoticePresented=false;
                ++failurePhase;
                return;
            }
            PostMessageW(FindWindowW(L"VulkanstormNativeLogin",nullptr),WM_CLOSE,0,0);
        };
        const auto captureRun=LLVKWindowMgr::run(rejected,error);
        if (captureDirectory)
        {
            std::ofstream report(std::filesystem::path(captureDirectory)/"stage-timing.csv");
            report<<"stage,count,meanMs,p95Ms,maxMs,totalMs\n";
            for (auto& [stage,samples] : timingSamples)
            {
                if (samples.empty()) continue;
                std::sort(samples.begin(),samples.end());
                double total=0.; for (const auto sample : samples) total+=sample;
                report<<stage<<','<<samples.size()<<','<<total/samples.size()<<','
                    <<samples[(samples.size()-1)*95/100]<<','<<samples.back()<<','<<total<<'\n';
            }
        }
        if (!captureRun && captureDirectory)
            std::ofstream(std::filesystem::path(captureDirectory)/"window-failure.txt")<<error;
        ensure("browser launch failure leaves viewer usable: "+error,captureRun);
        ensure("browser notification was presented",launchNoticePresented);
        ensure_equals("both login and auxiliary launch failures exercised",failurePhase,1u);
        ensure("browser failure fixture closes native window",FindWindowW(L"VulkanstormNativeLogin",nullptr)==nullptr);
        if (captureDirectory && capturePage)
        {
            ensure("working browser capture retires cache owner",session.shutdown().ok());
            return;
        }
        std::unique_ptr<LLVKSessionOwner::Service> cleanupGate=std::make_unique<CleanupGate>(cleanupState);
        ensure("install controlled cleanup dependency",session.install(LLVKSessionOwner::Lifetime::Application,3,cleanupGate).ok());
        configuration.sessionOwner=&session;
        int expectedWidth=0,expectedHeight=0;
        configuration.bindServices=[&settings,&expectedWidth,&expectedHeight,&textureCache,previewAsset,replacementAsset](LLVKViewerUi& ui)
        {
            std::string problem;
            ensure("authoritative echo cancellation override",settings.set("VoiceEchoCancellation",LLSD(false),false,problem));
            const auto focusWindow=FindWindowW(L"VulkanstormNativeLogin",nullptr);
            const auto focusCombo=ui.find("mode_combo");
            ensure("focus fixture opens mode popup",ui.tree().showComboList(focusCombo,problem));
            ensure_equals("mode popup owns top control",ui.tree().topControl(),focusCombo);
            const auto retainedFocus=ui.tree().keyboardFocus();
            SendMessageW(focusWindow,WM_KILLFOCUS,0,0);
            ensure_equals("focus loss clears top popup",ui.tree().topControl(),LLVKWidgetTree::Id(0));
            ensure_equals("focus loss releases pointer capture",ui.tree().mouseCapture(),LLVKWidgetTree::Id(0));
            ensure_equals("focus loss retains keyboard control",ui.tree().keyboardFocus(),retainedFocus);
            SendMessageW(focusWindow,WM_SETFOCUS,0,0);
            ensure_equals("focus regain restarts flash",ui.tree().focusFlashAmount(),1.f);
            ensure("authoritative automatic gain override",settings.set("VoiceAutomaticGainControl",LLSD(false),false,problem));
            ensure("authoritative noise suppression override",settings.set("VoiceNoiseSuppressionLevel",LLSD(2),false,problem));
            ensure("native Preferences in presentation",ui.showPreferences(problem));
            ensure("native About in presentation",ui.showAbout(problem));
            const bool debugOpened=ui.showDebugSettings(problem); ensure(problem,debugOpened);
            ui.takeNotices();
            const auto debug=ui.activeFloater();
            const auto search=ui.find("search_settings_input",debug);
            ensure("search live settings in native window",ui.tree().setValue(search,LLSD("RenderFarClip")) && ui.tree().commit(search));
            ensure("live Debug Settings paint",ui.preparePaint({},problem).has_value());
            ensure("close Debug Settings",ui.closeMenuWindow(problem));
            const bool colorsOpened=ui.showColorSettings(problem); ensure(problem,colorsOpened);
            ensure("live Color Settings paint",ui.preparePaint({},problem).has_value());
            ensure("close Color Settings",ui.closeMenuWindow(problem));
            const bool widgetsOpened=ui.showUiTest("test_widgets",problem); ensure(problem,widgetsOpened);
            ensure("original Widgets native paint",ui.preparePaint({},problem).has_value());
            const auto widgetMenu=ui.find("test_menu_bar",ui.activeFloater());
            const auto menuRect=ui.tree().screenRect(widgetMenu,problem);
            ensure("embedded Widgets menu bounds",menuRect.has_value());
            const auto widgetWindow=FindWindowW(L"VulkanstormNativeLogin",nullptr);
            RECT widgetClient{}; GetClientRect(widgetWindow,&widgetClient);
            const auto menuPoint=MAKELPARAM(menuRect->left+10,widgetClient.bottom-1-(menuRect->bottom+3));
            SendMessageW(widgetWindow,WM_LBUTTONDOWN,MK_LBUTTON,menuPoint);
            SendMessageW(widgetWindow,WM_LBUTTONUP,0,menuPoint);
            ensure("embedded Widgets menu opens through Win32",ui.tree().get(widgetMenu)->menu->open());
            SendMessageW(widgetWindow,WM_KEYDOWN,VK_ESCAPE,1);
            ensure("embedded Widgets menu consumes Escape",!ui.tree().get(widgetMenu)->menu->open());
            ensure("close original Widgets dialog",ui.closeMenuWindow(problem));
            const bool whitelistOpened=ui.showWhitelist(problem);
            ensure(problem,whitelistOpened);
            ensure("original whitelist in native window",ui.find("whitelist_folders_editor",ui.activeFloater())!=0);
            ensure("native whitelist presentation",ui.preparePaint({},problem).has_value());
            ensure("close integrated whitelist",ui.closeFloater(problem));
            ensure("return to About",ui.showAbout(problem));
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
            SendMessageW(window,WM_KEYDOWN,'J',1);
            ensure("Windows input closes key capture",ui.keyCaptureDialog()==0);
            ensure("resize dialog opens in actual window",ui.showWindowSize(problem));
            const auto sizeDialog=ui.activeFloater(),sizeCombo=ui.find("window_size_combo",sizeDialog);
            ensure("edit native client dimensions",ui.tree().setValue(ui.tree().get(sizeCombo)->combo->editor,LLSD("1100 x 800")));
            ensure("apply actual Win32 client dimensions",ui.tree().commit(ui.find("set_btn",sizeDialog)));
            ensure(ui.dialogError(),ui.dialogError().empty());
            RECT client{};
            ensure("read normal native client geometry",GetClientRect(window,&client)!=FALSE);
            ensure("exact requested client size",client.right-client.left==1100 && client.bottom-client.top==800);
            expectedWidth=client.right-client.left; expectedHeight=client.bottom-client.top;
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
            ui.menu().key(LLVKMenu::Key::Activate); ui.menu().key(LLVKMenu::Key::Right);
            SendMessageW(window,WM_CHAR,'g',1);
            ensure("Help jump key consumed and menu closed",!ui.menu().open());
            ensure(ui.dialogError(),ui.guidebook()!=0 && ui.dialogError().empty());
        };
        const bool ran = LLVKWindowMgr::run(configuration,error);
        ensure(error,ran);
        ensure_equals("browser, XUI preview, Help and login hyperlink sequence verified",guidebookStage,13);
        ensure("application and cache retirement completed",session.snapshot().state==LLVKSessionOwner::State::Stopped &&
            session.snapshot().owned[0]==0 && cleanupState->destroyed);
        ensure_equals("one pending poll and one explicit retry",cleanupState->attempts,3);
        ensure_equals("recovery remained presentable across frames",cleanupState->frames,3);
        ensure("orderly quit destroys HWND",FindWindowW(L"VulkanstormNativeLogin",nullptr)==nullptr);
        ensure_equals("native shutdown persists client width",settings.find("WindowWidth")->getSaveValue().asInteger(),expectedWidth);
        ensure_equals("native shutdown persists client height",settings.find("WindowHeight")->getSaveValue().asInteger(),expectedHeight);
        LLVKSettingsMgr reloaded;
        ensure("reload saved native window state",reloaded.loadFile(profile.path/"settings.xml",true,false,true,error));
        ensure("Guidebook visibility survives disk roundtrip",reloaded.find("floater_vis_guidebook") &&
            reloaded.find("floater_vis_guidebook")->getValue().asBoolean());
        ensure("Guidebook position survives disk roundtrip",reloaded.find("floater_pos_guidebook_x") &&
            reloaded.find("floater_pos_guidebook_x")->getValue().asReal()>=-1. &&
            reloaded.find("floater_pos_guidebook_x")->getValue().asReal()<=1.);
        ensure("no OpenGL parent module",GetModuleHandleW(L"opengl32.dll") == nullptr);
        std::cout << "Native login and Guidebook presented with independent mouse, keyboard, wheel, F1 close and reopen\n";
    }
}