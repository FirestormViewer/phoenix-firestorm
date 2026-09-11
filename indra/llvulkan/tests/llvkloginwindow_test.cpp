#include "linden_common.h"
#include "llvkloginwindow.h"
#include "llvkstartupsettings.h"
#include "llvkaudio.h"
#include "lltut.h"
#include <windows.h>

namespace tut
{
    struct loginwindow_data {};
    typedef test_group<loginwindow_data> loginwindow_group;
    typedef loginwindow_group::object loginwindow_object;
    loginwindow_group loginwindow_tests("llvkloginwindow");

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
        LLVKStartupSettings settings;
        std::string error;
        const bool loaded = settings.loadFile(viewer/"app_settings"/"settings.xml",true,true,true,error);
        ensure(error,loaded);
        LLVKLoginWindow::Configuration configuration;
        configuration.ui.skin.skinBaseDirectory = viewer/"skins";
        configuration.ui.skin.userAppDirectory = profile.path;
        configuration.ui.fontDescription = viewer/"fonts"/"fonts.xml";
        configuration.ui.fonts.platform = "Windows";
        configuration.ui.fonts.searchDirectories = {viewer/"fonts",std::filesystem::path(LLVK_LOGIN_PACKAGED_FONTS)};
        configuration.ui.settings = settings.values();
        configuration.ui.settingDefaults = settings.defaults();
        LLVKStartupSettings accountSettings;
        ensure("native account declaration load",accountSettings.loadFile(viewer/"app_settings"/"settings_per_account.xml",true,true,true,error));
        configuration.ui.accountSettings=accountSettings.values(); configuration.ui.accountDefaults=accountSettings.defaults();
        configuration.ui.dictionaryDirectory=std::filesystem::path(LLVK_LOGIN_PACKAGED_FONTS).parent_path()/"dictionaries";
        configuration.browser.helperDirectory = directory;
        configuration.browser.localesDirectory = directory/"locales";
        configuration.browser.cacheDirectory = profile.path/"browser";
        configuration.loginPage = "data:text/html,<html><body style='margin:0;background:rgb(45,90,120)'><h1>Native browser validation</h1></body></html>";
        configuration.stopAfterFrames = 6;
        configuration.bindServices=[](LLVKLoginUi& ui)
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
            ensure("original Spell Checker in presentation",ui.showSpellCheck(problem));
            ensure("actual dictionary service active",ui.spellCheck().active());
            ensure("original Translation Settings in presentation",ui.showTranslation(problem));
            const bool wasEnabled=ui.tree().setting("RestrainedLove")->asBoolean();
            ensure("RLVa original setting change",ui.tree().updateSetting("RestrainedLove",LLSD(!wasEnabled)));
            ensure("original AutoReplace in native presentation",ui.showAutoReplace(problem));
            ensure("native asynchronous XML picker starts",ui.tree().commit(ui.find("autoreplace_import_list")));
            ensure(ui.dialogError(),ui.dialogError().empty());
        };
        const bool ran = LLVKLoginWindow::run(configuration,error);
        ensure(error,ran);
        ensure("no OpenGL parent module",GetModuleHandleW(L"opengl32.dll") == nullptr);
        std::cout << "Native login window presented six frames with real widgets, fonts, skin and browser\n";
    }
}