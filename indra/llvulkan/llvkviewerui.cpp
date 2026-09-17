#include "llvkviewerui.h"
#include "llvkskinimages.h"
#include "lluri.h"
#include "llvkxmllayers.h"
#include <fstream>
#include <charconv>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

std::string LLVKViewerUi::uiLanguage(std::map<std::string,LLSD>& settings)
{
    std::string language="en";
    for (const auto* name : {"Language","InstallLanguage","SystemLanguage"})
    {
        const auto found=settings.find(name);
        if (found==settings.end()) continue;
        const auto value=found->second.asString();
        if (value.empty() || value=="default") continue;
        language=value;
        break;
    }
    const auto enabled=settings.find("FSEnabledLanguages");
    if (enabled!=settings.end())
        for (auto value=enabled->second.beginArray(); value!=enabled->second.endArray(); ++value)
            if (value->asString()==language) return language;
    settings.insert_or_assign("Language",LLSD("default"));
    return "en";
}

std::string LLVKViewerUi::pageUrl(const Page& page)
{
    const LLURI uri(page.url);
    LLSD query = uri.queryMap();
    const auto setting = [&](const char* name)
    { const auto found = page.settings.find(name); return found == page.settings.end() ? LLSD() : found->second; };
    query["lang"] = page.language;
    if (setting("FirstLoginThisInstall").asBoolean()) query["firstlogin"] = "TRUE";
    query["version"] = page.version;
    query["channel"] = page.channel;
    query["grid"] = page.grid;
    query["os"] = page.operatingSystem;
    query["sourceid"] = setting("sourceid").asString();
    query["login_content_version"] = setting("LoginContentVersion").asString();
    query["skin"] = page.skin+" "+page.theme;
    if (setting("FSNoVersionPopup").asBoolean()) query["noversionpopup"] = "true";
    for (const auto& [name,key] : {std::pair{"FSSplashScreenHideTopBar","hidetopbar"},std::pair{"FSSplashScreenHideBlogs","hideblogs"},
        std::pair{"FSSplashScreenHideDestinations","hidedestinations"},std::pair{"FSSplashScreenUseGrayMode","usegraymode"},
        std::pair{"FSSplashScreenUseHighContrast","usehighcontrast"},std::pair{"FSSplashScreenUseAllCaps","useallcaps"},
        std::pair{"FSSplashScreenUseLargerFonts","uselargerfonts"},std::pair{"FSSplashScreenNoTransparency","notransparency"}})
        query[key] = setting(name).asBoolean() ? "1" : "0";
    const auto authority = uri.scheme().empty() ? uri.authority() : uri.scheme()+"://"+uri.authority();
    return LLURI::buildHTTP(authority,uri.path(),query).asString();
}

