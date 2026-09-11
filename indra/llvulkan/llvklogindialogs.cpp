#include "llvkloginui.h"
#include "llstring.h"
#include "llsdutil.h"
#include "llvkxmllayers.h"
#include <expat/expat.h>
#include <fstream>
#include <algorithm>
#include <cmath>

namespace
{
    std::map<std::string,LLVKWidgetTree::Id> preferenceFields(const LLVKWidgetTree& tree,LLVKWidgetTree::Id panel)
    {
        std::map<std::string,LLVKWidgetTree::Id> fields;
        std::vector<LLVKWidgetTree::Id> children{panel};
        for (std::size_t index=0; index<children.size(); ++index)
        {
            const auto* node=tree.get(children[index]);
            if (!node) continue;
            fields.try_emplace(node->params.name,children[index]);
            children.insert(children.end(),node->children.begin(),node->children.end());
        }
        return fields;
    }

    std::optional<std::string> readText(const std::filesystem::path& path)
    {
        std::ifstream file(path,std::ios::binary|std::ios::ate);
        if (!file || file.tellg() < 0 || file.tellg() > 4*1024*1024) return {};
        std::string text(static_cast<std::size_t>(file.tellg()),'\0');
        file.seekg(0);
        if (!file.read(text.data(),text.size())) return std::nullopt;
        return text;
    }
}

