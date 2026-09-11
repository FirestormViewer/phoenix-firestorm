#include "llvkloginui.h"
#include "llstring.h"
#include "llvkxmllayers.h"
#include <expat/expat.h>
#include <fstream>
#include <algorithm>
#include <cmath>

namespace
{
    std::string readText(const std::filesystem::path& path)
    {
        std::ifstream file(path,std::ios::binary|std::ios::ate);
        if (!file || file.tellg() < 0 || file.tellg() > 4*1024*1024) return {};
        std::string text(static_cast<std::size_t>(file.tellg()),'\0');
        file.seekg(0);
        return file.read(text.data(),text.size()) ? text : std::string();
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
        std::vector<std::pair<std::string,std::string>>& pages;
        std::vector<std::string>& pageNames;
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
                if (std::string_view(tag) == "panel" && !state.stack.empty() && state.stack.back() == "tab_container")
                {
                    std::string label, name;
                    for (std::size_t index=0; attributes[index]; index+=2)
                    {
                        if (std::string_view(attributes[index]) == "label") label=attributes[index+1];
                        if (std::string_view(attributes[index]) == "name") name=attributes[index+1];
                    }
                    state.pages.emplace_back(label,"");
                    state.pageNames.push_back(name);
                }
                state.stack.emplace_back(tag);
                if (std::string_view(tag) == "string" ||
                    ((std::string_view(tag) == "text" || std::string_view(tag) == "text_editor") && !state.pages.empty()))
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
                    if (state.stack.back() == "string" || state.name == "support_intro" || state.name == "linden_intro")
                        state.strings.insert_or_assign(state.name,state.body);
                    else if (state.name != "support_editor" && state.name != "contrib_names" && !state.body.empty())
                        state.pages.back().second += state.body+"\n\n";
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
    Parser state{XML_ParserCreate(nullptr),mAboutPages,mAboutPageNames,mAboutStrings};
    if (!state.parser) { error="Native About parser allocation failed"; return false; }
    XML_SetUserData(state.parser,&state); XML_SetElementHandler(state.parser,Parser::start,Parser::end);
    XML_SetCharacterDataHandler(state.parser,Parser::text); XML_SetStartDoctypeDeclHandler(state.parser,Parser::doctype);
    const auto parsed=XML_Parse(state.parser,merged->data(),static_cast<int>(merged->size()),XML_TRUE);
    XML_ParserFree(state.parser);
    if (parsed != XML_STATUS_OK || state.failed || mAboutPages.empty()) { error="Invalid native About declaration"; return false; }
    const auto strings=mSkin->read("xui","strings.xml",LLVKSkinFiles::Policy::Current,error);
    if (!strings) return false;
    layers.clear();
    for (const auto& document : *strings) layers.push_back(document);
    const auto mergedStrings=LLVKXmlLayers::merge(layers,error);
    if (!mergedStrings) return false;
    Parser stringState{XML_ParserCreate(nullptr),mAboutPages,mAboutPageNames,mAboutStrings};
    if (!stringState.parser) { error="Native About strings parser allocation failed"; return false; }
    XML_SetUserData(stringState.parser,&stringState); XML_SetElementHandler(stringState.parser,Parser::start,Parser::end);
    XML_SetCharacterDataHandler(stringState.parser,Parser::text); XML_SetStartDoctypeDeclHandler(stringState.parser,Parser::doctype);
    const auto stringsParsed=XML_Parse(stringState.parser,mergedStrings->data(),static_cast<int>(mergedStrings->size()),XML_TRUE);
    XML_ParserFree(stringState.parser);
    if (stringsParsed != XML_STATUS_OK || stringState.failed) { error="Invalid native About strings"; return false; }
    const auto app = configuration.skin.skinBaseDirectory.parent_path()/"app_settings";
    for (std::size_t index=0; index<mAboutPages.size(); ++index)
    {
        auto& text=mAboutPages[index].second;
        if (mAboutPageNames[index] == "credits_panel")
        {
            auto contributors=readText(app/"contributors.txt");
            const auto newline=contributors.find_first_of("\r\n");
            if (newline != std::string::npos) contributors.resize(newline);
            text += contributors;
        }
        if (mAboutPageNames[index] == "licenses_panel")
            if (const auto packages=readText(app/"packages-info.txt"); !packages.empty()) text=packages;
    }
    mAboutInfo = "Vulkanstorm\nRenderer: native Vulkan\nNot connected\n";
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
            if (!mPreferencesAccepted) for (const auto& [name,value] : mPreferenceSnapshot) mTree.updateSetting(name,value);
            mPreferenceSnapshot.clear();
        });
    }
    if (!mPreferences->visible())
    {
        mPreferencesAccepted=false;
        const auto backend=mTree.setting("RenderBackend").value_or(LLSD("Vulkan"));
        if (!mTree.updateSetting("RenderBackendPending",backend)) mTree.defineSetting("RenderBackendPending",backend,LLVKWidgetTree::SettingType::String);
        for (const auto name : {"FSRememberUsername","RememberPassword","RenderBackendPending"})
            if (const auto value=mTree.setting(name)) mPreferenceSnapshot[name]=*value;
        mTree.setValue(find("native_renderer_choice"),backend);
    }
    mActiveFloater=mPreferences.get();
    return mPreferences->open(error);
}