std::unique_ptr<LLVKViewerUi> LLVKViewerUi::create(const Configuration& configuration, std::string& error)
{
    error.clear();
    auto ui = std::make_unique<LLVKViewerUi>();
    ui->mSkin = std::make_shared<LLVKSkinFiles>(configuration.skin);
    std::vector<std::string> descriptions;
    if (!configuration.fontDescription.empty())
    {
        std::ifstream stream(configuration.fontDescription,std::ios::binary | std::ios::ate);
        if (!stream) { error = "Native login font descriptor is unavailable"; return nullptr; }
        const auto size = stream.tellg();
        if (size <= 0 || size > 4*1024*1024) { error = "Native login font descriptor exceeds byte limit"; return nullptr; }
        std::string text(static_cast<std::size_t>(size),'\0');
        stream.seekg(0);
        if (!stream.read(text.data(),size)) { error = "Native login font descriptor read failed"; return nullptr; }
        descriptions.push_back(std::move(text));
    }
    else
    {
        auto files = ui->mSkin->read("xui","fonts.xml",LLVKSkinFiles::Policy::Current,error);
        if (!files) return nullptr;
        descriptions = std::move(*files);
    }
    const auto scaleSetting=configuration.settings.find("UIScaleFactor");
    const auto requestedScale=scaleSetting==configuration.settings.end() ? 1.0 : scaleSetting->second.asReal();
    if (!std::isfinite(requestedScale)) { error="Invalid native UI scale"; return nullptr; }
    ui->mDisplayScale=std::clamp(static_cast<float>(requestedScale),0.75f,7.f);
    auto fontConfiguration=configuration.fonts;
    const auto dpiSetting=configuration.settings.find("FontScreenDPI");
    if (dpiSetting!=configuration.settings.end())
        fontConfiguration.horizontalDpi=fontConfiguration.verticalDpi=static_cast<float>(dpiSetting->second.asReal());
    ui->mBaseFontDpiX=fontConfiguration.horizontalDpi;
    ui->mBaseFontDpiY=fontConfiguration.verticalDpi;
    fontConfiguration.displayScale=ui->mDisplayScale;
    fontConfiguration.horizontalDpi=std::floor(fontConfiguration.horizontalDpi*ui->mDisplayScale);
    fontConfiguration.verticalDpi=std::floor(fontConfiguration.verticalDpi*ui->mDisplayScale);
    ui->mFonts = LLVKFontRegistry::create(descriptions,fontConfiguration,error);
    if (!ui->mFonts) return nullptr;
    ui->mColors = std::make_shared<LLVKColorTable>();
    const auto colors = ui->mSkin->read("","colors.xml",LLVKSkinFiles::Policy::All,error);
    if (!colors) return nullptr;
    std::vector<std::string> warnings;
    for (const auto& document : *colors)
        if (!ui->mColors->load(document,LLVKColorTable::Layer::Loaded,warnings,error)) return nullptr;
    ui->mUserColorsFile=configuration.userColorsFile;
    if (!ui->mUserColorsFile.empty() && !ui->mColors->loadUserFile(ui->mUserColorsFile,error)) return nullptr;
    auto images = std::make_shared<LLVKSkinImages>(ui->mSkin);
    if (!images->loadDeclarations(error)) return nullptr;
    ui->mTree.setSkinImages(images);
    auto labels=configuration.labels;
    labels.defaults.try_emplace("APP_NAME","Vulkanstorm");
    ui->mTree.setLabelContext(std::move(labels));
    for (const auto& [name,value] : configuration.settings)
    {
        auto type = LLVKWidgetTree::SettingType::Opaque;
        if (value.isBoolean()) type = LLVKWidgetTree::SettingType::Boolean;
        else if (value.isInteger()) type = LLVKWidgetTree::SettingType::Integer;
        else if (value.isReal()) type = LLVKWidgetTree::SettingType::Real;
        else if (value.isString()) type = LLVKWidgetTree::SettingType::String;
        if (!ui->mTree.defineSetting(name,value,type)) { error = "Invalid native login setting: "+name; return nullptr; }
    }
    for (const auto& [name,value] : configuration.accountSettings)
    {
        if (ui->mTree.setting(name)) { error="Native account setting conflicts with global setting: "+name; return nullptr; }
        auto type=LLVKWidgetTree::SettingType::Opaque;
        if (value.isBoolean()) type=LLVKWidgetTree::SettingType::Boolean;
        else if (value.isInteger()) type=LLVKWidgetTree::SettingType::Integer;
        else if (value.isReal()) type=LLVKWidgetTree::SettingType::Real;
        else if (value.isString()) type=LLVKWidgetTree::SettingType::String;
        if (!ui->mTree.defineSetting(name,value,type)) { error="Invalid native account setting: "+name; return nullptr; }
    }
    LLVKWidgetFactory::Resources resources;
    for (const auto& [name,value] : std::map<std::string,LLSD>{{"floater_vis_guidebook",false},
        {"floater_pos_guidebook_x",10.},{"floater_pos_guidebook_y",10.}})
    {
        if (configuration.settingsGroup && !configuration.settingsGroup->controlExists(name))
            configuration.settingsGroup->declareControl(name,value.isBoolean() ? TYPE_BOOLEAN : TYPE_F32,value,
                "Guidebook window state",SANITY_TYPE_NONE,{},"",LLControlVariable::PERSIST_NONDFT);
        if (!ui->mTree.setting(name)) ui->mTree.defineSetting(name,value,value.isBoolean() ?
            LLVKWidgetTree::SettingType::Boolean : LLVKWidgetTree::SettingType::Real);
    }
    if (configuration.settingsGroup && !ui->mTree.bindSettings(*configuration.settingsGroup,error)) return nullptr;
    if (configuration.accountSettingsGroup && !ui->mTree.bindSettings(*configuration.accountSettingsGroup,error)) return nullptr;
    resources.skinFiles = ui->mSkin;
    resources.fontRegistry = ui->mFonts;
    resources.colors = ui->mColors;
    resources.defaultFontRequest = {"SansSerif","Small"};
    resources.fallbackFont=ui->mFonts->resolve({"SansSerif","Medium"},error);
    if (!resources.fallbackFont) return nullptr;
    resources.webLinkHandler = [owner=ui.get()](auto,const std::string& url)
    { owner->activateUrl(url,owner->mDialogError); };
    resources.colorPickerHandler=[owner=ui.get()](auto swatch,bool takeFocus)
    { owner->showColorPicker(swatch,takeFocus,owner->mDialogError); };
    resources.helpHandler=[owner=ui.get()](auto floater)
    {
        const auto topic=owner->mTree.findHelpTopic(floater);
        if (topic) owner->showHelp(*topic,owner->mDialogError);
    };
    resources.helpTooltip=[owner=ui.get()] { return owner->errorString("BUTTON_HELP","Help"); };
    resources.menuHandler=[owner=ui.get()](auto button,const std::string& filename,const std::string& position,const auto& callbacks)
    { owner->showButtonMenu(button,filename,position,callbacks); };
    LLVKWidgetFactory::PanelDefaults panel;
    panel.control.fontRequest = resources.defaultFontRequest;
    LLVKWidgetFactory::Callbacks callbacks;
    ui->mSettingDefaults=configuration.settingDefaults;
    ui->mAccountDefaults=configuration.accountDefaults;
    ui->mAccountSettingsLoaded=configuration.accountSettingsLoaded;
    ui->mSaveAccountPreferences=configuration.saveAccountPreferences;
    for (const auto& [name,value] : configuration.accountSettings) ui->mAccountSettingNames.insert(name);
    ui->mDesktopNotificationsAvailable=configuration.desktopNotificationsAvailable;
    ui->mFontPresetDirectories=configuration.fontPresetDirectories;
    if (ui->mFontPresetDirectories.empty() && !configuration.fontDescription.empty())
        ui->mFontPresetDirectories.push_back(configuration.fontDescription.parent_path());
    ui->mCrashSettings=configuration.crashSettings;
    ui->mCrashSettingsRequireRestart=configuration.crashSettingsRequireRestart;
    ui->mSaveCrashPreferences=configuration.saveCrashPreferences;
    ui->mScheduleSettingsReset=configuration.scheduleSettingsReset;
    ui->mCacheDirectory=configuration.cacheDirectory;
    ui->mDefaultCacheDirectory=configuration.defaultCacheDirectory;
    ui->mProfileDirectory=configuration.skin.userAppDirectory;
    ui->mSkinBaseDirectory=configuration.skin.skinBaseDirectory;
    callbacks.actions["Pref.ClearSettings"]=[owner=ui.get()](auto,const LLSD&)
    {
        owner->queueNotice("FirestormClearSettingsPrompt",{},[owner](int option,const LLSD&)
        {
            if (option!=0) return;
            if (!owner->mScheduleSettingsReset) { owner->mDialogError="Native settings reset service is not bound"; return; }
            if (owner->mScheduleSettingsReset(owner->mDialogError)) owner->queueNotice("SettingsWillClear",{}, {},owner->mDialogError);
        },owner->mDialogError);
    };
    callbacks.actions["ResetControl"]=[owner=ui.get()](auto,const LLSD& parameter)
    { owner->resetPreference(parameter.asString(),owner->mDialogError); };
    callbacks.actions["ResetPerAccountControl"]=[owner=ui.get()](auto,const LLSD& parameter)
    { owner->resetAccountPreference(parameter.asString(),owner->mDialogError); };
    callbacks.actions["PreviewUISound"]=[owner=ui.get()](auto,const LLSD& parameter)
    { owner->previewUiSound(parameter.asString(),owner->mDialogError); };
    callbacks.actions["Pref.AutoReplace"]=[owner=ui.get()](auto,const LLSD&)
    { owner->showAutoReplace(owner->mDialogError); };
    callbacks.actions["Pref.SpellChecker"]=[owner=ui.get()](auto,const LLSD&)
    { owner->showSpellCheck(owner->mDialogError); };
    callbacks.actions["Pref.TranslationSettings"]=[owner=ui.get()](auto,const LLSD&)
    { owner->showTranslation(owner->mDialogError); };
    callbacks.actions["Pref.Proxy"]=[owner=ui.get()](auto,const LLSD&)
    { owner->showProxy(owner->mDialogError); };
    callbacks.actions["Pref.OK"]=[owner=ui.get()](auto,const LLSD&)
    { owner->applyPreferences(owner->mDialogError); };
    callbacks.actions["Pref.Cancel"]=[owner=ui.get()](auto,const LLSD&)
    { if (owner->mPreferences) owner->mPreferences->close(owner->mDialogError); };
    callbacks.actions["UpdateFilter"]=[owner=ui.get()](auto,const LLSD&)
    { owner->filterPreferences(owner->mDialogError); };
    callbacks.actions["Pref.CopySearchAsSLURL"]=[owner=ui.get()](auto,const LLSD&)
    { owner->copyPreferenceSearch(owner->mDialogError); };
    callbacks.actions["Proxy.Change"]=[owner=ui.get()](auto,const LLSD&)
    { owner->updateProxyControls(); };
    callbacks.actions["Proxy.OK"]=[owner=ui.get()](auto,const LLSD&)
    { owner->acceptProxy(owner->mDialogError); };
    callbacks.actions["Proxy.Cancel"]=[owner=ui.get()](auto,const LLSD&)
    { if (owner->mProxy) owner->mProxy->close(owner->mDialogError); };
    callbacks.actions["PermsDefault.Copy"]=[owner=ui.get()](auto,const LLSD& category)
    {
        const auto copy=owner->mTree.setting(category.asString()+"NextOwnerCopy");
        if (copy && !copy->asBoolean()) owner->mTree.updateSetting(category.asString()+"NextOwnerTransfer",LLSD(true));
    };
    callbacks.actions["PermsDefault.OK"]=[owner=ui.get()](auto,const LLSD&)
    { owner->acceptDefaultPermissions(owner->mDialogError); };
    callbacks.actions["PermsDefault.Cancel"]=[owner=ui.get()](auto,const LLSD&)
    { if (owner->mDefaultPermissions) owner->mDefaultPermissions->close(owner->mDialogError); };
    callbacks.actions["Notification.Show"]=[owner=ui.get()](auto,const LLSD& parameter)
    { owner->queueNotice(parameter.asString(),{},{},owner->mDialogError); };
    callbacks.actions["Floater.Show"]=[owner=ui.get()](auto,const LLSD& parameter)
    {
        if (parameter.asString()=="pref_joystick") owner->showJoystick(owner->mDialogError);
        else if (parameter.asString()=="media_lists") owner->showMediaLists(owner->mDialogError);
        else owner->mDialogError="Native Preferences floater is not implemented: "+parameter.asString();
    };
    resources.panelClasses["panel_preference"]=[owner=ui.get()](auto& tree,const auto& defaults,const auto&,std::string& problem)
    {
        LLVKWidgetFactory::PanelInstance instance;
        auto view=defaults.view.view;
        const auto rectangle=defaults.view.geometry.resolve(problem);
        if (!rectangle) return instance;
        view.rect=*rectangle;
        auto control=defaults.control;
        if (!control.font) control.font=owner->mFonts->resolve(control.fontRequest.value_or(LLVKFontRegistry::Request{"SansSerif","Small"}),problem);
        if (!control.font) return instance;
        const auto panel=tree.createPanel(view,control,defaults.panel,0,problem);
        if (!panel) return instance;
        instance.id=*panel;
        instance.callbacks=std::make_shared<LLVKWidgetFactory::Callbacks>();
        instance.callbacks->actions["Pref.MaturitySettings"]=[owner,panel=*panel](auto,const LLSD&)
        { owner->updateStartupPreferenceMaturity(panel,owner->mDialogError); };
        instance.callbacks->actions["Pref.ClickActionChange"]=[owner,panel=*panel](auto,const LLSD&)
        { owner->updateClickActions(panel,true,owner->mDialogError); };
        instance.callbacks->actions["FS.CheckContactListColumnMode"]=[owner,panel=*panel](auto,const LLSD&)
        { owner->refreshContactColumns(panel,owner->mDialogError); };
        instance.callbacks->actions["Pref.UpdatePopupFilter"]=[owner,panel=*panel](auto,const LLSD&)
        { owner->refreshNotificationPreferences(panel,false,owner->mDialogError); };
        instance.callbacks->actions["Pref.SelectPopup"]=[owner,panel=*panel](auto,const LLSD&)
        { owner->refreshNotificationPreferences(panel,true,owner->mDialogError); };
        instance.callbacks->actions["Pref.Online_Notices"]=[owner,panel=*panel](auto,const LLSD&)
        { owner->initializeStartupPreferencePanel(panel,owner->mDialogError); };
        for (const auto action : {"WebBrowserClearCache","Javascript","InvClearCache","BrowseCache","SetCache","ResetCache",
            "ClearCache","BrowseSoundCache","SetSoundCache","ResetSoundCache","BrowseLogPath","LogPath","ResetLogPath",
            "BrowseCrashLogs","BrowseSettingsDir"})
            instance.callbacks->actions[std::string("Pref.")+action]=[owner,panel=*panel,action=std::string(action)](auto,const LLSD&)
            { owner->networkPreferenceAction(panel,action); };
        instance.postBuild=[owner](auto&,auto id,const auto&,std::string& error)
        { return owner->initializeStartupPreferencePanel(id,error); };
        return instance;
    };
    const auto basePreferenceConstructor=resources.panelClasses.at("panel_preference");
    resources.panelClasses["fs_panel_preference_ui_sounds"]=[owner=ui.get(),basePreferenceConstructor](auto& tree,const auto& defaults,const auto& context,std::string& error)
    {
        auto instance=basePreferenceConstructor(tree,defaults,context,error);
        if (!instance.id) return instance;
        for (const auto action : {"SelectUISound","UpdateUISoundFilter","CommitUISoundUUID","CommitUISoundPlayCheck",
            "CommitUISoundPlayCombo","PreviewSelectedUISound","ResetSelectedUISound"})
            instance.callbacks->actions[std::string("Pref.")+action]=[owner,panel=instance.id,action=std::string(action)](auto,const LLSD&)
            { owner->commitUiSound(panel,action); };
        instance.postBuild=[owner](auto&,auto id,const auto&,std::string& problem)
        { return owner->initializeUiSounds(id,problem); };
        return instance;
    };
    resources.panelClasses["panel_preference_crashreports"]=basePreferenceConstructor;
    resources.panelClasses["panel_preference_skins"]=basePreferenceConstructor;
    resources.panelClasses["panel_preference_backup"]=[owner=ui.get(),basePreferenceConstructor](auto& tree,const auto& defaults,const auto& context,std::string& error)
    {
        auto instance=basePreferenceConstructor(tree,defaults,context,error);
        if (!instance.id) return instance;
        for (const auto action : {"SetBackupSettingsPath","BackupSettings","RestoreSettings","BackupSelectAll","BackupDeselectAll"})
            instance.callbacks->actions[std::string("Pref.")+action]=[owner,panel=instance.id,action=std::string(action)](auto,const LLSD&)
            { owner->backupPreferenceAction(panel,action); };
        instance.postBuild=[owner](auto&,auto id,const auto&,std::string& problem)
        { return owner->initializeBackupPreferences(id,problem); };
        return instance;
    };
    resources.panelClasses["panel_preference_firestorm"]=[owner=ui.get(),basePreferenceConstructor](auto& tree,const auto& defaults,const auto& context,std::string& error)
    {
        auto instance=basePreferenceConstructor(tree,defaults,context,error);
        if (!instance.id) return instance;
        for (const auto action : {"NACL.AntiSpamUnblock","NACL.SetPreprocInclude","Pref.PermsDefault","Pref.SetExternalEditor","Perms.Copy","Perms.Trans"})
            instance.callbacks->actions[action]=[owner,panel=instance.id,action=std::string(action)](auto,const LLSD&)
            { owner->viewerPreferenceAction(panel,action); };
        instance.postBuild=[owner](auto&,auto id,const auto&,std::string& problem)
        { return owner->initializeViewerPreferences(id,problem); };
        return instance;
    };
    resources.panelClasses["panel_preference_graphics"]=[owner=ui.get(),basePreferenceConstructor](auto& tree,const auto& defaults,const auto& context,std::string& error)
    {
        auto instance=basePreferenceConstructor(tree,defaults,context,error);
        if (!instance.id) return instance;
        for (const auto action : {"HardwareDefaults","QualityPerformance","RenderOptionUpdate","UpdateIndirectMaxComplexity",
            "UpdateIndirectMaxNonImpostors","UpdateSliderText","PrefSave","PrefLoad","PrefDelete"})
            instance.callbacks->actions[std::string("Pref.")+action]=[owner,panel=instance.id,action=std::string(action)](auto,const LLSD& value)
            { owner->graphicsPreferenceAction(panel,action,value); };
        instance.callbacks->predicates["CheckControl"]=[owner](auto,const LLSD& value)
        { return owner->mTree.setting(value.asString()).value_or(LLSD(false)).asBoolean(); };
        instance.callbacks->predicates["Advanced.CheckWireframe"]=[owner](auto,const LLSD&)
        { return owner->mTree.setting("RenderWireframe").value_or(LLSD(false)).asBoolean(); };
        instance.callbacks->actions["Advanced.HandleAttachedLightParticles"]=[owner,panel=instance.id](auto,const LLSD& value)
        { owner->graphicsPreferenceAction(panel,"Attached",value); };
        instance.callbacks->actions["Advanced.ToggleWireframe"]=[owner,panel=instance.id](auto,const LLSD& value)
        { owner->graphicsPreferenceAction(panel,"Wireframe",value); };
        instance.postBuild=[owner](auto&,auto id,const auto&,std::string& problem)
        { return owner->initializeGraphicsPreferences(id,problem); };
        return instance;
    };
    resources.panelClasses["fs_panel_block_list_sidetray"]=[owner=ui.get(),basePreferenceConstructor](auto& tree,const auto& defaults,const auto& context,std::string& error)
    {
        auto instance=basePreferenceConstructor(tree,defaults,context,error);
        if (!instance.id) return instance;
        instance.callbacks->actions["Block.Action"]=[owner,panel=instance.id](auto,const LLSD& value)
        { owner->blockListAction(panel,value.asString()); };
        for (const auto kind : {"Check","Enable","Visible"})
            instance.callbacks->predicates[std::string("Block.")+kind]=[owner,panel=instance.id,kind=std::string(kind)](auto,const LLSD& value)
            { return owner->blockListPredicate(panel,value.asString(),kind); };
        instance.postBuild=[owner](auto&,auto id,const auto&,std::string& problem)
        { return owner->initializeBlockList(id,problem); };
        return instance;
    };
    resources.panelClasses["panel_preference_privacy"]=[owner=ui.get(),basePreferenceConstructor](auto& tree,const auto& defaults,const auto& context,std::string& error)
    {
        auto instance=basePreferenceConstructor(tree,defaults,context,error);
        if (!instance.id) return instance;
        for (const auto action : {"WebClearCache","ClearLog","DeleteTranscripts","BlockList"})
            instance.callbacks->actions[std::string("Pref.")+action]=[owner,action=std::string(action)](auto,const LLSD&)
            {
                if (action=="BlockList") { owner->showBlockList(owner->mDialogError); return; }
                if (!owner->mPrivacyActionHandler) { owner->mDialogError="Native privacy service is not bound: "+action; return; }
                owner->mPrivacyActionHandler(action,owner->mDialogError);
            };
        instance.postBuild=[owner](auto&,auto id,const auto&,std::string& problem)
        { return owner->initializePrivacyPreferences(id,problem); };
        return instance;
    };
    resources.panelClasses["panel_voice_device_settings"]=[owner=ui.get(),basePreferenceConstructor](auto& tree,const auto& defaults,const auto& context,std::string& error)
    {
        auto instance=basePreferenceConstructor(tree,defaults,context,error);
        instance.postBuild=[owner](auto&,auto id,const auto&,std::string& problem)
        { return owner->initializeVoiceDevices(id,problem); };
        return instance;
    };
    resources.panelClasses["panel_preference_sounds"]=[owner=ui.get(),basePreferenceConstructor](auto& tree,const auto& defaults,const auto& context,std::string& error)
    {
        auto instance=basePreferenceConstructor(tree,defaults,context,error);
        if (!instance.id) return instance;
        instance.callbacks->actions["Pref.setControlFalse"]=[owner](auto,const LLSD& parameter)
        {
            if (!owner->mTree.updateSetting(parameter.asString(),LLSD(false)))
                owner->mDialogError="Native audio mute setting update failed: "+parameter.asString();
        };
        for (const auto action : {"SetSounds","updateMediaAutoPlayCheckbox"})
            instance.callbacks->actions[std::string("Pref.")+action]=[owner,panel=instance.id,action=std::string(action)](auto,const LLSD&)
            { owner->updateSoundPreferences(panel,action,owner->mDialogError); };
        instance.callbacks->actions["Pref.ResetVoice"]=[owner,panel=instance.id](auto,const LLSD&)
        { owner->updateSoundPreferences(panel,"ResetVoice",owner->mDialogError); };
        instance.postBuild=[owner](auto&,auto id,const auto&,std::string& problem)
        { return owner->initializeSoundPreferences(id,problem); };
        return instance;
    };
    resources.panelClasses["panel_preference_controls"]=[owner=ui.get(),basePreferenceConstructor](auto& tree,const auto& defaults,const auto& context,std::string& error)
    {
        auto instance=basePreferenceConstructor(tree,defaults,context,error);
        instance.postBuild=[owner](auto&,auto id,const auto&,std::string& problem)
        { return owner->initializeControlsPanel(id,problem); };
        return instance;
    };
    callbacks.actions["Pref.getUIColor"]=[owner=ui.get()](auto id,const LLSD& parameter)
    {
        const auto color=owner->mColors->find(parameter.asString());
        if (!color) { owner->mDialogError="Missing native preference color: "+parameter.asString(); return; }
        LLSD value=LLSD::emptyArray();
        for (const auto channel : color->get()) value.append(channel);
        owner->mTree.setColorSwatchValue(id,value,owner->mDialogError);
    };
    callbacks.actions["Pref.applyUIColor"]=[owner=ui.get()](auto id,const LLSD& parameter)
    {
        const auto* node=owner->mTree.get(id);
        if (!node || !node->colorSwatch || !owner->mColors->set(parameter.asString(),node->colorSwatch->color))
            owner->mDialogError="Cannot update native preference color: "+parameter.asString();
    };
#if !defined(OPENSIM) || defined(SINGLEGRID)
    resources.excludedPanelClasses.insert("panel_preference_opensim");
#endif
#ifndef LL_SEND_CRASH_REPORTS
    resources.excludedPanelClasses.insert("panel_preference_crashreports");
#endif
    LLVKWidgetFactory factory({}, {}, {}, callbacks,resources,panel);
    for (const std::string widget : {"view_border","button","icon","line_editor","check_box","scroll_bar",
        "scroll_container","scroll_column_header","scroll_list","combo_box","flyout_button","text","web_browser","layout_stack","tab_container","simple_text_editor","text_editor","spinner","color_swatch","texture_picker","search_editor","filter_editor","slider_bar","slider","radio_item","radio_group","progress_bar"})
        if (!factory.loadDefaultsFile(ui->mTree,"widgets/"+widget+".xml",error))
        { error = "Native login "+widget+": "+error; return nullptr; }
    const auto root = factory.constructFile(ui->mTree,"panel_fs_nui_login.xml",0,error);
    if (!root) return nullptr;
    ui->mRoot = *root;
    if (!ui->mTree.prepareLayoutStacks(*root,0,error)) return nullptr;
    for (const auto name : {"login_html","username_combo","password_edit","start_location_combo","server_combo","connect_btn"})
        if (!ui->find(name)) { error = std::string("Native login is missing required control: ")+name; return nullptr; }
    LLVKControl::Callback togglePassword;
    togglePassword.function = [owner = ui.get()](auto,const LLSD&)
    {
        auto& tree = owner->tree();
        const auto password = owner->find("password_edit");
        const auto* node = tree.get(password);
        if (!node || !node->lineEditor) return;
        const bool mask = !node->lineEditor->params.text.password;
        tree.setLineEditorPassword(password,mask);
        tree.setVisible(owner->find("password_show_btn"),mask);
        tree.setVisible(owner->find("password_hide_btn"),!mask);
    };
    ui->mTree.setControlCommit(ui->find("password_show_btn"),togglePassword);
    ui->mTree.setControlCommit(ui->find("password_hide_btn"),std::move(togglePassword));
    ui->mTree.setPlainTextClicked(ui->find("create_new_account_text"),[owner=ui.get()](auto)
    {
        const auto url=owner->errorString("create_account_url","https://www.firestormviewer.org/join-secondlife/");
        owner->showMediaBrowser(url,owner->mDialogError);
    });
    ui->mTree.setPlainTextClicked(ui->find("forgot_password_text"),[owner=ui.get()](auto)
    {
        const auto* root=owner->mTree.get(owner->mRoot);
        const auto found=root->panel->params.strings.find("forgot_password_url");
        if (found==root->panel->params.strings.end() || found->second.empty())
        { owner->mDialogError="Native password recovery URL is unavailable"; return; }
        if (!owner->mOpenUrl) { owner->mDialogError="Native web link service is not bound"; return; }
        owner->mOpenUrl(found->second);
    });
    const auto mode = configuration.settings.find("SessionSettingsFile");
    if (mode != configuration.settings.end()) ui->mTree.setValue(ui->find("mode_combo"),mode->second);
    const auto menuFiles = ui->mSkin->read("xui","menu_login.xml",LLVKSkinFiles::Policy::Current,error);
    if (!menuFiles) return nullptr;
    std::vector<std::string_view> menuLayers;
    for (const auto& file : *menuFiles) menuLayers.push_back(file);
    const auto menuXml = LLVKXmlLayers::merge(menuLayers,error);
    if (!menuXml) return nullptr;
    const auto menuFont = ui->mFonts->resolve({"SansSerif","Small"},error);
    if (!menuFont) return nullptr;
    const auto debug = configuration.settings.find("UseDebugMenus");
    ui->mMenu = LLVKMenu::create(*menuXml,menuFont,ui->mColors,configuration.labels,
        debug != configuration.settings.end() && debug->second.asBoolean(),error);
    if (!ui->mMenu) return nullptr;
#ifndef OPENSIM
    for (const auto name : {"current_grid_help_login","current_grid_about_login","grid_help_seperator_login"})
        ui->mMenu->setVisible(name,false);
#endif
    ui->mDialogFactory = std::make_unique<LLVKWidgetFactory>(factory);
    ui->mMenu->setTearOffHandler([owner=ui.get()](auto item,auto rectangle)
    { owner->tearOffMenu(item,rectangle,owner->mDialogError); });
    if (!ui->initializeDialogs(configuration,error)) return nullptr;
    if (!ui->initializeNoticeLayout(error)) return nullptr;
    auto location=ui->mTree.setting("CmdLineLoginLocation").value_or(LLSD("")).asString();
    if (location.empty()) location=ui->mTree.setting("NextLoginLocation").value_or(LLSD("")).asString();
    if (location.empty()) location=ui->mTree.setting("LoginLocation").value_or(LLSD("last")).asString();
    const auto locationId=ui->find("start_location_combo");
    if (location=="last" || location=="home" || location.empty())
    {
        if (!ui->mTree.setComboValue(locationId,LLSD(location.empty() ? "home" : location),error)) return nullptr;
    }
    else
    {
        if (!ui->mTree.selectComboItem(locationId,std::nullopt,error) ||
            !ui->mTree.setValue(ui->mTree.get(locationId)->combo->editor,LLSD(location))) return nullptr;
    }
    ui->updateLoginControls();
    if (!ui->focusLoginFields(error)) return nullptr;
    return ui;
}