bool LLVKLoginUi::initializeDialogs(const Configuration& configuration,std::string& error)
{
    mSavePreferences = configuration.savePreferences;
    mAppliedSettingsMode = configuration.appliedSettingsMode;
    mMenu->bindItem("Floater.Toggle","preferences",[this](const auto&,const auto&)
    {
        if (mPreferences && mPreferences->visible()) mPreferences->close(mDialogError);
        else showPreferences(mDialogError);
    });
    mMenu->bindItem("Floater.Show","sl_about",[this](const auto&,const auto&) { showAbout(mDialogError); });
    const auto documents = mSkin->read("xui","floater_about.xml",LLVKSkinFiles::Policy::Current,error);
    if (!documents || documents->empty()) return false;
    struct Parser
    {
        XML_Parser parser;
        std::map<std::string,std::string>& strings;
        std::vector<std::string> stack;
        std::string body, name;
        bool failed = false;
        int textDepth = 0;
        static void XMLCALL start(void* pointer,const char* tag,const char** attributes)
        {
            auto& state = *static_cast<Parser*>(pointer);
            try
            {
                if (state.stack.size() >= 64) throw std::runtime_error("About depth");
                state.stack.emplace_back(tag);
                if (std::string_view(tag) == "string" || std::string_view(tag) == "global")
                {
                    state.textDepth = static_cast<int>(state.stack.size()); state.body.clear(); state.name.clear();
                    for (std::size_t index=0; attributes[index]; index+=2) if (std::string_view(attributes[index]) == "name") state.name=attributes[index+1];
                }
            }
            catch (...) { state.failed=true; XML_StopParser(state.parser,XML_FALSE); }
        }
        static void XMLCALL text(void* pointer,const char* text,int length)
        { auto& state=*static_cast<Parser*>(pointer); try { if (state.textDepth) state.body.append(text,length); } catch (...) { state.failed=true; XML_StopParser(state.parser,XML_FALSE); } }
        static void XMLCALL end(void* pointer,const char*)
        {
            auto& state=*static_cast<Parser*>(pointer);
            try
            {
                if (state.textDepth == static_cast<int>(state.stack.size()))
                {
                    LLStringUtil::trim(state.body);
                    state.strings.insert_or_assign(state.name,state.body);
                    state.textDepth=0;
                }
                if (!state.stack.empty()) state.stack.pop_back();
            }
            catch (...) { state.failed=true; XML_StopParser(state.parser,XML_FALSE); }
        }
        static void XMLCALL doctype(void* pointer,const char*,const char*,const char*,int)
        { auto& state=*static_cast<Parser*>(pointer); state.failed=true; XML_StopParser(state.parser,XML_FALSE); }
    };
    std::vector<std::string_view> layers;
    for (const auto& document : *documents) layers.push_back(document);
    const auto merged=LLVKXmlLayers::merge(layers,error);
    if (!merged) return false;
    Parser state{XML_ParserCreate(nullptr),mAboutStrings};
    if (!state.parser) { error="Native About parser allocation failed"; return false; }
    XML_SetUserData(state.parser,&state); XML_SetElementHandler(state.parser,Parser::start,Parser::end);
    XML_SetCharacterDataHandler(state.parser,Parser::text); XML_SetStartDoctypeDeclHandler(state.parser,Parser::doctype);
    const auto parsed=XML_Parse(state.parser,merged->data(),static_cast<int>(merged->size()),XML_TRUE);
    XML_ParserFree(state.parser);
    if (parsed != XML_STATUS_OK || state.failed) { error="Invalid native About declaration"; return false; }
    const auto strings=mSkin->read("xui","strings.xml",LLVKSkinFiles::Policy::Current,error);
    if (!strings) return false;
    layers.clear();
    for (const auto& document : *strings) layers.push_back(document);
    const auto mergedStrings=LLVKXmlLayers::merge(layers,error);
    if (!mergedStrings) return false;
    Parser stringState{XML_ParserCreate(nullptr),mAboutStrings};
    if (!stringState.parser) { error="Native About strings parser allocation failed"; return false; }
    XML_SetUserData(stringState.parser,&stringState); XML_SetElementHandler(stringState.parser,Parser::start,Parser::end);
    XML_SetCharacterDataHandler(stringState.parser,Parser::text); XML_SetStartDoctypeDeclHandler(stringState.parser,Parser::doctype);
    const auto stringsParsed=XML_Parse(stringState.parser,mergedStrings->data(),static_cast<int>(mergedStrings->size()),XML_TRUE);
    XML_ParserFree(stringState.parser);
    if (stringsParsed != XML_STATUS_OK || stringState.failed) { error="Invalid native About strings"; return false; }
    const auto notifications=mSkin->read("xui","notifications.xml",LLVKSkinFiles::Policy::Current,error);
    if (!notifications) return false;
    layers.clear();
    for (const auto& document : *notifications) layers.push_back(document);
    const auto notificationXml=LLVKXmlLayers::merge(layers,error);
    if (!notificationXml) return false;
    Parser notificationState{XML_ParserCreate(nullptr),mAboutStrings};
    if (!notificationState.parser) { error="Native notification parser allocation failed"; return false; }
    XML_SetUserData(notificationState.parser,&notificationState);
    XML_SetElementHandler(notificationState.parser,Parser::start,Parser::end);
    XML_SetCharacterDataHandler(notificationState.parser,Parser::text);
    XML_SetStartDoctypeDeclHandler(notificationState.parser,Parser::doctype);
    const auto parsedNotifications=XML_Parse(notificationState.parser,notificationXml->data(),static_cast<int>(notificationXml->size()),XML_TRUE);
    XML_ParserFree(notificationState.parser);
    if (parsedNotifications!=XML_STATUS_OK || notificationState.failed || !mAboutStrings.contains("implicitclosebutton"))
    { error="Native notification strings are incomplete"; return false; }
    if (mTree.setting("RestrainedLove"))
    {
        for (const auto name : {"RLVaToggleMessageLogin","RLVaToggleEnabled","RLVaToggleDisabled"})
            if (!mAboutStrings.contains(name)) { error=std::string("Missing native RLVa startup string: ")+name; return false; }
        const auto subscription=mTree.subscribeSetting("RestrainedLove",[this](const LLSD& value,const LLSD&)
        {
            auto message=mAboutStrings.at("RLVaToggleMessageLogin");
            LLStringUtil::format_map_t arguments;
            arguments["[STATE]"]=mAboutStrings.at(value.asBoolean() ? "RLVaToggleEnabled" : "RLVaToggleDisabled");
            LLStringUtil::format(message,arguments);
            mNotices.push_back({"GenericAlert",std::move(message)});
        });
        if (!subscription) { error="Cannot subscribe native RLVa startup setting"; return false; }
    }
    const auto app = configuration.skin.skinBaseDirectory.parent_path()/"app_settings";
    mAboutContributors=readText(app/"contributors.txt").value_or("");
    const auto newline=mAboutContributors.find_first_of("\r\n");
    if (newline!=std::string::npos) mAboutContributors.resize(newline);
    mAboutLicenses=readText(app/"packages-info.txt");
    mAboutInfo = "Vulkanstorm\nRenderer: native Vulkan\nNot connected\n";
    return true;
}