bool LLVKLoginUi::applyPreferences(std::string& error)
{
    error.clear();
    if (!mPreferences || !mPreferences->visible()) return false;
    std::map<std::string,LLSD> changed;
    for (const auto& [name,old] : mPreferenceSnapshot)
    {
        const auto value=mTree.setting(name);
        if (value && value->asString() != old.asString()) changed[name == "RenderBackendPending" ? "RenderBackend" : name]=*value;
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
        auto font=mFonts->resolve({"SansSerif","Small"},error);
        if (!font) return false;
        mAbout=LLVKFloater::create(mTree,*mDialogFactory,mRoot,"floater_about","About Vulkanstorm",515,600,font,error);
        if (!mAbout) return false;
        int left=7;
        for (std::size_t page=0; page<mAboutPages.size(); ++page)
        {
            const auto& label=mAboutPages[page].first;
            LLVKButton::Params button; const auto wide=utf8str_to_wstring(label); button.label={wide.begin(),wide.end()};
            const auto measured=font->measureRun(button.label,0,button.label.size(),1.f,true,false,error);
            if (!measured) { mAbout.reset(); return false; }
            const auto width=static_cast<int>(std::ceil(measured->width))+20;
            LLVKWidgetTree::Params view; view.name="about_tab_"+std::to_string(page); view.rect={left,545,left+width,570};
            LLVKControl::Params control; control.font=font;
            button.labelAlign=LLVKButton::Align::Center;
            button.images.unselected=mTree.findImage("PushButton_Off"); button.images.selected=mTree.findImage("PushButton_Selected");
            button.images.pressed=mTree.findImage("PushButton_Press"); button.pressedProvided=true; button.toggle=false;
            control.commit.function=[this,page](auto,const LLSD&) { selectAboutPage(page,mDialogError); };
            const auto tab=mTree.createButton(view,control,button,mAbout->id(),error);
            if (!tab) { mAbout.reset(); return false; }
            mAboutTabs.push_back(*tab); left+=width+1;
        }
        const auto scroll=mDialogFactory->construct(mTree,
            "<scroll_container name='about_scroll' left='14' bottom='55' width='487' height='475' reserve_scroll_corner='true'>"
            "<panel name='about_document' width='470' height='475' font='SansSerifSmall'/></scroll_container>",mAbout->id(),error);
        if (!scroll) { mAbout.reset(); return false; }
        mAboutScrollContainer=*scroll;
        mAboutDocument=mTree.get(*scroll)->scrollContainer->document;
        LLVKWidgetTree::Params view; view.name="native_about_body"; view.rect={0,0,470,475}; view.mouseOpaque=true;
        LLVKControl::Params control; control.font=font; control.tabStop=false;
        LLVKPlainControl::Params text; text.maximumBytes=65536; text.layout.wrap=true; text.readOnly=true;
        text.parseWebLinks=true;
        text.selectable=true;
        if (const auto color=mColors->find("TextSelectedColor")) text.selectionColor=*color;
        if (const auto color=mColors->find("TextSelectedBgColor")) text.selectionBackground=*color;
        if (const auto color=mColors->find("HTMLLinkColor")) text.linkColor=*color;
        if (const auto color=mColors->find("UriQueryPartColor")) text.queryColor=*color;
        text.linkClicked=[this](auto,const std::string& url)
        { if (mOpenUrl) mOpenUrl(url); else mDialogError="Native web link service is not bound"; };
        const auto body=mTree.createPlainText(view,control,text,mAboutDocument,error);
        if (!body) { mAbout.reset(); return false; }
        mAboutBody=*body;
        view.name="about_intro";
        view.rect={19,515,453,545};
        text.selectable=false;
        const auto intro=mTree.createPlainText(view,control,text,mAbout->id(),error);
        if (!intro) { mAbout.reset(); return false; }
        mAboutIntro=*intro;
        LLVKWidgetTree::Events linkEvents;
        linkEvents.cursor=[this](auto,bool hand) { if (mPointerCursor) mPointerCursor(hand); };
        mTree.setEvents(mAboutBody,linkEvents);
        mTree.setEvents(mAboutIntro,std::move(linkEvents));
        const auto copy=mDialogFactory->construct(mTree,"<button name='about_copy' label='Copy to Clipboard' left='12' bottom='12' width='180' height='25'/>",mAbout->id(),error);
        if (!copy) { mAbout.reset(); return false; }
        mAboutCopy=*copy;
        LLVKControl::Callback callback;
        callback.function=[this](auto,const LLSD&)
        {
            if (!mDialogClipboard) { mDialogError="Native clipboard unavailable"; return; }
            const auto parsed=LLVKWebText::parse(mAboutInfo,mDialogError);
            if (parsed) mDialogClipboard->write(parsed->text,false,mDialogError);
        };
        mTree.setControlCommit(*copy,std::move(callback));
    }
    mActiveFloater=mAbout.get();
    if (!mAbout->open(error)) return false;
    return selectAboutPage(mAboutPage,error);
}