bool LLVKViewerUi::focusLoginFields(std::string& error)
{
    const auto username=find("username_combo"),password=find("password_edit");
    const auto* combo=mTree.get(username);
    if (!combo || !combo->combo || !mTree.get(password))
    { error="Native login focus controls are unavailable"; return false; }
    const auto target=!mTree.value(username).asString().empty() && mTree.value(password).asString().empty() ?
        password : combo->combo->editor;
    return mTree.requestControlFocus(target,true,error);
}

void LLVKViewerUi::updateLoginControls()
{
    mTree.setVisible(find("grid_panel"),mTree.setting("ForceShowGrid").value_or(LLSD(false)).asBoolean());
    const bool prelogin=!mSessionOwner || mSessionOwner->snapshot().state==LLVKSessionOwner::State::PreLogin;
    const bool credentials=!mTree.value(find("username_combo")).asString().empty() &&
        !mTree.value(find("password_edit")).asString().empty();
    mTree.setEnabled(find("connect_btn"),prelogin && credentials);
    const auto* username=mTree.get(find("username_combo"));
    bool savedUsername=false;
    if (username && username->combo && username->combo->selected)
    {
        const auto& item=username->combo->items[*username->combo->selected];
        savedUsername=!item.value.asString().empty() &&
            mTree.value(username->combo->editor).asString()==item.label;
    }
    mTree.setEnabled(find("remove_user_btn"),prelogin && savedUsername);
}