bool LLVKLoginUi::advanceNotices(double time,std::string& error)
{
    error.clear();
    if (!std::isfinite(time) || time<mNoticeTime) { error="Invalid native notice clock"; return false; }
    mNoticeTime=time;
    if (mNoticePanel || mNotices.empty()) return true;
    if (mNotices.front().name!="GenericAlert") { error="Native notification form is not implemented"; return false; }
    const auto font=mFonts->resolve({"SansSerif","Medium"},error);
    if (!font) return false;
    const auto wide=utf8str_to_wstring(mNotices.front().message);
    LLVKPlainTextLayout::Options options; options.width=400; options.wrap=true;
    const auto document=LLVKPlainTextLayout::document(std::u32string(wide.begin(),wide.end()),*font,options,0,0,LLVKFont::VerticalAlign::Top,error);
    if (!document) return false;
    const auto labelWide=utf8str_to_wstring(mAboutStrings.at("implicitclosebutton"));
    const std::u32string label(labelWide.begin(),labelWide.end());
    const auto measured=font->measureRun(label,0,label.size(),1.f,true,false,error);
    const auto padding=font->measureRun(U"OO",0,2,1.f,true,false,error);
    if (!measured || !padding) return false;
    const auto buttonWidth=static_cast<int>(measured->width+0.99f)+static_cast<int>(padding->width)+20;
    const auto textWidth=std::min(400,document->fitWidth+25);
    const auto textHeight=document->fitHeight;
    const auto width=std::max(buttonWidth,textWidth)+50, height=textHeight+48+23;
    const auto root=mTree.get(mRoot)->params.rect;
    const auto left=std::max(0,(root.right-root.left-width)/2),bottom=std::max(0,(root.top-root.bottom-height)/2);
    LLVKWidgetTree::Params view; view.name="GenericAlert"; view.rect={left,bottom,left+width,bottom+height};
    view.focusRoot=true;
    LLVKControl::Params control; control.font=font;
    LLVKPanel::Params background; background.backgroundVisible=background.backgroundOpaque=true;
    background.opaqueImage=mTree.findImage("Window_Foreground",error);
    if (!error.empty()) return false;
    const auto panel=mTree.createPanel(view,control,background,mRoot,error);
    if (!panel) return false;
    const auto discard=[&] { std::string ignored; mTree.erase(*panel,ignored); };
    view.name="Alert message"; view.rect={25,height-16-textHeight,25+textWidth,height-16};
    view.focusRoot=false; view.mouseOpaque=false;
    control.tabStop=false; control.initialValue=mNotices.front().message;
    LLVKPlainControl::Params text; text.layout.wrap=true;
    if (const auto color=mColors->find("LabelTextColor")) text.textColor=text.readOnlyColor=*color;
    if (!mTree.createPlainText(view,control,text,*panel,error)) { discard(); return false; }
    const auto button=mDialogFactory->constructFile(mTree,"alert_button.xml",*panel,error);
    if (!button) { discard(); return false; }
    const auto buttonLeft=(width-buttonWidth)/2;
    if (!mTree.setShape(*button,{buttonLeft,16,buttonLeft+buttonWidth,39},error)) { discard(); return false; }
    mTree.setButtonLabel(*button,label);
    LLVKControl::Callback close;
    close.function=[this](auto,const LLSD&) { dismissNotice(mDialogError); };
    mTree.setControlCommit(*button,std::move(close));
    mNoticePreviousFocus=mTree.keyboardFocus();
    if (mTree.mouseCapture() && !mTree.setMouseCapture(0,error)) { discard(); return false; }
    if (mTree.topControl() && !mTree.setTopControl(0,error)) { discard(); return false; }
    mMenu->dismiss();
    if (!mTree.setKeyboardFocus(*panel,true,false,error) || !mTree.requestControlFocus(*button,true,error))
    { mTree.unlockFocus(); discard(); return false; }
    mNoticePanel=*panel; mNoticeButton=*button; mNoticeOpened=time;
    mNotices.erase(mNotices.begin());
    return true;
}