bool LLVKLoginUi::selectAboutPage(std::size_t page,std::string& error)
{
    if (page>=mAboutPages.size()) return false;
    mAboutPage=page;
    const bool support=mAboutPageNames[page]=="support_panel";
    const bool credits=mAboutPageNames[page]=="credits_panel";
    mTree.setVisible(mAboutCopy,support);
    mTree.setVisible(mAboutIntro,support || credits);
    if (support || credits)
    {
        if (!mTree.setPlainText(mAboutIntro,mAboutStrings[support ? "support_intro" : "linden_intro"],error) ||
            !mTree.setShape(mAboutIntro,support ? LLVKWidgetTree::Rect{19,515,453,545} : LLVKWidgetTree::Rect{12,475,501,545},error)) return false;
    }
    if (!mTree.setShape(mAboutScrollContainer,support ? LLVKWidgetTree::Rect{13,42,502,510} :
        credits ? LLVKWidgetTree::Rect{12,12,501,470} : LLVKWidgetTree::Rect{14,12,501,530},error)) return false;
    for (std::size_t index=0; index<mAboutTabs.size(); ++index) mTree.setButtonToggle(mAboutTabs[index],index==page,error);
    if (!updateAboutText(error)) return false;
    const auto vertical=mTree.get(mAboutScrollContainer)->scrollContainer->vertical;
    mTree.setScrollPosition(vertical,0,true,error);
    return error.empty() && mTree.updateScrollContainer(mAboutScrollContainer,error);
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
    const auto source=mAboutPageNames[mAboutPage]=="support_panel" ? mAboutInfo : mAboutPages[mAboutPage].second;
    const auto parsed=LLVKWebText::parse(source,error);
    if (!parsed) return false;
    const auto& text=parsed->text;
    LLVKPlainTextLayout::Options options; options.width=470; options.wrap=true;
    const auto* body=mTree.get(mAboutBody); if (!body) return false;
    const auto lines=LLVKPlainTextLayout::plain(text,*body->control->params.font,options,error);
    if (!lines) return false;
    const auto scrollRect=mTree.get(mAboutScrollContainer)->params.rect;
    const auto height=std::max(scrollRect.top-scrollRect.bottom,-lines->back().bottom);
    const auto vertical=mTree.get(mAboutScrollContainer)->scrollContainer->vertical;
    const auto position=mTree.get(vertical)->scrollbar->position;
    if (!mTree.setPlainText(mAboutBody,source,error) || !mTree.setShape(mAboutBody,{0,0,470,height},error) ||
        !mTree.reshape(mAboutDocument,470,height,error)) return false;
    mTree.setScrollPosition(vertical,position,true,error);
    if (!error.empty()) return false;
    return mTree.updateScrollContainer(mAboutScrollContainer,error);
}
LLVKWidgetTree::Id LLVKLoginUi::activeFloater() const
{
    for (const auto child : mTree.get(mRoot)->children)
        for (const auto* floater : {mPreferences.get(),mAbout.get()})
            if (floater && floater->id() == child && floater->visible()) return child;
    return 0;
}
bool LLVKLoginUi::pointOverFloater(int x,int y) const
{
    for (const auto* floater : {mPreferences.get(),mAbout.get()})
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
    for (auto* floater : {mPreferences.get(),mAbout.get()})
        if (floater && floater->id()==active) return floater->close(error);
    return false;
}
bool LLVKLoginUi::floaterPointer(const LLVKWidgetTree::PointerEvent& event,std::string& error)
{
    if (mTree.mouseCapture())
    {
        for (auto* floater : {mPreferences.get(),mAbout.get()})
            if (floater && floater->id()==mTree.mouseCapture()) return floater->pointer(event,error);
        return false;
    }
    if (event.kind == LLVKWidgetTree::PointerKind::LeftDown)
        for (const auto child : mTree.get(mRoot)->children)
        {
            auto* floater=mPreferences && mPreferences->id()==child ? mPreferences.get() : mAbout && mAbout->id()==child ? mAbout.get() : nullptr;
            if (!floater || !floater->visible()) continue;
            const auto rect=mTree.screenRect(floater->id(),error);
            if (rect && event.x>=rect->left && event.x<rect->right && event.y>=rect->bottom && event.y<rect->top)
            { mActiveFloater=floater; floater->open(error); return floater->pointer(event,error); }
        }
    return false;
}
bool LLVKLoginUi::floaterWheel(int x,int y,int clicks,std::string& error)
{
    if (!mAbout || activeFloater()!=mAbout->id()) return false;
    const auto rect=mTree.screenRect(mAboutScrollContainer,error);
    if (!rect || x<rect->left || x>=rect->right || y<rect->bottom || y>=rect->top) return false;
    return mTree.routeWheel(mAboutScrollContainer,x,y,clicks,false,error);
}