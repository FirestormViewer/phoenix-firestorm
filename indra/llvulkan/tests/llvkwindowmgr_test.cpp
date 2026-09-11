#include "linden_common.h"
#include "llvkwindowmgr.h"
#include "llvksettingsmgr.h"
#include "llvkaudio.h"
#include "llvkjoystick.h"
#include "lltut.h"
#include <windows.h>

namespace tut
{
    struct loginwindow_data {};
    typedef test_group<loginwindow_data> loginwindow_group;
    typedef loginwindow_group::object loginwindow_object;
    loginwindow_group loginwindow_tests("llvkwindowmgr");

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
        configuration.bindServices=[](LLVKViewerUi& ui)
        {
            std::string problem;
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
        };
        const bool ran = LLVKWindowMgr::run(configuration,error);
        ensure(error,ran);
        ensure("no OpenGL parent module",GetModuleHandleW(L"opengl32.dll") == nullptr);
        std::cout << "Native login window presented six frames with real widgets, fonts, skin and browser\n";
    }
}