bool LLVKLoginUi::dismissNotice(std::string& error)
{
    error.clear();
    if (!mNoticePanel) return false;
    const auto panel=mNoticePanel,focus=mNoticePreviousFocus;
    mTree.unlockFocus();
    mNoticePanel=mNoticeButton=mNoticePreviousFocus=0;
    if (!mTree.erase(panel,error)) return false;
    return !mTree.get(focus) || mTree.requestControlFocus(focus,true,error);
}

bool LLVKLoginUi::noticeKey(bool returnKey,bool modified,std::string& error)
{
    if (!mNoticePanel) return false;
    if (returnKey && !modified && mNoticeTime-mNoticeOpened>=0.5) dismissNotice(error);
    return true;
}

std::optional<LLVKWidgetTree::Id> LLVKLoginUi::constructPreferencePanel(const std::string& filename,
    LLVKWidgetTree::Id parent,std::string& error)
{
    error.clear();
    if (filename!="panel_preferences_colors.xml" && filename!="panel_preferences_general.xml")
    { error="Native Preferences panel application policy is not implemented: "+filename; return std::nullopt; }
    return mDialogFactory->construct(mTree,"<panel class='panel_preference' filename='"+filename+"'/>",parent,error);
}

bool LLVKLoginUi::initializeStartupPreferencePanel(LLVKWidgetTree::Id panel,std::string& error)
{
    error.clear();
    const auto fields=preferenceFields(mTree,panel);
    if (const auto display=fields.find("display_names_check"); display!=fields.end())
    {
        const bool enabled=mTree.setting("UsePeopleAPI").value_or(LLSD(false)).asBoolean();
        mTree.setEnabled(display->second,enabled);
        if (!enabled) mTree.setValue(display->second,LLSD(false));
    }
    if (!mTree.bindPreferenceColorAlpha(panel,mColors,error)) return false;
    const auto maturity=fields.find("maturity_desired_combobox");
    if (maturity!=fields.end())
    {
        const auto* combo=mTree.get(maturity->second);
        if (!combo || !combo->combo || !fields.contains("maturity_desired_textbox"))
        { error="Native General Preferences maturity controls are incomplete"; return false; }
        std::string label;
        if (combo->combo->selected && *combo->combo->selected<combo->combo->items.size())
            label=combo->combo->items[*combo->combo->selected].label;
        if (!mTree.setValue(fields.at("maturity_desired_textbox"),LLSD(label))) return false;
        mTree.setEnabled(maturity->second,false);
        return updateStartupPreferenceMaturity(panel,error);
    }
    return true;
}

bool LLVKLoginUi::updateStartupPreferenceMaturity(LLVKWidgetTree::Id panel,std::string& error)
{
    error.clear();
    const auto fields=preferenceFields(mTree,panel);
    for (const auto name : {"rating_icon_general","rating_icon_moderate","rating_icon_adult"})
        if (!fields.contains(name)) { error=std::string("Missing native maturity control: ")+name; return false; }
    const auto maturity=mTree.setting("PreferredMaturity").value_or(LLSD(13)).asInteger();
    mTree.setVisible(fields.at("rating_icon_general"),maturity==13 || maturity==21 || maturity==42);
    mTree.setVisible(fields.at("rating_icon_moderate"),maturity==21 || maturity==42);
    mTree.setVisible(fields.at("rating_icon_adult"),maturity==42);
    for (const auto setting : {"ShowMatureSims","ShowMatureLand","ShowMatureClassifieds","ShowAdultSims","ShowAdultLand","ShowAdultClassifieds"})
        if (mTree.setting(setting)) mTree.updateSetting(setting,LLSD(false));
    if (mTree.setting("FSSearchGroupMaturity")) mTree.updateSetting("FSSearchGroupMaturity",LLSD(13));
    return true;
}

