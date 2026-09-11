#include "llvkloginui.h"
#include "llstring.h"
#include "llsdutil.h"
#include "llvkxmllayers.h"
#include <expat/expat.h>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <boost/algorithm/string.hpp>

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
    if (!mAutoReplaceSettings.set(configuration.autoReplaceLists))
    { error="Invalid native AutoReplace settings"; return false; }
    mSaveAutoReplace=configuration.saveAutoReplace;
    mSpelling=std::make_unique<LLVKSpellCheck>(configuration.dictionaryDirectory.empty() ?
        configuration.skin.skinBaseDirectory.parent_path()/"app_settings"/"dictionaries" : configuration.dictionaryDirectory,
        configuration.skin.userAppDirectory/"user_settings"/"dictionaries");
    if (!mSpelling->refresh(error) || !updateSpelling(error)) return false;
    for (const auto setting : {"SpellCheck","SpellCheckDictionary"})
        if (mTree.setting(setting)) mTree.subscribeSetting(setting,[this,enabled=std::string_view(setting)=="SpellCheck"](const LLSD&,const LLSD&)
        {
            if (!updateSpelling(mDialogError)) return;
            if (mSpellCheck && mSpellCheck->visible()) refreshSpellCheck(!enabled && !mSpellMainChanged,mDialogError);
        });
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
        std::map<std::string,Notice>* notices = nullptr;
        std::optional<Notice> notice;
        int noticeDepth = 0;
        static void XMLCALL start(void* pointer,const char* tag,const char** attributes)
        {
            auto& state = *static_cast<Parser*>(pointer);
            try
            {
                if (state.stack.size() >= 64) throw std::runtime_error("About depth");
                state.stack.emplace_back(tag);
                if (state.notices && std::string_view(tag)=="notification")
                {
                    std::string name;
                    for (std::size_t index=0; attributes[index]; index+=2)
                        if (std::string_view(attributes[index])=="name") name=attributes[index+1];
                    if (name=="AddAutoReplaceList" || name=="RenameAutoReplaceList" || name=="RemoveAutoReplaceList" ||
                        name=="InvalidAutoReplaceList" || name=="InvalidAutoReplaceEntry" || name=="SpellingDictImportRequired" ||
                        name=="SpellingDictIsSecondary" || name=="SpellingDictImportFailed")
                    { state.notice=Notice{name,{}}; state.noticeDepth=static_cast<int>(state.stack.size()); }
                }
                if (state.notice && state.stack.size()==state.noticeDepth+2 && state.stack[state.stack.size()-2]=="form")
                {
                    std::map<std::string,std::string> fields;
                    for (std::size_t index=0; attributes[index]; index+=2) fields.emplace(attributes[index],attributes[index+1]);
                    if (std::string_view(tag)=="button")
                        state.notice->buttons.push_back({fields["name"],fields["text"],std::stoi(fields["index"]),fields["default"]=="true"});
                    else if (std::string_view(tag)=="input" && fields["type"]=="text") state.notice->inputName=fields["name"];
                    else throw std::runtime_error("Unsupported native AutoReplace notification field");
                }
                if (std::string_view(tag) == "string" || std::string_view(tag) == "global")
                {
                    state.textDepth = static_cast<int>(state.stack.size()); state.body.clear(); state.name.clear();
                    for (std::size_t index=0; attributes[index]; index+=2) if (std::string_view(attributes[index]) == "name") state.name=attributes[index+1];
                }
            }
            catch (...) { state.failed=true; XML_StopParser(state.parser,XML_FALSE); }
        }
        static void XMLCALL text(void* pointer,const char* text,int length)
        {
            auto& state=*static_cast<Parser*>(pointer);
            try
            {
                if (state.textDepth) state.body.append(text,length);
                if (state.notice && state.stack.size()==state.noticeDepth) state.notice->message.append(text,length);
            }
            catch (...) { state.failed=true; XML_StopParser(state.parser,XML_FALSE); }
        }
        static void XMLCALL end(void* pointer,const char*)
        {
            auto& state=*static_cast<Parser*>(pointer);
            try
            {
                if (state.notice && state.stack.size()==state.noticeDepth)
                {
                    LLStringUtil::trim(state.notice->message);
                    state.notices->insert_or_assign(state.notice->name,*state.notice);
                    state.notice.reset(); state.noticeDepth=0;
                }
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
    notificationState.notices=&mNoticeTemplates;
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

bool LLVKLoginUi::queueNotice(const std::string& name,const LLSD& arguments,
    std::function<void(int,const LLSD&)> response,std::string& error)
{
    error.clear();
    const auto found=mNoticeTemplates.find(name);
    if (found==mNoticeTemplates.end()) { error="Native notification template is unavailable: "+name; return false; }
    auto notice=found->second;
    LLStringUtil::format_map_t substitutions;
    for (auto entry=arguments.beginMap(); entry!=arguments.endMap(); ++entry)
        substitutions["["+entry->first+"]"]=entry->second.asString();
    LLStringUtil::format(notice.message,substitutions);
    for (auto& button : notice.buttons) LLStringUtil::format(button.label,substitutions);
    notice.response=std::move(response);
    mNotices.push_back(std::move(notice));
    return true;
}

bool LLVKLoginUi::advanceNotices(double time,std::string& error)
{
    error.clear();
    if (!std::isfinite(time) || time<mNoticeTime) { error="Invalid native notice clock"; return false; }
    mNoticeTime=time;
    if (mNoticePanel || mNotices.empty()) return true;
    auto notice=mNotices.front();
    if (notice.buttons.empty()) notice.buttons.push_back({"close",mAboutStrings.at("implicitclosebutton"),0,true});
    const auto font=mFonts->resolve({"SansSerif","Medium"},error);
    if (!font) return false;
    const auto wide=utf8str_to_wstring(mNotices.front().message);
    LLVKPlainTextLayout::Options options; options.width=400; options.wrap=true;
    const auto document=LLVKPlainTextLayout::document(std::u32string(wide.begin(),wide.end()),*font,options,0,0,LLVKFont::VerticalAlign::Top,error);
    if (!document) return false;
    const auto padding=font->measureRun(U"OO",0,2,1.f,true,false,error);
    if (!padding) return false;
    int buttonWidth=0;
    for (const auto& button : notice.buttons)
    {
        const auto wideLabel=utf8str_to_wstring(button.label);
        const std::u32string label(wideLabel.begin(),wideLabel.end());
        const auto measured=font->measureRun(label,0,label.size(),1.f,true,false,error);
        if (!measured) return false;
        buttonWidth=std::max(buttonWidth,static_cast<int>(measured->width+0.99f)+static_cast<int>(padding->width)+20);
    }
    const auto totalButtons=buttonWidth*static_cast<int>(notice.buttons.size())+10*static_cast<int>(notice.buttons.size()-1);
    const auto textWidth=std::min(400,document->fitWidth+25);
    const auto textHeight=document->fitHeight;
    const auto width=std::max(totalButtons,textWidth)+50, height=textHeight+48+23+(notice.inputName.empty() ? 0 : 36);
    const auto root=mTree.get(mRoot)->params.rect;
    const auto left=std::max(0,(root.right-root.left-width)/2),bottom=std::max(0,(root.top-root.bottom-height)/2);
    LLVKWidgetTree::Params view; view.name=notice.name; view.rect={left,bottom,left+width,bottom+height};
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
    text.parseWebLinks=true;
    text.linkClicked=[this](auto,const std::string& url) { if (mOpenUrl) mOpenUrl(url); };
    if (const auto color=mColors->find("HTMLLinkColor")) text.linkColor=*color;
    if (const auto color=mColors->find("LabelTextColor")) text.textColor=text.readOnlyColor=*color;
    if (!mTree.createPlainText(view,control,text,*panel,error)) { discard(); return false; }
    std::map<LLVKWidgetTree::Id,int> optionsById;
    LLVKWidgetTree::Id defaultButton=0,editor=0;
    int buttonLeft=(width-totalButtons)/2;
    for (const auto& option : notice.buttons)
    {
        const auto button=mDialogFactory->constructFile(mTree,"alert_button.xml",*panel,error);
        if (!button) { discard(); return false; }
        if (!mTree.setShape(*button,{buttonLeft,16,buttonLeft+buttonWidth,39},error)) { discard(); return false; }
        const auto wideLabel=utf8str_to_wstring(option.label);
        mTree.setButtonLabel(*button,std::u32string(wideLabel.begin(),wideLabel.end()));
        LLVKControl::Callback close;
        close.function=[this,index=option.option](auto,const LLSD&) { respondNotice(index,mDialogError); };
        mTree.setControlCommit(*button,std::move(close));
        optionsById.emplace(*button,option.option);
        if (!defaultButton || option.isDefault) defaultButton=*button;
        buttonLeft+=buttonWidth+10;
    }
    if (!notice.inputName.empty())
    {
        const auto inputFactory=std::make_unique<LLVKWidgetFactory>(*mDialogFactory);
        if (!inputFactory->loadDefaultsFile(mTree,"alert_line_editor.xml",error)) { discard(); return false; }
        const auto input=inputFactory->construct(mTree,"<line_editor name='notification_input' width='200' height='20' max_length_bytes='1023'/>",*panel,error);
        if (!input || !mTree.setShape(*input,{25,47,width-25,67},error)) { discard(); return false; }
        editor=*input;
    }
    mNoticePreviousFocus=mTree.keyboardFocus();
    if (mTree.mouseCapture() && !mTree.setMouseCapture(0,error)) { discard(); return false; }
    if (mTree.topControl() && !mTree.setTopControl(0,error)) { discard(); return false; }
    mMenu->dismiss();
    if (!mTree.setKeyboardFocus(*panel,true,false,error) || !mTree.requestControlFocus(editor ? editor : defaultButton,true,error))
    { mTree.unlockFocus(); discard(); return false; }
    mNoticePanel=*panel; mNoticeButton=defaultButton; mNoticeEditor=editor; mNoticeOpened=time;
    mNoticeOptions=std::move(optionsById); mActiveNotice=std::move(notice);
    mNotices.erase(mNotices.begin());
    return true;
}

bool LLVKLoginUi::respondNotice(int option,std::string& error)
{
    error.clear();
    if (!mActiveNotice || !mNoticePanel) return false;
    const auto found=std::find_if(mNoticeOptions.begin(),mNoticeOptions.end(),[option](const auto& item) { return item.second==option; });
    if (found==mNoticeOptions.end()) { error="Invalid native notice response"; return false; }
    if (found->first==mNoticeButton && mNoticeTime-mNoticeOpened<0.5) return true;
    LLSD values=LLSD::emptyMap();
    if (mNoticeEditor) values[mActiveNotice->inputName]=mTree.value(mNoticeEditor);
    const auto response=mActiveNotice->response;
    if (!dismissNotice(error)) return false;
    if (response) response(option,values);
    return true;
}

bool LLVKLoginUi::dismissNotice(std::string& error)
{
    error.clear();
    if (!mNoticePanel) return false;
    const auto panel=mNoticePanel,focus=mNoticePreviousFocus;
    mTree.unlockFocus();
    mNoticePanel=mNoticeButton=mNoticePreviousFocus=0;
    mNoticeEditor=0; mNoticeOptions.clear(); mActiveNotice.reset();
    if (!mTree.erase(panel,error)) return false;
    return !mTree.get(focus) || mTree.requestControlFocus(focus,true,error);
}

bool LLVKLoginUi::noticeKey(bool returnKey,bool modified,std::string& error)
{
    if (!mNoticePanel) return false;
    if (returnKey && !modified && mNoticeTime-mNoticeOpened>=0.5)
    {
        const auto focused=mNoticeOptions.find(mTree.keyboardFocus());
        respondNotice(focused==mNoticeOptions.end() ? mNoticeOptions.at(mNoticeButton) : focused->second,error);
    }
    return true;
}

std::optional<LLVKWidgetTree::Id> LLVKLoginUi::constructPreferencePanel(const std::string& filename,
    LLVKWidgetTree::Id parent,std::string& error)
{
    error.clear();
    if (filename!="panel_preferences_colors.xml" && filename!="panel_preferences_general.xml" && filename!="panel_preferences_chat.xml")
    { error="Native Preferences panel application policy is not implemented: "+filename; return std::nullopt; }
    return mDialogFactory->construct(mTree,"<panel class='panel_preference' filename='"+filename+"'/>",parent,error);
}

bool LLVKLoginUi::initializeStartupPreferencePanel(LLVKWidgetTree::Id panel,std::string& error)
{
    error.clear();
    const auto fields=preferenceFields(mTree,panel);
    if (fields.contains("send_im_to_email"))
    {
        if (!fields.contains("email_settings") || !fields.contains("email_settings_login_to_change"))
        { error="Native Chat Preferences email controls are incomplete"; return false; }
        for (const auto name : {"send_im_to_email","email_settings"})
        { mTree.setEnabled(fields.at(name),false); mTree.setVisible(fields.at(name),false); }
        mTree.setVisible(fields.at("email_settings_login_to_change"),true);
    }
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

void LLVKLoginUi::enableAutoReplaceEntry(bool enabled)
{
    for (const auto name : {"autoreplace_keyword","autoreplace_replacement","autoreplace_save_entry","autoreplace_delete_entry"})
        mTree.setEnabled(mAutoReplaceFields.at(name),enabled);
    if (!enabled)
    {
        mAutoReplaceKeyword.clear();
        mTree.setValue(mAutoReplaceFields.at("autoreplace_keyword"),LLSD(""));
        mTree.setValue(mAutoReplaceFields.at("autoreplace_replacement"),LLSD(""));
    }
}

bool LLVKLoginUi::refreshAutoReplace(bool entries,std::string& error)
{
    std::vector<LLVKWidgetTree::ListRow> names;
    const auto& lists=mAutoReplaceDraft.lists();
    for (auto list=lists.beginArray(); list!=lists.endArray(); ++list)
        if (list->isMap()) names.push_back({(*list)["name"],{(*list)["name"].asString()}});
    const auto namesId=mAutoReplaceFields.at("autoreplace_list_name");
    if (!mTree.setScrollListRows(namesId,names,error)) return false;
    if (!mAutoReplaceList.empty() && !mTree.selectScrollListValue(namesId,LLSD(mAutoReplaceList),true,error)) return false;
    const auto* selected=mAutoReplaceDraft.find(mAutoReplaceList);
    for (const auto name : {"autoreplace_export_list","autoreplace_delete_list"}) mTree.setEnabled(mAutoReplaceFields.at(name),selected!=nullptr);
    mTree.setEnabled(mAutoReplaceFields.at("autoreplace_list_up"),selected && !names.empty() && names.front().value.asString()!=mAutoReplaceList);
    mTree.setEnabled(mAutoReplaceFields.at("autoreplace_list_down"),selected && !names.empty() && names.back().value.asString()!=mAutoReplaceList);
    if (entries)
    {
        std::vector<LLVKWidgetTree::ListRow> rows;
        if (selected)
            for (auto entry=(*selected)["replacements"].beginMap(); entry!=(*selected)["replacements"].endMap(); ++entry)
                rows.push_back({LLSD(entry->first),{entry->first,entry->second.asString()}});
        const auto replacements=mAutoReplaceFields.at("autoreplace_list_replacements");
        if (!mTree.setScrollListRows(replacements,std::move(rows),error)) return false;
        mTree.setEnabled(replacements,selected!=nullptr);
        mTree.setEnabled(mAutoReplaceFields.at("autoreplace_add_entry"),selected!=nullptr);
        enableAutoReplaceEntry(false);
    }
    return true;
}

void LLVKLoginUi::promptAutoReplaceList(LLSD list,bool conflict)
{
    LLSD arguments;
    arguments["DUPNAME"]=list["name"];
    queueNotice(conflict ? "RenameAutoReplaceList" : "AddAutoReplaceList",arguments,
        [this,list,generation=mAutoReplaceGeneration,conflict](int option,const LLSD& response) mutable
    {
        if (generation!=mAutoReplaceGeneration || !mAutoReplace || !mAutoReplace->visible()) return;
        if (option!=1 && !(conflict && option==0)) return;
        if (option==1) list["name"]=response["listname"];
        const auto result=mAutoReplaceDraft.add(list,conflict && option==0);
        if (result==LLVKAutoReplaceSettings::AddResult::DuplicateName) { promptAutoReplaceList(list,true); return; }
        if (result==LLVKAutoReplaceSettings::AddResult::InvalidList)
        { queueNotice("InvalidAutoReplaceList",{}, {},mDialogError); mAutoReplaceList.clear(); }
        else mAutoReplaceList=list["name"].asString();
        refreshAutoReplace(true,mDialogError);
    },mDialogError);
}

void LLVKLoginUi::chooseAutoReplaceFile(bool save)
{
    if (!mTree.setting("LocalFileSystemBrowsingEnabled").value_or(LLSD(true)).asBoolean()) return;
    if (!mXmlFilePicker) { mDialogError="Native XML file picker is unavailable"; return; }
    LLSD exported;
    if (save)
    {
        const auto* list=mAutoReplaceDraft.find(mAutoReplaceList);
        if (!list) return;
        exported=*list;
    }
    mXmlFilePicker(save,save ? mAutoReplaceList+".xml" : std::string(),
        [this,save,exported,generation=mAutoReplaceGeneration](std::optional<std::filesystem::path> path,std::string error)
    {
        if (generation!=mAutoReplaceGeneration || !mAutoReplace || !mAutoReplace->visible()) return;
        if (!error.empty()) { mDialogError=std::move(error); return; }
        if (!path) return;
        if (save) { LLVKAutoReplaceSettings::writeListFile(*path,exported,mDialogError); return; }
        const auto list=LLVKAutoReplaceSettings::readListFile(*path,mDialogError);
        const auto result=list ? mAutoReplaceDraft.add(*list) : LLVKAutoReplaceSettings::AddResult::InvalidList;
        if (result==LLVKAutoReplaceSettings::AddResult::DuplicateName) { promptAutoReplaceList(*list,true); return; }
        if (result==LLVKAutoReplaceSettings::AddResult::InvalidList)
        { queueNotice("InvalidAutoReplaceList",{}, {},mDialogError); mAutoReplaceList.clear(); }
        else mAutoReplaceList=(*list)["name"].asString();
        refreshAutoReplace(true,mDialogError);
    },mDialogError);
}

bool LLVKLoginUi::showAutoReplace(std::string& error)
{
    error.clear();
    if (!mAutoReplace)
    {
        mAutoReplace=LLVKFloater::createFile(mTree,*mDialogFactory,mRoot,"floater_autoreplace.xml",error);
        if (!mAutoReplace) return false;
        mAutoReplaceFields=preferenceFields(mTree,mAutoReplace->id());
        for (const auto name : {"autoreplace_enable","autoreplace_import_list","autoreplace_export_list","autoreplace_new_list",
            "autoreplace_delete_list","autoreplace_list_name","autoreplace_list_up","autoreplace_list_down","autoreplace_add_entry",
            "autoreplace_delete_entry","autoreplace_keyword","autoreplace_replacement","autoreplace_save_entry",
            "autoreplace_save_changes","autoreplace_cancel","autoreplace_list_replacements"})
            if (!mAutoReplaceFields.contains(name)) { error=std::string("Missing native AutoReplace field: ")+name; mAutoReplace.reset(); return false; }
        const auto bind=[&](const char* name,std::function<void(LLVKWidgetTree::Id,const LLSD&)> function)
        { LLVKControl::Callback callback; callback.function=std::move(function); mTree.setControlCommit(mAutoReplaceFields.at(name),std::move(callback)); };
        bind("autoreplace_import_list",[this](auto,const LLSD&) { chooseAutoReplaceFile(false); });
        bind("autoreplace_export_list",[this](auto,const LLSD&) { chooseAutoReplaceFile(true); });
        mTree.setScrollListCommitOnSelection(mAutoReplaceFields.at("autoreplace_list_name"),true);
        mTree.setScrollListCommitOnSelection(mAutoReplaceFields.at("autoreplace_list_replacements"),true);
        bind("autoreplace_list_name",[this](auto id,const LLSD&)
        {
            const auto name=mTree.value(id).asString();
            if (name==mAutoReplaceList) return;
            mAutoReplaceList=name; refreshAutoReplace(true,mDialogError);
        });
        bind("autoreplace_list_replacements",[this](auto id,const LLSD&)
        {
            const auto keyword=mTree.value(id);
            const auto* list=mAutoReplaceDraft.find(mAutoReplaceList);
            if (!keyword.isDefined() || !list) { enableAutoReplaceEntry(false); return; }
            mAutoReplaceKeyword=keyword.asString(); enableAutoReplaceEntry(true);
            mTree.setValue(mAutoReplaceFields.at("autoreplace_keyword"),keyword);
            mTree.setValue(mAutoReplaceFields.at("autoreplace_replacement"),(*list)["replacements"][mAutoReplaceKeyword]);
            mTree.requestControlFocus(mAutoReplaceFields.at("autoreplace_replacement"),true,mDialogError);
        });
        for (const auto name : {"autoreplace_list_up","autoreplace_list_down"})
            bind(name,[this,up=std::string_view(name)=="autoreplace_list_up"](auto,const LLSD&)
            { mAutoReplaceDraft.move(mAutoReplaceList,up); refreshAutoReplace(false,mDialogError); });
        bind("autoreplace_add_entry",[this](auto,const LLSD&)
        {
            const auto list=mAutoReplaceFields.at("autoreplace_list_replacements");
            auto rows=mTree.get(list)->scrollList->rows;
            for (auto& row : rows) row.selected=false;
            mTree.setScrollListRows(list,std::move(rows),mDialogError);
            enableAutoReplaceEntry(false); enableAutoReplaceEntry(true);
            mTree.requestControlFocus(mAutoReplaceFields.at("autoreplace_keyword"),true,mDialogError);
        });
        bind("autoreplace_save_entry",[this](auto,const LLSD&)
        {
            if (!mAutoReplaceKeyword.empty()) mAutoReplaceDraft.removeEntry(mAutoReplaceList,mAutoReplaceKeyword);
            if (mAutoReplaceDraft.setEntry(mAutoReplaceList,mTree.value(mAutoReplaceFields.at("autoreplace_keyword")).asString(),
                mTree.value(mAutoReplaceFields.at("autoreplace_replacement")).asString())) refreshAutoReplace(true,mDialogError);
            else queueNotice("InvalidAutoReplaceEntry",{}, {},mDialogError);
        });
        bind("autoreplace_delete_entry",[this](auto,const LLSD&)
        {
            const auto list=mAutoReplaceFields.at("autoreplace_list_replacements");
            mAutoReplaceDraft.removeEntry(mAutoReplaceList,mTree.value(list).asString());
            auto rows=mTree.get(list)->scrollList->rows;
            std::erase_if(rows,[](const auto& row) { return row.selected; });
            mTree.setScrollListRows(list,std::move(rows),mDialogError); enableAutoReplaceEntry(false);
        });
        bind("autoreplace_new_list",[this](auto,const LLSD&)
        { LLSD list; list["name"]="Empty"; list["replacements"]=LLSD::emptyMap(); promptAutoReplaceList(list,false); });
        bind("autoreplace_delete_list",[this](auto,const LLSD&)
        {
            const auto* list=mAutoReplaceDraft.find(mAutoReplaceList);
            if (!list) return;
            const auto remove=[this,name=mAutoReplaceList,generation=mAutoReplaceGeneration](int option,const LLSD&)
            {
                if (option!=1 || generation!=mAutoReplaceGeneration || !mAutoReplace->visible()) return;
                mAutoReplaceDraft.remove(name); mAutoReplaceList.clear(); refreshAutoReplace(true,mDialogError);
            };
            if ((*list)["replacements"].size()==0) remove(1,{});
            else
            {
                LLSD arguments; arguments["LIST_NAME"]=mAutoReplaceList; arguments["MAP_SIZE"]=std::to_string((*list)["replacements"].size());
                queueNotice("RemoveAutoReplaceList",arguments,remove,mDialogError);
            }
        });
        bind("autoreplace_save_changes",[this](auto,const LLSD&)
        {
            if (!mTree.setting("AutoReplace")) { mDialogError="Missing native AutoReplace enable setting"; return; }
            if (!mSaveAutoReplace || !mSaveAutoReplace(mAutoReplaceDraft.lists(),mDialogError))
            { if (mDialogError.empty()) mDialogError="Native AutoReplace persistence is unavailable"; return; }
            const auto enabled=mTree.value(mAutoReplaceFields.at("autoreplace_enable"));
            if (enabled.asBoolean()!=mTree.setting("AutoReplace")->asBoolean() &&
                (!mSavePreferences || !mSavePreferences({{"AutoReplace",enabled}},mDialogError)))
            { if (mDialogError.empty()) mDialogError="Native AutoReplace enable preference could not be saved"; return; }
            mAutoReplaceSettings=mAutoReplaceDraft;
            mTree.updateSetting("AutoReplace",enabled);
            mAutoReplace->close(mDialogError);
        });
        bind("autoreplace_cancel",[this](auto,const LLSD&) { mAutoReplace->close(mDialogError); });
        mAutoReplace->onClose([this] { ++mAutoReplaceGeneration; });
    }
    if (!mAutoReplace->visible())
    {
        ++mAutoReplaceGeneration;
        mAutoReplaceDraft=mAutoReplaceSettings; mAutoReplaceList.clear(); mAutoReplaceKeyword.clear();
        mTree.setValue(mAutoReplaceFields.at("autoreplace_enable"),mTree.setting("AutoReplace").value_or(LLSD(false)));
        if (!refreshAutoReplace(true,error)) return false;
    }
    mActiveFloater=mAutoReplace.get();
    return mAutoReplace->open(error);
}

bool LLVKLoginUi::updateSpelling(std::string& error)
{
    std::vector<std::string> dictionaries;
    const auto setting=mTree.setting("SpellCheckDictionary").value_or(LLSD("")).asString();
    boost::split(dictionaries,setting,boost::is_any_of(","));
    const auto primary=dictionaries.front(); dictionaries.erase(dictionaries.begin());
    return mSpelling->activate(mTree.setting("SpellCheck").value_or(LLSD(false)).asBoolean() ? primary : "",dictionaries,error);
}

void LLVKLoginUi::updateSpellRemoval()
{
    if (!mSpellCheck || !mSpellCheck->visible()) return;
    const auto* list=mTree.get(mSpellFields.at("spellcheck_available_list"));
    if (!list || !list->scrollList) return;
    bool selected=false,allowed=true;
    for (const auto& row : list->scrollList->rows)
        if (row.selected) { selected=true; allowed&=mSpelling->canRemove(row.value.asString()); }
    mTree.setEnabled(mSpellFields.at("spellcheck_remove_btn"),selected && allowed);
}

bool LLVKLoginUi::commitSpellCheck(std::string& error)
{
    error.clear();
    const auto primary=mTree.value(mSpellFields.at("spellcheck_main_combo")).asString();
    std::string setting=primary;
    if (!primary.empty())
        for (const auto& row : mTree.get(mSpellFields.at("spellcheck_active_list"))->scrollList->rows)
        {
            const auto* dictionary=mSpelling->dictionary(row.value.asString());
            if (dictionary && (*dictionary)["installed"].asBoolean()) setting+=","+row.value.asString();
        }
    if (!mTree.updateSetting("SpellCheckDictionary",LLSD(setting)))
    { error="Native spelling dictionary setting update failed"; return false; }
    return true;
}

bool LLVKLoginUi::refreshSpellCheck(bool fromSettings,std::string& error)
{
    error.clear();
    if (mRefreshingSpelling) return true;
    mRefreshingSpelling=true;
    struct Guard { bool& value; ~Guard() { value=false; } } guard{mRefreshingSpelling};
    const bool enabled=mTree.setting("SpellCheck").value_or(LLSD(false)).asBoolean();
    const auto combo=mSpellFields.at("spellcheck_main_combo");
    const auto active=mSpellFields.at("spellcheck_active_list"),available=mSpellFields.at("spellcheck_available_list");
    std::string primary=mTree.value(combo).asString();
    if ((primary.empty() || fromSettings) && mSpelling->active()) primary=mSpelling->primary();
    std::vector<std::string> secondary;
    if (fromSettings || (mTree.get(active)->scrollList->rows.empty() && mTree.get(available)->scrollList->rows.empty()))
    { if (mSpelling->active()) secondary=mSpelling->secondary(); }
    else
        for (const auto& row : mTree.get(active)->scrollList->rows)
            if (row.value.asString()!=primary) secondary.push_back(row.value.asString());
    std::vector<LLVKWidgetTree::ComboItem> choices;
    const auto& catalog=mSpelling->dictionaries();
    for (auto dictionary=catalog.beginArray(); dictionary!=catalog.endArray(); ++dictionary)
        if ((*dictionary)["installed"].asBoolean() && (*dictionary)["is_primary"].asBoolean() && dictionary->has("language"))
        { const auto language=(*dictionary)["language"].asString(); choices.push_back({language,LLSD(language)}); }
    std::stable_sort(choices.begin(),choices.end(),[](const auto& first,const auto& second)
    { return LLStringUtil::compareDict(first.label,second.label)<0; });
    if (!mTree.replaceComboItems(combo,std::move(choices),error) || !mTree.setComboValue(combo,LLSD(primary),error)) return false;
    std::vector<LLVKWidgetTree::ListRow> activeRows,availableRows;
    const auto label=[&](const LLSD& dictionary,const std::string& language)
    { return language+(dictionary["user_installed"].asBoolean() ? " "+mAboutStrings.at("UserDictionary") : ""); };
    for (const auto& language : secondary)
    {
        const auto* dictionary=mSpelling->dictionary(language);
        activeRows.push_back({LLSD(language),{label(dictionary ? *dictionary : LLSD(),language)}});
    }
    for (auto dictionary=catalog.beginArray(); dictionary!=catalog.endArray(); ++dictionary)
    {
        const auto language=(*dictionary)["language"].asString();
        if ((*dictionary)["installed"].asBoolean() && language!=primary && std::find(secondary.begin(),secondary.end(),language)==secondary.end())
            availableRows.push_back({LLSD(language),{label(*dictionary,language)}});
    }
    if (!mTree.setScrollListRows(active,std::move(activeRows),error) || !mTree.setScrollListRows(available,std::move(availableRows),error)) return false;
    for (const auto name : {"spellcheck_main_combo","spellcheck_active_list","spellcheck_available_list","spellcheck_moveleft_btn","spellcheck_moveright_btn"})
        mTree.setEnabled(mSpellFields.at(name),enabled);
    updateSpellRemoval();
    return commitSpellCheck(error);
}

bool LLVKLoginUi::showSpellCheck(std::string& error)
{
    error.clear();
    if (!mSpellCheck)
    {
        mSpellCheck=LLVKFloater::createFile(mTree,*mDialogFactory,mRoot,"floater_spellcheck.xml",error);
        if (!mSpellCheck) return false;
        mSpellFields=preferenceFields(mTree,mSpellCheck->id());
        for (const auto name : {"spellcheck_main_combo","spellcheck_active_list","spellcheck_available_list","spellcheck_moveleft_btn",
            "spellcheck_moveright_btn","spellcheck_remove_btn","spellcheck_import_btn"})
            if (!mSpellFields.contains(name)) { error="Native Spell Checker declaration is incomplete"; mSpellCheck.reset(); return false; }
        const auto bind=[&](const char* name,std::function<void(LLVKWidgetTree::Id,const LLSD&)> function)
        { LLVKControl::Callback callback; callback.function=std::move(function); mTree.setControlCommit(mSpellFields.at(name),std::move(callback)); };
        bind("spellcheck_main_combo",[this](auto,const LLSD&) { mSpellMainChanged=true; refreshSpellCheck(false,mDialogError); });
        for (const bool toActive : {false,true})
            bind(toActive ? "spellcheck_moveright_btn" : "spellcheck_moveleft_btn",[this,toActive](auto,const LLSD&)
            {
                const auto from=mSpellFields.at(toActive ? "spellcheck_available_list" : "spellcheck_active_list");
                const auto to=mSpellFields.at(toActive ? "spellcheck_active_list" : "spellcheck_available_list");
                auto source=mTree.get(from)->scrollList->rows,target=mTree.get(to)->scrollList->rows;
                for (const auto& row : source) if (row.selected) target.push_back(row);
                std::erase_if(source,[](const auto& row) { return row.selected; });
                if (!mTree.setScrollListRows(from,std::move(source),mDialogError) || !mTree.setScrollListRows(to,std::move(target),mDialogError)) return;
                commitSpellCheck(mDialogError);
            });
        bind("spellcheck_remove_btn",[this](auto,const LLSD&)
        {
            const auto rows=mTree.get(mSpellFields.at("spellcheck_available_list"))->scrollList->rows;
            for (const auto& row : rows) if (row.selected && !mSpelling->remove(row.value.asString(),mDialogError)) return;
            refreshSpellCheck(!mSpellMainChanged,mDialogError);
        });
        bind("spellcheck_import_btn",[this](auto,const LLSD&) { showSpellImport(mDialogError); });
        mSpellCheck->onClose([this]
        {
            if (mSpellImport) mSpellImport->close(mDialogError);
            if (!commitSpellCheck(mDialogError)) return;
            std::map<std::string,LLSD> changed;
            for (const auto& [name,value] : mSpellSnapshot)
                if (const auto current=mTree.setting(name); current && !llsd_equals(*current,value)) changed[name]=*current;
            if (!changed.empty() && (!mSavePreferences || !mSavePreferences(changed,mDialogError)))
                if (mDialogError.empty()) mDialogError="Native spelling preferences could not be saved";
        });
    }
    if (!mSpellCheck->visible())
    {
        mSpellSnapshot.clear();
        for (const auto name : {"SpellCheck","SpellCheckDictionary"}) mSpellSnapshot[name]=mTree.setting(name).value_or(LLSD());
        if (!refreshSpellCheck(true,error)) return false;
    }
    mActiveFloater=mSpellCheck.get();
    return mSpellCheck->open(error);
}

bool LLVKLoginUi::showSpellImport(std::string& error)
{
    error.clear();
    if (!mSpellImport)
    {
        mSpellImport=LLVKFloater::createFile(mTree,*mDialogFactory,mRoot,"floater_spellcheck_import.xml",error);
        if (!mSpellImport) return false;
        mSpellImportFields=preferenceFields(mTree,mSpellImport->id());
        for (const auto name : {"dictionary_path","dictionary_name","dictionary_language","dictionary_path_browse","ok_btn","cancel_btn"})
            if (!mSpellImportFields.contains(name)) { error="Native dictionary import declaration is incomplete"; mSpellImport.reset(); return false; }
        LLVKControl::Callback browse;
        browse.function=[this](auto,const LLSD&)
        {
            if (!mTree.setting("LocalFileSystemBrowsingEnabled").value_or(LLSD(true)).asBoolean()) return;
            if (!mDictionaryFilePicker) { mDialogError="Native dictionary file picker is unavailable"; return; }
            mDictionaryFilePicker(false,"",[this,generation=mSpellImportGeneration](std::optional<std::filesystem::path> path,std::string error)
            {
                if (generation!=mSpellImportGeneration || !mSpellImport->visible()) return;
                if (!error.empty()) { mDialogError=std::move(error); return; }
                if (!path) return;
                path=LLVKSpellCheck::resolveImportPath(*path,mDialogError);
                if (!path) return;
                mSpellImportPath=*path;
                const auto utf8=path->u8string(),base=path->stem().u8string();
                mTree.setValue(mSpellImportFields.at("dictionary_path"),LLSD(std::string(utf8.begin(),utf8.end())));
                mTree.setValue(mSpellImportFields.at("dictionary_name"),LLSD(std::string(base.begin(),base.end())));
            },mDialogError);
        };
        mTree.setControlCommit(mSpellImportFields.at("dictionary_path_browse"),std::move(browse));
        LLVKControl::Callback accepted;
        accepted.function=[this](auto,const LLSD&)
        {
            const auto language=mTree.value(mSpellImportFields.at("dictionary_language")).asString();
            if (mSpellImportPath.empty() || language.find_first_not_of(" \t\r\n")==std::string::npos)
            { queueNotice("SpellingDictImportRequired",{}, {},mDialogError); return; }
            if (!mSpelling->importDictionary(mSpellImportPath,language,mDialogError)) return;
            auto aff=mSpellImportPath; aff.replace_extension(".aff");
            if (!std::filesystem::is_regular_file(aff))
            {
                LLSD arguments; const auto path=mSpellImportPath.u8string(); arguments["DIC_NAME"]=std::string(path.begin(),path.end());
                queueNotice("SpellingDictIsSecondary",arguments,{},mDialogError);
            }
            refreshSpellCheck(!mSpellMainChanged,mDialogError);
            mSpellImport->close(mDialogError);
        };
        mTree.setControlCommit(mSpellImportFields.at("ok_btn"),std::move(accepted));
        LLVKControl::Callback cancelled; cancelled.function=[this](auto,const LLSD&) { mSpellImport->close(mDialogError); };
        mTree.setControlCommit(mSpellImportFields.at("cancel_btn"),std::move(cancelled));
        mSpellImport->onClose([this] { ++mSpellImportGeneration; });
    }
    if (!mSpellImport->visible()) ++mSpellImportGeneration;
    mActiveFloater=mSpellImport.get();
    return mSpellImport->open(error);
}

LLSD LLVKLoginUi::translationKey(const std::string& service) const
{
    const auto editor=mTranslationFields.at(service+"_api_key");
    if (mTree.get(editor)->control->tentative) return service=="google" ? LLSD("") : LLSD();
    if (service=="google") return mTree.value(editor);
    LLSD key; key["id"]=mTree.value(editor);
    if (service=="azure")
    {
        key["endpoint"]=mTree.value(mTranslationFields.at("azure_api_endpoint_combo"));
        const auto region=mTranslationFields.at("azure_api_region");
        if (!mTree.get(region)->control->tentative) key["region"]=mTree.value(region);
    }
    else key["domain"]=mTree.value(mTranslationFields.at("deepl_api_domain_combo"));
    return key;
}

void LLVKLoginUi::updateTranslationControls()
{
    const bool enabled=mTree.value(mTranslationFields.at("translate_chat_checkbox")).asBoolean();
    const auto selected=mTree.value(mTranslationFields.at("translation_service_rg")).asString();
    for (const auto name : {"translation_service_rg","translate_language_combo","azure_api_endoint_label","azure_api_key_label",
        "azure_api_region_label","google_api_key_label","deepl_api_domain_label","deepl_api_key_label"}) mTree.setEnabled(mTranslationFields.at(name),enabled);
    for (const std::string service : {"azure","google","deepl"})
    {
        const bool current=enabled && selected==service;
        mTree.setEnabled(mTranslationFields.at(service+"_api_key"),current);
        const auto key=translationKey(service);
        mTree.setEnabled(mTranslationFields.at("verify_"+service+"_api_key_btn"),current && !mTranslationVerified[service] &&
            (service=="google" ? !key.asString().empty() : key.isMap()));
    }
    mTree.setEnabled(mTranslationFields.at("azure_api_endpoint_combo"),enabled && selected=="azure");
    mTree.setEnabled(mTranslationFields.at("azure_api_region"),enabled && selected=="azure");
    mTree.setEnabled(mTranslationFields.at("deepl_api_domain_combo"),enabled && selected=="deepl");
    mTree.updateSetting("TranslatingEnabled",LLSD(mTranslationVerified[selected]));
    mTree.setEnabled(mTranslationFields.at("ok_btn"),!enabled || mTranslationVerified[selected]);
}

void LLVKLoginUi::invalidateTranslation(const std::string& service)
{
    ++mTranslationRequests[service]; mTranslationVerified[service]=false;
    updateTranslationControls();
}

void LLVKLoginUi::verifyTranslation(const std::string& service,bool alert)
{
    if (!mTranslationVerifier) { mDialogError="Native translation verification service is unavailable"; return; }
    const auto key=translationKey(service);
    const auto request=++mTranslationRequests[service];
    mTranslationVerifier(service,key,[this,service,key,request,alert,generation=mTranslationGeneration](bool verified,int status)
    {
        if (!mTranslation || generation!=mTranslationGeneration || request!=mTranslationRequests[service] ||
            !llsd_equals(key,translationKey(service))) return;
        mTranslationVerified[service]=verified; updateTranslationControls();
        if (alert)
        {
            const auto message=mTree.panelString(mTranslation->id(),service+(verified ? "_api_key_verified" : "_api_key_not_verified"),
                {{"STATUS",std::to_string(status)}},mDialogError);
            if (message) mNotices.push_back({"GenericAlert",*message});
        }
    },mDialogError);
}

bool LLVKLoginUi::showTranslation(std::string& error)
{
    error.clear();
    if (!mTranslation)
    {
        mTranslation=LLVKFloater::createFile(mTree,*mDialogFactory,mRoot,"floater_translation_settings.xml",error);
        if (!mTranslation) return false;
        mTranslationFields=preferenceFields(mTree,mTranslation->id());
        for (const auto name : {"translate_chat_checkbox","translate_language_combo","translation_service_rg","azure_api_endpoint_combo",
            "azure_api_key","azure_api_region","google_api_key","deepl_api_domain_combo","deepl_api_key","ok_btn","cancel_btn",
            "verify_azure_api_key_btn","verify_google_api_key_btn","verify_deepl_api_key_btn","azure_api_endoint_label","azure_api_key_label",
            "azure_api_region_label","google_api_key_label","deepl_api_domain_label","deepl_api_key_label"})
            if (!mTranslationFields.contains(name)) { error="Native Translation Settings declaration is incomplete"; mTranslation.reset(); return false; }
        const auto bind=[&](const std::string& name,std::function<void(LLVKWidgetTree::Id,const LLSD&)> function)
        { LLVKControl::Callback callback; callback.function=std::move(function); mTree.setControlCommit(mTranslationFields.at(name),std::move(callback)); };
        for (const auto name : {"translate_chat_checkbox","translation_service_rg"})
            bind(name,[this](auto,const LLSD&) { updateTranslationControls(); });
        for (const std::string service : {"azure","google","deepl"})
        {
            bind("verify_"+service+"_api_key_btn",[this,service](auto,const LLSD&) { verifyTranslation(service,true); });
            const auto editor=mTranslationFields.at(service+"_api_key");
            LLVKWidgetTree::Events events;
            events.focusReceived=[this](auto id)
            {
                const auto* node=mTree.get(id);
                if (node && node->lineEditor && !node->lineEditor->readOnly && node->control->tentative)
                { mTree.setValue(id,LLSD("")); mTree.setTentative(id,false); }
            };
            LLVKControl::Callback edited; edited.function=[this,service](auto,const LLSD&) { invalidateTranslation(service); };
            mTree.setEvents(editor,events); mTree.setLineEditorKeystroke(editor,edited);
            if (service=="azure")
            { mTree.setEvents(mTranslationFields.at("azure_api_region"),events); mTree.setLineEditorKeystroke(mTranslationFields.at("azure_api_region"),edited); }
            if (service!="google")
            {
                const auto field=service=="azure" ? "azure_api_endpoint_combo" : "deepl_api_domain_combo";
                bind(field,[this,service](auto,const LLSD&) { invalidateTranslation(service); });
                LLVKWidgetTree::Events comboEvents; comboEvents.focusLost=[this,service](auto) { invalidateTranslation(service); };
                mTree.setEvents(mTranslationFields.at(field),std::move(comboEvents));
            }
        }
        bind("cancel_btn",[this](auto,const LLSD&) { mTranslation->close(mDialogError); });
        bind("ok_btn",[this](auto,const LLSD&)
        {
            std::map<std::string,LLSD> values{{"TranslateChat",mTree.value(mTranslationFields.at("translate_chat_checkbox"))},
                {"TranslateLanguage",mTree.value(mTranslationFields.at("translate_language_combo"))},
                {"TranslationService",mTree.value(mTranslationFields.at("translation_service_rg"))},
                {"AzureTranslateAPIKey",translationKey("azure")},{"GoogleTranslateAPIKey",translationKey("google")},{"DeepLTranslateAPIKey",translationKey("deepl")}};
            for (const auto& [name,value] : values) if (!mTree.setting(name)) { mDialogError="Native translation setting is missing: "+name; return; }
            if (!mSavePreferences || !mSavePreferences(values,mDialogError))
            { if (mDialogError.empty()) mDialogError="Native translation preferences could not be saved"; return; }
            for (const auto& [name,value] : values) mTree.updateSetting(name,value);
            mTranslation->close(mDialogError);
        });
        mTranslation->onClose([this]
        {
            const auto service=mTree.setting("TranslationService").value_or(LLSD("")).asString();
            mTree.updateSetting("TranslatingEnabled",LLSD(mTranslationVerified[service]));
            ++mTranslationGeneration;
        });
    }
    if (!mTranslation->visible())
    {
        ++mTranslationGeneration;
        mTree.setValue(mTranslationFields.at("translate_chat_checkbox"),mTree.setting("TranslateChat").value_or(LLSD(false)));
        mTree.setValue(mTranslationFields.at("translate_language_combo"),mTree.setting("TranslateLanguage").value_or(LLSD("default")));
        mTree.setValue(mTranslationFields.at("translation_service_rg"),mTree.setting("TranslationService").value_or(LLSD("google")));
        for (const std::string service : {"azure","google","deepl"})
        {
            const auto setting=service=="azure" ? "AzureTranslateAPIKey" : service=="google" ? "GoogleTranslateAPIKey" : "DeepLTranslateAPIKey";
            const auto key=mTree.setting(setting).value_or(LLSD());
            const auto id=service=="google" ? key.asString() : key["id"].asString();
            const auto editor=mTranslationFields.at(service+"_api_key");
            mTranslationVerified[service]=false;
            mTree.setTentative(editor,id.empty());
            if (!id.empty()) mTree.setValue(editor,LLSD(id));
            if (service=="azure")
            {
                const auto region=mTranslationFields.at("azure_api_region");
                mTree.setTentative(region,key["region"].asString().empty());
                if (!key["region"].asString().empty()) mTree.setValue(region,key["region"]);
                if (!id.empty()) mTree.setValue(mTranslationFields.at("azure_api_endpoint_combo"),key["endpoint"]);
            }
            else if (service=="deepl" && !id.empty()) mTree.setValue(mTranslationFields.at("deepl_api_domain_combo"),key["domain"]);
            if (!id.empty()) verifyTranslation(service,false);
        }
        updateTranslationControls();
    }
    mActiveFloater=mTranslation.get();
    return mTranslation->open(error);
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

bool LLVKLoginUi::previewUiSound(const std::string& name,std::string& error)
{
    error.clear();
    const auto setting=mTree.setting(name);
    if (!setting || !LLUUID::validate(setting->asString()))
    { error="Native UI sound setting is missing or invalid: "+name; return false; }
    const LLUUID asset(setting->asString());
    if (asset.isNull()) return true;
    if (!mUiSoundPlayer) { error="Native UI sound service is not bound"; return false; }
    return mUiSoundPlayer(asset.asString(),error);
}

bool LLVKLoginUi::resetAccountPreference(const std::string& name,std::string& error)
{
    error.clear();
    const auto found=mAccountDefaults.find(name);
    if (found==mAccountDefaults.end() || !mTree.setting(name))
    { error="Native account preference has no loaded default: "+name; return false; }
    if (!mTree.updateSetting(name,found->second))
    { error="Native account preference reset failed: "+name; return false; }
    return true;
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
    std::vector<LLVKFloater*> result{mPreferences.get(),mAbout.get(),mAutoReplace.get(),mSpellCheck.get(),mSpellImport.get(),mTranslation.get()};
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