bool LLVKViewerUi::refreshDisplayScale(std::string& error,float systemScale)
{
    error.clear();
    const auto requested=mTree.setting("UIScaleFactor").value_or(LLSD(1.f)).asReal();
    if (!std::isfinite(requested) || !std::isfinite(systemScale) || systemScale<=0.f)
    { error="Invalid native UI scale"; return false; }
    const auto scale=std::clamp(static_cast<float>(requested)*systemScale,0.75f,7.f);
    const auto dpi=mTree.setting("FontScreenDPI");
    const auto dpiX=dpi ? static_cast<float>(dpi->asReal()) : mBaseFontDpiX;
    const auto dpiY=dpi ? static_cast<float>(dpi->asReal()) : mBaseFontDpiY;
    if (scale==mDisplayScale && dpiX==mBaseFontDpiX && dpiY==mBaseFontDpiY) return true;
    if (!mFonts->setDisplayScale(scale,dpiX,dpiY,error)) return false;
    mBaseFontDpiX=dpiX; mBaseFontDpiY=dpiY;
    mDisplayScale=scale;
    mMenu->dismiss();
    if (!mTree.setTopControl(0,error)) return false;
    if (mActiveNotice)
    {
        const auto notice=*mActiveNotice;
        const auto entered=mNoticeEditor ? std::optional(mTree.value(mNoticeEditor)) : std::nullopt;
        const auto editorState=mNoticeEditor ? std::optional(mTree.get(mNoticeEditor)->lineEditor->text) : std::nullopt;
        const auto ignored=mNoticeIgnore ? std::optional(mTree.value(mNoticeIgnore)) : std::nullopt;
        const auto opened=mNoticeOpened;
        if (!dismissNotice(error)) return false;
        mNotices.insert(mNotices.begin(),notice);
        if (!advanceNotices(mNoticeTime,error)) return false;
        if (entered && mNoticeEditor) mTree.setValue(mNoticeEditor,*entered);
        if (editorState && mNoticeEditor && !mTree.restoreLineEditorSelection(mNoticeEditor,editorState->selectionStart(),
            editorState->selectionEnd(),editorState->cursor(),editorState->selecting(),error)) return false;
        if (ignored && mNoticeIgnore) mTree.setValue(mNoticeIgnore,*ignored);
        mNoticeOpened=opened;
    }
    return true;
}