bool LLVKLoginUi::showPreferences(std::string& error)
{
    error.clear();
    if (!mPreferences)
    {
        const auto font=mFonts->resolve({"SansSerif","Small"},error);
        if (!font) return false;
        mPreferences=LLVKFloater::create(mTree,*mDialogFactory,mRoot,"Preferences","Preferences",673,561,font,error);
        if (!mPreferences) return false;
        const auto content=mDialogFactory->construct(mTree,
            "<panel name='native_preferences_content' left='12' bottom='48' width='649' height='480' font='SansSerifSmall'>"
            "<text name='native_pref_general' left='12' bottom='444' width='600' height='24' font='SansSerif'>General</text>"
            "<check_box name='pref_remember_username' left='12' bottom='406' width='400' height='23' label='Remember username' control_name='FSRememberUsername'/>"
            "<check_box name='pref_remember_password' left='12' bottom='371' width='400' height='23' label='Remember password' control_name='RememberPassword' enabled_control='FSRememberUsername'/>"
            "<text name='native_pref_graphics' left='12' bottom='307' width='600' height='24' font='SansSerif'>Graphics</text>"
            "<text name='native_renderer_label' left='12' bottom='273' width='600' height='20'>Renderer (requires shutdown and restart):</text>"
            "<combo_box name='native_renderer_choice' left='12' bottom='235' width='220' height='26' control_name='RenderBackendPending'>"
            "<combo_box.item label='OpenGL' value='OpenGL'/><combo_box.item label='Vulkan' value='Vulkan'/><combo_box.item label='Mesa/Zink' value='Zink'/></combo_box>"
            "</panel>",mPreferences->id(),error);
        if (!content) { mPreferences.reset(); return false; }
        const auto ok=mDialogFactory->construct(mTree,"<button name='preferences_ok' label='OK' left='465' bottom='10' width='90' height='23'/>",mPreferences->id(),error);
        const auto cancel=mDialogFactory->construct(mTree,"<button name='preferences_cancel' label='Cancel' left='565' bottom='10' width='90' height='23'/>",mPreferences->id(),error);
        if (!ok || !cancel) { mPreferences.reset(); return false; }
        LLVKControl::Callback accepted;
        accepted.function=[this](auto,const LLSD&) { applyPreferences(mDialogError); };
        mTree.setControlCommit(*ok,std::move(accepted));
        LLVKControl::Callback cancelled;
        cancelled.function=[this](auto,const LLSD&) { mPreferences->close(mDialogError); };
        mTree.setControlCommit(*cancel,std::move(cancelled));
        mTree.setPanelDefaultButton(mPreferences->id(),*ok,error);
        mPreferences->onClose([this]
        {
            if (!mPreferencesAccepted) mTree.restorePreferences(mPreferenceSnapshot,{},mDialogError);
            mPreferenceSnapshot={};
        });
    }
    if (!mPreferences->visible())
    {
        mPreferencesAccepted=false;
        const auto backend=mTree.setting("RenderBackend").value_or(LLSD("Vulkan"));
        if (!mTree.updateSetting("RenderBackendPending",backend)) mTree.defineSetting("RenderBackendPending",backend,LLVKWidgetTree::SettingType::String);
        mTree.setValue(find("native_renderer_choice"),backend);
        const auto snapshot=mTree.snapshotPreferences(mPreferences->id(),error);
        if (!snapshot) return false;
        mPreferenceSnapshot=*snapshot;
    }
    mActiveFloater=mPreferences.get();
    return mPreferences->open(error);
}

bool LLVKLoginUi::resetPreference(const std::string& name,std::string& error)
{
    error.clear();
    const auto found=mSettingDefaults.find(name);
    if (found==mSettingDefaults.end() || !mTree.setting(name))
    { error="Native preference has no loaded default: "+name; return false; }
    if (!mTree.updateSetting(name,found->second))
    { error="Native preference reset failed: "+name; return false; }
    return true;
}