std::optional<LLVKWidgetPaint> LLVKViewerUi::preparePaint(const LLVKWidgetPaint::Input& input,std::string& error)
{
    if (mHelpRetiring)
    {
        if (mActiveFloater==mHelp.get()) mActiveFloater=nullptr;
        mHelp.reset(); mHelpFields.clear(); mHelpBrowser=0; mHelpRetiring=false;
    }
    updateLoginControls();
    for (auto& [item,dialog] : mTornMenus)
    {
        if (!dialog.floater->visible()) continue;
        bool focused=false;
        for (auto current=mTree.keyboardFocus(); mTree.get(current); current=mTree.get(current)->parent)
            if (current==dialog.floater->id()) { focused=true; break; }
        dialog.view->setDetachedFocus(focused);
        auto rectangle=mTree.get(dialog.floater->id())->params.rect;
        const auto dimensions=dialog.view->menuSize(item,error);
        if (!dimensions) return std::nullopt;
        const auto content=mTree.get(dialog.content)->params.rect;
        if (content.right-content.left!=dimensions->first || content.top-content.bottom!=dimensions->second)
        {
            const auto header=dialog.targetHeight-(content.top-content.bottom);
            dialog.targetHeight=dimensions->second+header;
            rectangle.right=rectangle.left+dimensions->first+4;
            rectangle.top=rectangle.bottom+dialog.targetHeight;
            if (!mTree.setShape(dialog.floater->id(),rectangle,error) ||
                !mTree.setShape(dialog.content,{1,1,dimensions->first+1,dimensions->second+1},error)) return std::nullopt;
        }
        const auto height=rectangle.top-rectangle.bottom;
        if (!std::isfinite(input.button.frameDelta) || input.button.frameDelta<0.f)
        { error="Invalid native tear-off animation delta"; return std::nullopt; }
        const auto amount=std::clamp(1.f-std::pow(2.f,-input.button.frameDelta/0.05f),0.f,1.f);
        const auto root=mTree.get(mRoot)->params.rect;
        const auto visibleHeight=std::min(16,rectangle.top-rectangle.bottom);
        const auto visibleWidth=std::min(16,rectangle.right-rectangle.left);
        const auto horizontal=rectangle.right-visibleWidth<0 ? visibleWidth-rectangle.right :
            rectangle.left+visibleWidth>root.right-root.left ? root.right-root.left-rectangle.left-visibleWidth : 0;
        const auto vertical=rectangle.top>root.top-root.bottom-19 ? root.top-root.bottom-19-rectangle.top :
            rectangle.top-visibleHeight<0 ? visibleHeight-rectangle.top : 0;
        rectangle.left+=horizontal; rectangle.right+=horizontal;
        rectangle.bottom+=vertical; rectangle.top+=vertical;
        rectangle.top=rectangle.bottom+static_cast<int>(std::ceil(height+(dialog.targetHeight-height)*amount));
        if (!mTree.setShape(dialog.floater->id(),rectangle,error)) return std::nullopt;
    }
    auto viewport = mTree.screenRect(mRoot,error);
    if (viewport && input.physicalWidth && input.physicalHeight)
    {
        viewport->right=static_cast<int>(std::ceil(input.physicalWidth/mDisplayScale));
        viewport->top=static_cast<int>(std::ceil(input.physicalHeight/mDisplayScale));
    }
    if (const auto notice=mTree.get(mNoticePanel))
    {
        const auto rectangle=notice->params.rect;
        const auto width=rectangle.right-rectangle.left,height=rectangle.top-rectangle.bottom;
        if (!mTree.setShape(mNoticePanel,noticeRectangle(width,height,viewport),error)) return std::nullopt;
    }
    if (!refreshVoiceDevices(error)) return std::nullopt;
    if (mDebugSettings && mDebugSettings->visible() && !refreshDebugSettings(false,error)) return std::nullopt;
    if (mColorSettings && mColorSettings->visible() && !refreshColorSettings(false,error)) return std::nullopt;
    if (mJoystick && mJoystick->visible() && !updateJoystickPreview(error)) return std::nullopt;
    if (mBeamColor && mBeamColor->visible() && !updateBeamColorPreview(error)) return std::nullopt;
    updateSpellRemoval();
    auto paintInput=input;
    paintInput.editor.useEditorClock=true;
    paintInput.foregroundFloaters.emplace();
    for (auto focused=mTree.keyboardFocus(); mTree.get(focused); focused=mTree.get(focused)->parent)
        if (mTree.get(focused)->floater) { paintInput.foregroundFloaters->insert(focused); break; }
    for (const auto& [swatch,picker] : mColorPickers)
    {
        if (!picker->visible()) continue;
        auto owner=mTree.get(swatch) ? mTree.get(swatch)->parent : 0;
        while (mTree.get(owner) && !mTree.get(owner)->floater) owner=mTree.get(owner)->parent;
        if (paintInput.foregroundFloaters->contains(picker->id())) paintInput.foregroundFloaters->insert(owner);
    }
    for (const auto& [swatch,picker] : mColorPickers)
    {
        if (!picker->visible()) continue;
        auto owner=mTree.get(swatch) ? mTree.get(swatch)->parent : 0;
        while (mTree.get(owner) && !mTree.get(owner)->floater) owner=mTree.get(owner)->parent;
        if (paintInput.foregroundFloaters->contains(owner)) paintInput.foregroundFloaters->insert(picker->id());
    }
    paintInput.activeControlFloaters.emplace();
    for (auto* floater : floaters())
    {
        if (!floater || !floater->visible()) continue;
        floater->updateForeground(paintInput.foregroundFloaters->contains(floater->id()));
        if (floater->controlActive()) paintInput.activeControlFloaters->insert(floater->id());
    }
    paintInput.floaterShadow=mColors->find("ColorDropShadow");
    if (const auto color=mColors->find("FocusColor"))
    {
        auto focus=color->get();
        const auto flash=mTree.focusFlashAmount();
        for (std::size_t channel=0; channel<focus.size(); ++channel) focus[channel]+=(1.f-focus[channel])*flash;
        if (!input.editor.applicationFocused) focus[3]*=0.4f;
        paintInput.button.focusColor=paintInput.editor.focusColor=focus;
        paintInput.button.focusWidth=paintInput.editor.focusWidth=static_cast<int>(std::floor(1.f+flash+0.5f));
    }
    if (const auto color=mColors->find("SearchableControlHighlightBgColor")) paintInput.searchBackground=*color;
    if (const auto color=mColors->find("SearchableControlHighlightFontColor")) paintInput.searchFont=*color;
    auto paint = LLVKWidgetPaint::prepare(mTree,mRoot,paintInput,error);
    if (!paint) return std::nullopt;
    paint->displayScale=mDisplayScale;
    paint->skinAnisotropy=mTree.setting("RenderAnisotropic").value_or(LLSD(false)).asBoolean();
    if (!viewport) return std::nullopt;
    const auto backingBottom=(std::floor(viewport->bottom*mDisplayScale)+
        std::ceil((viewport->top-viewport->bottom-mNoticeMenuHeight)*mDisplayScale)+1.f)/mDisplayScale;
    const auto menuAlpha=static_cast<float>(mTree.setting("FSMenuBackgroundAlpha").value_or(LLSD(1.f)).asReal());
    mMenu->setTime(mTree.time());
    if (!mMenu->paint(*paint,*viewport,error,{},true,backingBottom,menuAlpha)) return std::nullopt;
    if (mNoticePanel)
    {
        std::vector<LLVKWidgetPaint::Command> modalPass;
        for (const auto& command : paint->commands)
            for (auto owner=command.owner; mTree.get(owner); owner=mTree.get(owner)->parent)
                if (owner==mNoticePanel)
                {
                    modalPass.push_back(command);
                    break;
                }
        if (modalPass.size()>65536-paint->commands.size())
        { error="Native modal composition exceeds paint command budget"; return std::nullopt; }
        paint->commands.insert(paint->commands.end(),modalPass.begin(),modalPass.end());
    }
    if (!appendTooltip(*paint,input,error)) return std::nullopt;
    return paint;
}

bool LLVKViewerUi::initializeTooltip(std::string& error)
{
    if (!mTooltipTemplate.empty()) return true;
    const auto files=mSkin->read("xui","widgets/tool_tip.xml",LLVKSkinFiles::Policy::All,error);
    if (!files) return false;
    std::vector<std::string_view> layers;
    for (const auto& text : *files) layers.push_back(text);
    const auto merged=LLVKXmlLayers::merge(layers,error);
    if (!merged) return false;
    try
    {
        boost::property_tree::ptree document;
        std::istringstream stream(*merged);
        boost::property_tree::read_xml(stream,document);
        const auto& declaration=document.get_child("tool_tip");
        mTooltipMaximumWidth=declaration.get<int>("<xmlattr>.max_width",200);
        mTooltipPadding=declaration.get<int>("<xmlattr>.padding",4);
        if (mTooltipMaximumWidth<1 || mTooltipMaximumWidth>4096 || mTooltipPadding<0 || mTooltipPadding>128)
        { error="Native tooltip dimensions exceed their bounds"; return false; }
        boost::property_tree::ptree panel,label,output;
        for (const auto* name : {"background_visible","background_opaque","bg_opaque_image","bg_alpha_image","bg_opaque_color","bg_alpha_color"})
            if (const auto value=declaration.get_optional<std::string>(std::string("<xmlattr>.")+name))
                panel.put(std::string("<xmlattr>.")+name,*value);
        for (const auto* name : {"visible_time_over","visible_time_near","visible_time_far"})
            if (const auto value=declaration.get_optional<std::string>(std::string("<xmlattr>.")+name))
            {
                float seconds=0.f;
                const auto parsed=std::from_chars(value->data(),value->data()+value->size(),seconds);
                if (parsed.ec!=std::errc{} || parsed.ptr!=value->data()+value->size() || !std::isfinite(seconds) || seconds<0.f)
                { error="Invalid native tooltip timeout"; return false; }
                mTooltipTimeouts[name]=seconds;
            }
        panel.put("<xmlattr>.name","native_tooltip"); panel.put("<xmlattr>.mouse_opaque","false");
        panel.put("<xmlattr>.width",mTooltipMaximumWidth+2*mTooltipPadding);
        panel.put("<xmlattr>.height",10000+2*mTooltipPadding);
        label.put("<xmlattr>.name","tooltip_text");
        label.put("<xmlattr>.left",mTooltipPadding); label.put("<xmlattr>.bottom",mTooltipPadding);
        label.put("<xmlattr>.width",mTooltipMaximumWidth); label.put("<xmlattr>.height",10000);
        label.put("<xmlattr>.font",declaration.get<std::string>("<xmlattr>.font","SansSerif"));
        label.put("<xmlattr>.text_color",declaration.get<std::string>("<xmlattr>.text_color","ToolTipTextColor"));
        label.put("<xmlattr>.wrap",declaration.get<std::string>("<xmlattr>.wrap","true"));
        label.put("<xmlattr>.h_pad",0); label.put("<xmlattr>.v_pad",0);
        label.put("<xmlattr>.valign","center"); label.put("<xmlattr>.parse_urls","false");
        label.put("<xmlattr>.use_ellipses","true");
        panel.add_child("text",label); output.add_child("panel",panel);
        std::ostringstream xml; boost::property_tree::write_xml(xml,output);
        mTooltipTemplate=xml.str();
        return true;
    }
    catch (const boost::property_tree::ptree_error& failure) { error=failure.what(); return false; }
}