bool LLVKLoginUi::applyPreferences(std::string& error)
{
    error.clear();
    if (!mPreferences || !mPreferences->visible()) return false;
    std::map<std::string,LLSD> changed;
    for (const auto& [name,old] : mPreferenceSnapshot.settings)
    {
        const auto value=mTree.setting(name);
        if (value && !llsd_equals(*value,old)) changed[name == "RenderBackendPending" ? "RenderBackend" : name]=*value;
    }
    if (!changed.empty() && (!mSavePreferences || !mSavePreferences(changed,error)))
    { if (error.empty()) error="Native preference persistence is unavailable"; return false; }
    mPreferencesAccepted=true;
    return mPreferences->close(error);
}

bool LLVKLoginUi::showAbout(std::string& error)
{
    error.clear();
    if (!mAbout)
    {
        mAbout=LLVKFloater::createFile(mTree,*mDialogFactory,mRoot,"floater_about.xml",error);
        if (!mAbout) return false;
        const auto support=find("support_editor"), contributors=find("contrib_names"), licenses=find("licenses_editor"), copy=find("copy_btn");
        if (!support || !contributors || !licenses || !copy)
        { error="Native About declaration is missing required controls"; mAbout.reset(); return false; }
        for (const auto editor : {support,contributors,licenses})
        {
            if (!mTree.get(editor)->textEditor)
            { error="Native About requires text-editor ownership"; mAbout.reset(); return false; }
            mTree.setEnabled(editor,false);
        }
        mAboutBody=support;
        if (!updateAboutText(error)) { mAboutBody=0; mAbout.reset(); return false; }
        if (!mTree.setTextEditorText(contributors,mAboutContributors,error) ||
            (mAboutLicenses && !mTree.setTextEditorText(licenses,*mAboutLicenses,error)))
        { mAboutBody=0; mAbout.reset(); return false; }
        for (const auto editor : {support,contributors,licenses})
            if (!mTree.startTextEditorDocument(editor,error)) { mAboutBody=0; mAbout.reset(); return false; }
        LLVKWidgetTree::Events linkEvents;
        linkEvents.cursor=[this](auto,bool hand) { if (mPointerCursor) mPointerCursor(hand); };
        std::vector<LLVKWidgetTree::Id> pending{mAbout->id()};
        while (!pending.empty())
        {
            const auto id=pending.back(); pending.pop_back();
            const auto* node=mTree.get(id);
            pending.insert(pending.end(),node->children.begin(),node->children.end());
            if (node->plainText) mTree.setEvents(id,linkEvents);
        }
        LLVKControl::Callback callback;
        callback.function=[this,support](auto,const LLSD&)
        {
            if (!mDialogClipboard) { mDialogError="Native clipboard unavailable"; return; }
            mTree.selectAllPlainText(support);
            mTree.copyPlainText(support,mDialogError);
            mTree.deselectPlainText(support);
        };
        mTree.setControlCommit(copy,std::move(callback));
    }
    mActiveFloater=mAbout.get();
    return mAbout->open(error);
}
void LLVKLoginUi::setAboutInfo(std::string info)
{ mAboutInfo=std::move(info); if (mAboutBody) updateAboutText(mDialogError); }
bool LLVKLoginUi::setAboutInfo(const LLSD& info,std::string& error)
{
    error.clear();
    if (!info.isMap()) { error="Native About information must be a map"; return false; }
    LLStringUtil::format_map_t arguments;
    arguments["APP_NAME"]="Vulkanstorm";
    arguments["ReleaseNotes"]=mAboutStrings["ReleaseNotes"];
    LLStringUtil::format_map_t generationArguments;
    generationArguments["VERSION"]=info["VIEWER_VERSION"][0].asString();
    for (const auto name : {"VIEWER_GENERATION","SHORT_VIEWER_GENERATION"})
    {
        auto label=mAboutStrings[name];
        LLStringUtil::format(label,generationArguments);
        arguments[name]=label;
    }
    for (auto item=info.beginMap(); item!=info.endMap(); ++item)
    {
        if (item->second.isArray())
            for (LLSD::Integer index=0; index<item->second.size(); ++index)
                arguments[item->first+"_"+std::to_string(index)]=item->second[index].asString();
        else arguments[item->first]=item->second.isUndefined() ? mAboutStrings["none_text"] : item->second.asString();
    }
    if (!info.has("VIEWER_RELEASE_NOTES_URL"))
    {
        std::string version;
        for (LLSD::Integer index=0; index<info["VIEWER_VERSION"].size(); ++index)
        { if (index) version+='.'; version+=info["VIEWER_VERSION"][index].asString(); }
        arguments["VIEWER_RELEASE_NOTES_URL"]=mAboutStrings["RELEASE_NOTES_BASE_URL"]+version;
    }
    const auto translatedSetting=[&](const char* field,const char* prefix,const char* setting,const char* fallback)
    {
        const auto value=mTree.setting(setting).value_or(LLSD()).asString();
        const auto found=mAboutStrings.find(std::string(prefix)+value);
        auto label=found==mAboutStrings.end() ? mAboutStrings[fallback] : found->second;
        LLStringUtil::format(label,arguments);
        arguments[field]=label;
    };
    translatedSetting("FONT","font_","FSInternalFontSettingsFile","font_unknown");
    const auto mode=mAboutStrings.find("mode_"+mAppliedSettingsMode);
    auto modeLabel=mode==mAboutStrings.end() ? mAboutStrings["mode_unknown"] : mode->second;
    LLStringUtil::format(modeLabel,arguments);
    arguments["MODE"]=modeLabel;
    arguments["RLV_VERSION"]=mAboutStrings["RLVaStatusDisabled"];
    if (!info.has("AUDIO_DRIVER_VERSION")) arguments["AUDIO_DRIVER_VERSION"]="Undefined";
    arguments["J2C_VERSION"]=LLVKWidgetImage::j2cDecoderVersion();
    const auto quality=mTree.setting("RenderQualityPerformance").value_or(LLSD(-1)).asInteger();
    const char* qualities[]{"render_quality_low","render_quality_mediumlow","render_quality_medium","render_quality_mediumhigh",
        "render_quality_high","render_quality_highultra","render_quality_ultra"};
    arguments["RENDERQUALITY"]=mAboutStrings[quality>=0 && quality<7 ? qualities[quality] : "render_quality_unknown"];
    arguments["VOICE_VERSION"]=mAboutStrings["NotConnected"];
    std::string text;
    const auto append=[&](const char* name,const char* separator)
    {
        const auto found=mAboutStrings.find(name);
        if (found==mAboutStrings.end()) { error=std::string("Missing native About string: ")+name; return false; }
        auto section=found->second;
        LLStringUtil::format(section,arguments);
        text+=separator;
        text+=section;
        return true;
    };
    if (!append("AboutHeader","") || !append("AboutSystem","\n\n")) return false;
    text+='\n';
    if (info.has("GRAPHICS_DRIVER_VERSION") && !append("AboutDriver","\n")) return false;
    if (!append("AboutRenderer","\n") || !append("AboutLibs","\n\n")) return false;
    if (info.has("BANDWIDTH") && !append("AboutSettings","\n")) return false;
    if (info.has("DISK_CACHE_INFO") && !append("AboutCache","\n")) return false;
    if (info.has("COMPILER") && !append("AboutCompiler","\n")) return false;
    if (info.has("datetime") && !append("AboutTime","\n")) return false;
    mAboutInfo=std::move(text);
    return !mAboutBody || updateAboutText(error);
}
bool LLVKLoginUi::updateAboutText(std::string& error)
{
    return mTree.setTextEditorText(mAboutBody,mAboutInfo,error);
}
bool LLVKLoginUi::showColorPicker(LLVKWidgetTree::Id swatch,bool takeFocus,std::string& error)
{
    error.clear();
    const auto* source=mTree.get(swatch);
    if (!source || !source->colorSwatch) { error="Native picker has no owning swatch"; return false; }
    auto found=mColorPickers.find(swatch);
    if (found==mColorPickers.end())
    {
        auto picker=LLVKFloater::createFile(mTree,*mDialogFactory,mRoot,"floater_color_picker.xml",error);
        if (!picker) return false;
        const auto id=picker->id();
        if (!mTree.initializeColorPicker(id,swatch,error,[this,swatch]
            { const auto found=mColorPickers.find(swatch); if (found!=mColorPickers.end()) found->second->close(mDialogError); }) ||
            !mTree.setColorPickerPalette(id,mColors,error)) return false;
        picker->onClose([this,swatch]
        {
            const auto* node=mTree.get(swatch);
            if (node && node->colorSwatch && node->colorSwatch->picking)
                mTree.applyColorSelection(swatch,{},LLVKWidgetTree::ColorPickOperation::Cancel,mDialogError);
        });
        found=mColorPickers.emplace(swatch,std::move(picker)).first;
    }
    else
    {
        if (!mTree.beginColorSelection(swatch,error) ||
            !mTree.setColorPickerRgb(found->second->id(),mTree.get(swatch)->colorSwatch->color,false,error)) return false;
    }
    if (!found->second->open(error)) return false;
    mActiveFloater=found->second.get();
    const auto& fields=mTree.get(found->second->id())->colorPicker->fields;
    if (takeFocus) return mTree.requestControlFocus(fields.at("select_btn"),true,error);
    return true;
}