bool LLVKViewerUi::appendTooltip(LLVKWidgetPaint& paint,const LLVKWidgetPaint::Input& input,std::string& error)
{
    const auto now=mTree.time();
    const auto position=std::pair{input.button.mouseX,input.button.mouseY};
    const auto contains=[](const auto& rect,const auto& point)
    { return point.first>=rect.left && point.first<rect.right && point.second>=rect.bottom && point.second<rect.top; };
    const auto setting=[&](const char* name,double fallback)
    { return mTree.setting(name).value_or(LLSD(fallback)).asReal(); };
    if (!mTooltipPointer || *mTooltipPointer!=position)
    {
        if (!mTooltipPointer || (mTooltipPointer->first!=position.first && mTooltipPointer->second!=position.second && !contains(mTooltipNear,position)))
            mTooltipBlocked=false;
        mTooltipMoved=now;
        mTooltipPointer=position;
    }
    if (mTree.mouseCapture())
    {
        mTooltipBlocked=true;
        if (mTooltipPanel && !mTooltipFade) mTooltipFade=now;
    }
    const auto viewport=mTree.screenRect(mRoot,error);
    if (!viewport) return false;
    const auto root=mNoticePanel ? mNoticePanel : mTree.topControl() ? mTree.topControl() : mRoot;
    const auto owner=mTree.tooltipAt(root,position.first,position.second,error);
    if (!owner) return false;
    if (input.editor.applicationFocused && !mTooltipBlocked && *owner && mTree.setting("BasicUITooltips").value_or(LLSD(true)).asBoolean() &&
        static_cast<float>(now-mTooltipMoved)>static_cast<float>(mTooltipPanel ? setting("ToolTipFastDelay",0.1) : setting("ToolTipDelay",0.7)) &&
        (!mTooltipPanel || mTooltipFade || mTooltipOwner!=*owner))
    {
        const auto message=mTree.get(*owner)->params.tooltip;
        const auto near=mTree.screenRect(*owner,error);
        if (!near) return false;
        if (mTooltipPanel) { if (!mTree.erase(mTooltipPanel,error)) return false; mTooltipPanel=0; }
        if (!initializeTooltip(error)) return false;
        const auto panel=mDialogFactory->construct(mTree,mTooltipTemplate,0,error);
        if (!panel) return false;
        mTooltipPanel=*panel;
        const auto label=find("tooltip_text",mTooltipPanel);
        if (!label || !mTree.setPlainText(label,message,error) || !mTree.fitPlainText(label,error)) return false;
        const auto textRect=mTree.get(label)->params.rect;
        const auto padding=mTooltipPadding;
        const int width=std::min(mTooltipMaximumWidth,textRect.right-textRect.left)+2*padding,height=textRect.top-textRect.bottom+2*padding;
        int left=position.first+8,top=position.second-16;
        const auto initialLeft=left,initialTop=top;
        left=std::clamp(left,viewport->left,std::max(viewport->left,viewport->right-width));
        top=std::clamp(top,std::min(viewport->top,viewport->bottom+height),viewport->top);
        const LLVKWidgetTree::Rect exclusion{position.first-1,position.second-17,position.first+9,position.second+1};
        if ((left!=initialLeft || top!=initialTop) && left<exclusion.right && left+width>exclusion.left &&
            top>exclusion.bottom && top-height<exclusion.top)
        {
            if (left>initialLeft) left=exclusion.right;
            else if (left<initialLeft) left=exclusion.left-width;
            if (top>initialTop) top=exclusion.top+height;
            else if (top<initialTop) top=exclusion.bottom;
        }
        if (!mTree.setShape(mTooltipPanel,{left,top-height,left+width,top},error)) return false;
        if (!mTree.setShape(label,{padding,padding,width-padding,height-padding},error)) return false;
        mTooltipOwner=*owner; mTooltipNear=*near; mTooltipShown=now;
        mTooltipFade.reset(); mTooltipBlocked=true;
    }
    else if (mTooltipPanel && !mTooltipFade)
    {
        const auto tooltipRect=mTree.screenRect(mTooltipPanel,error);
        if (!tooltipRect) return false;
        const auto configured=[&](const char* name,const char* control,double fallback)
        { const auto found=mTooltipTimeouts.find(name); return found==mTooltipTimeouts.end() ? setting(control,fallback) : found->second; };
        const auto timeout=contains(mTooltipNear,position) ? contains(*tooltipRect,position) ?
            configured("visible_time_over","ToolTipVisibleTimeOver",1000.0) : configured("visible_time_near","ToolTipVisibleTimeNear",10.0) :
            configured("visible_time_far","ToolTipVisibleTimeFar",1.0);
        if (static_cast<float>(now-mTooltipShown)>static_cast<float>(timeout)) { mTooltipFade=now; mTooltipBlocked=false; }
    }
    if (!mTooltipPanel) return true;
    const auto fadeTime=static_cast<float>(setting("ToolTipFadeTime",0.2));
    const auto alpha=mTooltipFade ? fadeTime>0.f ? std::clamp(1.f-static_cast<float>(now-*mTooltipFade)/fadeTime,0.f,1.f) : 0.f : 1.f;
    if (alpha==0.0)
    {
        if (!mTree.erase(mTooltipPanel,error)) return false;
        mTooltipPanel=0; mTooltipOwner=0; mTooltipFade.reset();
        return true;
    }
    auto tooltipInput=input;
    tooltipInput.button.drawAlpha=tooltipInput.button.transparency=static_cast<float>(alpha);
    auto tooltip=LLVKWidgetPaint::prepare(mTree,mTooltipPanel,tooltipInput,error);
    if (!tooltip) return false;
    for (auto& command : tooltip->commands) command.clip=*viewport;
    paint.commands.insert(paint.commands.end(),tooltip->commands.begin(),tooltip->commands.end());
    return true;
}

LLVKMenu& LLVKViewerUi::menu() noexcept
{
    for (auto id=mTree.keyboardFocus(); mTree.get(id); id=mTree.get(id)->parent)
    {
        if (const auto& menu=mTree.get(id)->menu; menu && menu->open()) return *menu;
        for (const auto& [item,dialog] : mTornMenus)
            if (dialog.floater->id()==id && dialog.floater->visible()) return *dialog.view;
    }
    return *mMenu;
}

void LLVKViewerUi::blockTooltips()
{
    mTooltipBlocked=true;
    if (mTooltipPanel && !mTooltipFade) mTooltipFade=mTree.time();
}

bool LLVKViewerUi::menuPointer(const LLVKWidgetTree::PointerEvent& event)
{
    if (event.kind!=LLVKWidgetTree::PointerKind::Hover) blockTooltips();
    mMenu->setTime(mTree.time());
    auto& focused=menu();
    if (&focused!=mMenu.get() && focused.pointer(event)) return true;
    return mMenu->pointer(event);
}

bool LLVKViewerUi::menuShortcut(const std::string& key,bool control,bool shift,bool alt)
{
    return mMenu->shortcut(key,control,shift,alt);
}

bool LLVKViewerUi::updateMenuHover(const LLVKWidgetTree::PointerEvent& event,std::string& error)
{
    if (mTree.mouseCapture() || mNoticePanel) return true;
    for (auto& [item,dialog] : mTornMenus)
    {
        if (!dialog.floater->visible()) continue;
        const auto rectangle=mTree.screenRect(dialog.content,error);
        const auto viewport=mTree.screenRect(mRoot,error);
        if (!rectangle || !viewport) return false;
        LLVKWidgetPaint layout;
        if (!dialog.view->paint(layout,*viewport,error,*rectangle,true)) return false;
    }
    menuPointer(event);
    return error.empty();
}

bool LLVKViewerUi::tearOffMenu(std::size_t item,LLVKWidgetTree::Rect rectangle,std::string& error)
{
    error.clear();
    if (const auto found=mTornMenus.find(item); found!=mTornMenus.end())
    {
        if (found->second.floater->visible())
        {
            if (!found->second.floater->open(error)) return false;
            return mTree.setKeyboardFocus(found->second.content,false,false,error);
        }
        if (mActiveFloater==found->second.floater.get()) mActiveFloater=nullptr;
        mTornMenus.erase(found);
    }
    auto view=mMenu->detachedView(item,error);
    if (!view) return false;
    const auto width=rectangle.right-rectangle.left,height=rectangle.top-rectangle.bottom;
    auto floater=LLVKFloater::createXml(mTree,*mDialogFactory,mRoot,
        "<floater name='native_torn_menu_"+std::to_string(item)+"' title='' width='"+std::to_string(width+4)+
        "' height='"+std::to_string(height+18)+"' can_minimize='false' can_resize='false'/>",error);
    if (!floater) return false;
    mTree.setValue(find("floater_title",floater->id()),LLSD(mMenu->items()[item].label));
    LLVKWidgetTree::Params params;
    params.name="torn_menu_content"; params.rect={1,1,width+1,height+1};
    params.mouseOpaque=true;
    LLVKControl::Params control;
    control.font=mFonts->resolve({"SansSerif","Small"},error);
    if (!control.font) return false;
    const auto content=mTree.createControl(params,control,floater->id(),error);
    if (!content || !mTree.setMenu(*content,view)) return false;
    const auto id=floater->id();
    floater->onClose([this,item]
    {
        const auto found=mTornMenus.find(item);
        if (found==mTornMenus.end()) return;
        found->second.view->dismiss();
        found->second.view->setDetachedActive(false);
        mMenu->dismiss();
    });
    view->setTearOffHandler([this,item](auto selected,auto rectangle)
    {
        if (selected!=item) { tearOffMenu(selected,rectangle,mDialogError); return; }
        const auto found=mTornMenus.find(item);
        if (found!=mTornMenus.end()) found->second.floater->close(mDialogError);
    });
    const auto constructed=mTree.get(id)->params.rect;
    const auto totalHeight=constructed.top-constructed.bottom;
    const auto left=rectangle.left-1;
    auto placement=LLVKWidgetTree::Rect{left,rectangle.bottom,left+width+4,rectangle.top};
    const auto root=mTree.get(mRoot)->params.rect;
    const auto fitX=placement.left<0 ? -placement.left : 0;
    const auto fitY=placement.top>root.top-root.bottom-19 ? root.top-root.bottom-19-placement.top : 0;
    placement.left+=fitX; placement.right+=fitX;
    placement.bottom+=fitY; placement.top+=fitY;
    if (!floater->open(error) || !mTree.setShape(id,placement,error) ||
        !mTree.setKeyboardFocus(*content,false,false,error)) return false;
    view->setDetachedFocus(true);
    if (fitX && fitY) view->dismiss();
    view->setDetachedActive(true);
    mTornMenus.emplace(item,TornMenu{std::move(floater),std::move(view),*content,totalHeight});
    return true;
}

LLVKWidgetTree::Id LLVKViewerUi::find(std::string_view name,LLVKWidgetTree::Id within) const
{
    const auto visit = [&](const auto& self,LLVKWidgetTree::Id id) -> LLVKWidgetTree::Id
    {
        const auto* node = mTree.get(id);
        if (!node) return 0;
        if (node->params.name == name) return id;
        for (const auto child : node->children) if (const auto found = self(self,child)) return found;
        return 0;
    };
    return visit(visit,within ? within : mRoot);
}