std::vector<LLVKFloater*> LLVKLoginUi::floaters() const
{
    std::vector<LLVKFloater*> result{mPreferences.get(),mAbout.get()};
    for (const auto& [swatch,picker] : mColorPickers) result.push_back(picker.get());
    return result;
}

LLVKWidgetTree::Id LLVKLoginUi::activeFloater() const
{
    for (const auto child : mTree.get(mRoot)->children)
        for (const auto* floater : floaters())
            if (floater && floater->id() == child && floater->visible()) return child;
    return 0;
}
bool LLVKLoginUi::pointOverFloater(int x,int y) const
{
    for (const auto* floater : floaters())
    {
        if (!floater || !floater->visible()) continue;
        std::string error;
        const auto rect=mTree.screenRect(floater->id(),error);
        if (rect && x>=rect->left && x<rect->right && y>=rect->bottom && y<rect->top) return true;
    }
    return false;
}
bool LLVKLoginUi::closeFloater(std::string& error)
{
    const auto active=activeFloater();
    for (auto* floater : floaters())
        if (floater && floater->id()==active) return floater->close(error);
    return false;
}
bool LLVKLoginUi::floaterPointer(const LLVKWidgetTree::PointerEvent& event,std::string& error)
{
    if (mTree.mouseCapture())
    {
        for (auto* floater : floaters())
            if (floater && floater->id()==mTree.mouseCapture()) return floater->pointer(event,error);
        return false;
    }
    if (event.kind == LLVKWidgetTree::PointerKind::LeftDown)
        for (const auto child : mTree.get(mRoot)->children)
        {
            LLVKFloater* floater=nullptr;
            for (auto* candidate : floaters()) if (candidate && candidate->id()==child) { floater=candidate; break; }
            if (!floater || !floater->visible()) continue;
            const auto rect=mTree.screenRect(floater->id(),error);
            if (rect && event.x>=rect->left && event.x<rect->right && event.y>=rect->bottom && event.y<rect->top)
            { mActiveFloater=floater; floater->open(error); return floater->pointer(event,error); }
        }
    return false;
}
bool LLVKLoginUi::floaterWheel(int x,int y,int clicks,std::string& error)
{
    const auto active=activeFloater();
    if (!active) return false;
    const auto rect=mTree.screenRect(active,error);
    if (!rect || x<rect->left || x>=rect->right || y<rect->bottom || y>=rect->top) return false;
    return mTree.routeWheel(active,x,y,clicks,false,error);
}