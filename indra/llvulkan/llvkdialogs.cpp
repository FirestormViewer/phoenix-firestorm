#include "llerrorcontrol.h"
#include "llvkviewerui.h"
#include "lluriparser.h"
#include <boost/regex.hpp>
#include "llstring.h"
#include "llsdutil.h"
#include "llsdserialize.h"
#include "lluri.h"
#include "llrect.h"
#include "llvkxmllayers.h"
#include <expat/expat.h>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <charconv>
#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

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

bool LLVKViewerUi::initializeDialogs(const Configuration& configuration,std::string& error)
{
    mPreviewSkin=configuration.skin;
    mBackupHandler=configuration.backupHandler;
    mClearSpamQueues=configuration.clearSpamQueues;
    mLoadProxyCredentials=configuration.loadProxyCredentials;
    mSaveProxyCredentials=configuration.saveProxyCredentials;
    if (!mMediaFilter.load(configuration.mediaFilterRules,error)) return false;
    mSaveMediaFilterRules=configuration.saveMediaFilterRules;
    mSavePreferences = configuration.savePreferences;
    struct DebugControls final : LLControlGroup::ApplyFunctor
    {
        std::map<std::string,LLControlVariablePtr>& controls;
        std::set<std::string>* account;
        DebugControls(std::map<std::string,LLControlVariablePtr>& values,std::set<std::string>* names) : controls(values),account(names) {}
        void apply(const std::string& name,LLControlVariable* control) override
        { controls[name]=control; if (account) account->insert(name); }
    } baseControls(mDebugControls,nullptr),accountControls(mDebugControls,&mDebugAccountNames);
    if (configuration.settingsGroup) configuration.settingsGroup->applyToAll(&baseControls);
    if (configuration.accountSettingsGroup) configuration.accountSettingsGroup->applyToAll(&accountControls);
    mViewerExecutable=configuration.viewerExecutable;
    mPluginLauncher=configuration.pluginLauncher;
    mBrowserHelper=configuration.browserHelper;
    mVoiceExecutable=configuration.voiceExecutable;
    mSaveKeyBindings=configuration.saveKeyBindings;
    if (!mDefaultBindings.loadFile(configuration.skin.skinBaseDirectory.parent_path()/"app_settings"/"key_bindings.xml",error)) return false;
    mBindings=mDefaultBindings;
    const auto userBindings=configuration.skin.userAppDirectory/"user_settings"/"key_bindings.xml";
    if (!configuration.skin.userAppDirectory.empty() && std::filesystem::is_regular_file(userBindings))
    {
        if (!mBindings.loadFile(userBindings,error)) { mBindings=mDefaultBindings; error.clear(); }
    }
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
    mMenu->bindItem("Floater.Show","fs_whitelist_floater",[this](const auto&,const auto&) { showWhitelist(mDialogError); });
    mMenu->bindItem("Floater.Show","window_size",[this](const auto&,const auto&) { showWindowSize(mDialogError); });
    for (const auto name : {"test_textbox","test_text_editor","font_test","test_widgets"})
        mMenu->bindItem("Floater.Show",name,[this](const auto&,const std::string& name) { showUiTest(name,mDialogError); });
    mMenu->bind("Develop.Fonts.Dump",[this](const auto&,const auto&)
    { LL_INFOS("NativeFonts") << fontDiagnostics() << LL_ENDL; });
    mMenu->bind("Develop.Fonts.DumpTextures",[this](const auto&,const auto&) { dumpFontTextures(mDialogError); });
    mMenu->bindItem("Floater.Toggle","ui_preview",[this](const auto&,const auto&)
    { if (mUiPreview && mUiPreview->visible()) mUiPreview->close(mDialogError); else showUiPreview(mDialogError); });
    mMenu->bind("Advanced.ReportBug",[this](const auto&,const auto&) { reportProblem(mDialogError); });
    mMenu->bind("Advanced.WebContentTest",[this](const auto&,const std::string& url)
    { showMediaBrowser(url=="HOME_PAGE" ? mTree.setting("FSBrowserHomePage").value_or(LLSD()).asString() : url,mDialogError); });
    mMenu->bind("Advanced.ShowDebugSettings",[this](const auto&,const auto&) { showDebugSettings(mDialogError); });
    mMenu->bindItem("Floater.Toggle","settings_color",[this](const auto&,const auto&)
    {
        if (mColorSettings && mColorSettings->visible()) mColorSettings->close(mDialogError);
        else showColorSettings(mDialogError);
    });
    mMenu->bind("File.CloseWindow",[this](const auto&,const auto&) { closeMenuWindow(mDialogError); });
    mMenu->bindPredicate("File.EnableCloseWindow",[this](const auto&) { return canCloseMenuWindow(); });
    mMenu->bind("ToggleControl",[this](const auto&,const std::string& name)
    {
        const auto value=mTree.setting(name);
        if (!value || !value->isBoolean()) { mDialogError="Native menu boolean setting is unavailable: "+name; return; }
        mTree.updateSetting(name,LLSD(!value->asBoolean()));
    });
    mMenu->bindPredicate("CheckControl",[this](const std::string& name) { return mTree.setting(name).value_or(LLSD(false)).asBoolean(); });
    if (mTree.setting("UseDebugMenus")) mTree.subscribeSetting("UseDebugMenus",[this](const LLSD& value,const LLSD&)
    { mMenu->setVisible("Debug",value.asBoolean()); });
    mMenu->bind("Develop.SetLoggingLevel",[](const auto&,const std::string& level)
    {
        if (level.size()==1 && level[0]>='0' && level[0]<='4')
            LLError::setDefaultLevel(static_cast<LLError::ELevel>(level[0]-'0'));
    });
    mMenu->bindPredicate("Develop.CheckLoggingLevel",[](const std::string& level)
    { return level.size()==1 && level[0]>='0' && level[0]<='4' && static_cast<int>(LLError::getDefaultLevel())==level[0]-'0'; });
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
        std::map<std::string,Notice> formTemplates;
        bool formTemplate = false;
        static void XMLCALL start(void* pointer,const char* tag,const char** attributes)
        {
            auto& state = *static_cast<Parser*>(pointer);
            try
            {
                if (state.stack.size() >= 64) throw std::runtime_error("About depth");
                state.stack.emplace_back(tag);
                if (state.notices && (std::string_view(tag)=="notification" || std::string_view(tag)=="template"))
                {
                    std::string name;
                    for (std::size_t index=0; attributes[index]; index+=2)
                        if (std::string_view(attributes[index])=="name") name=attributes[index+1];
                    if (name=="AddAutoReplaceList" || name=="RenameAutoReplaceList" || name=="RemoveAutoReplaceList" ||
                        name=="InvalidAutoReplaceList" || name=="InvalidAutoReplaceEntry" || name=="SpellingDictImportRequired" ||
                        name=="SpellingDictIsSecondary" || name=="SpellingDictImportFailed" || name=="PreferenceControlsDefaults" || name=="yesnocancelbuttons" ||
                        name=="FirestormClearSettingsPrompt" || name=="SettingsWillClear" || name=="okcancelbuttons" || name=="AddToMediaList" ||
                        name=="ConfirmClearCache" || name=="ConfirmClearWebBrowserCache" || name=="CacheWillClear" || name=="CacheWillBeMoved" ||
                        name=="SoundCacheWillBeMoved" || name=="DisableJavascriptBreaksSearch" || name=="ChangeSkin" || name=="SkinDefaultsChangeSettings" || name=="ChangeRenderBackend" ||
                        name=="SettingsConfirmBackup" || name=="SettingsRestoreNeedsLogout" || name=="BackupPathEmpty" ||
                        name=="BackupFinished" || name=="RestoreFinished" || name=="okbutton" ||
                        name=="DebugSettingsWarning" || name=="ControlNameCopiedToClipboard" || name=="SanityCheck" || name=="MediaPluginFailed" || name=="ChangeLanguage" ||
                        name=="WebLaunchExternalTarget" || name=="okcancelignore")
                    { state.notice=Notice{name,{}}; state.noticeDepth=static_cast<int>(state.stack.size()); }
                    state.formTemplate=std::string_view(tag)=="template";
                }
                if (state.notice && std::string_view(tag)=="usetemplate")
                {
                    std::map<std::string,std::string> fields;
                    for (std::size_t index=0; attributes[index]; index+=2) fields.emplace(attributes[index],attributes[index+1]);
                    const auto found=state.formTemplates.find(fields["name"]);
                    if (found==state.formTemplates.end()) throw std::runtime_error("Unknown native notice form template");
                    state.notice->buttons=found->second.buttons;
                    for (auto& button : state.notice->buttons)
                        for (const auto& [name,value] : fields) if (name!="name") LLStringUtil::replaceString(button.label,"$"+name,value);
                }
                if (state.notice && state.stack.size()==state.noticeDepth+2 && state.stack[state.stack.size()-2]=="form")
                {
                    std::map<std::string,std::string> fields;
                    for (std::size_t index=0; attributes[index]; index+=2) fields.emplace(attributes[index],attributes[index+1]);
                    if (std::string_view(tag)=="button")
                        state.notice->buttons.push_back({fields["name"],fields["text"],std::stoi(fields["index"]),fields["default"]=="true"});
                    else if (std::string_view(tag)=="input" && fields["type"]=="text") state.notice->inputName=fields["name"];
                    else if (std::string_view(tag)!="ignore") throw std::runtime_error("Unsupported native notification field");
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
                    if (state.formTemplate) state.formTemplates.insert_or_assign(state.notice->name,*state.notice);
                    else state.notices->insert_or_assign(state.notice->name,*state.notice);
                    state.notice.reset(); state.noticeDepth=0;
                    state.formTemplate=false;
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
    if (!loadNotificationPreferences(*notificationXml,configuration,error)) return false;
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
            enqueueNotice({"GenericAlert",std::move(message)},mDialogError);
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

bool LLVKViewerUi::queueNotice(const std::string& name,const LLSD& arguments,
    std::function<void(int,const LLSD&)> response,std::string& error)
{
    error.clear();
    if (const auto preference=mNotificationPreferences.find(name); preference!=mNotificationPreferences.end())
    {
        const auto& policy=preference->second;
        bool show=true;
        if (policy.control.empty()) show=mWarningSettings->getBOOL(name);
        else if (const auto value=mTree.setting(policy.control)) show=policy.inverted ? !value->asBoolean() : value->asBoolean();
        if (!show)
        {
            auto value=policy.defaultResponse;
            int option=policy.defaultOption;
            if (policy.saveResponse)
            {
                value=mWarningSettings->getLLSD("Default"+name);
                option=-1;
                for (const auto& [button,index] : policy.responseOptions) if (value[button].asBoolean()) { option=index; break; }
            }
            if (response) response(option,value);
            return true;
        }
    }
    const auto found=mNoticeTemplates.find(name);
    if (found==mNoticeTemplates.end()) { error="Native notification template is unavailable: "+name; return false; }
    auto notice=found->second;
    LLStringUtil::format_map_t substitutions;
    for (auto entry=arguments.beginMap(); entry!=arguments.endMap(); ++entry)
        substitutions["["+entry->first+"]"]=entry->second.asString();
    LLStringUtil::format(notice.message,substitutions);
    for (auto& button : notice.buttons) LLStringUtil::format(button.label,substitutions);
    notice.response=std::move(response);
    return enqueueNotice(std::move(notice),error);
}

bool LLVKViewerUi::enqueueNotice(Notice notice,std::string& error)
{
    error.clear();
    if (mNotices.size()>=64) { error="Native notice queue is full"; return false; }
    if (notice.response)
    {
        auto delivered=std::make_shared<bool>(false);
        notice.response=[delivered,response=std::move(notice.response)](int option,const LLSD& values)
        {
            if (*delivered) return;
            *delivered=true;
            response(option,values);
        };
    }
    mNotices.push_back(std::move(notice));
    return true;
}

std::string LLVKViewerUi::errorString(std::string_view key,std::string_view fallback) const
{
    const auto found=mAboutStrings.find(std::string(key));
    auto value=found==mAboutStrings.end() || found->second.empty() ? std::string(fallback) : found->second;
    for (auto& character : value) if (character=='\n' || character=='\r' || character=='\t') character=' ';
    return value;
}

bool LLVKViewerUi::queueError(const LLVKError& failure,std::vector<Notice::Button> actions,
    std::function<void(int)> response,std::string& error,std::string name)
{
    error.clear();
    if (mNotices.size()>=64) { error="Native error notice queue is full"; return false; }
    const auto message=failure.format([this](std::string_view key) { return errorString(key,{}); });
    Notice notice;
    notice.name=name;
    notice.message=message.title+"\n\n"+message.body;
    notice.buttons=std::move(actions);
    notice.buttons.push_back({"close",errorString("implicitclosebutton","Close"),0,true});
    if (mDialogClipboard)
        notice.buttons.push_back({"copy",errorString("NativeErrorCopy","Copy details"),-1,false});
    notice.response=[this,failure,name,diagnostic=message.diagnostic,actions=notice.buttons,response=std::move(response)](int option,const LLSD&)
    {
        if (option==-1)
        {
            const auto wide=utf8str_to_wstring(diagnostic);
            std::string copyError;
            if (mDialogClipboard) mDialogClipboard->write(std::u32string(wide.begin(),wide.end()),false,copyError);
            auto repeated=actions;
            std::erase_if(repeated,[](const auto& button) { return button.option==0 || button.option==-1; });
            if (!queueError(failure,std::move(repeated),response,mDialogError,name)) mReportedSession.reset();
            else if (!copyError.empty()) mDialogError=std::move(copyError);
        }
        else if (response) response(option);
    };
    return enqueueNotice(std::move(notice),error);
}

bool LLVKViewerUi::showError(const LLVKError& failure,std::string& error)
{
    error.clear();
    auto gate=mErrorGate;
    if (!gate.begin(failure.operation,failure.generation) || !gate.accept(failure)) return true;
    if (!queueError(failure,{},[this,failure](int option)
    { if (!option && failure.policy().recovery==LLVKError::Recovery::Stop && mQuitRequest) mQuitRequest(); },error)) return false;
    mErrorGate=gate;
    return true;
}

void LLVKViewerUi::setSessionOwner(LLVKSessionOwner* owner)
{
    mSessionOwner=owner;
    mReportedSession.reset();
    LLVKControl::Callback login;
    if (owner) login.function=[this,owner](auto,const LLSD&)
    {
        updateLoginControls();
        if (mSessionOwner!=owner || !mTree.get(find("connect_btn"))->params.enabled) return;
        owner->beginLogin();
        mReportedSession.reset();
        refreshSession(mDialogError);
    };
    mTree.setControlCommit(find("connect_btn"),std::move(login));
    updateLoginControls();
}

bool LLVKViewerUi::refreshSession(std::string& error,bool repeat)
{
    error.clear();
    if (!mSessionOwner) return true;
    if (repeat) mReportedSession.reset();
    using Owner=LLVKSessionOwner;
    mSessionSnapshot=mSessionOwner->snapshot();
    const auto snapshot=mSessionSnapshot;
    const auto same=[](const Owner::Status& left,const Owner::Status& right)
    { return left.code==right.code && left.action==right.action && left.operation==right.operation &&
        left.generation==right.generation && left.service==right.service; };
    if (mReportedSession && mReportedSession->tag==snapshot.tag && mReportedSession->state==snapshot.state &&
        same(mReportedSession->status,snapshot.status) && same(mReportedSession->cleanup,snapshot.cleanup)) return true;
    const bool prelogin=snapshot.state==Owner::State::PreLogin;
    for (const auto name : {"connect_btn","username_combo","password_edit","server_combo","start_location_combo"})
        mTree.setEnabled(find(name),prelogin);
    updateLoginControls();
    if (mActiveNotice && (mActiveNotice->name=="NativeSessionError" || mActiveNotice->name=="NativeSessionProgress" ||
        mActiveNotice->name=="NativeSessionAgreement"))
        if (!dismissNotice(error)) return false;
    std::erase_if(mNotices,[](const auto& notice)
    { return notice.name=="NativeSessionError" || notice.name=="NativeSessionProgress" || notice.name=="NativeSessionAgreement"; });
    if (snapshot.state==Owner::State::AwaitingAgreement && snapshot.agreement)
    {
        if (mNotices.size()>=64) { error="Native session notice queue is full"; return false; }
        Notice notice;
        notice.name="NativeSessionAgreement";
        notice.message=snapshot.agreement->text;
        notice.buttons.push_back({"reject",errorString("NativeAgreementReject","Decline"),0,true});
        notice.buttons.push_back({"accept",errorString("NativeAgreementAccept","Accept"),1,false});
        notice.response=[this,owner=mSessionOwner,tag=snapshot.tag,agreement=*snapshot.agreement](int option,const LLSD&)
        {
            if (mSessionOwner!=owner || (option!=0 && option!=1)) return;
            owner->decideAgreement(tag,agreement,option==1);
            refreshSession(mDialogError);
        };
        if (!enqueueNotice(std::move(notice),error)) return false;
        mReportedSession=snapshot;
        return true;
    }
    const bool cleaning=snapshot.state==Owner::State::Disconnecting;
    const auto status=cleaning ? snapshot.cleanup : snapshot.status;
    if (status.code==Owner::Code::Ok || status.code==Owner::Code::Cancelled || status.code==Owner::Code::Pending)
    {
        const bool active=snapshot.state==Owner::State::Authenticating || snapshot.state==Owner::State::Connecting;
        if (active)
        {
            if (mNotices.size()>=64) { error="Native session notice queue is full"; return false; }
            Notice notice;
            notice.name="NativeSessionProgress";
            notice.message=errorString(snapshot.state==Owner::State::Connecting ? "LoginConnectingToRegion" :
                "LoginInProgress","Login in progress...");
            notice.buttons.push_back({"cancel",errorString("Cancel","Cancel"),1,true});
            notice.response=[this,owner=mSessionOwner,tag=snapshot.tag](int option,const LLSD&)
            {
                if (option!=1 || mSessionOwner!=owner) return;
                owner->cancel(tag);
                refreshSession(mDialogError);
            };
            if (!enqueueNotice(std::move(notice),error)) return false;
        }
        mReportedSession=snapshot;
        return true;
    }
    auto code=LLVKError::Code::SessionFailed;
    if (cleaning) code=LLVKError::Code::SessionCleanupFailed;
    else switch (status.code)
    {
        case Owner::Code::TransportUnavailable:
            code=status.action==Owner::Action::Close ? LLVKError::Code::TransportUnavailable : LLVKError::Code::NetworkUnavailable;
            break;
        case Owner::Code::AuthenticationFailed: code=LLVKError::Code::AuthenticationFailed; break;
        case Owner::Code::ConnectionFailed: code=LLVKError::Code::ConnectionFailed; break;
        case Owner::Code::Timeout: code=LLVKError::Code::SessionTimeout; break;
        default: break;
    }
    const LLVKError failure{code,LLVKError::Operation::Session,snapshot.tag.generation,snapshot.tag.request};
    std::vector<Notice::Button> actions;
    if (cleaning && status.action==Owner::Action::RetryCleanup)
        actions.push_back({"retry_cleanup",errorString("NativeErrorRetryCleanup","Retry cleanup"),1,false});
    if (prelogin && status.action==Owner::Action::RetryLogin)
        actions.push_back({"retry_login",errorString("NativeErrorRetryLogin","Retry login"),2,false});
    if (!queueError(failure,std::move(actions),[this,owner=mSessionOwner,snapshot](int option)
    {
        if (!option || mSessionOwner!=owner || owner->snapshot().tag!=snapshot.tag) return;
        if (option==1 && snapshot.cleanup.action==Owner::Action::RetryCleanup) owner->retryCleanup(snapshot.tag);
        else if (option==2 && snapshot.status.action==Owner::Action::RetryLogin) owner->beginLogin();
        else return;
        mReportedSession.reset();
        refreshSession(mDialogError);
    },error,"NativeSessionError")) return false;
    try { LL_WARNS("NativeSession") << failure.diagnostic() << LL_ENDL; } catch (...) {}
    mReportedSession=snapshot;
    return true;
}

bool LLVKViewerUi::initializeNoticeLayout(std::string& error)
{
    mNoticeTopRight=mTree.setting("ShowGroupNoticesTopRight").value_or(LLSD(false)).asBoolean();
    const auto declaration=[&](const char* file) -> std::optional<boost::property_tree::ptree>
    {
        const auto files=mSkin->read("xui",file,LLVKSkinFiles::Policy::Current,error);
        if (!files) return {};
        std::vector<std::string_view> layers;
        for (const auto& text : *files) layers.push_back(text);
        const auto xml=LLVKXmlLayers::merge(layers,error);
        if (!xml) return {};
        boost::property_tree::ptree parsed;
        std::istringstream stream(*xml);
        boost::property_tree::read_xml(stream,parsed);
        return parsed;
    };
    const auto named=[](const auto& self,const boost::property_tree::ptree& node,std::string_view name) -> const boost::property_tree::ptree*
    {
        if (node.get<std::string>("<xmlattr>.name","")==name) return &node;
        for (const auto& child : node)
            if (child.first!="<xmlattr>") if (const auto found=self(self,child.second,name)) return found;
        return nullptr;
    };
    try
    {
        const auto main=declaration("main_view.xml"),toolbar=declaration("panel_toolbar_view.xml"),toast=declaration("panel_toast.xml");
        if (!main || !toolbar || !toast) return false;
        const auto menu=named(named,*main,"status_bar_container");
        const auto bottom=named(named,*toolbar,"bottom_toolbar_panel");
        const auto stack=named(named,*toolbar,"bottom_toolbar_stack");
        const auto chiclet=named(named,*toolbar,"chiclet_container");
        const auto outer=named(named,*toast,"toast"),wrapper=named(named,*toast,"wrapper_panel");
        if (!menu || !bottom || !stack || !chiclet || !outer || !wrapper)
        { error="Native notification layout declaration is incomplete"; return false; }
        mNoticeMenuHeight=menu->get<int>("<xmlattr>.height");
        mNoticeBottomHeight=bottom->get<int>("<xmlattr>.height");
        mNoticeStackSpacing=stack->get<int>("<xmlattr>.border_size",3);
        mNoticeChicletInset=chiclet->get<int>("<xmlattr>.top")+chiclet->get<int>("<xmlattr>.height");
        mNoticeRightPad=outer->get<int>("<xmlattr>.width")-wrapper->get<int>("<xmlattr>.width");
        mNoticeTopPad=outer->get<int>("<xmlattr>.height")-wrapper->get<int>("<xmlattr>.height");
        for (const auto value : {mNoticeMenuHeight,mNoticeBottomHeight,mNoticeStackSpacing,mNoticeChicletInset,mNoticeRightPad,mNoticeTopPad})
            if (value<0 || value>16384) { error="Native notification layout dimension is invalid"; return false; }
    }
    catch (const boost::property_tree::ptree_error& failure) { error=failure.what(); return false; }
    return true;
}

LLVKWidgetTree::Rect LLVKViewerUi::noticeRectangle(int width,int height,std::optional<LLVKWidgetTree::Rect> viewport) const
{
    const auto root=viewport.value_or(mTree.get(mRoot)->params.rect);
    const auto margin=std::clamp(mTree.setting("ChannelBottomPanelMargin").value_or(LLSD(35)).asInteger(),0,16384);
    const auto gap=std::clamp(mTree.setting("ToastGap").value_or(LLSD(7)).asInteger(),0,16384);
    const auto topInset=mNoticeTopRight ? mNoticeChicletInset : 0;
    const auto channelHeight=std::max(0,root.top-root.bottom-mNoticeMenuHeight-mNoticeBottomHeight-mNoticeStackSpacing-margin-topInset);
    const auto outerHeight=height+mNoticeTopPad;
    const auto left=std::max(0,(root.right-root.left)/2-(width+mNoticeRightPad)/2);
    const auto bottom=std::clamp(channelHeight/2+gap+2*(outerHeight/2)-outerHeight,0,std::max(0,root.top-root.bottom-height-mNoticeTopPad));
    return {left,bottom,left+width,bottom+height};
}

bool LLVKViewerUi::advanceNotices(double time,std::string& error)
{
    error.clear();
    if (!std::isfinite(time) || time<mNoticeTime) { error="Invalid native notice clock"; return false; }
    mNoticeTime=time;
    if (mVoiceReset && time>=mVoiceReset->second)
    {
        mTree.updateSetting("EnableVoiceChat",LLSD(true));
        if (mTree.get(mVoiceReset->first))
        {
            const auto fields=preferenceFields(mTree,mVoiceReset->first);
            for (const auto name : {"enable_voice_check","enable_voice_check_volume"})
                if (fields.contains(name)) mTree.setEnabled(fields.at(name),true);
        }
        mVoiceReset.reset();
    }
    if (keyCaptureDialog() && mKeyClickDeadline && time>=*mKeyClickDeadline)
    {
        mKeyClickDeadline.reset();
        if (!applyCapturedBinding(LLKeyData(CLICK_LEFT,KEY_NONE,mKeyClickMask,true),false,
            mTree.value(mKeyCaptureFields.at("apply_all")).asBoolean(),error)) return false;
        mKeyCapture->close(error);
    }
    if (mNoticePanel)
    {
        if (time-mNoticeOpened>=0.5 && !mTree.setPanelDefaultButton(mNoticePanel,mNoticeButton,error)) return false;
        return true;
    }
    if (mNotices.empty()) return true;
    auto notice=mNotices.front();
    const auto preference=mNotificationPreferences.find(notice.name);
    const bool hasIgnore=preference!=mNotificationPreferences.end();
    std::string ignoreLabel;
    if (hasIgnore)
        ignoreLabel=mAboutStrings.at(preference->second.sessionOnly ? "skipnexttimesessiononly" :
            preference->second.saveResponse ? "alwayschoose" : "skipnexttime");
    if (notice.buttons.empty()) notice.buttons.push_back({"close",mAboutStrings.at("implicitclosebutton"),0,true});
    const auto font=mFonts->resolve({"SansSerif","Default"},error);
    if (!font) return false;
    const auto root=mTree.get(mRoot)->params.rect;
    LLVKPlainTextLayout::Options options; options.width=std::max(1,std::min(400,root.right-root.left-70)); options.wrap=true;
    const auto parsed=LLVKWebText::parse(mNotices.front().message,error);
    if (!parsed) return false;
    LLVKPlainControl message;
    message.text=parsed->text;
    for (const auto& icon : parsed->icons)
    {
        const auto image=mTree.findImage(icon.name,error);
        if (!image) return false;
        message.icons.emplace(icon.position,image);
    }
    const auto document=message.prepareDocument(font,options,0,error);
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
        buttonWidth=std::max(buttonWidth,static_cast<int>(std::floor(measured->width+0.5f))+static_cast<int>(std::floor(padding->width+0.5f))+8);
    }
    const auto totalButtons=buttonWidth*static_cast<int>(notice.buttons.size())+8*static_cast<int>(notice.buttons.size()-1);
    const auto textWidth=std::min(static_cast<int>(options.width),document->bounds.right-document->bounds.left+25);
    const auto textHeight=std::min(document->fitHeight,std::max(40,root.top-root.bottom-120));
    const auto lineHeight=static_cast<int>(std::ceil(font->metrics().ascender)+std::ceil(font->metrics().descender));
    const auto ignoreLines=1+static_cast<int>(std::count(ignoreLabel.begin(),ignoreLabel.end(),'\n'));
    const int ignoreHeight=hasIgnore ? lineHeight*ignoreLines+lineHeight/2 : 0;
    auto width=std::max(totalButtons,textWidth)+50;
    if (hasIgnore)
    {
        const auto firstLine=utf8str_to_wstring(ignoreLabel.substr(0,ignoreLabel.find('\n')));
        const std::u32string label(firstLine.begin(),firstLine.end());
        const auto measured=font->measureRun(label,0,label.size(),1.f,true,false,error);
        if (!measured) return false;
        width=std::max(width,static_cast<int>(measured->width+0.99f)+16+50);
    }
    const auto height=textHeight+48+23+(notice.inputName.empty() ? 0 : 36)+ignoreHeight;
    LLVKWidgetTree::Params view; view.name=notice.name; view.rect=noticeRectangle(width,height);
    view.focusRoot=true;
    LLVKControl::Params control; control.font=font;
    LLVKPanel::Params background; background.backgroundVisible=background.backgroundOpaque=true;
    background.opaqueImage=mTree.findImage("Toast_Over",error);
    background.alertShadowColor=mColors->find("ColorDropShadow");
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
    if (notice.name=="NativeSessionAgreement" || textHeight<document->fitHeight)
    {
        const auto body=mDialogFactory->construct(mTree,
            "<text_editor name='Alert message' read_only='true' word_wrap='true' max_length='65536' parse_urls='false'/>",*panel,error);
        if (!body || !mTree.setShape(*body,view.rect,error) || !mTree.setValue(*body,LLSD(notice.message)))
        { discard(); return false; }
    }
    else if (!mTree.createPlainText(view,control,text,*panel,error)) { discard(); return false; }
    std::map<LLVKWidgetTree::Id,int> optionsById;
    LLVKWidgetTree::Id defaultButton=0,editor=0,ignore=0;
    int buttonLeft=(width-totalButtons)/2;
    const auto buttonFactory=std::make_unique<LLVKWidgetFactory>(*mDialogFactory);
    if (!buttonFactory->loadDefaultsFile(mTree,"alert_button.xml",error)) { discard(); return false; }
    for (const auto& option : notice.buttons)
    {
        auto name=option.name;
        LLStringUtil::replaceString(name,"&","&amp;");
        LLStringUtil::replaceString(name,"'","&apos;");
        LLStringUtil::replaceString(name,"<","&lt;");
        LLStringUtil::replaceString(name,">","&gt;");
        const auto button=buttonFactory->construct(mTree,"<button name='"+name+"' font='SansSerif'/>",*panel,error);
        if (!button) { discard(); return false; }
        if (!mTree.setShape(*button,{buttonLeft,16,buttonLeft+buttonWidth,39},error)) { discard(); return false; }
        const auto wideLabel=utf8str_to_wstring(option.label);
        mTree.setButtonLabel(*button,std::u32string(wideLabel.begin(),wideLabel.end()));
        LLVKControl::Callback close;
        close.function=[this,index=option.option](auto,const LLSD&) { respondNotice(index,mDialogError); };
        mTree.setControlCommit(*button,std::move(close));
        optionsById.emplace(*button,option.option);
        if (!defaultButton || option.isDefault) defaultButton=*button;
        buttonLeft+=buttonWidth+8;
    }
    if (!notice.inputName.empty())
    {
        const auto inputFactory=std::make_unique<LLVKWidgetFactory>(*mDialogFactory);
        if (!inputFactory->loadDefaultsFile(mTree,"alert_line_editor.xml",error)) { discard(); return false; }
        const auto input=inputFactory->construct(mTree,"<line_editor name='notification_input' width='200' height='20' max_length_bytes='1023'/>",*panel,error);
        const auto inputBottom=47+(hasIgnore ? 20 : 0);
        if (!input || !mTree.setShape(*input,{25,inputBottom,width-25,inputBottom+20},error)) { discard(); return false; }
        editor=*input;
    }
    if (hasIgnore)
    {
        LLStringUtil::replaceString(ignoreLabel,"&","&amp;");
        LLStringUtil::replaceString(ignoreLabel,"'","&apos;");
        LLStringUtil::replaceString(ignoreLabel,"<","&lt;");
        LLStringUtil::replaceString(ignoreLabel,">","&gt;");
        const auto checkFactory=std::make_unique<LLVKWidgetFactory>(*mDialogFactory);
        if (!checkFactory->loadDefaultsFile(mTree,"alert_check_box.xml",error)) { discard(); return false; }
        const auto check=checkFactory->construct(mTree,"<check_box name='notification_ignore' label='"+ignoreLabel+"' word_wrap='down'/>",*panel,error);
        const auto checkBottom=39+lineHeight/2;
        if (!check || !mTree.setShape(*check,{25,checkBottom,width-25,checkBottom+lineHeight*ignoreLines},error)) { discard(); return false; }
        ignore=*check;
    }
    mNoticePreviousFocus=mTree.keyboardFocus();
    if (mTree.mouseCapture() && !mTree.setMouseCapture(0,error)) { discard(); return false; }
    if (mTree.topControl() && !mTree.setTopControl(0,error)) { discard(); return false; }
    mMenu->dismiss();
    if (!mTree.setKeyboardFocus(*panel,true,false,error) || !mTree.requestControlFocus(editor ? editor : defaultButton,true,error))
    { mTree.unlockFocus(); discard(); return false; }
    mNoticePanel=*panel; mNoticeButton=defaultButton; mNoticeEditor=editor; mNoticeOpened=time;
    mNoticeIgnore=ignore;
    mNoticeOptions=std::move(optionsById); mActiveNotice=std::move(notice);
    mNotices.erase(mNotices.begin());
    return true;
}

bool LLVKViewerUi::respondNotice(int option,std::string& error)
{
    error.clear();
    if (!mActiveNotice || !mNoticePanel) return false;
    const auto found=std::find_if(mNoticeOptions.begin(),mNoticeOptions.end(),[option](const auto& item) { return item.second==option; });
    if (found==mNoticeOptions.end()) { error="Invalid native notice response"; return false; }
    if (found->first==mNoticeButton && mNoticeTime-mNoticeOpened<0.5) return true;
    LLSD values=LLSD::emptyMap();
    if (mNoticeEditor) values[mActiveNotice->inputName]=mTree.value(mNoticeEditor);
    if (mNoticeIgnore)
    {
        const auto& preference=mNotificationPreferences.at(mActiveNotice->name);
        const bool ignored=mTree.value(mNoticeIgnore).asBoolean();
        values["ignore"]=ignored;
        std::map<std::string,LLSD> changes;
        if (preference.control.empty()) changes[mActiveNotice->name]=!ignored;
        if (ignored && preference.saveResponse)
        {
            auto saved=preference.defaultResponse;
            for (const auto& [name,index] : preference.responseOptions) saved[name]=index==option;
            changes["Default"+mActiveNotice->name]=saved;
        }
        if (!preference.sessionOnly && mSaveWarningPreferences && !changes.empty() && !mSaveWarningPreferences(changes,error)) return false;
        for (const auto& [name,value] : changes)
            if (auto control=mWarningSettings->getControl(name)) control->setValue(value,!preference.sessionOnly);
        if (!preference.control.empty()) mTree.updateSetting(preference.control,LLSD(preference.inverted ? ignored : !ignored));
    }
    const auto response=mActiveNotice->response;
    if (!dismissNotice(error)) return false;
    if (response) response(option,values);
    return true;
}

bool LLVKViewerUi::dismissNotice(std::string& error)
{
    error.clear();
    if (!mNoticePanel) return false;
    const auto panel=mNoticePanel,focus=mNoticePreviousFocus;
    mTree.unlockFocus();
    mNoticePanel=mNoticeButton=mNoticePreviousFocus=0;
    mNoticeEditor=mNoticeIgnore=0; mNoticeOptions.clear(); mActiveNotice.reset();
    if (!mTree.erase(panel,error)) return false;
    return !mTree.get(focus) || mTree.requestControlFocus(focus,true,error);
}

bool LLVKViewerUi::noticeKey(bool returnKey,bool modified,std::string& error)
{
    if (!mNoticePanel) return false;
    if (returnKey && !modified && mNoticeTime-mNoticeOpened>=0.5)
    {
        const auto focused=mNoticeOptions.find(mTree.keyboardFocus());
        respondNotice(focused==mNoticeOptions.end() ? mNoticeOptions.at(mNoticeButton) : focused->second,error);
    }
    return true;
}

std::optional<LLVKWidgetTree::Id> LLVKViewerUi::constructPreferencePanel(const std::string& filename,
    LLVKWidgetTree::Id parent,std::string& error)
{
    error.clear();
    if (filename=="panel_preferences_controls.xml")
        return mDialogFactory->construct(mTree,"<panel class='panel_preference_controls' filename='panel_preferences_controls.xml'/>",parent,error);
    if (filename=="panel_preferences_sound.xml")
        return mDialogFactory->construct(mTree,"<panel class='panel_preference_sounds' filename='panel_preferences_sound.xml'/>",parent,error);
    if (filename=="panel_preferences_privacy.xml")
        return mDialogFactory->construct(mTree,"<panel class='panel_preference_privacy' filename='panel_preferences_privacy.xml'/>",parent,error);
    if (filename=="panel_preferences_skins.xml")
        return mDialogFactory->construct(mTree,"<panel class='panel_preference_skins' filename='panel_preferences_skins.xml'/>",parent,error);
    if (filename=="panel_preferences_graphics1.xml")
        return mDialogFactory->construct(mTree,"<panel class='panel_preference_graphics' filename='panel_preferences_graphics1.xml'/>",parent,error);
    if (filename=="panel_preferences_firestorm.xml")
        return mDialogFactory->construct(mTree,"<panel class='panel_preference_firestorm' filename='panel_preferences_firestorm.xml'/>",parent,error);
    if (filename=="panel_preferences_backup.xml")
        return mDialogFactory->construct(mTree,"<panel class='panel_preference_backup' filename='panel_preferences_backup.xml'/>",parent,error);
    if (filename=="panel_fs_block_list_sidetray.xml")
        return mDialogFactory->construct(mTree,"<panel class='fs_panel_block_list_sidetray' filename='panel_fs_block_list_sidetray.xml'/>",parent,error);
    if (filename=="panel_preferences_alerts.xml" || filename=="panel_preferences_setup.xml")
        return mDialogFactory->construct(mTree,"<panel class='panel_preference' filename='"+filename+"'/>",parent,error);
    if (filename!="panel_preferences_colors.xml" && filename!="panel_preferences_general.xml" && filename!="panel_preferences_chat.xml" && filename!="panel_preferences_move.xml" && filename!="panel_preferences_crashreports.xml" && filename!="panel_preferences_advanced.xml" && filename!="panel_preferences_UI.xml")
    { error="Native Preferences panel application policy is not implemented: "+filename; return std::nullopt; }
    return mDialogFactory->construct(mTree,"<panel class='panel_preference' filename='"+filename+"'/>",parent,error);
}

bool LLVKViewerUi::loadNotificationPreferences(const std::string& xml,const Configuration& configuration,std::string& error)
{
    error.clear();
    mWarningSettings=configuration.warningSettingsGroup;
    if (!mWarningSettings)
    {
        mOwnedWarningSettings=std::make_unique<LLControlGroup>("NativeWarnings-"+LLUUID::generateNewID().asString());
        mWarningSettings=mOwnedWarningSettings.get();
    }
    mSaveWarningPreferences=configuration.saveWarningPreferences;
    try
    {
        using Tree=boost::property_tree::ptree;
        Tree document;
        std::istringstream stream(xml);
        boost::property_tree::read_xml(stream,document,boost::property_tree::xml_parser::trim_whitespace);
        std::map<std::string,Tree> templates;
        const auto& root=document.get_child("notifications");
        for (const auto& [tag,node] : root)
            if (tag=="template") templates[node.get<std::string>("<xmlattr>.name","")]=node;
        for (const auto& [tag,node] : root)
        {
            if (tag!="notification") continue;
            const auto name=node.get<std::string>("<xmlattr>.name","");
            std::optional<Tree> ignore;
            std::optional<Tree> form;
            std::map<std::string,std::string> substitutions;
            for (const auto& [childTag,child] : node)
            {
                if (childTag=="form")
                { form=child; if (const auto value=child.get_child_optional("ignore")) ignore=*value; }
                else if (childTag=="usetemplate")
                {
                    const auto found=templates.find(child.get<std::string>("<xmlattr>.name",""));
                    if (found==templates.end()) continue;
                    if (const auto value=found->second.get_child_optional("form")) form=*value;
                    if (const auto value=found->second.get_child_optional("form.ignore")) ignore=*value;
                    if (const auto attrs=child.get_child_optional("<xmlattr>"))
                        for (const auto& [key,value] : *attrs) substitutions[key]=value.data();
                }
            }
            if (!ignore || ignore->get<bool>("<xmlattr>.checkbox_only",false)) continue;
            NotificationPreference preference;
            preference.label=ignore->get<std::string>("<xmlattr>.text","");
            for (const auto& [key,value] : substitutions) LLStringUtil::replaceString(preference.label,"$"+key,value);
            preference.control=ignore->get<std::string>("<xmlattr>.control","");
            preference.inverted=ignore->get<bool>("<xmlattr>.invert_control",false);
            preference.sessionOnly=ignore->get<bool>("<xmlattr>.session_only",false);
            preference.saveResponse=ignore->get<bool>("<xmlattr>.save_option",false);
            preference.defaultResponse=LLSD::emptyMap();
            if (form)
                for (const auto& [elementTag,element] : *form)
                {
                    const auto button=element.get<std::string>("<xmlattr>.name","");
                    if (elementTag=="button")
                    {
                        const int option=element.get<int>("<xmlattr>.index",0);
                        const bool selected=element.get<bool>("<xmlattr>.default",false);
                        preference.responseOptions[button]=option;
                        preference.defaultResponse[button]=selected;
                        if (selected) preference.defaultOption=option;
                    }
                    else if (elementTag=="input") preference.defaultResponse[button]=element.get<std::string>("<xmlattr>.value","");
                }
            if (preference.control.empty())
            {
                if (!mWarningSettings->getControl(name)) mWarningSettings->declareBOOL(name,true,"Show notification with this name");
                if (ignore->get<bool>("<xmlattr>.save_option",false) && !mWarningSettings->getControl("Default"+name))
                    mWarningSettings->declareLLSD("Default"+name,LLSD(""),"Default response for notification "+name);
            }
            mNotificationPreferences[name]=std::move(preference);
        }
    }
    catch (const std::exception& failure) { error=std::string("Native notification preference parsing failed: ")+failure.what(); return false; }
    return true;
}

bool LLVKViewerUi::refreshNotificationPreferences(LLVKWidgetTree::Id panel,bool fromList,std::string& error)
{
    error.clear();
    const auto fields=preferenceFields(mTree,panel);
    if (!fields.contains("all_popups") || !fields.contains("popup_filter")) { error="Native notification preferences are incomplete"; return false; }
    const auto list=fields.at("all_popups");
    if (fromList)
    {
        const auto rows=mTree.get(list)->scrollList->rows;
        std::map<std::string,LLSD> changes;
        for (const auto& row : rows) if (row.selected)
        {
            const auto found=mNotificationPreferences.find(row.value.asString());
            if (found==mNotificationPreferences.end()) continue;
            const bool show=row.cells[1]=="true";
            if (found->second.control.empty()) changes[found->first]=LLSD(show);
            else mTree.updateSetting(found->second.control,LLSD(found->second.inverted ? !show : show));
        }
        const bool draft=mPreferences && mPreferences->visible();
        if (!draft && !changes.empty() && mSaveWarningPreferences && !mSaveWarningPreferences(changes,error)) return false;
        for (const auto& [name,value] : changes) mWarningSettings->getControl(name)->setValue(value,!draft);
        return true;
    }
    auto filter=mTree.value(fields.at("popup_filter")).asString(); LLStringUtil::toLower(filter);
    LLVKWidgetTree::ListCellStyle check; check.type=LLVKWidgetTree::ListCellStyle::Type::CheckBox;
    check.image=mTree.findImage("Checkbox_Off",error); check.checkedImage=mTree.findImage("Checkbox_On",error);
    check.disabledImage=mTree.findImage("Checkbox_Off_Disabled",error); check.disabledCheckedImage=mTree.findImage("Checkbox_On_Disabled",error);
    if (!check.image || !check.checkedImage || !check.disabledImage || !check.disabledCheckedImage) return false;
    std::vector<LLVKWidgetTree::ListRow> rows;
    for (const auto& [name,preference] : mNotificationPreferences)
    {
        auto label=preference.label; LLStringUtil::toLower(label);
        if (!filter.empty() && label.find(filter)==std::string::npos) continue;
        bool show=true;
        if (preference.control.empty()) show=mWarningSettings->getBOOL(name);
        else if (const auto value=mTree.setting(preference.control)) show=preference.inverted ? !value->asBoolean() : value->asBoolean();
        LLVKWidgetTree::ListRow row{LLSD(name),{"",show ? "true" : "false",preference.label}};
        row.styles={{},check,{}}; rows.push_back(std::move(row));
    }
    return mTree.setScrollListRows(list,std::move(rows),error);
}

bool LLVKViewerUi::initializeBlockList(LLVKWidgetTree::Id panel,std::string& error)
{
    BlockListPanel state; state.fields=preferenceFields(mTree,panel);
    for (const auto name : {"block_list","block_limit","blocked_filter_input","unblock_btn","blocked_gear_btn"})
        if (!state.fields.contains(name)) { error=std::string("Missing native Block List control: ")+name; return false; }
    mBlockListPanels[panel]=std::move(state);
    const auto& fields=mBlockListPanels.at(panel).fields;
    LLVKControl::Callback filter;
    filter.function=[this,panel](auto,const LLSD&) { refreshBlockList(panel,mDialogError); };
    mTree.setControlCommit(fields.at("blocked_filter_input"),std::move(filter));
    LLVKControl::Callback selected;
    selected.function=[this,panel](auto,const LLSD&)
    {
        const auto& fields=mBlockListPanels.at(panel).fields;
        const bool selection=blockListPredicate(panel,"unblock_item","Enable");
        mTree.setEnabled(fields.at("unblock_btn"),selection); mTree.setEnabled(fields.at("blocked_gear_btn"),selection);
    };
    mTree.setControlCommit(fields.at("block_list"),std::move(selected));
    mTree.setScrollListCommitOnSelection(fields.at("block_list"),true);
    LLVKControl::Callback remove;
    remove.function=[this,panel](auto,const LLSD&) { blockListAction(panel,"unblock_item"); };
    mTree.setControlCommit(fields.at("unblock_btn"),std::move(remove));
    return refreshBlockList(panel,error);
}

bool LLVKViewerUi::refreshBlockList(LLVKWidgetTree::Id panel,std::string& error)
{
    error.clear();
    auto& state=mBlockListPanels.at(panel);
    state.entries=mMuteList.entries();
    auto filter=mTree.value(state.fields.at("blocked_filter_input")).asString(); LLStringUtil::trimHead(filter); LLStringUtil::toUpper(filter);
    std::vector<LLVKWidgetTree::ListRow> rows;
    const std::array<const char*,5> types{"MuteByName","MuteAgent","MuteObject","MuteGroup","MuteExternal"};
    for (std::size_t index=0; index<state.entries.size(); ++index)
    {
        const auto& entry=state.entries[index]; auto name=entry.name; LLStringUtil::toUpper(name);
        if (!filter.empty() && name.find(filter)==std::string::npos) continue;
        const auto type=static_cast<std::size_t>(entry.type);
        if (type>=types.size()) { error="Invalid native mute entry type"; return false; }
        const auto label=mAboutStrings.find(types[type]);
        rows.push_back({LLSD(static_cast<int>(index)),{entry.name,label==mAboutStrings.end() ? types[type] : label->second,
            std::to_string(type),entry.id.asString()}});
    }
    const auto list=state.fields.at("block_list");
    if (!mTree.setScrollListRows(list,std::move(rows),error)) return false;
    const auto order=mTree.setting("BlockPeopleSortOrder").value_or(LLSD(0)).asInteger();
    if (!mTree.sortScrollList(list,order%2==0 ? 0 : 1,order<2,error)) return false;
    if (!mTree.setPlainTextArgument(state.fields.at("block_limit"),"[COUNT]",std::to_string(state.entries.size()),error) ||
        !mTree.setPlainTextArgument(state.fields.at("block_limit"),"[LIMIT]",mTree.setting("MuteListLimit").value_or(LLSD(1000)).asString(),error)) return false;
    mTree.setEnabled(state.fields.at("unblock_btn"),false); mTree.setEnabled(state.fields.at("blocked_gear_btn"),false);
    return true;
}

bool LLVKViewerUi::blockListPredicate(LLVKWidgetTree::Id panel,const std::string& action,const std::string& kind) const
{
    const auto state=mBlockListPanels.find(panel);
    if (state==mBlockListPanels.end()) return false;
    const auto* list=mTree.get(state->second.fields.at("block_list"));
    if (!list || !list->scrollList) return false;
    const LLVKMuteList::Entry* selected=nullptr; int count=0;
    for (const auto& row : list->scrollList->rows) if (row.selected)
    {
        ++count; const auto index=row.value.asInteger();
        if (index>=0 && static_cast<std::size_t>(index)<state->second.entries.size()) selected=&state->second.entries[index];
    }
    if (kind=="Enable" && action=="unblock_item") return count>0;
    if (kind=="Check" && action.starts_with("sort_by_"))
    {
        const auto order=mTree.setting("BlockPeopleSortOrder").value_or(LLSD(0)).asInteger();
        const std::array<std::string,4> names{"sort_by_name","sort_by_type","sort_by_name_desc","sort_by_type_desc"};
        return order>=0 && order<4 && names[order]==action;
    }
    const bool agent=count==1 && selected && selected->type==LLVKMuteList::Type::Agent;
    if (kind=="Enable" || kind=="Visible") return agent;
    if (!selected) return false;
    const auto flag=action=="block_voice" ? LLVKMuteList::Voice : action=="block_text" ? LLVKMuteList::Text :
        action=="block_particles" ? LLVKMuteList::Particles : action=="block_obj_sounds" ? LLVKMuteList::Sounds : 0;
    return flag && (selected->allowed&flag)==0;
}

void LLVKViewerUi::blockListAction(LLVKWidgetTree::Id panel,const std::string& action)
{
    const auto state=mBlockListPanels.find(panel);
    if (state==mBlockListPanels.end()) return;
    if (action=="block_obj_by_name") { showBlockObjectName(panel,mDialogError); return; }
    const std::array<std::string,4> sorts{"sort_by_name","sort_by_type","sort_by_name_desc","sort_by_type_desc"};
    const auto sort=std::find(sorts.begin(),sorts.end(),action);
    if (sort!=sorts.end())
    { mTree.updateSetting("BlockPeopleSortOrder",LLSD(static_cast<int>(sort-sorts.begin()))); refreshBlockList(panel,mDialogError); return; }
    const auto* list=mTree.get(state->second.fields.at("block_list"));
    if (!list || !list->scrollList) return;
    std::vector<LLVKMuteList::Entry> selected;
    for (const auto& row : list->scrollList->rows) if (row.selected)
    {
        const auto index=row.value.asInteger();
        if (index>=0 && static_cast<std::size_t>(index)<state->second.entries.size()) selected.push_back(state->second.entries[index]);
    }
    if (action=="unblock_item")
    {
        for (const auto& entry : selected) mMuteList.remove(entry.id,entry.name);
        refreshBlockList(panel,mDialogError); return;
    }
    const U32 flag=action=="block_voice" ? LLVKMuteList::Voice : action=="block_text" ? LLVKMuteList::Text :
        action=="block_particles" ? LLVKMuteList::Particles : action=="block_obj_sounds" ? LLVKMuteList::Sounds : 0;
    if (flag && selected.size()==1 && selected.front().type==LLVKMuteList::Type::Agent)
    {
        const auto& entry=selected.front();
        if ((entry.allowed&flag)==0) mMuteList.remove(entry.id,entry.name,flag);
        else if (!mMuteList.add(entry,flag,LLUUID::null,mTree.setting("MuteListLimit").value_or(LLSD(1000)).asInteger(),mDialogError)) return;
        refreshBlockList(panel,mDialogError); return;
    }
    mDialogError="Native block-list account action is not yet available: "+action;
}

bool LLVKViewerUi::showBlockList(std::string& error)
{
    error.clear();
    if (!mTree.setting("FSUseStandaloneBlocklistFloater").value_or(LLSD(false)).asBoolean())
    { error="Native People sidebar Block List route is not yet integrated"; return false; }
    if (!mBlockList)
    {
        mBlockList=LLVKFloater::createFile(mTree,*mDialogFactory,mRoot,"floater_fs_blocklist.xml",error);
        if (!mBlockList) return false;
        mBlockList->onClose([this]
        { if (mBlockObjectName) mBlockObjectName->close(mDialogError); });
    }
    const auto fields=preferenceFields(mTree,mBlockList->id());
    const auto panel=fields.find("panel_block_list_sidetray");
    if (panel==fields.end() || !refreshBlockList(panel->second,error)) return false;
    mActiveFloater=mBlockList.get();
    return mBlockList->open(error);
}

bool LLVKViewerUi::showBlockObjectName(LLVKWidgetTree::Id panel,std::string& error)
{
    error.clear();
    if (!mBlockObjectName)
    {
        mBlockObjectName=LLVKFloater::createFile(mTree,*mDialogFactory,mRoot,"floater_mute_object.xml",error);
        if (!mBlockObjectName) return false;
        const auto fields=preferenceFields(mTree,mBlockObjectName->id());
        for (const auto name : {"object_name","OK","Cancel"})
            if (!fields.contains(name)) { error="Native block-object dialog is incomplete"; mBlockObjectName.reset(); return false; }
        LLVKControl::Callback accept;
        accept.function=[this,editor=fields.at("object_name")](auto,const LLSD&)
        {
            const auto name=mTree.value(editor).asString();
            if (!name.empty())
            {
                if (mMuteList.add({LLUUID::null,name,LLVKMuteList::Type::Name},0,LLUUID::null,
                    mTree.setting("MuteListLimit").value_or(LLSD(1000)).asInteger(),mDialogError))
                {
                    if (mTree.get(mBlockObjectPanel)) refreshBlockList(mBlockObjectPanel,mDialogError);
                }
            }
            std::string ignored; mBlockObjectName->close(ignored);
        };
        mTree.setControlCommit(fields.at("OK"),accept);
        mTree.setControlCommit(fields.at("object_name"),std::move(accept));
        LLVKControl::Callback cancel;
        cancel.function=[this](auto,const LLSD&) { mBlockObjectName->close(mDialogError); };
        mTree.setControlCommit(fields.at("Cancel"),std::move(cancel));
        if (!mTree.setPanelDefaultButton(mBlockObjectName->id(),fields.at("OK"),error)) return false;
    }
    mBlockObjectPanel=panel; mActiveFloater=mBlockObjectName.get();
    return mBlockObjectName->open(error);
}

void LLVKViewerUi::showButtonMenu(LLVKWidgetTree::Id button,const std::string& filename,const std::string& position,
    const LLVKWidgetFactory::Callbacks& callbacks)
{
    if (mMenuButton==button && mMenu->open()) { mMenu->dismiss(); return; }
    const auto files=mSkin->read("xui",filename,LLVKSkinFiles::Policy::Current,mDialogError);
    if (!files) return;
    std::vector<std::string_view> layers;
    for (const auto& file : *files) layers.push_back(file);
    const auto xml=LLVKXmlLayers::merge(layers,mDialogError);
    if (!xml) return;
    const auto* node=mTree.get(button);
    if (!node || !node->control) return;
    const auto menu=LLVKMenu::create(*xml,node->control->params.font,mColors,{},false,mDialogError);
    if (!menu) return;
    auto items=menu->items();
    for (auto& item : items)
    {
        const auto predicate=[&](const std::string& action,const std::string& parameter,bool fallback)
        {
            if (action.empty()) return fallback;
            const auto found=callbacks.predicates.find(action);
            if (found==callbacks.predicates.end()) { mDialogError="Missing native menu predicate: "+action; return false; }
            return found->second(button,LLSD(parameter));
        };
        item.visible=predicate(item.visibleAction,item.visibleParameter,item.visible);
        item.enabled=predicate(item.enableAction,item.enableParameter,item.enabled);
        item.checked=predicate(item.checkAction,item.checkParameter,item.checkable);
        if (!mDialogError.empty()) return;
        if (item.separator) continue;
        const auto action=callbacks.actions.find(item.action);
        if (action==callbacks.actions.end()) { mDialogError="Missing native menu action: "+item.action; return; }
        item.invoke=[this,button,handler=action->second,parameter=item.parameter]
        { if (mTree.get(button)) handler(button,LLSD(parameter)); };
    }
    const auto rect=mTree.screenRect(button,mDialogError);
    if (!rect) return;
    if (!mMenu->showPopup(std::move(items),*rect,position,[this,button]
        { mTree.setButtonForcePressed(button,false); mMenuButton=0; },mDialogError)) return;
    mMenuButton=button; mTree.setButtonForcePressed(button,true);
    if (mTree.mouseCapture()) mTree.setMouseCapture(0,mDialogError);
}

bool LLVKViewerUi::initializePrivacyPreferences(LLVKWidgetTree::Id panel,std::string& error)
{
    error.clear();
    const auto fields=preferenceFields(mTree,panel);
    for (const auto name : {"showlookat","autoresponse_item","clear_autoresponse_item"})
        if (!fields.contains(name)) { error=std::string("Missing native Privacy control: ")+name; return false; }
    for (const auto& [setting,label] : {std::pair{"DoNotDisturbModeResponse","DoNotDisturbModeResponseDefault"},
        std::pair{"FSAutorespondModeResponse","AutoResponseModeDefault"},std::pair{"FSAutorespondNonFriendsResponse","AutoResponseModeNonFriendsDefault"},
        std::pair{"FSRejectTeleportOffersResponse","RejectTeleportOffersResponseDefault"},std::pair{"FSRejectFriendshipRequestsResponse","RejectFriendshipRequestsResponseDefault"},
        std::pair{"FSMutedAvatarResponse","MutedAvatarsResponseDefault"},std::pair{"FSAwayAvatarResponse","AwayAvatarResponseDefault"}})
    {
        const auto text=mAboutStrings.find(label);
        if (text==mAboutStrings.end() || !mTree.updateSetting(setting,LLSD(text->second)))
        { error=std::string("Native localized autoresponse setting is unavailable: ")+setting; return false; }
    }
    const auto look=fields.at("showlookat");
    mTree.setValue(look,LLSD(mTree.setting("DebugLookAt").value_or(LLSD(0)).asInteger()!=0));
    if (!mTree.subscribeSetting("DebugLookAt",[this,look](const LLSD& value,const LLSD&)
        { if (mTree.get(look)) mTree.setValue(look,LLSD(value.asInteger()!=0)); }))
    { error="Native DebugLookAt setting is unavailable"; return false; }
    LLVKControl::Callback changed;
    changed.function=[this,look](auto,const LLSD&) { mTree.updateSetting("DebugLookAt",LLSD(int(mTree.value(look).asBoolean()))); };
    mTree.setControlCommit(look,std::move(changed));
    const auto notLoggedIn=mTree.panelString(panel,"AutoresponseItemNotLoggedIn",{},error);
    if (!notLoggedIn || !mTree.setValue(fields.at("autoresponse_item"),LLSD(*notLoggedIn))) return false;
    mTree.setEnabled(fields.at("clear_autoresponse_item"),false);
    mTree.setPreferenceSnapshotAllowlist(panel,std::set<std::string>{"AutoDisengageMic"});
    for (const auto name : {"clear_webcache","clear_log","delete_transcripts","conversation_log_combo","LogNearbyChat","voice_call_friends_only_check","online_visibility","favorites_on_login_check"})
        if (fields.contains(name)) mTree.setEnabled(fields.at(name),false);
    return initializeStartupPreferencePanel(panel,error);
}

bool LLVKViewerUi::publishMediaRules(LLVKMediaFilter updated,std::string& error)
{
    error.clear();
    if (mSaveMediaFilterRules && !mSaveMediaFilterRules(updated.rules(),error)) return false;
    mMediaFilter=std::move(updated);
    return refreshMediaLists(error);
}

bool LLVKViewerUi::refreshMediaLists(std::string& error)
{
    if (!mMediaLists) return true;
    const auto fields=preferenceFields(mTree,mMediaLists->id());
    for (const bool allow : {true,false})
    {
        std::vector<LLVKWidgetTree::ListRow> rows;
        for (auto rule=mMediaFilter.rules().beginArray(); rule!=mMediaFilter.rules().endArray(); ++rule)
            if ((*rule)["action"].asString()==(allow ? "allow" : "deny"))
                rows.push_back({(*rule)["domain"],{(*rule)["domain"].asString()}});
        const auto list=fields.at(allow ? "whitelist" : "blacklist");
        if (!mTree.setScrollListRows(list,std::move(rows),error) || !mTree.sortScrollList(list,0,true,error)) return false;
    }
    return true;
}

bool LLVKViewerUi::showMediaLists(std::string& error)
{
    error.clear();
    if (!mMediaLists)
    {
        mMediaLists=LLVKFloater::createFile(mTree,*mDialogFactory,mRoot,"floater_media_lists.xml",error);
        if (!mMediaLists) return false;
        const auto fields=preferenceFields(mTree,mMediaLists->id());
        for (const auto name : {"whitelist","blacklist","add_whitelist","add_blacklist","remove_whitelist","remove_blacklist"})
            if (!fields.contains(name)) { error=std::string("Missing native media-list control: ")+name; mMediaLists.reset(); return false; }
        for (const bool allow : {true,false})
        {
            const auto suffix=allow ? "whitelist" : "blacklist";
            LLVKControl::Callback add;
            add.function=[this,allow](auto,const LLSD&)
            {
                LLSD arguments; arguments["LIST"]=mAboutStrings.at(allow ? "MediaFilterWhitelist" : "MediaFilterBlacklist");
                queueNotice("AddToMediaList",arguments,[this,allow](int option,const LLSD& response)
                {
                    if (option!=0 || LLVKMediaFilter::domain(response["url"].asString()).empty()) return;
                    auto updated=mMediaFilter;
                    if (updated.add(response["url"].asString(),allow ? LLVKMediaFilter::Action::Allow : LLVKMediaFilter::Action::Deny,mDialogError))
                        publishMediaRules(std::move(updated),mDialogError);
                },mDialogError);
            };
            mTree.setControlCommit(fields.at(std::string("add_")+suffix),std::move(add));
            LLVKControl::Callback remove;
            remove.function=[this,list=fields.at(suffix)](auto,const LLSD&)
            {
                const auto selected=mTree.value(list);
                if (selected.isUndefined()) return;
                auto updated=mMediaFilter;
                if (updated.remove(selected.asString())) publishMediaRules(std::move(updated),mDialogError);
            };
            mTree.setControlCommit(fields.at(std::string("remove_")+suffix),std::move(remove));
        }
    }
    if (!refreshMediaLists(error)) return false;
    mActiveFloater=mMediaLists.get();
    return mMediaLists->open(error);
}

bool LLVKViewerUi::initializeVoiceDevices(LLVKWidgetTree::Id panel,std::string& error)
{
    VoiceDevicePanel state; state.fields=preferenceFields(mTree,panel);
    for (const auto name : {"voice_input_device","voice_output_device","mic_volume_slider","unmute_btn","wait_text","disabled_text"})
        if (!state.fields.contains(name)) { error=std::string("Missing native voice device control: ")+name; return false; }
    state.input=mTree.setting("VoiceInputAudioDevice").value_or(LLSD("Default")).asString();
    state.output=mTree.setting("VoiceOutputAudioDevice").value_or(LLSD("Default")).asString();
    state.gain=static_cast<float>(mTree.setting("AudioLevelMic").value_or(LLSD(1.f)).asReal());
    for (int index=0; index<5; ++index)
    {
        const auto name="bar"+std::to_string(index);
        if (!state.fields.contains(name)) { error="Missing native voice meter anchor: "+name; return false; }
        const auto anchor=state.fields.at(name);
        const auto* node=mTree.get(anchor);
        const auto width=node->params.rect.right-node->params.rect.left,height=node->params.rect.top-node->params.rect.bottom;
        LLVKWidgetTree::Params view; view.name="native_voice_meter"; view.rect={0,0,width,height}; view.visible=false; view.mouseOpaque=false;
        LLVKControl::Params control; control.font=node->control->params.font; control.tabStop=false;
        LLVKPanel::Params background; background.backgroundVisible=true; background.backgroundOpaque=true; background.opaqueColor=LLVKColor{0.5f,0.5f,0.5f,1.f};
        const auto outer=mTree.createPanel(view,control,background,anchor,error);
        if (!outer) return false;
        const auto colorName="NativeVoiceMeter"+std::to_string(*outer);
        mColors->setRuntime(colorName,{0.f,0.f,0.f,1.f});
        background.opaqueColor=*mColors->find(colorName);
        view.name="native_voice_meter_fill"; view.visible=true; view.rect={1,1,width-1,height-1};
        if (!mTree.createPanel(view,control,background,*outer,error)) return false;
        state.fields["meter"+std::to_string(index)]=*outer;
    }
    for (const bool input : {true,false})
    {
        const auto id=state.fields.at(input ? "voice_input_device" : "voice_output_device");
        LLVKControl::Callback callback;
        callback.function=[this,panel,input](auto id,const LLSD&)
        {
            const auto selected=mTree.value(id).asString();
            if (!mVoiceDeviceServices.select) { mDialogError="Native voice device service is not bound"; return; }
            if (!mVoiceDeviceServices.select(input,selected,mDialogError)) return;
            mTree.updateSetting(input ? "VoiceInputAudioDevice" : "VoiceOutputAudioDevice",LLSD(selected));
            auto& state=mVoiceDevicePanels.at(panel);
            (input ? state.input : state.output)=selected;
        };
        mTree.setControlCommit(id,std::move(callback));
    }
    LLVKControl::Callback unmute;
    unmute.function=[this](auto,const LLSD&) { mTree.updateSetting("EnableVoiceChat",LLSD(true)); };
    mTree.setControlCommit(state.fields.at("unmute_btn"),std::move(unmute));
    mVoiceDevicePanels[panel]=std::move(state);
    return true;
}

bool LLVKViewerUi::refreshVoiceDevices(std::string& error)
{
    error.clear();
    bool tuning=false;
    float gain=1.f;
    const bool enabled=mTree.setting("EnableVoiceChat").value_or(LLSD(false)).asBoolean() &&
        !mTree.setting("CmdLineDisableVoice").value_or(LLSD(false)).asBoolean();
    for (auto& [panel,state] : mVoiceDevicePanels)
    {
        if (!mTree.get(panel)) continue;
        const bool visible=mTree.visibleInChain(panel);
        if (!visible)
        {
            if (state.visible) mTree.updateSetting("ShowDeviceSettings",LLSD(false));
            state.visible=false; continue;
        }
        if (!mVoiceDeviceServices.refresh || !mVoiceDeviceServices.state || !mVoiceDeviceServices.tune)
        { error="Native voice device service is not bound"; return false; }
        if (!state.visible)
        {
            state.input=mTree.setting("VoiceInputAudioDevice").value_or(LLSD("Default")).asString();
            state.output=mTree.setting("VoiceOutputAudioDevice").value_or(LLSD("Default")).asString();
            state.generation=0;
            if (!mVoiceDeviceServices.refresh(error)) return false;
            state.visible=true;
        }
        const auto devices=mVoiceDeviceServices.state(error);
        if (!devices) return false;
        const bool available=!devices->inputs.empty() && !devices->outputs.empty();
        const auto& fields=state.fields;
        for (const auto name : {"voice_input_device","voice_output_device","mic_volume_slider"}) mTree.setEnabled(fields.at(name),available);
        mTree.setVisible(fields.at("wait_text"),enabled && !devices->tuning);
        mTree.setVisible(fields.at("disabled_text"),!enabled);
        mTree.setVisible(fields.at("unmute_btn"),false);
        const int power=std::clamp(static_cast<int>(std::floor(devices->energy/0.7f*5.f+0.1f)),0,5);
        for (int index=0; index<5; ++index)
        {
            const auto meter=fields.at("meter"+std::to_string(index));
            mTree.setVisible(meter,enabled && devices->tuning);
            const auto color=mColors->find(index<power ? (index>=3 ? "OverdrivenColor" : "SpeakingColor") : "PanelFocusBackgroundColor");
            if (!color) { error="Native voice meter color is missing"; return false; }
            mColors->setRuntime("NativeVoiceMeter"+std::to_string(meter),color->get());
        }
        if (!available || state.generation!=devices->generation)
        {
            for (const bool input : {true,false})
            {
                auto& selected=input ? state.input : state.output;
                const auto id=fields.at(input ? "voice_input_device" : "voice_output_device");
                const auto& choices=input ? devices->inputs : devices->outputs;
                std::vector<LLVKWidgetTree::ComboItem> items;
                const auto localize=[&](const std::string& label)
                {
                    if (label=="Default") return uiSoundString(panel,"default_text");
                    if (label=="No Device") return uiSoundString(panel,"name_no_device");
                    if (label=="Default System Device") return uiSoundString(panel,"name_default_system_device");
                    return label;
                };
                if (!available)
                {
                    const bool known=selected=="Default" || selected=="No Device" || selected=="Default System Device";
                    items.push_back({known ? localize(selected) : uiSoundString(panel,"device_not_loaded"),LLSD(selected)});
                }
                else
                {
                    items.push_back({localize("Default"),LLSD("Default")});
                    bool found=selected=="Default";
                    for (const auto& device : choices) { items.push_back({localize(device.label),LLSD(device.id)}); found|=device.id==selected; }
                    if (!found)
                    {
                        selected="Default";
                        mTree.updateSetting(input ? "VoiceInputAudioDevice" : "VoiceOutputAudioDevice",LLSD(selected));
                    }
                }
                if (!mTree.replaceComboItems(id,std::move(items),error) || !mTree.setValue(id,LLSD(selected))) return false;
            }
            state.generation=devices->generation;
        }
        tuning|=enabled;
        gain=static_cast<float>(mTree.value(fields.at("mic_volume_slider")).asReal());
    }
    return !mVoiceDeviceServices.tune || mVoiceDeviceServices.tune(tuning,gain,error);
}

bool LLVKViewerUi::initializeSoundPreferences(LLVKWidgetTree::Id panel,std::string& error)
{
    const auto fields=preferenceFields(mTree,panel);
    if (fields.contains("ui_sounds_list") && !initializeUiSounds(mTree.get(fields.at("ui_sounds_list"))->parent,error)) return false;
    if (fields.contains("output_device_settings_panel")) mTree.setVisible(fields.at("output_device_settings_panel"),false);
    for (const auto name : {"media_first_click_all","media_first_click_any","media_first_click_hud","media_first_click_own",
        "media_first_click_group","media_first_click_friend","media_first_click_land"})
    {
        if (!fields.contains(name)) { error=std::string("Missing native media interaction control: ")+name; return false; }
        LLVKControl::Callback callback;
        callback.function=[this,panel](auto,const LLSD&) { updateSoundPreferences(panel,"CommitInteraction",mDialogError); };
        mTree.setControlCommit(fields.at(name),std::move(callback));
    }
    if (!mTree.subscribeSetting("MediaFirstClickInteract",[this,panel](const LLSD&,const LLSD&)
        { if (mTree.get(panel)) updateSoundPreferences(panel,"RefreshInteraction",mDialogError); }))
    { error="Cannot subscribe native media interaction setting"; return false; }
    return updateSoundPreferences(panel,"RefreshInteraction",error) && updateSoundPreferences(panel,"SetSounds",error) &&
        updateSoundPreferences(panel,"updateMediaAutoPlayCheckbox",error) && initializeStartupPreferencePanel(panel,error);
}

bool LLVKViewerUi::updateSoundPreferences(LLVKWidgetTree::Id panel,const std::string& action,std::string& error)
{
    error.clear();
    const auto fields=preferenceFields(mTree,panel);
    if (action=="ResetVoice")
    {
        if (mTree.setting("EnableVoiceChat").value_or(LLSD(false)).asBoolean() && !mTree.setting("CmdLineDisableVoice").value_or(LLSD(false)).asBoolean())
        {
            mTree.updateSetting("EnableVoiceChat",LLSD(false));
            for (const auto name : {"enable_voice_check","enable_voice_check_volume"})
                if (fields.contains(name)) mTree.setEnabled(fields.at(name),false);
            mVoiceReset=std::pair{panel,mNoticeTime+5.0};
        }
        return true;
    }
    if (action=="SetSounds")
    {
        const bool muted=mTree.setting("MuteSounds").value_or(LLSD(false)).asBoolean() || mTree.setting("MuteAudio").value_or(LLSD(false)).asBoolean();
        for (const auto name : {"gesture_audio_play_btn","collisions_audio_play_btn"})
        {
            if (!fields.contains(name)) { error=std::string("Missing native audio control: ")+name; return false; }
            mTree.setEnabled(fields.at(name),!muted);
        }
        return true;
    }
    if (action=="updateMediaAutoPlayCheckbox")
    {
        for (const auto name : {"enable_music","enable_media","media_auto_play_combo"})
            if (!fields.contains(name)) { error=std::string("Missing native autoplay control: ")+name; return false; }
        return mTree.setEnabled(fields.at("media_auto_play_combo"),mTree.value(fields.at("enable_music")).asBoolean() || mTree.value(fields.at("enable_media")).asBoolean());
    }
    constexpr int any=32767,all=32768;
    const std::array<std::pair<const char*,int>,5> parts{{{"media_first_click_hud",1},{"media_first_click_own",2},
        {"media_first_click_friend",4},{"media_first_click_group",8},{"media_first_click_land",16}}};
    if (action=="CommitInteraction")
    {
        int mask=0;
        if (mTree.value(fields.at("media_first_click_all")).asBoolean()) mask=all|any;
        else if (mTree.value(fields.at("media_first_click_any")).asBoolean()) mask=any;
        else for (const auto& [name,bit] : parts) if (mTree.value(fields.at(name)).asBoolean()) mask|=bit;
        return mTree.updateSetting("MediaFirstClickInteract",LLSD(mask));
    }
    const auto mask=mTree.setting("MediaFirstClickInteract").value_or(LLSD(0)).asInteger();
    const bool allSelected=(mask&all)!=0,anySelected=(mask&any)==any;
    mTree.setValue(fields.at("media_first_click_all"),LLSD(allSelected));
    mTree.setValue(fields.at("media_first_click_any"),LLSD(anySelected));
    mTree.setEnabled(fields.at("media_first_click_any"),!allSelected);
    for (const auto& [name,bit] : parts)
    {
        mTree.setValue(fields.at(name),LLSD((mask&bit)!=0));
        mTree.setEnabled(fields.at(name),!allSelected && !anySelected);
    }
    return true;
}

std::string LLVKViewerUi::uiSoundString(LLVKWidgetTree::Id panel,const std::string& name) const
{
    while (const auto* node=mTree.get(panel))
    {
        std::string ignored;
        const auto text=mTree.panelString(panel,name,{},ignored);
        if (text) return *text;
        panel=node->parent;
    }
    return {};
}

bool LLVKViewerUi::initializeUiSounds(LLVKWidgetTree::Id panel,std::string& error)
{
    UiSoundPanel state; state.fields=preferenceFields(mTree,panel);
    for (const auto name : {"ui_sounds_list","ui_sound_filter","ui_sound_uuid","ui_sound_play_checkbox","ui_sound_play_combo",
        "ui_sound_selected_label","ui_sound_setting_name","ui_sound_preview","ui_sound_default"})
        if (!state.fields.contains(name)) { error=std::string("Missing native UI sound control: ")+name; return false; }
    for (const auto suffix : {"Alert","BadKeystroke","Click","ClickRelease","HealthReductionF","HealthReductionM",
        "MoneyChangeDown","MoneyChangeUp","NearbyChat","NewIncomingIMSession","NewIncomingGroupIMSession","NewIncomingConfIMSession",
        "StartIM","ChatMention","ObjectCreate","ObjectDelete","ObjectRezIn","ObjectRezOut","Snapshot","TeleportOut",
        "PieMenuAppear","PieMenuHide","PieMenuSliceHighlight0","PieMenuSliceHighlight1","PieMenuSliceHighlight2","PieMenuSliceHighlight3",
        "PieMenuSliceHighlight4","PieMenuSliceHighlight5","PieMenuSliceHighlight6","PieMenuSliceHighlight7","Typing","WindowClose","WindowOpen",
        "ScriptFloaterOpen","ScriptFloaterClose","FriendOnline","FriendOffline","FriendshipOffer","TeleportOffer","InventoryOffer",
        "IncomingVoiceCall","GroupInvitation","GroupNotice","QuestionExperience","InvalidOp","MovelockToggle","Footsteps","TrackerBeacon","MicToggle","Restart"})
    {
        const auto sound=std::string("UISnd")+suffix;
        if (!mTree.setting(sound) || !mTree.setting("PlayMode"+sound)) { error="Missing native UI sound setting: "+sound; return false; }
        state.sounds.push_back(sound);
    }
    const auto playLabel=uiSoundString(panel,"ui_sound_play_this");
    if (!playLabel.empty() && !mTree.setCheckBoxLabel(state.fields.at("ui_sound_play_checkbox"),playLabel,error)) return false;
    std::vector<LLVKWidgetTree::ComboItem> modes;
    for (const auto& [value,key] : {std::pair{1,"ui_sound_playmode_new_session"},std::pair{2,"ui_sound_playmode_every_message"},
        std::pair{3,"ui_sound_playmode_not_focus"},std::pair{0,"ui_sound_playmode_mute"}})
        modes.push_back({uiSoundString(panel,key),LLSD(value)});
    if (!mTree.replaceComboItems(state.fields.at("ui_sound_play_combo"),std::move(modes),error)) return false;
    mUiSoundPanels[panel]=std::move(state);
    LLVKControl::Callback preview;
    preview.function=[this,panel](auto,const LLSD&)
    { commitUiSound(panel,"SelectUISound"); commitUiSound(panel,"PreviewSelectedUISound"); };
    mTree.setScrollListActions(mUiSoundPanels.at(panel).fields.at("ui_sounds_list"),std::move(preview),
        [this,panel](auto list,int x,int y)
    {
        commitUiSound(panel,"SelectUISound");
        const auto& selected=mUiSoundPanels.at(panel).selected;
        if (selected.empty()) return;
        const auto rect=mTree.screenRect(list,mDialogError);
        if (!rect) return;
        LLVKMenu::Item copy;
        copy.name="copy_uuid"; copy.label=uiSoundString(panel,"ui_sound_copy_uuid"); copy.action="UISounds.CopyUUID"; copy.parameter=selected;
        mMenu->showContext({std::move(copy)},rect->left+x,rect->bottom+y,mDialogError);
    });
    mMenu->bind("UISounds.CopyUUID",[this](const auto&,const std::string& sound)
    {
        const auto value=mTree.setting(sound);
        if (!value || !mDialogClipboard) { mDialogError="Native UI sound clipboard is unavailable"; return; }
        const auto wide=utf8str_to_wstring(value->asString());
        mDialogClipboard->write(std::u32string(wide.begin(),wide.end()),false,mDialogError);
    });
    return refreshUiSounds(panel,true,error);
}

bool LLVKViewerUi::refreshUiSounds(LLVKWidgetTree::Id panel,bool rebuild,std::string& error)
{
    error.clear();
    auto& state=mUiSoundPanels.at(panel);
    const auto& fields=state.fields;
    const auto list=fields.at("ui_sounds_list");
    const auto comboSound=[](const std::string& sound)
    { return sound=="UISndNewIncomingIMSession" || sound=="UISndNewIncomingGroupIMSession" || sound=="UISndNewIncomingConfIMSession"; };
    if (rebuild)
    {
        std::vector<LLVKWidgetTree::ListRow> rows;
        auto filter=mTree.value(fields.at("ui_sound_filter")).asString(); LLStringUtil::toLower(filter);
        LLVKWidgetTree::ListCellStyle check;
        check.type=LLVKWidgetTree::ListCellStyle::Type::CheckBox;
        check.image=mTree.findImage("Checkbox_Off",error); check.checkedImage=mTree.findImage("Checkbox_On",error);
        check.disabledImage=mTree.findImage("Checkbox_Off_Disabled",error); check.disabledCheckedImage=mTree.findImage("Checkbox_On_Disabled",error);
        if (!check.image || !check.checkedImage || !check.disabledImage || !check.disabledCheckedImage) return false;
        for (const auto& sound : state.sounds)
        {
            auto label=uiSoundString(panel,"textFS"+sound.substr(5));
            if (label.empty()) label=sound;
            LLStringUtil::trim(label); if (!label.empty() && label.back()==':') label.pop_back();
            auto folded=label; LLStringUtil::toLower(folded);
            if (!filter.empty() && folded.find(filter)==std::string::npos) continue;
            const auto mode=mTree.setting("PlayMode"+sound).value();
            const bool enabled=sound=="UISndSnapshot" ? !mode.asBoolean() : mode.asBoolean();
            const char* key=enabled ? "ui_sound_play_this" : "ui_sound_playmode_mute";
            const bool combo=comboSound(sound);
            if (combo) key=mode.asInteger()==1 ? "ui_sound_playmode_new_session" : mode.asInteger()==2 ? "ui_sound_playmode_every_message" :
                mode.asInteger()==3 ? "ui_sound_playmode_not_focus" : "ui_sound_playmode_mute";
            LLVKWidgetTree::ListRow row{LLSD(sound),{label,combo ? "" : enabled ? "true" : "false",uiSoundString(panel,key)}};
            if (!combo) row.styles={{},check,{}};
            rows.push_back(std::move(row));
        }
        if (!mTree.setScrollListRows(list,rows,error)) return false;
        if (!state.selected.empty()) mTree.selectScrollListValue(list,LLSD(state.selected),true,error);
        if (mTree.value(list).isUndefined() && !rows.empty()) mTree.selectScrollListValue(list,rows.front().value,true,error);
    }
    state.selected=mTree.value(list).asString();
    const bool selected=!state.selected.empty();
    mTree.setEnabled(fields.at("ui_sound_uuid"),selected);
    mTree.setVisible(fields.at("ui_sound_play_checkbox"),selected && !comboSound(state.selected));
    mTree.setVisible(fields.at("ui_sound_play_combo"),selected && comboSound(state.selected));
    mTree.setValue(fields.at("ui_sound_setting_name"),LLSD(state.selected));
    std::string label=uiSoundString(panel,"ui_sound_select_prompt");
    for (const auto& row : mTree.get(list)->scrollList->rows) if (row.selected) { label=row.cells.front(); break; }
    mTree.setValue(fields.at("ui_sound_selected_label"),LLSD(label));
    mTree.setValue(fields.at("ui_sound_uuid"),selected ? mTree.setting(state.selected).value() : LLSD(""));
    if (selected)
    {
        const auto defaultValue=mSettingDefaults.find(state.selected);
        if (defaultValue!=mSettingDefaults.end()) mTree.setTooltip(fields.at("ui_sound_uuid"),defaultValue->second.asString());
        const auto mode=mTree.setting("PlayMode"+state.selected).value();
        mTree.setValue(fields.at("ui_sound_play_combo"),LLSD(mode.asInteger()));
        mTree.setValue(fields.at("ui_sound_play_checkbox"),LLSD(state.selected=="UISndSnapshot" ? !mode.asBoolean() : mode.asBoolean()));
    }
    return true;
}

bool LLVKViewerUi::publishUiSoundChanges(const std::map<std::string,LLSD>& changes,std::string& error)
{
    error.clear();
    std::map<std::string,LLSD> modified;
    for (const auto& [name,value] : changes)
    {
        const auto current=mTree.setting(name);
        if (!current) { error="Missing native UI sound setting: "+name; return false; }
        if (!llsd_equals(*current,value)) modified[name]=value;
    }
    if (modified.empty()) return true;
    if (mSavePreferences && !mSavePreferences(modified,error)) return false;
    for (const auto& [name,value] : modified)
        if (!mTree.updateSetting(name,value)) { error="Native UI sound update was rejected: "+name; return false; }
    return true;
}

void LLVKViewerUi::commitUiSound(LLVKWidgetTree::Id panel,const std::string& action)
{
    const auto found=mUiSoundPanels.find(panel);
    if (found==mUiSoundPanels.end() || !mTree.get(panel)) return;
    auto& state=found->second;
    const auto& fields=state.fields;
    if (action=="UpdateUISoundFilter") { refreshUiSounds(panel,true,mDialogError); return; }
    if (action=="SelectUISound")
    {
        for (const auto& row : mTree.get(fields.at("ui_sounds_list"))->scrollList->rows)
            if (row.selected && row.styles.size()>1 && row.styles[1].type==LLVKWidgetTree::ListCellStyle::Type::CheckBox)
            {
                const auto sound=row.value.asString(); const bool checked=row.cells[1]=="true";
                if (!publishUiSoundChanges({{"PlayMode"+sound,LLSD(sound=="UISndSnapshot" ? !checked : checked)}},mDialogError))
                { std::string ignored; refreshUiSounds(panel,true,ignored); return; }
            }
        state.selected=mTree.value(fields.at("ui_sounds_list")).asString();
        refreshUiSounds(panel,true,mDialogError); return;
    }
    if (state.selected.empty()) return;
    const auto sound=state.selected;
    if (action=="PreviewSelectedUISound") { previewUiSound(sound,mDialogError); return; }
    if (action=="CommitUISoundUUID")
    {
        if (!publishUiSoundChanges({{sound,mTree.value(fields.at("ui_sound_uuid"))}},mDialogError))
        { std::string ignored; refreshUiSounds(panel,false,ignored); }
        return;
    }
    std::map<std::string,LLSD> changes;
    if (action=="CommitUISoundPlayCheck")
    {
        const bool checked=mTree.value(fields.at("ui_sound_play_checkbox")).asBoolean();
        changes["PlayMode"+sound]=LLSD(sound=="UISndSnapshot" ? !checked : checked);
    }
    if (action=="CommitUISoundPlayCombo") changes["PlayMode"+sound]=LLSD(mTree.value(fields.at("ui_sound_play_combo")).asInteger());
    if (action=="ResetSelectedUISound")
    {
        for (const auto& name : {sound,"PlayMode"+sound})
        {
            const auto found=mSettingDefaults.find(name);
            if (found==mSettingDefaults.end()) { mDialogError="Native UI sound default is unavailable: "+name; return; }
            changes[name]=found->second;
        }
    }
    if (!publishUiSoundChanges(changes,mDialogError))
    { std::string ignored; refreshUiSounds(panel,true,ignored); return; }
    refreshUiSounds(panel,true,mDialogError);
}

bool LLVKViewerUi::initializeControlsPanel(LLVKWidgetTree::Id panel,std::string& error)
{
    const auto fields=preferenceFields(mTree,panel);
    for (const auto name : {"key_mode","controls_list","restore_defaults"})
        if (!fields.contains(name)) { error="Native Controls panel is incomplete"; return false; }
    LLVKControl::Callback changed;
    changed.function=[this,panel](auto,const LLSD&) { refreshControlsPanel(panel,mDialogError); };
    mTree.setControlCommit(fields.at("key_mode"),std::move(changed));
    LLVKControl::Callback selected;
    selected.function=[this,panel](auto,const LLSD&) { showKeyCapture(panel,mDialogError); };
    mTree.setControlCommit(fields.at("controls_list"),std::move(selected));
    LLVKControl::Callback defaults;
    defaults.function=[this,panel](auto,const LLSD&)
    {
        queueNotice("PreferenceControlsDefaults",{},[this,panel](int option,const LLSD&)
        {
            if (!mTree.get(panel) || option==2) return;
            const auto fields=preferenceFields(mTree,panel);
            const auto current=static_cast<LLVKKeyBindings::Mode>(mTree.value(fields.at("key_mode")).asInteger());
            for (int index=0; index<4; ++index)
            {
                const auto mode=static_cast<LLVKKeyBindings::Mode>(index);
                if (option==0 || mode==current) mBindings.setControls(mode,mDefaultBindings.controls(mode));
            }
            mTree.updateSetting("DoubleClickTeleport",LLSD(mBindings.handles(LLVKKeyBindings::Mode::ThirdPerson,"teleport_to",CLICK_DOUBLELEFT,KEY_NONE,0)));
            refreshControlsPanel(panel,mDialogError);
        },mDialogError);
    };
    mTree.setControlCommit(fields.at("restore_defaults"),std::move(defaults));
    if (!mTree.setValue(fields.at("key_mode"),LLSD(1))) return false;
    return refreshControlsPanel(panel,error);
}

bool LLVKViewerUi::refreshControlsPanel(LLVKWidgetTree::Id panel,std::string& error)
{
    error.clear();
    if (!mTree.get(panel)) return false;
    const auto fields=preferenceFields(mTree,panel);
    const auto modeIndex=mTree.value(fields.at("key_mode")).asInteger();
    if (modeIndex<0 || modeIndex>3) { error="Invalid native Controls mode"; return false; }
    const auto mode=static_cast<LLVKKeyBindings::Mode>(modeIndex);
    const auto contents=std::make_unique<LLVKWidgetTree::ScrollListParams>();
    if (!mDialogFactory->loadListContents(mTree,"control_table_contents_columns_basic.xml",*contents,error)) return false;
    const auto separator=[&]
    {
        LLVKWidgetTree::ListRow row; row.enabled=false; row.cells.resize(4); row.styles.resize(4);
        auto& style=row.styles.front(); style.type=LLVKWidgetTree::ListCellStyle::Type::Icon;
        style.image=mTree.findImage("menu_separator",error); style.imageColor=LLVKColor{0,0,0,0.7f}; style.alignment=LLVKButton::Align::Center;
        contents->rows.push_back(std::move(row));
    };
    std::vector<std::string> groups{"movement"};
    if (mode!=LLVKKeyBindings::Mode::FirstPerson) { groups.push_back("camera"); groups.push_back("editing"); }
    groups.push_back("media");
    for (std::size_t index=0; index<groups.size(); ++index)
    {
        if (index) separator();
        if (!error.empty() || !mDialogFactory->loadListContents(mTree,"control_table_contents_"+groups[index]+".xml",*contents,error)) return false;
    }
    const auto& bindings=mBindings.controls(mode);
    std::erase_if(contents->rows,[&](const auto& row)
    {
        const auto found=bindings.find(row.value.asString());
        return found!=bindings.end() && !found->second.assignable && found->second.binding.empty();
    });
    const auto translate=[this](const std::string& key)
    { const auto found=mAboutStrings.find(key); return found==mAboutStrings.end() ? key : found->second; };
    const auto font=mFonts->resolve({"SansSerif","Medium"},error);
    if (!font) return false;
    for (auto& row : contents->rows)
    {
        const auto command=row.value.asString();
        if (command.empty() || command=="menu_separator") continue;
        const auto found=bindings.find(command);
        row.enabled=found==bindings.end() || found->second.assignable;
        row.cells.resize(4); row.styles.resize(4);
        for (std::uint32_t index=0; index<3; ++index)
        {
            row.cells[index+1]=found==bindings.end() ? "" : LLVKKeyBindings::label(found->second.binding.getKeyData(index),translate);
            row.styles[index+1].font=font;
        }
    }
    const auto list=fields.at("controls_list");
    return mTree.setScrollListColumns(list,std::move(contents->columns),error) && mTree.setScrollListRows(list,std::move(contents->rows),error);
}

LLVKWidgetTree::Id LLVKViewerUi::keyCaptureDialog() const noexcept
{
    return mKeyCapture && mKeyCapture->visible() ? mKeyCapture->id() : 0;
}

bool LLVKViewerUi::reservedPreferenceKey(KEY key,MASK mask) const
{
    std::string name;
    if (key>='A' && key<='Z') name.assign(1,static_cast<char>(key));
    else if (key>=KEY_F1 && key<=KEY_F12) name="F"+std::to_string(key-KEY_F1+1);
    else return false;
    std::string shortcut;
    if (mask&MASK_CONTROL) shortcut+="control|";
    if (mask&MASK_ALT) shortcut+="alt|";
    if (mask&MASK_SHIFT) shortcut+="shift|";
    shortcut+=name;
    for (const auto& item : mMenu->items()) if (item.visible && item.shortcut==shortcut) return true;
    return false;
}

bool LLVKViewerUi::showKeyCapture(LLVKWidgetTree::Id panel,std::string& error)
{
    error.clear();
    if (!mTree.get(panel)) return false;
    const auto fields=preferenceFields(mTree,panel);
    const auto list=fields.at("controls_list");
    const auto& rows=mTree.get(list)->scrollList->rows;
    const auto selected=std::find_if(rows.begin(),rows.end(),[](const auto& row) { return row.selected; });
    if (selected==rows.end() || !selected->enabled || selected->value.asString().empty() || selected->selectedCell<=0) return true;
    const auto command=selected->value.asString();
    const auto slot=static_cast<std::uint32_t>(selected->selectedCell-1);
    const auto mode=static_cast<LLVKKeyBindings::Mode>(mTree.value(fields.at("key_mode")).asInteger());
    if (!mKeyCapture)
    {
        mKeyCapture=LLVKFloater::createFile(mTree,*mDialogFactory,mRoot,"floater_select_key.xml",error);
        if (!mKeyCapture) return false;
        mKeyCaptureFields=preferenceFields(mTree,mKeyCapture->id());
        for (const auto name : {"SetEmpty","Default","Cancel","apply_all","description"})
            if (!mKeyCaptureFields.contains(name)) { error="Native key selection dialog is incomplete"; mKeyCapture.reset(); return false; }
        for (const std::string action : {"SetEmpty","Default","Cancel"})
        {
            LLVKControl::Callback callback;
            callback.function=[this,action](auto,const LLSD&)
            {
                if (action!="Cancel")
                    applyCapturedBinding(LLKeyData(),action=="Default",mTree.value(mKeyCaptureFields.at("apply_all")).asBoolean(),mDialogError);
                mKeyCapture->close(mDialogError);
            };
            mTree.setControlCommit(mKeyCaptureFields.at(action),std::move(callback));
        }
        mKeyCapture->onClose([this]
        {
            mTree.unlockFocus();
            if (mTree.get(mKeyCapturePanel))
            {
                const auto fields=preferenceFields(mTree,mKeyCapturePanel);
                const auto list=fields.at("controls_list");
                auto rows=mTree.get(list)->scrollList->rows;
                for (auto& row : rows) row.selected=false;
                mTree.setScrollListRows(list,std::move(rows),mDialogError);
            }
            if (mTree.get(mKeyCapturePreviousFocus)) mTree.requestControlFocus(mKeyCapturePreviousFocus,true,mDialogError);
            mKeyCapturePanel=0; mLastModifierKey=KEY_NONE;
            mKeyClickDeadline.reset(); mKeyMouseRecorded=false;
        });
    }
    mKeyCapturePanel=panel; mKeyCaptureCommand=command; mKeyCaptureMode=mode; mKeyCaptureSlot=slot;
    mKeyCapturePreviousFocus=mTree.keyboardFocus(); mLastModifierKey=KEY_NONE;
    mKeyClickDeadline.reset(); mKeyMouseRecorded=false;
    const auto mouse=mTree.panelString(mKeyCapture->id(),"mouse",{},error);
    const auto keyboard=mTree.panelString(mKeyCapture->id(),"keyboard",{},error);
    if (!mouse || !keyboard) return false;
    const auto description=mTree.panelString(mKeyCapture->id(),"basic_description",{{"INPUT",*mouse+", "+*keyboard}},error);
    if (!description || !mTree.setValue(mKeyCaptureFields.at("description"),LLSD(*description))) return false;
    mActiveFloater=mKeyCapture.get();
    if (!mKeyCapture->open(error) || !mTree.setKeyboardFocus(mKeyCapture->id(),true,true,error)) return false;
    return mTree.requestControlFocus(mKeyCaptureFields.at("Cancel"),true,error);
}

bool LLVKViewerUi::applyCapturedBinding(const LLKeyData& data,bool defaults,bool allModes,std::string& error)
{
    error.clear();
    if (!mTree.get(mKeyCapturePanel)) return false;
    for (int index=0; index<4; ++index)
    {
        const auto mode=static_cast<LLVKKeyBindings::Mode>(index);
        if (!allModes && mode!=mKeyCaptureMode) continue;
        LLKeyData selected=data;
        if (defaults)
        {
            const auto& controls=mDefaultBindings.controls(mode);
            const auto found=controls.find(mKeyCaptureCommand);
            selected=found==controls.end() ? LLKeyData() : found->second.binding.getKeyData(mKeyCaptureSlot);
        }
        const auto& controls=mBindings.controls(mode);
        const auto found=controls.find(mKeyCaptureCommand);
        if (found!=controls.end() && !found->second.assignable) continue;
        if (!mBindings.assign(mode,mKeyCaptureCommand,mKeyCaptureSlot,selected,[this](const auto& key)
            { return reservedPreferenceKey(key.mKey,key.mMask); }))
        { error="Native control binding conflicts with a reserved input"; return false; }
    }
    mTree.updateSetting("DoubleClickTeleport",LLSD(mBindings.handles(LLVKKeyBindings::Mode::ThirdPerson,"teleport_to",CLICK_DOUBLELEFT,KEY_NONE,0)));
    return refreshControlsPanel(mKeyCapturePanel,error);
}

bool LLVKViewerUi::recordPreferenceMouse(const LLVKWidgetTree::PointerEvent& event,EMouseClickType click,bool down,MASK mask,std::string& error)
{
    error.clear();
    const auto dialog=keyCaptureDialog();
    if (!dialog) return false;
    if (mKeyMouseRecorded && !down) { mKeyCapture->close(error); return true; }
    if (click==CLICK_LEFT)
    {
        bool overControl=mTree.mouseCapture()!=0;
        for (const auto name : {"SetEmpty","Default","Cancel","apply_all"})
        {
            const auto rectangle=mTree.screenRect(mKeyCaptureFields.at(name),error);
            if (!rectangle) return false;
            overControl|=event.x>=rectangle->left && event.x<rectangle->right && event.y>=rectangle->bottom && event.y<rectangle->top;
        }
        if (overControl) { mTree.routePointer(dialog,event,error); return true; }
        if (down && !(mask&(MASK_SHIFT|MASK_CONTROL)) && !mKeyClickDeadline)
        { mKeyClickDeadline=event.time+0.7; mKeyClickMask=mask; }
        return true;
    }
    if (click==CLICK_RIGHT && mask==0) return true;
    mKeyClickDeadline.reset();
    if (!mKeyMouseRecorded)
    {
        if (!applyCapturedBinding(LLKeyData(click,KEY_NONE,mask,true),false,mTree.value(mKeyCaptureFields.at("apply_all")).asBoolean(),error)) return true;
        mKeyMouseRecorded=true;
    }
    if (!down) mKeyCapture->close(error);
    return true;
}

bool LLVKViewerUi::recordPreferenceKey(KEY key,MASK mask,bool down,std::string& error)
{
    error.clear();
    if (!down && key!=KEY_NONE && key==mCapturedReleaseKey) { mCapturedReleaseKey=KEY_NONE; return true; }
    if (!keyCaptureDialog()) return false;
    if (key==KEY_ESCAPE || (key=='Q' && mask==MASK_CONTROL))
    { if (down) mCapturedReleaseKey=key; mKeyCapture->close(error); return true; }
    if (key==KEY_DELETE)
    { if (down) mCapturedReleaseKey=key; applyCapturedBinding(LLKeyData(),false,false,error); mKeyCapture->close(mDialogError); return true; }
    if (key==KEY_NONE || key==KEY_RETURN || key==KEY_BACKSPACE) return true;
    if (key==KEY_CONTROL || key==KEY_SHIFT || key==KEY_ALT)
    {
        if (down) { mLastModifierKey=key; return true; }
        if (mLastModifierKey!=key) return true;
        mask &= ~(key==KEY_CONTROL ? MASK_CONTROL : key==KEY_SHIFT ? MASK_SHIFT : MASK_ALT);
    }
    else if (!down) return true;
    if (reservedPreferenceKey(key,mask))
    {
        const auto translate=[this](const std::string& name)
        { const auto found=mAboutStrings.find(name); return found==mAboutStrings.end() ? name : found->second; };
        const auto description=mTree.panelString(mKeyCapture->id(),"reserved_by_menu",
            {{"KEYSTR",LLVKKeyBindings::label(LLKeyData(CLICK_NONE,key,mask,true),translate)}},error);
        if (description) mTree.setValue(mKeyCaptureFields.at("description"),LLSD(*description));
        mLastModifierKey=KEY_NONE; return true;
    }
    applyCapturedBinding(LLKeyData(CLICK_NONE,key,mask,true),false,mTree.value(mKeyCaptureFields.at("apply_all")).asBoolean(),error);
    if (down) mCapturedReleaseKey=key;
    mKeyCapture->close(mDialogError);
    return true;
}

bool LLVKViewerUi::updateClickActions(LLVKWidgetTree::Id panel,bool fromControls,std::string& error)
{
    error.clear();
    const auto fields=preferenceFields(mTree,panel);
    if (!fields.contains("single_click_action_combo") || !fields.contains("double_click_action_combo"))
    { error="Native click-action controls are missing"; return false; }
    const auto single=fields.at("single_click_action_combo"),doubleClick=fields.at("double_click_action_combo");
    using Mode=LLVKKeyBindings::Mode;
    if (fromControls)
    {
        const auto singleAction=mTree.value(single).asInteger(),doubleAction=mTree.value(doubleClick).asInteger();
        if (!mBindings.setClickAction("walk_to",CLICK_LEFT,singleAction==1) ||
            !mBindings.setClickAction("walk_to",CLICK_DOUBLELEFT,doubleAction==1) ||
            !mBindings.setClickAction("teleport_to",CLICK_DOUBLELEFT,doubleAction==2))
        { error="Native click-action binding update failed"; return false; }
        mTree.updateSetting("DoubleClickTeleport",LLSD(mBindings.handles(Mode::ThirdPerson,"teleport_to",CLICK_DOUBLELEFT,KEY_NONE,0)));
        const auto controls=find("controls_list");
        if (controls && mTree.get(controls)->parent && !refreshControlsPanel(mTree.get(controls)->parent,error)) return false;
        return true;
    }
    const bool walk=mBindings.handles(Mode::ThirdPerson,"walk_to",CLICK_LEFT,KEY_NONE,0);
    const bool doubleWalk=mBindings.handles(Mode::ThirdPerson,"walk_to",CLICK_DOUBLELEFT,KEY_NONE,0);
    const bool teleport=mBindings.handles(Mode::ThirdPerson,"teleport_to",CLICK_DOUBLELEFT,KEY_NONE,0);
    return mTree.setValue(single,LLSD(int(walk))) && mTree.setValue(doubleClick,LLSD(teleport ? 2 : int(doubleWalk)));
}

bool LLVKViewerUi::initializeStartupPreferencePanel(LLVKWidgetTree::Id panel,std::string& error)
{
    error.clear();
    const auto fields=preferenceFields(mTree,panel);
    if (fields.contains("cache_location"))
    {
        const auto bytes=mCacheDirectory.u8string();
        const std::string path(bytes.begin(),bytes.end());
        mTree.setEnabled(fields.at("cache_location"),false);
        mTree.setValue(fields.at("cache_location"),LLSD(path));
        mTree.setTooltip(fields.at("cache_location"),path);
        for (const auto name : {"FSSoundCacheLocation","log_path_string-panelsetup"})
            if (fields.contains(name)) mTree.setEnabled(fields.at(name),false);
        for (const auto name : {"ClearInventoryCache","open_log_path_button-panelsetup","log_path_button-panelsetup","reset_logpath"})
            if (fields.contains(name)) mTree.setEnabled(fields.at(name),false);
    }
    if (fields.contains("skin_combobox"))
    {
        if (!refreshSkinPreferences(panel,true,false,error)) return false;
        for (const auto name : {"skin_combobox","theme_combobox"})
        {
            LLVKControl::Callback changed;
            changed.function=[this,panel,skin=std::string_view(name)=="skin_combobox"](auto,const LLSD&)
            { refreshSkinPreferences(panel,false,skin,mDialogError); };
            mTree.setControlCommit(fields.at(name),std::move(changed));
        }
    }
    if (fields.contains("all_popups") && !refreshNotificationPreferences(panel,false,error)) return false;
    if (fields.contains("OnlineOfflinetoNearbyChatHistory"))
        mTree.setEnabled(fields.at("OnlineOfflinetoNearbyChatHistory"),mTree.setting("OnlineOfflinetoNearbyChat").value_or(LLSD(false)).asBoolean());
    if (fields.contains("notify_growl_checkbox"))
    {
        mTree.setEnabled(fields.at("notify_growl_checkbox"),mDesktopNotificationsAvailable);
        const bool enabled=mDesktopNotificationsAvailable && mTree.setting("FSEnableGrowl").value_or(LLSD(false)).asBoolean();
        for (const auto name : {"notify_growl_always_checkbox","FSFilterGrowlKeywordDuplicateIMs"})
            if (fields.contains(name)) mTree.setEnabled(fields.at(name),enabled);
        LLVKControl::Callback changed;
        changed.function=[this,panel](auto,const LLSD&) { initializeStartupPreferencePanel(panel,mDialogError); };
        mTree.setControlCommit(fields.at("notify_growl_checkbox"),std::move(changed));
    }
    if (fields.contains("textFriendlistColumns") && !refreshContactColumns(panel,error)) return false;
    if (fields.contains("Fontsettingsfile") && !populateFontPresets(fields.at("Fontsettingsfile"),error)) return false;
    if (fields.contains("checkSendCrashReports"))
    {
        if (!refreshCrashPreferences(panel,true,error)) return false;
        LLVKControl::Callback callback;
        callback.function=[this,panel](auto,const LLSD&) { refreshCrashPreferences(panel,false,mDialogError); };
        mTree.setControlCommit(fields.at("checkSendCrashReports"),std::move(callback));
        if (std::find(mCrashPanels.begin(),mCrashPanels.end(),panel)==mCrashPanels.end()) mCrashPanels.push_back(panel);
    }
    if (fields.contains("single_click_action_combo") && !updateClickActions(panel,false,error)) return false;
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
    if (fields.contains("time_format_combobox"))
    {
        const auto clock=fields.at("time_format_combobox");
        if (!mTree.setValue(clock,LLSD(mTree.setting("Use24HourClock").value_or(LLSD(false)).asBoolean() ? "1" : "0"))) return false;
        const auto changed=[this]
        {
            if (!mLanguageChanged && queueNotice("ChangeLanguage",{},{},mDialogError)) mLanguageChanged=true;
        };
        LLVKControl::Callback callback;
        callback.function=[this,changed](auto id,const LLSD&)
        {
            if (mTree.updateSetting("Use24HourClock",LLSD(mTree.value(id).asString()=="1"))) changed();
        };
        mTree.setControlCommit(clock,std::move(callback));
        if (fields.contains("language_combobox"))
        {
            LLVKControl::Callback language;
            language.function=[changed](auto,const LLSD&) { changed(); };
            mTree.setControlCommit(fields.at("language_combobox"),std::move(language));
        }
    }
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

bool LLVKViewerUi::initializeBackupPreferences(LLVKWidgetTree::Id panel,std::string& error)
{
    error.clear();
    const auto fields=preferenceFields(mTree,panel);
    for (const auto name : {"restore_global_files_list","restore_per_account_files_list","restore_global_folders_list"})
        if (!fields.contains(name) || !mTree.get(fields.at(name))->scrollList)
        { error=std::string("Original backup list is missing: ")+name; return false; }
#ifndef OPENSIM
    const auto folders=fields.at("restore_global_folders_list");
    auto rows=mTree.get(folders)->scrollList->rows;
    std::erase_if(rows,[](const auto& row) { return row.value.asString().starts_with("windlight"); });
    if (!mTree.setScrollListRows(folders,std::move(rows),error)) return false;
#endif
    const auto account=mTree.setting("PerAccountSettingsFile").value_or(LLSD("")).asString();
    if (fields.contains("restore_per_account_disable_cover")) mTree.setVisible(fields.at("restore_per_account_disable_cover"),account.empty());
    return true;
}

void LLVKViewerUi::backupPreferenceAction(LLVKWidgetTree::Id panel,const std::string& action)
{
    mDialogError.clear();
    const auto fields=preferenceFields(mTree,panel);
    if (action=="BackupSelectAll" || action=="BackupDeselectAll")
    {
        for (const auto name : {"restore_global_files_list","restore_per_account_files_list","restore_global_folders_list"})
        {
            const auto list=fields.at(name);
            auto rows=mTree.get(list)->scrollList->rows;
            for (auto& row : rows) if (row.enabled && !row.cells.empty()) row.cells[0]=action=="BackupSelectAll" ? "true" : "false";
            if (!mTree.setScrollListRows(list,std::move(rows),mDialogError)) return;
        }
        return;
    }
    const auto path=mTree.setting("SettingsBackupPath").value_or(LLSD("")).asString();
    if (action=="SetBackupSettingsPath")
    {
        if (!mDirectoryPicker) { mDialogError="Native directory picker is unavailable"; return; }
        mDirectoryPicker(std::filesystem::path(std::u8string(path.begin(),path.end())),[this,panel,generation=mPreferenceGeneration](auto selected,std::string error)
        {
            if (generation!=mPreferenceGeneration || !mTree.get(panel)) return;
            if (!error.empty()) { mDialogError=std::move(error); return; }
            if (!selected || selected->empty()) return;
            const auto bytes=selected->u8string();
            mTree.updateSetting("SettingsBackupPath",LLSD(std::string(bytes.begin(),bytes.end())));
        },mDialogError);
        return;
    }
    if (path.empty()) { queueNotice("BackupPathEmpty",{},{},mDialogError); return; }
    const bool restore=action=="RestoreSettings";
    LLSD arguments; arguments["DIRECTORY"]=path;
    queueNotice(restore ? "SettingsRestoreNeedsLogout" : "SettingsConfirmBackup",arguments,[this,panel,restore](int option,const LLSD&)
    {
        if (option!=0 || !mTree.get(panel)) return;
        if (!mBackupHandler || (restore && !mQuitRequest))
        { mDialogError="Native backup/restore services are not bound"; return; }
        const auto fields=preferenceFields(mTree,panel);
        BackupRequest request;
        const auto path=mTree.setting("SettingsBackupPath").value_or(LLSD("")).asString();
        request.directory=std::filesystem::path(std::u8string(path.begin(),path.end()));
        request.restore=restore;
        request.globalSettings=!restore || mTree.setting("RestoreGlobalSettings").value_or(LLSD(false)).asBoolean();
        if (restore && request.globalSettings)
        {
            const auto recommended=graphicsPolicyValues(0,true,mDialogError);
            if (!recommended) return;
            request.recommendedGraphics=*recommended;
        }
        request.accountSettings=!mTree.setting("PerAccountSettingsFile").value_or(LLSD("")).asString().empty() &&
            (!restore || mTree.setting("RestorePerAccountSettings").value_or(LLSD(false)).asBoolean());
        for (const auto& [name,output] : {std::pair{"restore_global_files_list",&request.globalFiles},
            std::pair{"restore_per_account_files_list",&request.accountFiles},std::pair{"restore_global_folders_list",&request.folders}})
            for (const auto& row : mTree.get(fields.at(name))->scrollList->rows)
                if (row.cells.size()>=3 && (!restore || row.cells[0]=="true" || row.cells[0]=="1")) output->push_back(row.cells[2]);
        if (restore && mPreferences && mPreferences->visible() && !applyPreferences(mDialogError)) return;
        if (!restore && !mUserColorsFile.empty() && !mColors->saveUserFile(mUserColorsFile,mDialogError)) return;
        if (!mBackupHandler(request,mDialogError)) return;
        if (restore)
        {
            mSaveSettingsOnExit=false;
            queueNotice("RestoreFinished",{},[this](int,const LLSD&) { if (mQuitRequest) mQuitRequest(); },mDialogError);
        }
        else queueNotice("BackupFinished",{},{},mDialogError);
    },mDialogError);
}

bool LLVKViewerUi::refreshBeamPreferences(LLVKWidgetTree::Id panel,std::string& error)
{
    error.clear();
    const auto fields=preferenceFields(mTree,panel);
    const auto off=mTree.panelString(panel,"BeamsOffLabel",{},error);
    if (!off) return false;
    for (const auto& [folder,combo,setting] : {std::tuple{"beams","FSBeamShape_combo","FSBeamShape"},
        std::tuple{"beamsColors","BeamColor_combo","FSBeamColorFile"}})
    {
        std::vector<LLVKWidgetTree::ComboItem> items{{*off,LLSD("")}};
        try
        {
            for (const auto& directory : {mSkinBaseDirectory.parent_path()/"app_settings"/folder,mProfileDirectory/"user_settings"/folder})
            {
                if (!std::filesystem::is_directory(directory)) continue;
                for (const auto& entry : std::filesystem::directory_iterator(directory))
                {
                    if (!entry.is_regular_file() || entry.path().extension()!=".xml") continue;
                    const auto bytes=entry.path().stem().u8string();
                    const auto name=LLURI::unescape(std::string(bytes.begin(),bytes.end()));
                    items.push_back({name,LLSD(name)});
                    if (items.size()>4096) { error="Native beam catalog exceeds its limit"; return false; }
                }
            }
        }
        catch (const std::filesystem::filesystem_error&) { error="Cannot enumerate native beam catalog"; return false; }
        if (!fields.contains(combo) || !mTree.replaceComboItems(fields.at(combo),std::move(items),error)) return false;
        mTree.setValue(fields.at(combo),mTree.setting(setting).value_or(LLSD("")));
    }
    return true;
}

bool LLVKViewerUi::initializeViewerPreferences(LLVKWidgetTree::Id panel,std::string& error)
{
    error.clear();
    const auto fields=preferenceFields(mTree,panel);
    for (const auto name : {"FSBuildPrefs_EmbedItem","FSBuildPrefs_UseCustomScript","reset_default_folders"})
        if (fields.contains(name)) mTree.setEnabled(fields.at(name),false);
    const auto unavailable=mTree.panelString(panel,"EmbeddedItemNotLoggedIn",{},error);
    if (!unavailable) return false;
    for (const auto& [field,argument] : {std::pair{"build_item_add_disp_rect_txt","[ITEM]"},std::pair{"custom_script_disp_rect_txt","[SCRIPT]"}})
        if (fields.contains(field))
        {
            auto text=mTree.value(fields.at(field)).asString();
            LLStringUtil::replaceString(text,argument,*unavailable);
            mTree.setValue(fields.at(field),LLSD(text));
        }
    for (const auto name : {"BeamColor_new","BeamColor_refresh","BeamColor_delete","custom_beam_btn","refresh_beams","delete_beam","reset_default_folders"})
        if (fields.contains(name))
        {
            LLVKControl::Callback callback;
            callback.function=[this,panel,action=std::string(name)](auto,const LLSD&) { viewerPreferenceAction(panel,action); };
            mTree.setControlCommit(fields.at(name),std::move(callback));
        }
    return refreshBeamPreferences(panel,error) && initializeStartupPreferencePanel(panel,error);
}

void LLVKViewerUi::viewerPreferenceAction(LLVKWidgetTree::Id panel,const std::string& action)
{
    mDialogError.clear();
    if (action=="Pref.PermsDefault") { showDefaultPermissions(mDialogError); return; }
    if (action=="NACL.AntiSpamUnblock")
    {
        if (!mClearSpamQueues) { mDialogError="Native anti-spam queue service is unavailable"; return; }
        mClearSpamQueues();
        return;
    }
    if (action=="BeamColor_new") { showBeamColor(panel,mDialogError); return; }
    if (action=="custom_beam_btn") { showBeamShape(panel,mDialogError); return; }
    if (action=="Pref.SetExternalEditor")
    {
        if (!mExecutableFilePicker) { mDialogError="Native executable picker is unavailable"; return; }
        const auto generation=mPreferenceGeneration;
        mExecutableFilePicker(false,"",[this,panel,generation](auto selected,std::string problem)
        {
            if (generation!=mPreferenceGeneration || !mTree.get(panel)) return;
            if (!problem.empty()) { mDialogError=std::move(problem); return; }
            if (!selected || selected->empty()) return;
            const auto bytes=selected->u8string();
            std::string path(bytes.begin(),bytes.end());
            if (path.find('"')==std::string::npos) path='"'+path+'"';
            if (!mTree.updateSetting("ExternalEditor",LLSD(path))) mDialogError="Cannot update native external editor setting";
        },mDialogError);
        return;
    }
    if (action=="refresh_beams" || action=="BeamColor_refresh") { refreshBeamPreferences(panel,mDialogError); return; }
    if (action=="delete_beam" || action=="BeamColor_delete")
    {
        const bool color=action=="BeamColor_delete";
        const auto fields=preferenceFields(mTree,panel);
        const auto selected=fields.find(color ? "BeamColor_combo" : "FSBeamShape_combo");
        if (selected==fields.end()) { mDialogError="Native beam selection is missing"; return; }
        const auto name=mTree.value(selected->second).asString();
        if (name.empty()) return;
        if (name=="." || name==".." || name.find_first_of("/\\:")!=std::string::npos || name.find('\0')!=std::string::npos)
        { mDialogError="Invalid native beam preset name"; return; }
        const auto filename=std::filesystem::path(std::u8string(name.begin(),name.end())+u8".xml");
        const auto folder=color ? "beamsColors" : "beams";
        bool removed=false;
        std::string failure;
        for (const auto& root : {mSkinBaseDirectory.parent_path()/"app_settings",mProfileDirectory/"user_settings"})
        {
            if (!root.is_absolute()) { failure="Native beam directory must be absolute"; continue; }
            const auto path=root/folder/filename;
            try
            {
                bool linked=false;
                for (auto component=path; !component.empty();)
                {
                    if (std::filesystem::is_symlink(std::filesystem::symlink_status(component))) { linked=true; break; }
                    const auto parent=component.parent_path();
                    if (parent==component) break;
                    component=parent;
                }
                if (linked) { failure="Linked beam preset paths cannot be deleted"; continue; }
                if (!std::filesystem::exists(path)) continue;
                if (!std::filesystem::is_regular_file(path)) { failure="Beam preset is not a regular file"; continue; }
                removed=std::filesystem::remove(path)||removed;
            }
            catch (const std::filesystem::filesystem_error&) { failure="Cannot delete native beam preset"; }
        }
        if (removed) mTree.updateSetting(color ? "FSBeamColorFile" : "FSBeamShape",LLSD(""));
        refreshBeamPreferences(panel,mDialogError);
        if (!failure.empty()) mDialogError=std::move(failure);
        return;
    }
    if (action=="NACL.SetPreprocInclude")
    {
        if (!mDirectoryPicker) { mDialogError="Native directory picker is unavailable"; return; }
        const auto current=mTree.setting("_NACL_PreProcHDDIncludeLocation").value_or(LLSD("")).asString();
        mDirectoryPicker(std::filesystem::path(std::u8string(current.begin(),current.end())),[this,panel,generation=mPreferenceGeneration](auto selected,std::string error)
        {
            if (generation!=mPreferenceGeneration || !mTree.get(panel)) return;
            if (!error.empty()) { mDialogError=std::move(error); return; }
            if (!selected || selected->empty()) return;
            const auto bytes=selected->u8string();
            mTree.updateSetting("_NACL_PreProcHDDIncludeLocation",LLSD(std::string(bytes.begin(),bytes.end())));
        },mDialogError);
        return;
    }
    if (action=="Perms.Copy" || action=="Perms.Trans")
    {
        const auto fields=preferenceFields(mTree,panel);
        if (action=="Perms.Copy")
        {
            const auto copy=mTree.setting("NextOwnerCopy").value_or(LLSD(false)).asBoolean();
            if (!copy) mTree.updateSetting("NextOwnerTransfer",LLSD(true));
            if (fields.contains("next_owner_transfer")) mTree.setEnabled(fields.at("next_owner_transfer"),copy);
        }
        else if (!mTree.setting("NextOwnerTransfer").value_or(LLSD(false)).asBoolean()) mTree.updateSetting("NextOwnerCopy",LLSD(true));
        return;
    }
    if (!mViewerPreferenceHandler) { mDialogError="Native Viewer preference service is not bound: "+action; return; }
    mViewerPreferenceHandler(action,mDialogError);
}

bool LLVKViewerUi::updateBeamShapeImage(std::string& error)
{
    const auto panel=find("beamshape_draw",mBeamShape->id());
    const auto rect=mTree.get(panel)->params.rect;
    const int width=rect.right-rect.left,height=rect.top-rect.bottom;
    if (width<=0 || height<=0 || width>2048 || height>2048) { error="Invalid native beam drawing dimensions"; return false; }
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width)*height*4,0);
    const auto pixel=[&](int column,int row,const LLVKColor::Value& color)
    {
        if (column<0 || column>=width || row<0 || row>=height) return;
        const auto offset=(row*width+column)*4;
        for (int channel=0; channel<4; ++channel) pixels[offset+channel]=static_cast<std::uint8_t>(std::clamp(color[channel],0.f,1.f)*255.f+0.5f);
    };
    for (int row=0; row<height; ++row)
        for (int column=0; column<width; ++column)
        {
            const float dx=static_cast<float>(column-width/2),dy=static_cast<float>(row-height/2);
            const float distance=std::sqrt(dx*dx+dy*dy);
            for (int ring=0; ring<5; ++ring)
                if (std::abs(distance-(ring==0 ? 2.f : 30.f*ring))<0.6f)
                    pixel(column,row,ring%2 ? LLVKColor::Value{0,0,0,1} : LLVKColor::Value{1,1,1,1});
        }
    for (const auto& point : mBeamShapeDraft.points)
    {
        const auto centerX=point.horizontal-rect.left,centerY=point.vertical-rect.bottom;
        for (int row=std::max(0,centerY-9); row<std::min(height,centerY+10); ++row)
            for (int column=std::max(0,centerX-9); column<std::min(width,centerX+10); ++column)
            {
                const int dx=column-centerX,dy=row-centerY,squared=dx*dx+dy*dy;
                if (squared<=81) pixel(column,row,squared<=49 ? point.color : squared<=64 ? LLVKColor::Value{0,0,0,1} : LLVKColor::Value{1,1,1,1});
            }
    }
    const auto image=LLVKWidgetImage::fromRgba("native-beam-shape",width,height,pixels,error);
    return image && mTree.setButtonImages(mBeamShapeCanvas,image,image);
}

bool LLVKViewerUi::showBeamShape(LLVKWidgetTree::Id owner,std::string& error)
{
    error.clear();
    if (!mTree.get(owner)) { error="Native beam editor owner is missing"; return false; }
    if (!mBeamShape)
    {
        mBeamShape=LLVKFloater::createFile(mTree,*mDialogFactory,mRoot,"floater_beamshape.xml",error);
        if (!mBeamShape) return false;
        const auto panel=find("beamshape_draw",mBeamShape->id());
        if (!panel) { error="Original beam drawing panel is missing"; mBeamShape.reset(); return false; }
        const auto rect=mTree.get(panel)->params.rect;
        const auto canvas=mDialogFactory->construct(mTree,"<button name='native_beam_shape' left='0' bottom='0' width='400' height='300' follows='all' label='' hover_glow_amount='0' display_pressed_state='false'/>",panel,error);
        if (!canvas) { mBeamShape.reset(); return false; }
        mBeamShapeCanvas=*canvas;
        if (!mTree.setShape(*canvas,{0,0,rect.right-rect.left,rect.top-rect.bottom},error)) { mBeamShape.reset(); return false; }
        LLSD red=LLSD::emptyArray(); for (const auto channel : {1.,0.,0.,1.}) red.append(channel);
        mTree.setColorSwatchValue(find("beam_color_swatch",mBeamShape->id()),red,error);
        LLVKWidgetTree::Events events;
        events.pointer=[this,panel](auto,const auto& event)
        {
            if (event.kind!=LLVKWidgetTree::PointerKind::LeftDown && event.kind!=LLVKWidgetTree::PointerKind::RightDown) return;
            const auto rect=mTree.get(panel)->params.rect;
            const auto swatch=mTree.get(find("beam_color_swatch",mBeamShape->id()));
            if (mBeamShapeDraft.select(event.x+rect.left,event.y+rect.bottom,event.kind==LLVKWidgetTree::PointerKind::RightDown,swatch->colorSwatch->color))
                updateBeamShapeImage(mDialogError);
        };
        mTree.setEvents(*canvas,std::move(events));
        for (const auto name : {"beamshape_save","beamshape_load","beamshape_clear","cancel"})
        {
            LLVKControl::Callback callback;
            callback.function=[this,name=std::string(name)](auto,const LLSD&)
            {
                if (name=="cancel") mBeamShape->close(mDialogError);
                else if (name=="beamshape_clear") { mBeamShapeDraft.points.clear(); updateBeamShapeImage(mDialogError); }
                else beamShapeFile(name=="beamshape_save");
            };
            mTree.setControlCommit(find(name,mBeamShape->id()),std::move(callback));
        }
        mBeamShape->onClose([this] { ++mBeamShapeGeneration; });
    }
    mBeamShapeOwner=owner;
    if (!mBeamShape->visible()) { ++mBeamShapeGeneration; if (!updateBeamShapeImage(error)) return false; }
    mActiveFloater=mBeamShape.get();
    return mBeamShape->open(error);
}

void LLVKViewerUi::beamShapeFile(bool save)
{
    mDialogError.clear();
    if (!mXmlFilePicker) { mDialogError="Native XML picker is unavailable"; return; }
    const auto initial=(mProfileDirectory/"user_settings"/"beams"/"NewBeam.xml").u8string();
    mXmlFilePicker(save,save ? std::string(initial.begin(),initial.end()) : "",[this,save,generation=mBeamShapeGeneration](auto path,std::string problem)
    {
        if (!mBeamShape || !mBeamShape->visible() || generation!=mBeamShapeGeneration) return;
        if (!problem.empty()) { mDialogError=std::move(problem); return; }
        if (!path || path->empty()) return;
        const auto rect=mTree.get(find("beamshape_draw",mBeamShape->id()))->params.rect;
        const auto width=rect.right-rect.left,height=rect.top-rect.bottom;
        if (save)
        {
            if (!llvkSaveBeamPreset(*path,mBeamShapeDraft.serialize(rect.left,rect.bottom,width,height),mDialogError)) return;
            const auto name=path->stem().u8string(); mTree.updateSetting("FSBeamShape",LLSD(std::string(name.begin(),name.end())));
            if (mTree.get(mBeamShapeOwner)) refreshBeamPreferences(mBeamShapeOwner,mDialogError);
        }
        else
        {
            const auto xml=readText(*path); if (!xml) { mDialogError="Cannot read beam shape preset"; return; }
            std::istringstream input(*xml); LLSD document;
            if (LLSDSerialize::fromXML(document,input)<=0) { mDialogError="Invalid beam shape preset XML"; return; }
            if (mBeamShapeDraft.load(document,rect.left,rect.bottom,width,height,mDialogError)) updateBeamShapeImage(mDialogError);
        }
    },mDialogError);
}

bool LLVKViewerUi::updateBeamColorStrip(std::string& error)
{
    std::vector<std::uint8_t> pixels(410*76*4,0);
    const auto pixel=[&](int column,int row,const LLVKColor::Value& color)
    {
        if (column<0 || column>=410 || row<0 || row>=76) return;
        const auto offset=(row*410+column)*4;
        for (int channel=0; channel<4; ++channel) pixels[offset+channel]=static_cast<std::uint8_t>(std::clamp(color[channel],0.f,1.f)*255.f+0.5f);
    };
    for (int degrees=0; degrees<=720; ++degrees)
        for (int row=0; row<76; ++row) pixel(LLVKBeamColor::position(static_cast<float>(degrees)),row,LLVKBeamColor::hue(static_cast<float>(degrees)));
    for (const float hue : {mBeamColorDraft.startHue,mBeamColorDraft.endHue})
    {
        const auto center=LLVKBeamColor::position(hue);
        for (int row=0; row<76; ++row)
            for (int column=std::max(0,center-26); column<std::min(410,center+27); ++column)
            {
                const int dx=column-center,dy=row-37;
                const float radius=std::sqrt(static_cast<float>(dx*dx+dy*dy));
                const bool circle=radius>=6.5f && radius<=9.5f;
                const bool cross=(std::abs(dx)<=1 && std::abs(dy)<=28) || (std::abs(dy)<=1 && std::abs(dx)<=25);
                if (!circle && !cross) continue;
                const bool black=circle ? radius>=7.5f && radius<8.5f : dx==0 || dy==0;
                pixel(column,row,black ? LLVKColor::Value{0,0,0,1} : LLVKColor::Value{1,1,1,1});
            }
    }
    const auto image=LLVKWidgetImage::fromRgba("native-beam-color-strip",410,76,pixels,error);
    if (!image || !mTree.setButtonImages(mBeamColorStrip,image,image)) return false;
    for (const auto& [name,hue] : {std::pair{"native_start_hue",mBeamColorDraft.startHue},std::pair{"native_end_hue",mBeamColorDraft.endHue}})
    {
        const auto label=find(name,mBeamColor->id());
        const auto center=LLVKBeamColor::position(hue);
        if (!mTree.setShape(label,{center-40,165,center+40,181},error)) return false;
    }
    return true;
}

bool LLVKViewerUi::updateBeamColorPreview(std::string& error)
{
    const auto color=mBeamColorDraft.preview(mNoticeTime);
    if (!color) { error="Invalid native beam preview time"; return false; }
    LLSD value=LLSD::emptyArray();
    for (const auto channel : *color) value.append(channel);
    return mTree.setColorSwatchValue(find("BeamColor_Preview",mBeamColor->id()),value,error);
}

bool LLVKViewerUi::showBeamColor(LLVKWidgetTree::Id owner,std::string& error)
{
    error.clear();
    if (!mTree.get(owner)) { error="Native beam editor owner is missing"; return false; }
    if (!mBeamColor)
    {
        mBeamColor=LLVKFloater::createFile(mTree,*mDialogFactory,mRoot,"floater_beamcolor.xml",error);
        if (!mBeamColor) return false;
        const auto discard=[&] { mBeamColor.reset(); return false; };
        const auto strip=mDialogFactory->construct(mTree,
            "<button name='native_beam_hues' left='0' bottom='161' width='410' height='76' label='' hover_glow_amount='0' display_pressed_state='false'/>" ,mBeamColor->id(),error);
        if (!strip) return discard();
        mBeamColorStrip=*strip;
        for (const auto& [name,key] : {std::pair{"native_start_hue","start_hue"},std::pair{"native_end_hue","end_hue"}})
        {
            const auto label=mDialogFactory->construct(mTree,std::string("<text name='")+name+"' width='80' height='16' halign='center' mouse_opaque='false'/>",mBeamColor->id(),error);
            const auto text=mTree.panelString(mBeamColor->id(),key,{},error);
            if (!label || !text || !mTree.setValue(*label,LLSD(*text))) return discard();
        }
        LLVKWidgetTree::Events events;
        events.pointer=[this](auto,const auto& event)
        {
            if (event.kind!=LLVKWidgetTree::PointerKind::LeftDown && event.kind!=LLVKWidgetTree::PointerKind::RightDown) return;
            if (mBeamColorDraft.select(event.x,event.y+161,event.kind==LLVKWidgetTree::PointerKind::RightDown)) updateBeamColorStrip(mDialogError);
        };
        mTree.setEvents(mBeamColorStrip,std::move(events));
        for (const auto name : {"BeamColor_Save","BeamColor_Load","BeamColor_Cancel","BeamColor_Speed"})
        {
            const auto control=find(name,mBeamColor->id());
            if (!control) { error="Original beam editor control is missing"; return discard(); }
            LLVKControl::Callback callback;
            callback.function=[this,name=std::string(name)](auto id,const LLSD&)
            {
                if (name=="BeamColor_Cancel") mBeamColor->close(mDialogError);
                else if (name=="BeamColor_Speed")
                {
                    if (!mBeamColorDraft.setSpeed(static_cast<float>(mTree.value(id).asReal()))) mDialogError="Invalid beam rotation speed";
                    else updateBeamColorStrip(mDialogError);
                }
                else beamColorFile(name=="BeamColor_Save");
            };
            mTree.setControlCommit(control,std::move(callback));
        }
        mBeamColor->onClose([this] { ++mBeamColorGeneration; });
    }
    mBeamColorOwner=owner;
    if (!mBeamColor->visible())
    {
        ++mBeamColorGeneration;
        if (!mTree.setValue(find("BeamColor_Speed",mBeamColor->id()),LLSD(mBeamColorDraft.rotateSpeed*100.f)) || !updateBeamColorStrip(error)) return false;
    }
    mActiveFloater=mBeamColor.get();
    return mBeamColor->open(error);
}

void LLVKViewerUi::beamColorFile(bool save)
{
    mDialogError.clear();
    if (!mXmlFilePicker) { mDialogError="Native XML picker is unavailable"; return; }
    const auto destination=mProfileDirectory/"user_settings"/"beamsColors"/"NewBeamColor.xml";
    const auto bytes=destination.u8string();
    mXmlFilePicker(save,save ? std::string(bytes.begin(),bytes.end()) : "",
        [this,save,generation=mBeamColorGeneration](auto path,std::string problem)
    {
        if (!mBeamColor || !mBeamColor->visible() || generation!=mBeamColorGeneration) return;
        if (!problem.empty()) { mDialogError=std::move(problem); return; }
        if (!path || path->empty()) return;
        if (save)
        {
            if (!mBeamColorDraft.saveFile(*path,mDialogError)) return;
            const auto bytes=path->stem().u8string();
            mTree.updateSetting("FSBeamColorFile",LLSD(std::string(bytes.begin(),bytes.end())));
            if (mTree.get(mBeamColorOwner) && !refreshBeamPreferences(mBeamColorOwner,mDialogError)) return;
            mBeamColor->close(mDialogError);
        }
        else
        {
            const auto xml=readText(*path);
            if (!xml) { mDialogError="Cannot read native beam color preset"; return; }
            std::istringstream input(*xml); LLSD value;
            if (LLSDSerialize::fromXML(value,input)<=0) { mDialogError="Invalid beam color preset XML"; return; }
            if (!mBeamColorDraft.load(value,mDialogError)) return;
            mTree.setValue(find("BeamColor_Speed",mBeamColor->id()),LLSD(mBeamColorDraft.rotateSpeed*100.f));
            updateBeamColorStrip(mDialogError);
        }
    },mDialogError);
}

bool LLVKViewerUi::showDefaultPermissions(std::string& error)
{
    error.clear();
    if (!mDefaultPermissions)
    {
        std::map<std::string,LLSD> migration;
        if (!mTree.setting("DefaultUploadPermissionsConverted").value_or(LLSD(false)).asBoolean())
        {
            for (const auto suffix : {"EveryoneCopy","NextOwnerCopy","NextOwnerModify","NextOwnerTransfer","ShareWithGroup"})
            {
                const auto value=mTree.setting(suffix);
                if (!value || !mTree.setting(std::string("Uploads")+suffix))
                { error="Default upload permission settings are missing"; return false; }
                migration[std::string("Uploads")+suffix]=*value;
            }
            migration["DefaultUploadPermissionsConverted"]=true;
        }
        mDefaultPermissions=LLVKFloater::createFile(mTree,*mDialogFactory,mRoot,"floater_perms_default.xml",error);
        if (!mDefaultPermissions) return false;
        if (!migration.empty())
        {
            if (!mSavePreferences || !mSavePreferences(migration,error))
            { if (error.empty()) error="Cannot persist default upload permission migration"; mDefaultPermissions.reset(); return false; }
            for (const auto& [name,value] : migration) mTree.updateSetting(name,value);
        }
        mDefaultPermissions->onClose([this]
        {
            if (!mDefaultPermissionsAccepted) mTree.restorePreferences(mDefaultPermissionsSnapshot,{},mDialogError);
            mDefaultPermissionsSnapshot={};
        });
    }
    if (!mDefaultPermissions->visible())
    {
        const auto snapshot=mTree.snapshotPreferences(mDefaultPermissions->id(),error);
        if (!snapshot) return false;
        mDefaultPermissionsSnapshot=*snapshot;
        for (const auto category : {"Objects","Uploads","Scripts","Notecards","Gestures","Wearables","Settings","Materials"})
            for (const auto suffix : {"NextOwnerCopy","NextOwnerModify","NextOwnerTransfer","ShareWithGroup","EveryoneCopy"})
            {
                const auto name=std::string(category)+suffix;
                if (const auto value=mTree.setting(name)) mDefaultPermissionsSnapshot.settings[name]=*value;
                else if (std::string_view(category)!="Settings" || (std::string_view(suffix)!="NextOwnerCopy" &&
                    std::string_view(suffix)!="ShareWithGroup" && std::string_view(suffix)!="EveryoneCopy"))
                { error="Missing default creation permission: "+name; return false; }
            }
        mDefaultPermissionsAccepted=false;
    }
    mActiveFloater=mDefaultPermissions.get();
    return mDefaultPermissions->open(error);
}

bool LLVKViewerUi::acceptDefaultPermissions(std::string& error)
{
    error.clear();
    if (!mDefaultPermissions || !mDefaultPermissions->visible()) return false;
    std::map<std::string,LLSD> changed;
    for (const auto& [name,before] : mDefaultPermissionsSnapshot.settings)
        if (const auto value=mTree.setting(name); value && !llsd_equals(*value,before)) changed[name]=*value;
    if (!changed.empty() && (!mSavePreferences || !mSavePreferences(changed,error)))
    { if (error.empty()) error="Cannot persist default creation permissions"; return false; }
    mDefaultPermissionsAccepted=true;
    return mDefaultPermissions->close(error);
}

bool LLVKViewerUi::refreshGraphicPresetDialogs(std::string& error)
{
    for (const auto& [action,dialog] : mGraphicPresetDialogs)
    {
        const auto names=mGraphicPresets->names(action=="PrefLoad",error);
        if (!names) return false;
        std::vector<LLVKWidgetTree::ComboItem> items;
        for (const auto& name : *names) items.push_back({name=="Default" ? mAboutStrings["Default"] : name,LLSD(name)});
        const auto combo=find("preset_combo",dialog->id());
        if (!mTree.replaceComboItems(combo,std::move(items),error)) return false;
        const auto active=mTree.setting("PresetGraphicActive").value_or(LLSD("")).asString();
        if (std::find(names->begin(),names->end(),active)!=names->end()) mTree.setValue(combo,LLSD(active));
        else if (!names->empty()) mTree.setValue(combo,LLSD(names->front()));
        if (action=="PrefSave")
            mTree.setEnabled(find("save",dialog->id()),!mTree.value(combo).asString().empty());
        else mTree.setEnabled(find(action=="PrefLoad" ? "ok" : "delete",dialog->id()),!names->empty());
    }
    const auto label=find("preset_text",mGraphicPresetOwner);
    const auto name=mTree.setting("PresetGraphicActive").value_or(LLSD("")).asString();
    if (label) mTree.setValue(label,LLSD(name.empty() ? mAboutStrings["none_paren_cap"] : name=="Default" ? mAboutStrings["Default"] : name));
    return true;
}

bool LLVKViewerUi::showGraphicPreset(LLVKWidgetTree::Id owner,const std::string& action,std::string& error)
{
    error.clear();
    if (action!="PrefSave" && action!="PrefLoad" && action!="PrefDelete") { error="Invalid graphics preset action"; return false; }
    if (!mGraphicPresets)
    {
        auto presets=std::make_unique<LLVKGraphicPresets>();
        if (!presets->initialize(mSkinBaseDirectory.parent_path()/"app_settings",mProfileDirectory/"user_settings"/"presets"/"graphic",error)) return false;
        mGraphicPresets=std::move(presets);
        for (const auto& control : mGraphicPresets->controls())
            if (mTree.setting(control)) mTree.subscribeSetting(control,[this](const LLSD&,const LLSD&)
            {
                if (mLoadingGraphicPreset) return;
                mTree.updateSetting("PresetGraphicActive",LLSD(""));
                const auto label=find("preset_text",mGraphicPresetOwner);
                if (label) mTree.setValue(label,LLSD(mAboutStrings["none_paren_cap"]));
            });
    }
    mGraphicPresetOwner=owner;
    if (mGraphicsDevice)
    {
        auto defaults=graphicsPolicyValues(0,true,error);
        if (!defaults) return false;
        for (const auto& control : mGraphicPresets->controls())
            if (!defaults->contains(control)) if (const auto value=mTree.setting(control)) (*defaults)[control]=*value;
        if (!mGraphicPresets->createDefault(*defaults,error)) return false;
    }
    auto& dialog=mGraphicPresetDialogs[action];
    if (!dialog)
    {
        const auto file=action=="PrefSave" ? "floater_save_pref_preset.xml" : action=="PrefLoad" ? "floater_load_pref_preset.xml" : "floater_delete_pref_preset.xml";
        dialog=LLVKFloater::createFile(mTree,*mDialogFactory,mRoot,file,error);
        if (!dialog) { mGraphicPresetDialogs.erase(action); return false; }
        const auto id=dialog->id();
        LLVKControl::Callback accept;
        accept.function=[this,action](auto,const LLSD&) { acceptGraphicPreset(action,mDialogError); };
        mTree.setControlCommit(find(action=="PrefSave" ? "save" : action=="PrefLoad" ? "ok" : "delete",id),accept);
        LLVKControl::Callback cancel;
        cancel.function=[this,action](auto,const LLSD&) { mGraphicPresetDialogs.at(action)->close(mDialogError); };
        mTree.setControlCommit(find("cancel",id),std::move(cancel));
        if (action=="PrefSave")
        {
            const auto combo=find("preset_combo",id);
            mTree.setControlCommit(combo,std::move(accept));
            const auto editor=mTree.get(combo)->combo->editor;
            const auto previous=editor ? mTree.get(editor)->lineEditor->params.keystroke : LLVKControl::Callback{};
            LLVKControl::Callback changed;
            changed.function=[this,id,previous](auto editor,const LLSD& value)
            {
                if (previous.function) previous.function(editor,previous.parameter.value_or(value));
                if (mTree.get(editor)) mTree.setEnabled(find("save",id),!mTree.value(editor).asString().empty());
            };
            if (editor) mTree.setLineEditorKeystroke(editor,std::move(changed));
        }
    }
    if (!refreshGraphicPresetDialogs(error)) return false;
    mActiveFloater=dialog.get();
    return dialog->open(error);
}

bool LLVKViewerUi::acceptGraphicPreset(const std::string& action,std::string& error)
{
    error.clear();
    const auto found=mGraphicPresetDialogs.find(action);
    if (found==mGraphicPresetDialogs.end() || !found->second->visible()) return false;
    const auto combo=find("preset_combo",found->second->id());
    const auto* state=mTree.get(combo);
    const auto name=state->combo->editor ? mTree.value(state->combo->editor).asString() : mTree.value(combo).asString();
    if (name.empty()) return false;
    if ((action=="PrefSave" || action=="PrefDelete") && name==mAboutStrings["Default"])
    { error="The Default graphics preset is protected"; return false; }
    if (action=="PrefSave")
    {
        std::map<std::string,LLSD> values;
        for (const auto& control : mGraphicPresets->controls())
            if (const auto value=mTree.setting(control)) values[control]=*value;
        if (!mGraphicPresets->save(name,values,error)) return false;
        mTree.updateSetting("PresetGraphicActive",LLSD(name));
    }
    else if (action=="PrefLoad")
    {
        const auto values=mGraphicPresets->load(name,error);
        if (!values) return false;
        struct Loading { bool& flag; bool previous; explicit Loading(bool& value) : flag(value),previous(value) { flag=true; } ~Loading() { flag=previous; } } loading(mLoadingGraphicPreset);
        for (const auto& [control,value] : *values)
        {
            const auto before=mTree.setting(control);
            if (!before) continue;
            if (mPreferences && mPreferences->visible()) mPreferenceSnapshot.settings.try_emplace(control,*before);
            if (!mTree.updateSetting(control,value)) { error="Cannot apply graphics preset setting: "+control; return false; }
        }
        mTree.updateSetting("PresetGraphicActive",LLSD(name));
        if (!initializeGraphicsPreferences(mGraphicPresetOwner,error)) return false;
    }
    else
    {
        if (!mGraphicPresets->remove(name,error)) return false;
        if (mTree.setting("PresetGraphicActive").value_or(LLSD()).asString()==name) mTree.updateSetting("PresetGraphicActive",LLSD(""));
    }
    if (!refreshGraphicPresetDialogs(error)) return false;
    return found->second->close(error);
}

std::optional<std::map<std::string,LLSD>> LLVKViewerUi::graphicsPolicyValues(int level,bool recommended,std::string& error)
{
    if (!mGraphicsDevice) { error="Native graphics device facts are unavailable"; return {}; }
    if (!mGraphicsPolicy)
    {
        auto policy=std::make_unique<LLVKGraphicsPolicy>();
        const auto viewer=mSkinBaseDirectory.parent_path();
        if (!policy->initialize(viewer/"featuretable.txt",viewer/"app_settings"/"settings.xml",error)) return {};
        mGraphicsPolicy=std::move(policy);
    }
    auto device=*mGraphicsDevice;
    device.skipBenchmark=mTree.setting("SkipBenchmark").value_or(LLSD(false)).asBoolean();
    if (mTree.setting("NoHardwareProbe").value_or(LLSD(false)).asBoolean()) device.bandwidth=-1.f;
    if (const auto threshold=mTree.setting("RenderClass1MemoryBandwidth")) device.classOneBandwidth=static_cast<float>(threshold->asReal());
    return mGraphicsPolicy->settings(level,device,recommended,error);
}

bool LLVKViewerUi::initializeGraphicsPreferences(LLVKWidgetTree::Id panel,std::string& error)
{
    error.clear();
    const auto fields=preferenceFields(mTree,panel);
    for (const auto name : {"render_backend","IndirectMaxComplexity","IndirectMaxNonImpostors","preset_text"})
        if (!fields.contains(name)) { error=std::string("Original Graphics control is missing: ")+name; return false; }
    auto backend=mTree.setting("RenderBackend").value_or(LLSD("Vulkan")).asString();
    if (backend!="Vulkan" && backend!="Zink") backend="OpenGL";
    mTree.updateSetting("RenderBackendPending",LLSD(backend));
    mTree.setValue(fields.at("render_backend"),LLSD(backend));
    LLVKControl::Callback changed;
    changed.function=[this,panel](auto,const LLSD&) { graphicsPreferenceAction(panel,"Backend",{}); };
    mTree.setControlCommit(fields.at("render_backend"),std::move(changed));
#if !LL_DARWIN
    if (fields.contains("use HiDPI")) mTree.setEnabled(fields.at("use HiDPI"),false);
#endif
    const auto complexity=mTree.setting("RenderAvatarMaxComplexity").value_or(LLSD(0)).asInteger();
    const float minimum=std::log(20000.f),scale=(std::log(350000.f)-minimum)/99.f;
    const auto indirect=complexity<=0 ? 101 : static_cast<int>(std::floor((std::log(static_cast<float>(complexity))-minimum)/scale+0.5f))+1;
    mTree.updateSetting("IndirectMaxComplexity",LLSD(indirect));
    const auto avatars=mTree.setting("RenderAvatarMaxNonImpostors").value_or(LLSD(0)).asInteger();
    const auto slider=mTree.get(fields.at("IndirectMaxNonImpostors"));
    const auto maximum=slider && slider->sliderControl ? static_cast<int>(slider->sliderControl->params->bar.maximum) : 66;
    mTree.updateSetting("IndirectMaxNonImpostors",LLSD(avatars==0 ? maximum : avatars));
    auto preset=mTree.setting("PresetGraphicActive").value_or(LLSD("")).asString();
    if (preset.empty()) preset=mAboutStrings["none_paren_cap"];
    else if (preset=="Default") preset=mAboutStrings["Default"];
    mTree.setValue(fields.at("preset_text"),LLSD(preset));
    graphicsPreferenceAction(panel,"RenderOptionUpdate",{});
    error=mDialogError;
    return error.empty();
}

void LLVKViewerUi::graphicsPreferenceAction(LLVKWidgetTree::Id panel,const std::string& action,const LLSD& value)
{
    mDialogError.clear();
    if (action=="PrefSave" || action=="PrefLoad" || action=="PrefDelete") { showGraphicPreset(panel,action,mDialogError); return; }
    if (action=="HardwareDefaults" || action=="QualityPerformance")
    {
        const auto values=graphicsPolicyValues(value.asInteger(),action=="HardwareDefaults",mDialogError);
        if (!values) return;
        for (const auto& [name,setting] : *values)
        {
            const auto before=mTree.setting(name); if (!before) continue;
            if (mPreferences && mPreferences->visible()) mPreferenceSnapshot.settings.try_emplace(name,*before);
            if (!mTree.updateSetting(name,setting)) { mDialogError="Cannot apply native graphics setting: "+name; return; }
        }
        mTree.updateSetting("PresetGraphicActive",LLSD(""));
        initializeGraphicsPreferences(panel,mDialogError);
        return;
    }
    const auto fields=preferenceFields(mTree,panel);
    const auto boolean=[&](const char* name) { return mTree.setting(name).value_or(LLSD(false)).asBoolean(); };
    if (action=="Backend")
    {
        auto active=mTree.setting("RenderBackend").value_or(LLSD("Vulkan")).asString();
        if (active!="Vulkan" && active!="Zink") active="OpenGL";
        const auto selected=mTree.setting("RenderBackendPending").value_or(LLSD(active)).asString();
        if (selected==active) return;
        LLSD arguments; arguments["BACKEND"]=selected; arguments["APP_NAME"]="Vulkanstorm";
        queueNotice("ChangeRenderBackend",arguments,[this,active,selected](int option,const LLSD&)
        {
            if (option!=0) { mTree.updateSetting("RenderBackendPending",LLSD(active)); return; }
            if (!mQuitRequest || !mSavePreferences)
            { mDialogError="Native renderer switch requires persistence and shutdown services"; return; }
            if (selected!="OpenGL" && selected!="Vulkan" && selected!="Zink")
            { mDialogError="Invalid native renderer selection"; return; }
            if (!mSavePreferences({{"RenderBackend",LLSD(selected)}},mDialogError)) return;
            mQuitRequest();
        },mDialogError);
        return;
    }
    if (action=="UpdateIndirectMaxComplexity")
    {
        const auto indirect=mTree.setting("IndirectMaxComplexity").value_or(LLSD(101)).asInteger();
        const float minimum=std::log(20000.f),scale=(std::log(350000.f)-minimum)/99.f;
        const auto complexity=indirect==101 ? 0 : static_cast<int>(std::floor(std::exp(minimum+scale*(indirect-1))+0.5f));
        mTree.updateSetting("RenderAvatarMaxComplexity",LLSD(complexity));
    }
    else if (action=="UpdateIndirectMaxNonImpostors")
    {
        const auto indirect=mTree.setting("IndirectMaxNonImpostors").value_or(LLSD(0)).asInteger();
        const auto slider=mTree.get(fields.at("IndirectMaxNonImpostors"));
        const auto maximum=slider && slider->sliderControl ? static_cast<int>(slider->sliderControl->params->bar.maximum) : 66;
        mTree.updateSetting("RenderAvatarMaxNonImpostors",LLSD(indirect>=maximum || indirect<=0 ? 0 : indirect));
    }
    else if (action=="Attached")
    {
        if (value.asString()!="RenderAttachedLights" && value.asString()!="RenderAttachedParticles")
        { mDialogError="Invalid attached rendering setting"; return; }
        mTree.updateSetting(value.asString(),LLSD(!boolean(value.asString().c_str())));
    }
    else if (action!="RenderOptionUpdate" && action!="UpdateSliderText")
    {
        if (!mGraphicsPreferenceHandler) { mDialogError="Native graphics preference service is not bound: "+action; return; }
        if (!mGraphicsPreferenceHandler(action,value,mDialogError)) return;
    }
    for (const auto& [setting,label] : {std::pair{"RenderAvatarMaxComplexity","IndirectMaxComplexityText"},
        std::pair{"RenderAvatarMaxNonImpostors","IndirectMaxNonImpostorsText"}})
    {
        const auto amount=mTree.setting(setting).value_or(LLSD(0)).asInteger();
        if (fields.contains(label)) mTree.setValue(fields.at(label),LLSD(amount==0 ? mAboutStrings["no_limit"] : std::to_string(amount)));
    }
    for (const auto name : {"TonemapMix","TonemapType","TonemapTypeText","RenderExposure","RenderSharpness"})
        if (fields.contains(name)) mTree.setEnabled(fields.at(name),boolean("RenderDisableVintageMode"));
}

bool LLVKViewerUi::refreshSkinPreferences(LLVKWidgetTree::Id panel,bool fromSettings,bool skinChanged,std::string& error)
{
    error.clear();
    if (mSkinCatalog.isUndefined())
    {
        const auto xml=readText(mSkinBaseDirectory/"skins.xml");
        if (!xml) { error="Native skin catalog is unavailable"; return false; }
        std::istringstream stream(*xml);
        if (LLSDSerialize::fromXML(mSkinCatalog,stream)<=0 || !mSkinCatalog.isArray() || mSkinCatalog.size()>256)
        { error="Invalid native skin catalog"; return false; }
    }
    const auto fields=preferenceFields(mTree,panel);
    if (!fields.contains("skin_combobox") || !fields.contains("theme_combobox") || !fields.contains("skin_preview"))
    { error="Original skin selector controls are missing"; return false; }
    const auto skinCombo=fields.at("skin_combobox"),themeCombo=fields.at("theme_combobox");
    auto& draft=mSkinDrafts[panel];
    draft.edited=!fromSettings;
    if (fromSettings)
    {
        draft.skin=mTree.setting("SkinCurrent").value_or(LLSD("default")).asString();
        draft.theme=mTree.setting("SkinCurrentTheme").value_or(LLSD("")).asString();
    }
    else if (skinChanged) draft.skin=mTree.value(skinCombo).asString();
    else draft.theme=mTree.value(themeCombo).asString();
    const auto component=[](const std::string& value,bool empty)
    { return (empty || !value.empty()) && value!="." && value!=".." && value.find_first_of("/\\:")==std::string::npos; };
    std::vector<LLVKWidgetTree::ComboItem> skins,themes;
    for (auto iterator=mSkinCatalog.beginArray(); iterator!=mSkinCatalog.endArray(); ++iterator)
    {
        const auto& entry=*iterator;
        const auto folder=entry["folder"].asString();
        if (!component(folder,false)) { error="Invalid native skin catalog folder"; return false; }
        if (std::filesystem::is_directory(mSkinBaseDirectory/folder)) skins.push_back({entry["name"].asString(),LLSD(folder)});
    }
    if (skins.empty()) { error="No installed native skins were found"; return false; }
    auto selected=std::find_if(skins.begin(),skins.end(),[&](const auto& item) { return item.value.asString()==draft.skin; });
    if (selected==skins.end()) selected=std::find_if(skins.begin(),skins.end(),[](const auto& item) { return item.value.asString()=="default"; });
    if (selected==skins.end()) selected=skins.begin();
    draft.skin=selected->value.asString(); draft.skinName=selected->label;
    for (auto iterator=mSkinCatalog.beginArray(); iterator!=mSkinCatalog.endArray(); ++iterator)
    {
        const auto& entry=*iterator;
        if (entry["folder"].asString()!=draft.skin) continue;
        for (auto themeIterator=entry["themes"].beginArray(); themeIterator!=entry["themes"].endArray(); ++themeIterator)
        {
            const auto& theme=*themeIterator;
            const auto folder=theme["folder"].asString();
            if (!component(folder,true)) { error="Invalid native theme catalog folder"; return false; }
            if (folder.empty() || std::filesystem::is_directory(mSkinBaseDirectory/draft.skin/"themes"/folder))
                themes.push_back({theme["name"].asString(),LLSD(folder)});
        }
    }
    if (themes.empty()) { error="No installed themes were found for the selected skin"; return false; }
    const auto theme=std::find_if(themes.begin(),themes.end(),[&](const auto& item) { return item.value.asString()==draft.theme; });
    const auto& selectedTheme=theme==themes.end() ? themes.front() : *theme;
    draft.theme=selectedTheme.value.asString(); draft.themeName=selectedTheme.label;
    if (!mTree.replaceComboItems(skinCombo,std::move(skins),error) || !mTree.setValue(skinCombo,LLSD(draft.skin)) ||
        !mTree.replaceComboItems(themeCombo,std::move(themes),error) || !mTree.setValue(themeCombo,LLSD(draft.theme))) return false;
    auto preview="skin "+draft.skinName+" "+draft.themeName;
    LLStringUtil::toLower(preview);
    const auto image=mTree.findImage(preview,error);
    if (!image) return false;
    return mTree.setButtonImages(fields.at("skin_preview"),image,image);
}

void LLVKViewerUi::networkPreferenceAction(LLVKWidgetTree::Id panel,const std::string& action)
{
    mDialogError.clear();
    const auto settingPath=[&](const char* name)
    {
        const auto value=mTree.setting(name).value_or(LLSD("")).asString();
        return std::filesystem::path(std::u8string(value.begin(),value.end()));
    };
    const auto pathValue=[](const std::filesystem::path& path)
    { const auto bytes=path.u8string(); return LLSD(std::string(bytes.begin(),bytes.end())); };
    if (action=="Javascript")
    {
        if (!mTree.setting("BrowserJavascriptEnabled").value_or(LLSD(true)).asBoolean())
            queueNotice("DisableJavascriptBreaksSearch",{},{},mDialogError);
    }
    else if (action=="ClearCache" || action=="WebBrowserClearCache")
    {
        const bool browser=action=="WebBrowserClearCache";
        queueNotice(browser ? "ConfirmClearWebBrowserCache" : "ConfirmClearCache",{},[this,browser](int option,const LLSD&)
        {
            if (option!=0) return;
            const auto name=browser ? "FSStartupClearBrowserCache" : "PurgeCacheOnNextStartup";
            if (!mSavePreferences || !mSavePreferences({{name,LLSD(true)}},mDialogError))
            { if (mDialogError.empty()) mDialogError="Cache clearing request could not be saved"; return; }
            mTree.updateSetting(name,LLSD(true));
            if (!browser) queueNotice("CacheWillClear",{},{},mDialogError);
        },mDialogError);
    }
    else if (action=="ResetCache")
    {
        if (mCacheDirectory==mDefaultCacheDirectory) return;
        mTree.updateSetting("NewCacheLocation",LLSD(""));
        mTree.updateSetting("NewCacheLocationTopFolder",LLSD(""));
        mTree.updateSetting("CacheLocation",pathValue(mCacheDirectory));
        mTree.updateSetting("CacheLocationTopFolder",pathValue(mCacheDirectory.filename()));
        queueNotice("CacheWillBeMoved",{},{},mDialogError);
    }
    else if (action=="ResetSoundCache")
    {
        mTree.updateSetting("FSSoundCacheLocation",LLSD(""));
        queueNotice("SoundCacheWillBeMoved",{},{},mDialogError);
    }
    else if (action=="SetCache" || action=="SetSoundCache")
    {
        if (!mDirectoryPicker) { mDialogError="Native directory picker is unavailable"; return; }
        const bool sound=action=="SetSoundCache";
        const auto current=settingPath(sound ? "FSSoundCacheLocation" : "CacheLocation");
        mDirectoryPicker(current,[this,panel,sound,current,pathValue,generation=mPreferenceGeneration](auto selected,std::string problem)
        {
            if (generation!=mPreferenceGeneration || !mTree.get(panel)) return;
            if (!problem.empty()) { mDialogError=std::move(problem); return; }
            if (!selected || selected->empty() || *selected==current) return;
            if (!selected->is_absolute()) { mDialogError="Native cache directory must be absolute"; return; }
            if (sound) mTree.updateSetting("FSSoundCacheLocation",pathValue(*selected));
            else
            {
                mTree.updateSetting("NewCacheLocation",pathValue(*selected));
                mTree.updateSetting("NewCacheLocationTopFolder",pathValue(selected->filename()));
            }
            queueNotice(sound ? "SoundCacheWillBeMoved" : "CacheWillBeMoved",{},{},mDialogError);
        },mDialogError);
    }
    else if (action=="BrowseCache" || action=="BrowseSoundCache" || action=="BrowseCrashLogs" || action=="BrowseSettingsDir")
    {
        auto path=action=="BrowseCache" ? mCacheDirectory : action=="BrowseSoundCache" ? settingPath("FSSoundCacheLocation") :
            mProfileDirectory/(action=="BrowseCrashLogs" ? "logs" : "user_settings");
        if (action=="BrowseSoundCache" && path.empty()) path=mCacheDirectory;
        if (!mDirectoryOpener) { mDialogError="Native directory opener is unavailable"; return; }
        mDirectoryOpener(path,mDialogError);
    }
    else mDialogError="This directory operation requires an authenticated account";
}

bool LLVKViewerUi::refreshContactColumns(LLVKWidgetTree::Id panel,std::string& error)
{
    error.clear();
    const auto fields=preferenceFields(mTree,panel);
    const std::array<std::string,3> names{"FSFriendListColumnShowUserName","FSFriendListColumnShowDisplayName","FSFriendListColumnShowFullName"};
    std::array<bool,3> enabled{};
    for (std::size_t index=0; index<names.size(); ++index)
    {
        const auto setting=mTree.setting(names[index]);
        if (!fields.contains(names[index]) || !setting) { error="Missing native contact column preference: "+names[index]; return false; }
        enabled[index]=setting->asBoolean();
    }
    for (std::size_t index=0; index<names.size(); ++index)
        mTree.setEnabled(fields.at(names[index]),enabled[(index+1)%3] || enabled[(index+2)%3]);
    return true;
}

bool LLVKViewerUi::populateFontPresets(LLVKWidgetTree::Id combo,std::string& error)
{
    error.clear();
    std::vector<LLVKWidgetTree::ComboItem> items;
    try
    {
        for (const auto& directory : mFontPresetDirectories)
        {
            if (!std::filesystem::exists(directory)) continue;
            for (const auto& entry : std::filesystem::directory_iterator(directory))
            {
                if (!entry.is_regular_file() || entry.path().extension()!=".xml") continue;
                const auto filename=entry.path().filename().string();
                std::string label;
                if (filename=="fonts.xml") label="Inter";
                else if (filename.starts_with("fonts_") && filename.size()>10)
                {
                    label=filename.substr(6,filename.size()-10);
                    LLStringUtil::replaceChar(label,'_',' ');
                    label.front()=LLStringOps::toUpper(label.front());
                }
                else continue;
                items.push_back({label,LLSD(filename)});
                if (items.size()>1024) { error="Native font preset limit exceeded"; return false; }
            }
        }
    }
    catch (const std::exception& failure) { error=std::string("Cannot enumerate native font presets: ")+failure.what(); return false; }
    if (!mTree.replaceComboItems(combo,std::move(items),error)) return false;
    return mTree.setValue(combo,mTree.setting("FSFontSettingsFile").value_or(LLSD("fonts.xml")));
}

bool LLVKViewerUi::refreshCrashPreferences(LLVKWidgetTree::Id panel,bool fromSettings,std::string& error)
{
    error.clear();
    const auto fields=preferenceFields(mTree,panel);
    for (const auto name : {"checkSendCrashReports","checkSendCrashReportsAlwaysAsk","checkSendSettings","checkSendName","textInformation4","textRestartRequired"})
        if (!fields.contains(name)) { error=std::string("Missing native crash preference control: ")+name; return false; }
    if (fromSettings)
    {
        for (const auto name : {"CrashSubmitBehavior","CrashSubmitSettings","CrashSubmitName"})
            if (!mCrashSettings.contains(name)) { error=std::string("Missing native crash setting: ")+name; return false; }
        const auto behavior=mCrashSettings.at("CrashSubmitBehavior").asInteger();
        mTree.setValue(fields.at("checkSendCrashReports"),LLSD(behavior!=2));
        mTree.setValue(fields.at("checkSendCrashReportsAlwaysAsk"),LLSD(behavior==0));
        mTree.setValue(fields.at("checkSendSettings"),mCrashSettings.at("CrashSubmitSettings"));
        mTree.setValue(fields.at("checkSendName"),mCrashSettings.at("CrashSubmitName"));
        const auto url=mTree.panelString(panel,"PrivacyPolicyUrl",{},error);
        if (!url || !mTree.setPlainTextArgument(fields.at("textInformation4"),"[URL]",*url,error)) return false;
        mTree.setVisible(fields.at("textRestartRequired"),mCrashSettingsRequireRestart);
    }
    mTree.setEnabled(fields.at("checkSendCrashReports"),true);
    const bool enabled=mTree.value(fields.at("checkSendCrashReports")).asBoolean();
    for (const auto name : {"checkSendCrashReportsAlwaysAsk","checkSendSettings","checkSendName"}) mTree.setEnabled(fields.at(name),enabled);
    return true;
}

bool LLVKViewerUi::updateStartupPreferenceMaturity(LLVKWidgetTree::Id panel,std::string& error)
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

void LLVKViewerUi::enableAutoReplaceEntry(bool enabled)
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

bool LLVKViewerUi::refreshAutoReplace(bool entries,std::string& error)
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

void LLVKViewerUi::promptAutoReplaceList(LLSD list,bool conflict)
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

void LLVKViewerUi::chooseAutoReplaceFile(bool save)
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

bool LLVKViewerUi::showAutoReplace(std::string& error)
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

bool LLVKViewerUi::updateSpelling(std::string& error)
{
    std::vector<std::string> dictionaries;
    const auto setting=mTree.setting("SpellCheckDictionary").value_or(LLSD("")).asString();
    boost::split(dictionaries,setting,boost::is_any_of(","));
    const auto primary=dictionaries.front(); dictionaries.erase(dictionaries.begin());
    return mSpelling->activate(mTree.setting("SpellCheck").value_or(LLSD(false)).asBoolean() ? primary : "",dictionaries,error);
}

void LLVKViewerUi::updateSpellRemoval()
{
    if (!mSpellCheck || !mSpellCheck->visible()) return;
    const auto* list=mTree.get(mSpellFields.at("spellcheck_available_list"));
    if (!list || !list->scrollList) return;
    bool selected=false,allowed=true;
    for (const auto& row : list->scrollList->rows)
        if (row.selected) { selected=true; allowed&=mSpelling->canRemove(row.value.asString()); }
    mTree.setEnabled(mSpellFields.at("spellcheck_remove_btn"),selected && allowed);
}

bool LLVKViewerUi::commitSpellCheck(std::string& error)
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

bool LLVKViewerUi::refreshSpellCheck(bool fromSettings,std::string& error)
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

bool LLVKViewerUi::showSpellCheck(std::string& error)
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

bool LLVKViewerUi::showSpellImport(std::string& error)
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

LLSD LLVKViewerUi::translationKey(const std::string& service) const
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

void LLVKViewerUi::updateTranslationControls()
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

void LLVKViewerUi::invalidateTranslation(const std::string& service)
{
    ++mTranslationRequests[service]; mTranslationVerified[service]=false;
    updateTranslationControls();
}

void LLVKViewerUi::verifyTranslation(const std::string& service,bool alert)
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
            if (message) enqueueNotice({"GenericAlert",*message},mDialogError);
        }
    },mDialogError);
}

bool LLVKViewerUi::showTranslation(std::string& error)
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

bool LLVKViewerUi::updateJoystickPreview(std::string& error)
{
    error.clear();
    if (!mJoystickServices.poll) { error="Native joystick service is unavailable"; return false; }
    const auto state=mJoystickServices.poll(error);
    if (!state) return false;
    const bool enabled=mTree.setting("JoystickEnabled").value_or(LLSD(false)).asBoolean();
    for (std::size_t index=0; index<8; ++index)
    {
        const auto group=mJoystickFields.at("axis_view_"+std::to_string(index));
        const auto bar=mJoystickFields.at("axis"+std::to_string(index));
        const bool present=enabled && index<state->axisCount;
        if (mTree.get(group)->containerView->displayChildren!=present && !mTree.setContainerExpanded(group,present,error)) return false;
        if (present)
        {
            const auto value=state->axes[index];
            if (std::fabs(value)>mTree.get(bar)->statBar->maximum && !mTree.setStatBarRange(bar,-std::fabs(value),std::fabs(value),error)) return false;
            if (!mTree.sampleStatBar(bar,value,error)) return false;
        }
    }
    for (std::size_t index=0; index<16; ++index)
    {
        const char* name=!enabled || index>=state->buttonCount ? "DkGray" : state->buttons[index] ? "White" : "Gray";
        const auto color=mColors->find(name);
        if (!color || !mTree.setIconColor(mJoystickFields.at("button_light_"+std::to_string(index)),*color)) return false;
    }
    return true;
}

bool LLVKViewerUi::restoreJoystick(std::string& error)
{
    if (!mTree.restorePreferences(mJoystickSnapshot,{},error)) return false;
    if (!mJoystickServices.select) { error="Native joystick selection service is unavailable"; return false; }
    const auto selected=mJoystickServices.select(mTree.setting("JoystickDeviceUUID").value_or(LLSD("")),error);
    return selected.has_value();
}

void LLVKViewerUi::setJoystickDefaults()
{
    const std::array<int,7> axes{1,0,2,4,3,5,-1};
    for (std::size_t index=0; index<axes.size(); ++index) mTree.updateSetting("JoystickAxis"+std::to_string(index),LLSD(axes[index]));
    for (const auto& [name,value] : std::map<std::string,bool>{{"Cursor3D",true},{"AutoLeveling",true},{"ZoomDirect",false}}) mTree.updateSetting(name,LLSD(value));
    const std::map<std::string,std::vector<float>> groups{{"AvatarAxisScale",{2,2,1,0,0.1f,0.1f}},
        {"BuildAxisScale",{0.3f,0.3f,0.3f,0.3f,0.3f,0.3f}},{"FlycamAxisScale",{2.1f,2,2,0,0.1f,0.15f,0}},
        {"AvatarAxisDeadZone",{0.1f,0.1f,0.1f,1,0.02f,0.01f}},{"BuildAxisDeadZone",{0.01f,0.01f,0.01f,0.01f,0.01f,0.01f}},
        {"FlycamAxisDeadZone",{0.01f,0.01f,0.01f,0.01f,0.01f,0.01f,1}}};
    for (const auto& [name,values] : groups)
        for (std::size_t index=0; index<values.size(); ++index) mTree.updateSetting(name+std::to_string(index),LLSD(values[index]));
    mTree.updateSetting("AvatarFeathering",LLSD(6.f)); mTree.updateSetting("BuildFeathering",LLSD(12.f)); mTree.updateSetting("FlycamFeathering",LLSD(5.f));
}

void LLVKViewerUi::updateProxyControls()
{
    if (mProxyFields.empty()) return;
    const bool authenticated=mTree.setting("Socks5AuthType").value_or(LLSD("None")).asString()=="UserPass";
    for (const auto name : {"socks5_username","socks5_password"}) mTree.setEnabled(mProxyFields.at(name),authenticated);
    const auto mode=mTree.setting("HttpProxyType").value_or(LLSD("None")).asString();
    if ((mode=="Socks" && !mTree.setting("Socks5ProxyEnabled").value_or(LLSD(false)).asBoolean()) ||
        (mode=="Web" && !mTree.setting("BrowserProxyEnabled").value_or(LLSD(false)).asBoolean()))
        mTree.updateSetting("HttpProxyType",LLSD("None"));
}

bool LLVKViewerUi::acceptProxy(std::string& error)
{
    error.clear();
    if (!mProxy || !mProxy->visible()) { error="Proxy settings are not open"; return false; }
    if (const auto focused=mTree.get(mTree.keyboardFocus()); focused && focused->lineEditor)
        mTree.commit(mTree.keyboardFocus());
    updateProxyControls();
    if (!httpProxy(error)) return false;
    std::optional<LLVKProxy::Credentials> credentials;
    if (mTree.setting("Socks5AuthType").value_or(LLSD("None")).asString()=="UserPass")
    {
        credentials=LLVKProxy::Credentials{mTree.value(mProxyFields.at("socks5_username")).asString(),
            mTree.value(mProxyFields.at("socks5_password")).asString()};
        if (credentials->username.empty() || credentials->username.size()>255 ||
            credentials->password.empty() || credentials->password.size()>255 ||
            credentials->username.find('\0')!=std::string::npos || credentials->password.find('\0')!=std::string::npos)
        { error="SOCKS5 requires a username and password of 1 to 255 bytes"; return false; }
    }
    if (!mSaveProxyCredentials) { error="Protected proxy credential service is unavailable"; return false; }
    if (!mSaveProxyCredentials(credentials,error)) return false;
    mProxyAccepted=true;
    return mProxy->close(error);
}

bool LLVKViewerUi::showProxy(std::string& error)
{
    error.clear();
    if (!mProxy)
    {
        mProxy=LLVKFloater::createFile(mTree,*mDialogFactory,mRoot,"floater_preferences_proxy.xml",error);
        if (!mProxy) return false;
        mProxyFields=preferenceFields(mTree,mProxy->id());
        mProxy->onClose([this]
        {
            if (!mProxyAccepted) mTree.restorePreferences(mProxySnapshot,{},mDialogError);
            for (const auto name : {"socks5_username","socks5_password"}) mTree.setValue(mProxyFields.at(name),LLSD(""));
            mProxySnapshot={};
        });
    }
    if (!mProxy->visible())
    {
        const auto snapshot=mTree.snapshotPreferences(mProxy->id(),error);
        if (!snapshot) return false;
        mProxySnapshot=*snapshot;
        mProxyAccepted=false;
        if (mTree.setting("Socks5AuthType").value_or(LLSD("None")).asString()=="UserPass")
        {
            const auto credentials=proxyCredentials(error);
            if (!credentials) return false;
            mTree.setValue(mProxyFields.at("socks5_username"),LLSD(credentials->username));
            mTree.setValue(mProxyFields.at("socks5_password"),LLSD(credentials->password));
        }
        updateProxyControls();
    }
    mActiveFloater=mProxy.get();
    return mProxy->open(error);
}

std::optional<LLVKProxy::Endpoint> LLVKViewerUi::httpProxy(std::string& error) const
{
    std::map<std::string,LLSD> settings;
    for (const auto name : {"HttpProxyType","BrowserProxyEnabled","BrowserProxyAddress","BrowserProxyPort",
        "Socks5ProxyEnabled","Socks5ProxyHost","Socks5ProxyPort","Socks5AuthType"})
        if (const auto value=mTree.setting(name)) settings[name]=*value;
    return LLVKProxy::select(settings,false,error);
}

std::optional<LLVKProxy::Credentials> LLVKViewerUi::proxyCredentials(std::string& error) const
{
    error.clear();
    if (!mLoadProxyCredentials) { error="Protected proxy credential service is unavailable"; return {}; }
    return mLoadProxyCredentials(error);
}

bool LLVKViewerUi::showJoystick(std::string& error)
{
    error.clear();
    if (!mJoystickServices.enumerate || !mJoystickServices.select || !mJoystickServices.poll)
    { error="Native joystick services are not bound"; return false; }
    if (!mJoystick)
    {
        mJoystick=LLVKFloater::createFile(mTree,*mDialogFactory,mRoot,"floater_joystick.xml",error);
        if (!mJoystick) return false;
        mJoystickFields=preferenceFields(mTree,mJoystick->id());
        const auto bind=[&](const char* name,std::function<void(LLVKWidgetTree::Id,const LLSD&)> function)
        {
            const auto found=mJoystickFields.find(name);
            if (found==mJoystickFields.end()) { error="Missing native joystick action"; return false; }
            LLVKControl::Callback callback; callback.function=std::move(function);
            return mTree.setControlCommit(found->second,std::move(callback));
        };
        for (std::size_t index=0; index<8; ++index)
        {
            const auto bar=mJoystickFields.find("axis"+std::to_string(index));
            if (bar==mJoystickFields.end() || !mTree.get(bar->second)->statBar || !mTree.setStatBarRange(bar->second,-0.5f,0.5f,error))
            { mJoystick.reset(); return false; }
            const auto spinner=mJoystickFields.find("JoystickAxis"+std::to_string(index));
            if (spinner!=mJoystickFields.end() && !mTree.setSpinnerRange(spinner->second,-1,7,error)) { mJoystick.reset(); return false; }
        }
        if (!bind("joystick_combo",[this](auto id,const LLSD&)
        {
            const auto value=mTree.value(id);
            const auto selected=mJoystickServices.select(value,mDialogError);
            if (!selected) return;
            mTree.updateSetting("JoystickDeviceUUID",LLSD(*selected));
            mTree.updateSetting("JoystickEnabled",LLSD(!value.isInteger() || value.asInteger()!=0));
            updateJoystickPreview(mDialogError);
        }) || !bind("JoystickFlycamEnabled",[this](auto,const LLSD&) { updateJoystickPreview(mDialogError); }) ||
            !bind("SpaceNavigatorDefaults",[this](auto,const LLSD&) { setJoystickDefaults(); }) ||
            !bind("cancel_btn",[this](auto,const LLSD&) { if (restoreJoystick(mDialogError)) mJoystick->close(mDialogError); }) ||
            !bind("ok_btn",[this](auto,const LLSD&)
            {
                std::map<std::string,LLSD> changed;
                for (const auto& [name,value] : mJoystickSnapshot.settings)
                    if (const auto current=mTree.setting(name); current && !llsd_equals(*current,value)) changed[name]=*current;
                if (!changed.empty() && (!mSavePreferences || !mSavePreferences(changed,mDialogError)))
                { if (mDialogError.empty()) mDialogError="Native joystick preferences could not be saved"; return; }
                mJoystick->close(mDialogError);
            })) { mJoystick.reset(); return false; }
    }
    if (!mJoystick->visible())
    {
        const auto snapshot=mTree.snapshotPreferences(mJoystick->id(),error);
        if (!snapshot) return false;
        mJoystickSnapshot=*snapshot;
        for (const auto name : {"JoystickEnabled","JoystickDeviceUUID"})
            mJoystickSnapshot.settings[name]=mTree.setting(name).value_or(LLSD());
        const auto devices=mJoystickServices.enumerate(error);
        if (!devices) return false;
        const auto none=mTree.panelString(mJoystick->id(),"JoystickDisabled",{},error);
        if (!none) return false;
        std::vector<LLVKWidgetTree::ComboItem> items{{*none,LLSD(0)}};
        for (const auto& device : *devices) items.push_back({device.name,device.id});
        const auto combo=mJoystickFields.at("joystick_combo");
        if (!mTree.replaceComboItems(combo,std::move(items),error)) return false;
        const auto selected=mJoystickServices.select(mTree.setting("JoystickDeviceUUID").value_or(LLSD("")),error);
        if (!selected) return false;
        if (!mTree.setValue(combo,LLSD(0))) return false;
        if (mTree.setting("JoystickEnabled").value_or(LLSD(false)).asBoolean())
            for (const auto& device : *devices)
            {
                if (device.persistedId==*selected) { mTree.setValue(combo,device.id); break; }
            }
    }
    mActiveFloater=mJoystick.get();
    return mJoystick->open(error);
}

bool LLVKViewerUi::activateUrl(const std::string& url,std::string& error)
{
    error.clear();
    if (url.empty() || url.size()>65536 || url.find('\0')!=url.npos)
    { error="Invalid native hyperlink"; return false; }
    const LLURI uri(url);
    auto scheme=uri.scheme(); LLStringUtil::toLower(scheme);
    if (scheme!="secondlife")
    {
        if ((scheme!="http" && scheme!="https" && scheme!="ftp") || uri.hostName().empty())
        { error="Unsupported native hyperlink scheme"; return false; }
        if (!mOpenUrl) { error="Native web link service is not bound"; return false; }
        mOpenUrl(url);
        return true;
    }
    if (!uri.authority().empty() || uri.path()!="/app/openfloater/preferences")
    { error="Native internal hyperlink destination is unavailable"; return false; }
    if (!showPreferences(error)) return false;
    const auto query=uri.queryMap();
    const auto core=find("pref core",mPreferences->id());
    if (query.has("tab"))
    {
        const auto tabs=mTree.get(core)->tabContainer->tabs;
        for (const auto& tab : tabs)
            if (mTree.get(tab.panel)->params.name==query["tab"].asString())
            {
                if (!mTree.selectTabPanel(core,tab.panel,error)) return false;
                if (query.has("subtab"))
                {
                    const auto nested=find("tabs",tab.panel);
                    const auto* container=mTree.get(nested);
                    if (container && container->tabContainer)
                        for (const auto& child : container->tabContainer->tabs)
                            if (mTree.get(child.panel)->params.name==query["subtab"].asString())
                                return mTree.selectTabPanel(nested,child.panel,error);
                }
                break;
            }
    }
    else if (query.has("search"))
    {
        if (!mTree.setValue(find("search_prefs_edit",mPreferences->id()),query["search"]))
        { error="Native Preferences search field is unavailable"; return false; }
        return filterPreferences(error);
    }
    return true;
}

bool LLVKViewerUi::copyPreferenceSearch(std::string& error)
{
    error.clear();
    if (!mPreferences || !mDialogClipboard) { error="Native Preferences clipboard is unavailable"; return false; }
    const auto fields=preferenceFields(mTree,mPreferences->id());
    const auto text="secondlife:///app/openfloater/preferences?search="+LLURI::escape(mTree.value(fields.at("search_prefs_edit")).asString());
    const auto wide=utf8str_to_wstring(text);
    return mDialogClipboard->write(std::u32string(wide.begin(),wide.end()),false,error);
}

bool LLVKViewerUi::filterPreferences(std::string& error)
{
    error.clear();
    if (!mPreferences) return true;
    const auto fields=preferenceFields(mTree,mPreferences->id());
    auto query=utf8str_to_wstring(mTree.value(fields.at("search_prefs_edit")).asString());
    LLWStringUtil::toLower(query);
    const auto matches=[&](const std::string& text)
    { auto wide=utf8str_to_wstring(text); LLWStringUtil::toLower(wide); return wide.find(query)!=LLWString::npos; };
    const auto visit=[&](const auto& self,LLVKWidgetTree::Id id) -> bool
    {
        const auto* node=mTree.get(id);
        if (!node) return false;
        mTree.setSearchHighlighted(id,false);
        bool match=query.empty();
        if (node->tabContainer)
        {
            const auto tabs=node->tabContainer->tabs;
            LLVKWidgetTree::Id first=0;
            for (const auto& tab : tabs)
            {
                const bool visible=self(self,tab.panel);
                if (!mTree.setTabVisibility(id,tab.panel,visible,error)) return false;
                self(self,tab.button);
                if (visible && !first) first=tab.panel;
                match|=visible;
            }
            if (first) mTree.selectTabPanel(id,first,error);
            return match;
        }
        if (node->control && !node->panel && node->params.visible)
        {
            std::string text=node->params.name+" "+node->params.tooltip;
            if (node->button)
            {
                const auto& label=node->button->params.label;
                text+=" "+wstring_to_utf8str(LLWString(label.begin(),label.end()));
            }
            if (node->plainText) text+=" "+mTree.value(id).asString();
            if (node->combo) for (const auto& item : node->combo->items) text+=" "+item.label;
            if (node->control->params.valueSetting) text+=" "+*node->control->params.valueSetting;
            const bool matched=matches(text);
            mTree.setSearchHighlighted(id,!query.empty() && matched);
            match|=matched;
        }
        const auto children=node->children;
        for (const auto child : children) match|=self(self,child);
        return match;
    };
    visit(visit,mPreferences->id());
    return error.empty();
}

bool LLVKViewerUi::showPreferences(std::string& error)
{
    error.clear();
    if (!mPreferences)
    {
        mPreferences=LLVKFloater::createFile(mTree,*mDialogFactory,mRoot,"floater_preferences.xml",error);
        if (!mPreferences) return false;
        const auto fields=preferenceFields(mTree,mPreferences->id());
        if (!fields.contains("OK") || !fields.contains("Cancel") || !fields.contains("pref core"))
        { error="Original Preferences hierarchy is missing required controls"; mPreferences.reset(); return false; }
        if (!mTree.setPanelDefaultButton(mPreferences->id(),fields.at("OK"),error)) { mPreferences.reset(); return false; }
        LLVKControl::Callback filter;
        filter.function=[this](auto,const LLSD&) { filterPreferences(mDialogError); };
        if (!mTree.setSearchEditorKeystroke(fields.at("search_prefs_edit"),std::move(filter)))
        { error="Native Preferences search editor is unavailable"; mPreferences.reset(); return false; }
        mPreferences->onCloseDependents([this](std::string& problem)
        {
            for (auto& [swatch,picker] : mColorPickers)
            {
                if (!picker->visible()) continue;
                for (auto ancestor=swatch; mTree.get(ancestor); ancestor=mTree.get(ancestor)->parent)
                    if (ancestor==mPreferences->id())
                    {
                        if (!picker->close(problem)) return false;
                        break;
                    }
            }
            return true;
        });
        mPreferences->onClose([this]
        {
            ++mPreferenceGeneration;
            if (mBeamColor && mBeamColor->visible()) mBeamColor->close(mDialogError);
            if (mBeamShape && mBeamShape->visible()) mBeamShape->close(mDialogError);
            for (const auto& [action,dialog] : mGraphicPresetDialogs) if (dialog->visible()) dialog->close(mDialogError);
            const auto core=find("pref core",mPreferences->id());
            const auto* tabs=mTree.get(core);
            std::optional<int> lastTab;
            if (tabs && tabs->tabContainer && mTree.value(find("search_prefs_edit",mPreferences->id())).asString().empty())
            {
                const auto& entries=tabs->tabContainer->tabs;
                const auto selected=std::find_if(entries.begin(),entries.end(),[&](const auto& tab) { return tab.panel==tabs->tabContainer->selected; });
                if (selected!=entries.end()) lastTab=static_cast<int>(selected-entries.begin());
            }
            if (!mPreferencesAccepted && !mApplicationQuitting)
            {
                struct Loading { bool& flag; bool previous; explicit Loading(bool& value) : flag(value),previous(value) { flag=true; } ~Loading() { flag=previous; } } loading(mLoadingGraphicPreset);
                if (mBindingSnapshot) mBindings=*mBindingSnapshot;
                for (const auto& [name,value] : mWarningSnapshot)
                    if (auto control=mWarningSettings->getControl(name)) control->setValue(value,false);
                mTree.restorePreferences(mPreferenceSnapshot,{},mDialogError);
                for (const auto panel : mCrashPanels)
                    if (mTree.get(panel)) refreshCrashPreferences(panel,true,mDialogError);
                for (const auto& [panel,draft] : mSkinDrafts)
                    if (mTree.get(panel)) refreshSkinPreferences(panel,true,false,mDialogError);
                if (mGraphicPresets) refreshGraphicPresetDialogs(mDialogError);
            }
            mBindingSnapshot.reset();
            mPreferenceSnapshot={};
            mWarningSnapshot.clear();
            if (lastTab && mTree.setting("LastPrefTab").value_or(LLSD(0)).asInteger()!=*lastTab)
            {
                if (mApplicationQuitting || !mSaveSettingsOnExit || !mSavePreferences || mSavePreferences({{"LastPrefTab",LLSD(*lastTab)}},mDialogError))
                    mTree.updateSetting("LastPrefTab",LLSD(*lastTab));
            }
        });
    }
    if (!mPreferences->visible())
    {
        ++mPreferenceGeneration;
        mPreferencesAccepted=false;
        const auto snapshot=mTree.snapshotPreferences(mPreferences->id(),error);
        if (!snapshot) return false;
        mPreferenceSnapshot=*snapshot;
        if (const auto clock=mTree.setting("Use24HourClock"))
        {
            mPreferenceSnapshot.settings["Use24HourClock"]=*clock;
            if (const auto combo=find("time_format_combobox",mPreferences->id()))
                mTree.setValue(combo,LLSD(clock->asBoolean() ? "1" : "0"));
        }
        if (const auto preset=mTree.setting("PresetGraphicActive")) mPreferenceSnapshot.settings["PresetGraphicActive"]=*preset;
        mBindingSnapshot=mBindings;
        const auto colors=mColors->serializeUser(error);
        if (!colors) return false;
        mUserColorsSnapshot=*colors;
        mWarningSnapshot.clear();
        for (const auto& [name,policy] : mNotificationPreferences)
            if (policy.control.empty())
                if (auto control=mWarningSettings->getControl(name)) mWarningSnapshot[name]=control->getValue();
        const auto alerts=find("msgs",mPreferences->id());
        if (alerts && !refreshNotificationPreferences(alerts,false,error)) return false;
        const auto core=find("pref core",mPreferences->id());
        const auto& tabs=mTree.get(core)->tabContainer->tabs;
        if (mTree.value(find("search_prefs_edit",mPreferences->id())).asString().empty() && !tabs.empty())
        {
            const auto saved=mTree.setting("LastPrefTab").value_or(LLSD(0)).asInteger();
            const auto selected=saved>=0 && static_cast<std::size_t>(saved)<tabs.size() ? static_cast<std::size_t>(saved) : 0;
            if (!mTree.selectTabPanel(core,tabs[selected].panel,error)) return false;
        }
    }
    mActiveFloater=mPreferences.get();
    auto placement=mTree.get(mRoot)->params.rect;
    placement.top=std::max(placement.bottom,placement.top-mNoticeMenuHeight);
    return mPreferences->open(error,placement);
}

bool LLVKViewerUi::previewUiSound(const std::string& name,std::string& error)
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

bool LLVKViewerUi::resetAccountPreference(const std::string& name,std::string& error)
{
    error.clear();
    const auto found=mAccountDefaults.find(name);
    if (found==mAccountDefaults.end() || !mTree.setting(name))
    { error="Native account preference has no loaded default: "+name; return false; }
    if (!mTree.updateSetting(name,found->second))
    { error="Native account preference reset failed: "+name; return false; }
    return true;
}

bool LLVKViewerUi::resetPreference(const std::string& name,std::string& error)
{
    error.clear();
    const auto found=mSettingDefaults.find(name);
    if (found==mSettingDefaults.end() || !mTree.setting(name))
    { error="Native preference has no loaded default: "+name; return false; }
    if (!mTree.updateSetting(name,found->second))
    { error="Native preference reset failed: "+name; return false; }
    return true;
}

bool LLVKViewerUi::applyPreferences(std::string& error)
{
    error.clear();
    if (!mPreferences || !mPreferences->visible()) return false;
    const auto focus=mTree.keyboardFocus();
    if (const auto* node=mTree.get(focus); node && node->lineEditor) mTree.commit(focus);
    auto crash=mCrashSettings;
    for (const auto panel : mCrashPanels)
    {
        if (!mTree.get(panel)) continue;
        const auto fields=preferenceFields(mTree,panel);
        const bool send=mTree.value(fields.at("checkSendCrashReports")).asBoolean();
        const bool ask=mTree.value(fields.at("checkSendCrashReportsAlwaysAsk")).asBoolean();
        crash["CrashSubmitBehavior"]=LLSD(send ? (ask ? 0 : 1) : 2);
        crash["CrashSubmitSettings"]=mTree.value(fields.at("checkSendSettings"));
        crash["CrashSubmitName"]=mTree.value(fields.at("checkSendName"));
    }
    std::map<std::string,LLSD> crashChanges;
    for (const auto& [name,value] : crash)
        if (!mCrashSettings.contains(name) || !llsd_equals(value,mCrashSettings.at(name))) crashChanges[name]=value;
    if (!crashChanges.empty() && (!mSaveCrashPreferences || !mSaveCrashPreferences(crashChanges,error)))
    { if (error.empty()) error="Native crash preference persistence is unavailable"; return false; }
    mCrashSettings=std::move(crash);
    if (mBindingSnapshot)
    {
        const auto before=mBindingSnapshot->serialize(error),after=mBindings.serialize(error);
        if (!before || !after) return false;
        if (*before!=*after && (!mSaveKeyBindings || !mSaveKeyBindings(mBindings,error)))
        { if (error.empty()) error="Native keybinding persistence is unavailable"; return false; }
    }
    std::map<std::string,LLSD> changed,accountChanges;
    for (const auto& [name,old] : mPreferenceSnapshot.settings)
    {
        if (name=="RenderBackendPending") continue;
        const auto value=mTree.setting(name);
        if (!value || llsd_equals(*value,old)) continue;
        if (mAccountSettingNames.contains(name))
        {
            if (mAccountSettingsLoaded) accountChanges[name]=*value;
        }
        else changed[name]=*value;
    }
    std::map<std::string,LLSD> skinChanges;
    std::string skinMessage;
    for (const auto& [panel,draft] : mSkinDrafts)
    {
        if (!mTree.get(panel) || !draft.edited || (draft.skin==mTree.setting("SkinCurrent").value_or(LLSD()).asString() &&
            draft.theme==mTree.setting("SkinCurrentTheme").value_or(LLSD()).asString())) continue;
        skinChanges["SkinCurrent"]=draft.skin; skinChanges["SkinCurrentTheme"]=draft.theme;
        skinChanges["FSSkinCurrentReadableName"]=draft.skinName; skinChanges["FSSkinCurrentThemeReadableName"]=draft.themeName;
        if (mTree.setting("FSSkinClobbersToolbarPrefs").value_or(LLSD(false)).asBoolean()) skinChanges["ResetToolbarSettings"]=true;
        if (draft.skin=="starlight" || draft.skin=="starlightcui")
        {
            if (mTree.setting("ShowMenuBarLocation").value_or(LLSD(false)).asBoolean())
            {
                skinChanges["ShowMenuBarLocation"]=false;
                skinMessage=mAboutStrings["skin_defaults_starlight_location"];
            }
            if (!mTree.setting("ShowNavbarNavigationPanel").value_or(LLSD(false)).asBoolean())
            {
                skinChanges["ShowNavbarNavigationPanel"]=true;
                if (!skinMessage.empty()) skinMessage+='\n';
                skinMessage+=mAboutStrings["skin_defaults_starlight_navbar"];
            }
        }
    }
    for (const auto& [name,value] : skinChanges) changed[name]=value;
    std::map<std::string,LLSD> warningChanges;
    for (const auto& [name,value] : mWarningSnapshot)
        if (auto control=mWarningSettings->getControl(name); control && !llsd_equals(control->getValue(),value))
            warningChanges[name]=control->getValue();
    if (!warningChanges.empty() && !mSaveWarningPreferences)
    { error="Native notification preference persistence is unavailable"; return false; }
    if (!accountChanges.empty() && !mSaveAccountPreferences)
    { error="Native account preference persistence is unavailable"; return false; }
    const auto colors=mColors->serializeUser(error);
    if (!colors) return false;
    if (*colors!=mUserColorsSnapshot && !mUserColorsFile.empty() && !mColors->saveUserFile(mUserColorsFile,error)) return false;
    if (!changed.empty() && (!mSavePreferences || !mSavePreferences(changed,error)))
    { if (error.empty()) error="Native preference persistence is unavailable"; return false; }
    if (!accountChanges.empty() && !mSaveAccountPreferences(accountChanges,error)) return false;
    if (!warningChanges.empty() && !mSaveWarningPreferences(warningChanges,error)) return false;
    for (const auto& [name,value] : warningChanges) mWarningSettings->getControl(name)->setValue(value,true);
    for (const auto& [name,value] : skinChanges) mTree.updateSetting(name,value);
    if (!skinChanges.empty())
    {
        const auto restart=[this]
        {
            LLSD arguments; arguments["APP_NAME"]="Vulkanstorm";
            queueNotice("ChangeSkin",arguments,[this](int option,const LLSD&)
            {
                if (option!=0) return;
                if (mQuitRequest) mQuitRequest();
                else mDialogError="Native shutdown handler is unavailable";
            },mDialogError);
        };
        if (skinMessage.empty()) restart();
        else
        {
            LLSD arguments; arguments["MESSAGE"]=skinMessage;
            queueNotice("SkinDefaultsChangeSettings",arguments,[restart](int,const LLSD&) { restart(); },mDialogError);
        }
    }
    mPreferencesAccepted=true;
    return mPreferences->close(error);
}

bool LLVKViewerUi::showAbout(std::string& error)
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
bool LLVKViewerUi::showUiPreview(std::string& error)
{
    error.clear();
    if (!mUiPreview)
    {
        auto floater=LLVKFloater::createFile(mTree,*mDialogFactory,mRoot,"floater_ui_preview.xml",error);
        if (!floater) return false;
        mPreviewFields=preferenceFields(mTree,floater->id());
        mTree.setVisible(mPreviewFields.at("overlap_scroll"),false);
        std::vector<LLVKWidgetTree::ComboItem> languages;
        std::error_code status;
        for (const auto& entry : std::filesystem::directory_iterator(mPreviewSkin.skinBaseDirectory/"default"/"xui",status))
        {
            const auto name=entry.path().filename().string();
            if (entry.is_directory() && !name.starts_with("template") && name.find('.')==std::string::npos)
                languages.push_back({name,LLSD(name)});
        }
        if (status) { error="Cannot enumerate native preview languages: "+status.message(); return false; }
        std::sort(languages.begin(),languages.end(),[](const auto& first,const auto& second)
        { if (first.label=="en" || second.label=="en") return first.label=="en" && second.label!="en"; return first.label<second.label; });
        for (std::size_t slot=0; slot<2; ++slot)
        {
            const std::string suffix=slot ? "_2" : "";
            const auto language=mPreviewFields.at(slot ? "language_select_combo_2" : "language_select_combo");
            if (!mTree.replaceComboItems(language,languages,error) ||
                !mTree.setComboValue(language,LLSD(languages.empty() ? "" : languages.front().label),error)) return false;
            LLVKControl::Callback show,hide,changed;
            show.function=[this,slot](auto,const LLSD&) { displayUiPreview(slot,mDialogError); };
            hide.function=[this,slot,suffix](auto,const LLSD&)
            {
                if (mPreviewFloaters[slot]) mPreviewFloaters[slot]->close(mDialogError);
                mTree.setEnabled(mPreviewFields.at("close_displayed_floater"+suffix),false);
            };
            changed.function=[this,slot](auto,const LLSD&)
            {
                if (slot==0 && !refreshUiPreview(mDialogError)) return;
                if (mPreviewFloaters[slot] && mPreviewFloaters[slot]->visible()) displayUiPreview(slot,mDialogError);
            };
            mTree.setControlCommit(mPreviewFields.at("display_floater"+suffix),std::move(show));
            mTree.setControlCommit(mPreviewFields.at("close_displayed_floater"+suffix),std::move(hide));
            mTree.setControlCommit(language,std::move(changed));
        }
        LLVKControl::Callback refresh;
        refresh.function=[this](auto,const LLSD&) { refreshUiPreview(mDialogError); };
        mTree.setControlCommit(mPreviewFields.at("refresh_btn"),std::move(refresh));
        LLVKControl::Callback doubleClick;
        doubleClick.function=[this](auto,const LLSD&) { displayUiPreview(0,mDialogError); };
        mTree.setScrollListActions(mPreviewFields.at("name_list"),std::move(doubleClick),{});
        LLVKControl::Callback overlap;
        overlap.function=[this](auto,const LLSD&)
        {
            const auto root=mTree.get(mUiPreview->id())->params.rect;
            const auto main=mTree.get(mPreviewFields.at("main_panel"))->params.rect;
            const auto scroll=mTree.get(mPreviewFields.at("overlap_scroll"))->params.rect;
            const auto width=scroll.right-scroll.left;
            if (!mTree.reshape(mUiPreview->id(),root.right-root.left+(mPreviewOverlaps ? -width : width),root.top-root.bottom,mDialogError)) return;
            mTree.setShape(mPreviewFields.at("main_panel"),main,mDialogError);
            mPreviewOverlaps=!mPreviewOverlaps;
            mTree.setVisible(mPreviewFields.at("overlap_scroll"),mPreviewOverlaps);
            mTree.setVisible(mPreviewFields.at("overlap_panel"),mPreviewOverlaps);
        };
        mTree.setControlCommit(mPreviewFields.at("toggle_overlap_panel"),std::move(overlap));
        if (!mTree.setPanelDefaultButton(floater->id(),mPreviewFields.at("display_floater"),error)) return false;
        floater->onClose([this]
        { for (auto& preview : mPreviewFloaters) if (preview) preview->close(mDialogError); });
        mUiPreview=std::move(floater);
        if (!refreshUiPreview(error)) return false;
    }
    mActiveFloater=mUiPreview.get();
    return mUiPreview->open(error);
}

bool LLVKViewerUi::refreshUiPreview(std::string& error)
{
    error.clear();
    const auto language=mTree.value(mPreviewFields.at("language_select_combo")).asString();
    std::vector<LLVKWidgetTree::ListRow> rows;
    std::error_code status;
    for (const auto& entry : std::filesystem::directory_iterator(mPreviewSkin.skinBaseDirectory/"default"/"xui"/language,status))
    {
        if (!entry.is_regular_file() || entry.path().extension()!=".xml") continue;
        const auto file=entry.path().filename().string();
        if (!file.starts_with("floater_") && !file.starts_with("panel_") && !file.starts_with("menu_") &&
            !file.starts_with("inspect_") && !file.starts_with("sidepanel_") && !file.starts_with("fs_") && !file.starts_with("exo_")) continue;
        const auto xml=readText(entry.path());
        if (!xml) { error="Cannot read native preview file: "+file; return false; }
        try
        {
            boost::property_tree::ptree document; std::istringstream input(*xml);
            boost::property_tree::read_xml(input,document,boost::property_tree::xml_parser::no_comments);
            if (document.empty()) continue;
            const auto& root=document.begin()->second;
            LLVKWidgetTree::ListRow row; row.value=file;
            row.cells={root.get<std::string>("<xmlattr>.title",""),file,root.get<std::string>("<xmlattr>.name","Error: unable to load "+file)};
            rows.push_back(std::move(row));
        }
        catch (const std::exception& exception) { error="Native preview XML "+file+": "+exception.what(); return false; }
    }
    if (status) { error="Cannot enumerate native preview files: "+status.message(); return false; }
    std::sort(rows.begin(),rows.end(),[](const auto& first,const auto& second) { return first.cells[1]<second.cells[1]; });
    if (!rows.empty()) rows.front().selected=true;
    return mTree.setScrollListRows(mPreviewFields.at("name_list"),std::move(rows),error);
}

bool LLVKViewerUi::displayUiPreview(std::size_t slot,std::string& error)
{
    error.clear();
    if (slot>=mPreviewFloaters.size()) { error="Invalid native preview slot"; return false; }
    const auto* list=mTree.get(mPreviewFields.at("name_list"));
    const auto selected=std::find_if(list->scrollList->rows.begin(),list->scrollList->rows.end(),[](const auto& row) { return row.selected; });
    if (selected==list->scrollList->rows.end()) return true;
    const auto file=selected->value.asString();
    if (file.starts_with("menu_")) return true;
    auto skin=mPreviewSkin;
    skin.language=mTree.value(mPreviewFields.at(slot ? "language_select_combo_2" : "language_select_combo")).asString();
    auto factory=*mDialogFactory;
    factory.setSkinFiles(std::make_shared<LLVKSkinFiles>(skin));
    std::unique_ptr<LLVKFloater> floater;
    if (file.starts_with("floater_") || file.starts_with("inspect_"))
        floater=LLVKFloater::createFile(mTree,factory,mRoot,file,error);
    else
    {
        floater=LLVKFloater::createXml(mTree,factory,mRoot,
            "<floater name='native_panel_preview' width='400' height='300' can_resize='true' min_width='10' min_height='25'/>",error);
        if (!floater) return false;
        const auto panel=factory.constructFile(mTree,file,floater->id(),error);
        if (!panel || !mTree.get(*panel)->panel) return false;
        const auto rectangle=mTree.get(*panel)->params.rect;
        const auto bounds=mTree.boundingRect(*panel,0,error);
        if (!bounds) return false;
        const auto width=std::max(rectangle.right,bounds->right)-std::min(rectangle.left,bounds->left);
        const auto height=std::max(rectangle.top,bounds->top)-std::min(rectangle.bottom,bounds->bottom);
        if (!mTree.reshape(floater->id(),width+8,height+26,error) || !mTree.setShape(*panel,{2,2,width+2,height+2},error)) return false;
        mTree.setValue(find("floater_title",floater->id()),LLSD(file));
    }
    if (!floater) return false;
    mTree.setVisible(find("floater_close",floater->id()),false);
    const auto title=find("floater_title",floater->id());
    mTree.setValue(title,LLSD(mTree.value(title).asString()+" ["+skin.language+(slot ? " - Secondary]" : " - Primary]")));
    if (!floater->open(error)) return false;
    if (!mTree.setOverlapElements(mPreviewFields.at("overlap_panel"),{},error)) return false;
    if (mActiveFloater==mPreviewFloaters[slot].get()) mActiveFloater=nullptr;
    mPreviewFloaters[slot]=std::move(floater);
    mTree.setEnabled(mPreviewFields.at(slot ? "close_displayed_floater_2" : "close_displayed_floater"),true);
    mActiveFloater=mPreviewFloaters[slot].get();
    return true;
}

bool LLVKViewerUi::previewPointer(int x,int y,std::string& error)
{
    error.clear();
    if (!mUiPreview || !mUiPreview->visible() || !mPreviewOverlaps) return false;
    const auto active=activeFloater();
    const auto found=std::find_if(mPreviewFloaters.begin(),mPreviewFloaters.end(),[active](const auto& floater)
    { return floater && floater->visible() && floater->id()==active; });
    if (found==mPreviewFloaters.end()) return false;
    const auto inside=[&](auto id)
    {
        const auto rect=mTree.screenRect(id,error);
        return rect && x>=rect->left && x<rect->right && y>=rect->bottom && y<rect->top;
    };
    if (!inside(active)) return false;
    const auto select=[&](const auto& self,LLVKWidgetTree::Id id) -> LLVKWidgetTree::Id
    {
        const auto* node=mTree.get(id);
        if (node->panel || node->layoutStack)
            for (const auto child : node->children)
                if (mTree.get(child)->params.visible && inside(child)) return self(self,child);
        return id;
    };
    const auto selected=select(select,active);
    std::vector<LLVKWidgetTree::Id> elements{selected};
    const auto* node=mTree.get(selected);
    if (const auto* parent=mTree.get(node->parent))
        for (const auto sibling : parent->children)
        {
            const auto* candidate=mTree.get(sibling);
            if (sibling==selected || candidate->border || candidate->params.name.starts_with("floater_")) continue;
            const auto first=node->params.rect,second=candidate->params.rect;
            if (first.left<=second.right-2 && second.left<=first.right-2 && first.bottom<=second.top-2 && second.bottom<=first.top-2)
                elements.push_back(sibling);
        }
    return mTree.setOverlapElements(mPreviewFields.at("overlap_panel"),std::move(elements),error);
}

bool LLVKViewerUi::dumpFontTextures(std::string& error)
{
    error.clear();
    if (!mFontTextureDump) { error="Native font atlas diagnostic service is unavailable"; return false; }
    return mFontTextureDump(error);
}

bool LLVKViewerUi::showUiTest(const std::string& name,std::string& error)
{
    error.clear();
    const std::map<std::string,std::string> files{{"test_textbox","floater_test_textbox.xml"},
        {"test_text_editor","floater_test_text_editor.xml"},{"font_test","floater_font_test.xml"},
        {"test_widgets","floater_test_widgets.xml"}};
    const auto file=files.find(name);
    if (file==files.end()) { error="Unknown native UI test dialog"; return false; }
    auto& floater=mUiTests[name];
    if (!floater)
    {
        floater=LLVKFloater::createFile(mTree,*mDialogFactory,mRoot,file->second,error);
        if (!floater) return false;
    }
    mActiveFloater=floater.get();
    return floater->open(error);
}

bool LLVKViewerUi::showColorSettings(std::string& error)
{
    error.clear();
    if (!mColorSettings)
    {
        auto factory=*mDialogFactory;
        factory.bindAction("CommitSettings",[this](auto,const LLSD&) { colorSettingsAction(false); });
        factory.bindAction("ClickDefault",[this](auto,const LLSD&) { colorSettingsAction(true); });
        auto floater=LLVKFloater::createFile(mTree,factory,mRoot,"floater_settings_color.xml",error);
        if (!floater) return false;
        mColorSettingFields=preferenceFields(mTree,floater->id());
        LLVKControl::Callback filter,select;
        filter.function=[this](auto,const LLSD&) { refreshColorSettings(true,mDialogError); };
        select.function=[this](auto,const LLSD&) { refreshColorSettings(false,mDialogError); };
        mTree.setControlCommit(mColorSettingFields.at("filter_input"),filter);
        mTree.setLineEditorKeystroke(mColorSettingFields.at("filter_input"),std::move(filter));
        mTree.setControlCommit(mColorSettingFields.at("setting_list"),std::move(select));
        mTree.setScrollListCommitOnSelection(mColorSettingFields.at("setting_list"),true);
        mColorSettings=std::move(floater);
    }
    mActiveFloater=mColorSettings.get();
    return refreshColorSettings(true,error) && mColorSettings->open(error);
}

bool LLVKViewerUi::refreshColorSettings(bool rebuild,std::string& error)
{
    error.clear();
    if (!mColorSettings) return true;
    auto filter=mTree.value(mColorSettingFields.at("filter_input")).asString(); LLStringUtil::toLower(filter);
    const bool hide=mTree.setting("ColorSettingsHideDefault").value_or(LLSD(false)).asBoolean();
    const auto list=mColorSettingFields.at("setting_list");
    std::string selected;
    for (const auto& row : mTree.get(list)->scrollList->rows) if (row.selected) { selected=row.value.asString(); break; }
    if (rebuild || filter!=mColorSettingsFilter || hide!=mColorSettingsHideDefault)
    {
        mColorSettingsFilter=filter; mColorSettingsHideDefault=hide;
        std::vector<LLVKWidgetTree::ListRow> rows;
        std::optional<std::size_t> chosen;
        for (const auto& name : mColors->names())
        {
            auto lower=name; LLStringUtil::toLower(lower);
            if ((hide && mColors->isDefault(name)) || lower.find(filter)==lower.npos) continue;
            LLVKWidgetTree::ListRow row; row.value=name; row.cells={mColors->isDefault(name) ? "" : "*",name};
            if (!filter.empty() && (!chosen || (name==selected && lower.starts_with(filter)))) chosen=rows.size();
            rows.push_back(std::move(row));
        }
        if (rows.empty()) { LLVKWidgetTree::ListRow row; row.cells={"","No matching colors."}; row.enabled=false; rows.push_back(std::move(row)); }
        selected.clear();
        if (chosen) { rows[*chosen].selected=true; selected=rows[*chosen].value.asString(); }
        if (!mTree.setScrollListRows(list,std::move(rows),error)) return false;
    }
    const auto color=selected.empty() ? std::nullopt : mColors->find(selected);
    const bool visible=color.has_value() && !(hide && mColors->isDefault(selected));
    for (const auto name : {"color_name_txt","color_swatch","alpha_spinner","default_btn"}) mTree.setVisible(mColorSettingFields.at(name),visible);
    if (!visible) return true;
    mTree.setValue(mColorSettingFields.at("color_name_txt"),LLSD(selected));
    LLSD value=LLSD::emptyArray(); for (const auto component : color->get()) value.append(component);
    const auto swatch=mColorSettingFields.at("color_swatch");
    if (!llsd_equals(mTree.value(swatch),value)) mTree.setValue(swatch,value);
    const auto alpha=mColorSettingFields.at("alpha_spinner");
    return mTree.keyboardFocus()==mTree.get(alpha)->spinner->editor || mTree.setSpinnerValue(alpha,LLSD((*color)[3]),true,error);
}

void LLVKViewerUi::colorSettingsAction(bool reset)
{
    const auto list=mColorSettingFields.at("setting_list");
    std::string selected;
    for (const auto& row : mTree.get(list)->scrollList->rows) if (row.selected) { selected=row.value.asString(); break; }
    if (selected.empty() || !mColors->find(selected)) return;
    if (reset) mColors->resetToDefault(selected);
    else
    {
        const auto value=mTree.value(mColorSettingFields.at("color_swatch"));
        const auto alpha=mTree.value(mColorSettingFields.at("alpha_spinner")).asReal();
        if (!mColors->set(selected,{float(value[0].asReal()),float(value[1].asReal()),float(value[2].asReal()),float(alpha)}))
        { mDialogError="Invalid native color setting"; return; }
    }
    if (mTree.setting("ColorSettingsHideDefault").value_or(LLSD(false)).asBoolean() && mColors->isDefault(selected))
        refreshColorSettings(true,mDialogError);
    else
    {
        auto rows=mTree.get(list)->scrollList->rows;
        for (auto& row : rows) if (row.value.asString()==selected) row.cells[0]=mColors->isDefault(selected) ? "" : "*";
        if (mTree.setScrollListRows(list,std::move(rows),mDialogError)) refreshColorSettings(false,mDialogError);
    }
}

bool LLVKViewerUi::showDebugSettings(std::string& error)
{
    error.clear();
    if (mDebugControls.empty()) { error="Native debug control metadata is unavailable"; return false; }
    if (!mDebugSettings)
    {
        auto factory=*mDialogFactory;
        for (const auto action : {"SettingSelect","CommitSettings","ClickDefault","UpdateFilter","ClickCopy","ClickSanityIcon"})
            factory.bindAction(action,[this,action=std::string(action)](auto,const LLSD&) { debugSettingsAction(action); });
        auto floater=LLVKFloater::createFile(mTree,factory,mRoot,"floater_settings_debug.xml",error);
        if (!floater) return false;
        mDebugFields=preferenceFields(mTree,floater->id());
        LLVKControl::Callback filter;
        filter.function=[this](auto,const LLSD&) { refreshDebugSettings(true,mDialogError); };
        mTree.setLineEditorKeystroke(mDebugFields.at("search_settings_input"),std::move(filter));
        mDebugSettings=std::move(floater);
        if (!queueNotice("DebugSettingsWarning",{},{},error)) return false;
    }
    mActiveFloater=mDebugSettings.get();
    if (!refreshDebugSettings(true,error) || !mDebugSettings->open(error)) return false;
    return mTree.requestControlFocus(mDebugFields.at("search_settings_input"),true,error);
}

bool LLVKViewerUi::refreshDebugSettings(bool filter,std::string& error)
{
    error.clear();
    if (!mDebugSettings) return true;
    const auto search=mTree.value(mDebugFields.at("search_settings_input")).asString();
    const bool hide=mTree.setting("DebugSettingsHideDefault").value_or(LLSD(false)).asBoolean();
    const auto list=mDebugFields.at("settings_scroll_list");
    if (filter || search!=mDebugFilter || hide!=mDebugHideDefault)
    {
        mDebugFilter=search; mDebugHideDefault=hide;
        std::string needle=search; LLStringUtil::toLower(needle);
        std::vector<LLVKWidgetTree::ListRow> rows;
        for (auto& [name,control] : mDebugControls)
        {
            if (control->isHiddenFromSettingsEditor() || (hide && control->isDefault())) continue;
            auto haystack=name+" "+control->getComment(); LLStringUtil::toLower(haystack);
            if (!needle.empty() && haystack.find(needle)==haystack.npos) continue;
            LLVKWidgetTree::ListRow row;
            row.value=name; row.cells={control->isDefault() ? "" : "*",name};
            row.selected=!needle.empty() && rows.empty();
            rows.push_back(std::move(row));
        }
        if (!mTree.setScrollListRows(list,std::move(rows),error)) return false;
    }
    mDebugSelected=nullptr;
    for (const auto& row : mTree.get(list)->scrollList->rows)
        if (row.selected)
        {
            const auto control=mDebugControls.find(row.value.asString());
            if (control!=mDebugControls.end()) mDebugSelected=control->second;
            break;
        }
    const bool editable=mDebugSelected && !mDebugSelected->isHiddenFromSettingsEditor();
    const auto selectedType=mDebugSelected ? mDebugSelected->type() : TYPE_COUNT;
    for (const auto name : {"val_spinner_1","val_spinner_2","val_spinner_3","val_spinner_4","val_color_swatch","val_text","boolean_combo"})
    {
        const std::string field=name;
        bool visible=false;
        if (field=="val_text") visible=selectedType==TYPE_STRING;
        else if (field=="boolean_combo") visible=selectedType==TYPE_BOOLEAN;
        else if (field=="val_color_swatch") visible=selectedType==TYPE_COL3 || selectedType==TYPE_COL4;
        else
        {
            const int index=field.back()-'0';
            visible=selectedType==TYPE_RECT || selectedType==TYPE_QUAT ||
                ((selectedType==TYPE_VEC3 || selectedType==TYPE_VEC3D) && index<=3) ||
                ((selectedType==TYPE_U32 || selectedType==TYPE_S32 || selectedType==TYPE_F32) && index==1) ||
                (selectedType==TYPE_COL4 && index==4);
        }
        mTree.setVisible(mDebugFields.at(name),visible);
        mTree.setEnabled(mDebugFields.at(name),editable);
    }
    mTree.setVisible(mDebugFields.at("sanity_warning_btn"),mDebugSelected && !mDebugSelected->isSane());
    mTree.setEnabled(mDebugFields.at("copy_btn"),mDebugSelected.notNull());
    mTree.setEnabled(mDebugFields.at("default_btn"),editable);
    if (!mDebugSelected) return mTree.setTextEditorText(mDebugFields.at("comment_text"),"",error);
    auto control=mDebugSelected;
    const auto value=control->getValue();
    const auto type=control->type();
    auto comment=control->getName()+": "+control->getComment();
    if (type==TYPE_LLSD) { std::ostringstream text; LLSDSerialize::toPrettyNotation(value,text); comment=text.str(); }
    if (mTree.value(mDebugFields.at("comment_text")).asString()!=comment &&
        !mTree.setTextEditorText(mDebugFields.at("comment_text"),comment,error)) return false;
    mTree.setVisible(mDebugFields.at("sanity_warning_btn"),!control->isSane());
    const auto expose=[&](const char* name)
    { const auto id=mDebugFields.at(name); mTree.setVisible(id,true); mTree.setEnabled(id,editable); return id; };
    if (type==TYPE_BOOLEAN)
    {
        const auto id=expose("boolean_combo");
        return mTree.setRadioValue(id,LLSD(value.asBoolean() ? "true" : ""),error);
    }
    if (type==TYPE_STRING)
    {
        const auto id=expose("val_text");
        return mTree.keyboardFocus()==id || mTree.setValue(id,value);
    }
    if (type==TYPE_COL3 || type==TYPE_COL4)
    {
        const auto id=expose("val_color_swatch");
        if (!llsd_equals(mTree.value(id),value)) mTree.setValue(id,value);
    }
    const auto scalar=type==TYPE_U32 || type==TYPE_S32 || type==TYPE_F32;
    const int count=scalar ? 1 : (type==TYPE_VEC3 || type==TYPE_VEC3D) ? 3 : (type==TYPE_QUAT || type==TYPE_RECT || type==TYPE_COL4) ? 4 : 0;
    LLRect rectangle; if (type==TYPE_RECT) rectangle.setValue(value);
    const int coordinates[]{rectangle.mLeft,rectangle.mRight,rectangle.mBottom,rectangle.mTop};
    const char* components[]{"X","Y","Z","S"};
    const char* edges[]{"Left","Right","Bottom","Top"};
    for (int index=0; index<count; ++index)
    {
        if (type==TYPE_COL4 && index!=3) continue;
        const auto name="val_spinner_"+std::to_string(index+1);
        const auto id=expose(name.c_str());
        if (mTree.keyboardFocus()==mTree.get(id)->spinner->editor) continue;
        const bool integer=type==TYPE_U32 || type==TYPE_S32 || type==TYPE_RECT;
        const auto minimum=type==TYPE_U32 || type==TYPE_COL4 ? 0.f : integer ? float(INT32_MIN) : -FLT_MAX;
        const auto maximum=type==TYPE_COL4 ? 1.f : type==TYPE_U32 ? float(UINT32_MAX) : integer ? float(INT32_MAX) : FLT_MAX;
        if (!mTree.setSpinnerRange(id,minimum,maximum,error) ||
            !mTree.setSpinnerFormat(id,scalar ? "value" : type==TYPE_RECT ? edges[index] : type==TYPE_COL4 ? "Alpha" : components[index],
                integer ? 0 : type==TYPE_QUAT ? 4 : 3,integer ? 1.f : .1f,error) ||
            !mTree.setSpinnerValue(id,scalar ? value : type==TYPE_RECT ? LLSD(coordinates[index]) : value[index],true,error)) return false;
    }
    return true;
}

void LLVKViewerUi::debugSettingsAction(const std::string& action)
{
    if (action=="UpdateFilter" || action=="SettingSelect")
    { refreshDebugSettings(action=="UpdateFilter",mDialogError); return; }
    auto control=mDebugSelected;
    if (!control) return;
    if (action=="ClickCopy")
    {
        if (!mDialogClipboard) { mDialogError="Native clipboard unavailable"; return; }
        const auto wide=utf8str_to_wstring(control->getName());
        if (mDialogClipboard->write(std::u32string(wide.begin(),wide.end()),false,mDialogError))
            queueNotice("ControlNameCopiedToClipboard",{},{},mDialogError);
        return;
    }
    if (control->isHiddenFromSettingsEditor()) return;
    if (action=="ClickDefault") control->resetToDefault(true);
    else if (action=="CommitSettings")
    {
        const auto type=control->type();
        const auto spin=[&](int index) { return mTree.value(mDebugFields.at("val_spinner_"+std::to_string(index))); };
        LLSD value;
        if (type==TYPE_BOOLEAN) value=LLSD(!mTree.value(mDebugFields.at("boolean_combo")).asString().empty());
        else if (type==TYPE_STRING) value=mTree.value(mDebugFields.at("val_text"));
        else if (type==TYPE_F32) value=spin(1);
        else if (type==TYPE_S32 || type==TYPE_U32)
        {
            const auto number=spin(1).asReal();
            if (number<(type==TYPE_U32 ? 0. : double(INT32_MIN)) || number>(type==TYPE_U32 ? double(UINT32_MAX) : double(INT32_MAX)))
            { mDialogError="Debug integer value is outside its representable range"; return; }
            value=type==TYPE_U32 ? LLSD(static_cast<U32>(number)) : LLSD(static_cast<S32>(number));
        }
        else if (type==TYPE_RECT)
        {
            for (int index=1; index<=4; ++index)
                if (spin(index).asReal()<double(INT32_MIN) || spin(index).asReal()>double(INT32_MAX))
                { mDialogError="Debug rectangle value is outside its representable range"; return; }
            LLRect rect; rect.mLeft=spin(1).asInteger(); rect.mRight=spin(2).asInteger();
            rect.mBottom=spin(3).asInteger(); rect.mTop=spin(4).asInteger(); value=rect.getValue();
        }
        else if (type==TYPE_COL3 || type==TYPE_COL4)
        {
            const auto color=mTree.value(mDebugFields.at("val_color_swatch"));
            value=LLSD::emptyArray(); for (int index=0; index<3; ++index) value.append(color[index]);
            if (type==TYPE_COL4) value.append(spin(4));
        }
        else if (type==TYPE_VEC3 || type==TYPE_VEC3D || type==TYPE_QUAT)
        { value=LLSD::emptyArray(); for (int index=1; index<=(type==TYPE_QUAT ? 4 : 3); ++index) value.append(spin(index)); }
        else return;
        if (!(*control->getValidateSignal())(control.get(),value)) { mDialogError="Debug setting validation rejected the value"; return; }
        control->setValue(value,true);
    }
    mDebugChanges[control->getName()]=control->getValue();
    if (!control->isSane())
    {
        const auto key="SanityCheck"+LLControlGroup::sanityTypeEnumToString(control->getSanityType());
        auto message=mAboutStrings[key];
        const auto values=control->getSanityValues();
        LLSD arguments; arguments["CONTROL_NAME"]=control->getName();
        if (!values.empty()) arguments["VALUE_1"]=values[0];
        if (values.size()>1) arguments["VALUE_2"]=values[1];
        LLStringUtil::format(message,arguments);
        arguments["SANITY_MESSAGE"]=message; arguments["SANITY_COMMENT"]=control->getSanityComment(); arguments["CURRENT_VALUE"]=control->getValue();
        queueNotice("SanityCheck",arguments,[this,control](int option,const LLSD&) mutable
        {
            if (option==0 && !control->isHiddenFromSettingsEditor())
            { control->resetToDefault(true); mDebugChanges[control->getName()]=control->getValue(); }
        },mDialogError);
    }
    refreshDebugSettings(false,mDialogError);
}

bool LLVKViewerUi::showWindowSize(std::string& error)
{
    error.clear();
    if (!mWindowSizeService) { error="Native window resize service is unavailable"; return false; }
    if (!mWindowSize)
    {
        auto floater=LLVKFloater::createFile(mTree,*mDialogFactory,mRoot,"floater_window_size.xml",error);
        if (!floater) return false;
        const auto fields=preferenceFields(mTree,floater->id());
        const auto combo=fields.at("window_size_combo");
        LLVKControl::Callback set,cancel;
        set.function=[this,combo](auto,const LLSD&)
        {
            const auto* node=mTree.get(combo);
            const auto text=mTree.value(node->combo->editor ? node->combo->editor : combo).asString();
            const auto first=text.find_first_not_of("0123456789");
            const auto second=first==text.npos ? text.npos : text.find_first_of("0123456789",first);
            int width=0,height=0;
            bool parsed=false;
            if (first!=0 && first!=text.npos && second!=text.npos)
            {
                const auto horizontal=std::from_chars(text.data(),text.data()+first,width);
                const auto vertical=std::from_chars(text.data()+second,text.data()+text.size(),height);
                parsed=horizontal.ec==std::errc() && horizontal.ptr==text.data()+first &&
                    vertical.ec==std::errc() && vertical.ptr==text.data()+text.size();
            }
            if (parsed)
            {
                if (width<1 || height<1 || width>8192 || height>8192 || std::int64_t(width)*height>16*1024*1024)
                { mDialogError="Requested window size exceeds native browser surface limits"; return; }
                if (!mWindowSizeService(width,height,mDialogError)) return;
                mTree.updateSetting("WindowWidth",LLSD(width)); mTree.updateSetting("WindowHeight",LLSD(height));
            }
            mWindowSize->close(mDialogError);
        };
        cancel.function=[this](auto,const LLSD&) { mWindowSize->close(mDialogError); };
        mTree.setControlCommit(fields.at("set_btn"),std::move(set));
        mTree.setControlCommit(fields.at("cancel_btn"),std::move(cancel));
        if (!mTree.setPanelDefaultButton(floater->id(),fields.at("set_btn"),error)) return false;
        mWindowSize=std::move(floater);
    }
    const auto root=mTree.get(mRoot)->params.rect;
    const auto resolution=std::to_string(root.right-root.left)+" x "+std::to_string(root.top-root.bottom);
    const auto combo=find("window_size_combo",mWindowSize->id());
    auto items=mTree.get(combo)->combo->params->items;
    if (std::none_of(items.begin(),items.end(),[&](const auto& item) { return item.value.asString()==resolution; }))
        items.insert(items.begin(),{resolution,LLSD(resolution)});
    if (!mTree.replaceComboItems(combo,std::move(items),error) || !mTree.setComboValue(combo,LLSD(resolution),error)) return false;
    mActiveFloater=mWindowSize.get();
    return mWindowSize->open(error);
}

bool LLVKViewerUi::showWhitelist(std::string& error)
{
    error.clear();
    for (const auto& path : {mViewerExecutable,mPluginLauncher,mBrowserHelper,mVoiceExecutable,mProfileDirectory,mCacheDirectory})
        if (!path.is_absolute()) { error="Native whitelist paths are unavailable"; return false; }
    const auto text=[](const std::filesystem::path& path)
    { const auto bytes=path.u8string(); return std::string(bytes.begin(),bytes.end()); };
    if (!mWhitelist)
    {
        auto floater=LLVKFloater::createFile(mTree,*mDialogFactory,mRoot,"floater_whitelist.xml",error);
        if (!floater) return false;
        const auto folders=find("whitelist_folders_editor",floater->id());
        const auto executables=find("whitelist_exes_editor",floater->id());
        if (!folders || !executables || !mTree.get(folders)->textEditor || !mTree.get(executables)->textEditor)
        { error="Original whitelist editors are missing"; return false; }
        const auto folderText=text(mViewerExecutable.parent_path())+"\n"+text(mProfileDirectory)+"\n"+text(mCacheDirectory);
        std::string executableText;
        for (const auto& path : {mViewerExecutable,mVoiceExecutable,mPluginLauncher,mBrowserHelper})
        {
            if (!executableText.empty()) executableText+='\n';
            executableText+=text(path.filename())+"\n"+text(path);
        }
        if (!mTree.setTextEditorText(folders,folderText,error) || !mTree.setTextEditorText(executables,executableText,error) ||
            !mTree.startTextEditorDocument(folders,error) || !mTree.startTextEditorDocument(executables,error)) return false;
        mWhitelist=std::move(floater);
    }
    mActiveFloater=mWhitelist.get();
    return mWhitelist->open(error);
}

std::string LLVKViewerUi::helpUrl(const std::string& format,const std::string& topic,const LLSD& substitutions)
{
    LLSD values=substitutions;
    values["TOPIC"]=LLURI::escape(topic.empty() ? "this_is_fallbacktopic" : topic,
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._");
    auto url=format;
    LLStringUtil::format(url,values);
    std::string escaped;
    for (const char character : url)
    {
        if (character==' ') escaped+="%20";
        else if (character=='\\') escaped+="%5C";
        else escaped+=character;
    }
    return escaped;
}

bool LLVKViewerUi::helpUsesExternalBrowser(const std::string& url,unsigned behavior)
{
    if (behavior==0) return true;
    if (behavior==1)
    {
        LLUriParser parsed(url);
        parsed.normalize();
        parsed.extractParts();
        const auto host=parsed.host();
        const boost::regex domains("\\b(lindenlab.com|secondlife.com|secondlife.io|secondlifegrid.net|secondlife-status.statuspage.io)$",
            boost::regex::perl|boost::regex::icase);
        return !boost::regex_search(host,domains);
    }
    const boost::regex mail("^mailto:",boost::regex::perl|boost::regex::icase);
    return boost::regex_search(url,mail);
}

void LLVKViewerUi::setHelpServices(HelpContext context,HelpExternal external)
{
    mHelpContext=std::move(context); mHelpExternal=std::move(external);
}

bool LLVKViewerUi::showHelp(const std::string& requested,std::string& error)
{
    error.clear();
    auto topic=requested.empty() ? "this_is_fallbacktopic" : requested;
    if (topic=="f1_help")
    {
        topic=mTree.findHelpTopic(mTree.keyboardFocus()).value_or("this_is_fallbacktopic");
        if (topic=="this_is_fallbacktopic" && !sessionSnapshot().identity) topic="start";
    }
    if (!mHelpContext) { error="Native Help URL context is unavailable"; return false; }
    const auto values=mHelpContext(error);
    if (!values || !values->isMap())
    { if (error.empty()) error="Native Help URL context is not a map"; return false; }
    const auto format=mTree.setting("HelpURLFormat").value_or(LLSD()).asString();
    const auto url=helpUrl(format,topic,*values);
    if (url.empty() || url.find('\0')!=url.npos) { error="Native Help URL is invalid"; return false; }
    if (boost::regex_search(url,boost::regex("\\[[A-Z][A-Z0-9_]*\\]")))
    { error="Native Help URL requires unavailable context substitutions"; return false; }
    if (helpUsesExternalBrowser(url,mTree.setting("PreferredBrowserBehavior").value_or(LLSD(0)).asInteger()))
    {
        if (mTree.setting("DisableExternalBrowser").value_or(LLSD(false)).asBoolean()) return true;
        if (!mHelpExternal) { error="Native external Help browser is unavailable"; return false; }
        LLSD arguments; arguments["UNTRUSTED_URL"]=url;
        return queueNotice("WebLaunchExternalTarget",arguments,[this,url](int option,const LLSD&)
        { if (option==0 && mHelpExternal) mHelpExternal(url,mDialogError); },error);
    }
    if (!mGuidebookOpen || !mGuidebookClose || !mBrowserCommand)
    { error="Native internal Help browser is unavailable"; return false; }
    mHelpErrorPageUsed=false;
    if (mHelp && mHelp->visible())
    {
        if (!mBrowserCommand(mHelpBrowser,"Navigate",url,error)) return false;
        mActiveFloater=mHelp.get();
        return mHelp->open(error);
    }
    if (mActiveFloater==mHelp.get()) mActiveFloater=nullptr;
    mHelp.reset(); mHelpFields.clear(); mHelpBrowser=0; mHelpRetiring=false;
    auto floater=LLVKFloater::createFile(mTree,*mDialogFactory,mRoot,"floater_help_browser.xml",error);
    if (!floater) return false;
    const auto fields=preferenceFields(mTree,floater->id());
    if (!fields.contains("browser") || !fields.contains("status_text"))
    { error="Native Help declaration is missing browser or status text"; return false; }
    const auto browser=fields.at("browser");
    mTree.setVisible(browser,false);
    auto placement=mTree.get(mRoot)->params.rect;
    placement.top-=19;
    if (!mTree.prepareLayoutStacks(floater->id(),0,error) || !floater->open(error,placement)) return false;
    if (!mGuidebookOpen(browser,url,error))
    {
        mGuidebookClose(browser);
        return false;
    }
    floater->onCloseFocus([this](std::string& problem)
    {
        return !sessionSnapshot().identity ? focusLoginFields(problem) : mTree.setKeyboardFocus(0,false,false,problem);
    });
    floater->onClose([this,browser]
    {
        mTree.setVisible(browser,false);
        if (mGuidebookClose) mGuidebookClose(browser);
        if (!mApplicationQuitting)
        {
            mTree.updateSetting("HelpFloaterOpen",LLSD(false));
            mGuidebookChanges["HelpFloaterOpen"]=false;
        }
        mHelpRetiring=true;
    });
    mHelpFields=fields; mHelpBrowser=browser; mHelpCurrentUrl.clear();
    mActiveFloater=floater.get(); mHelp=std::move(floater);
    mTree.updateSetting("HelpFloaterOpen",LLSD(true));
    mGuidebookChanges["HelpFloaterOpen"]=true;
    return true;
}

bool LLVKViewerUi::showMediaBrowser(const std::string& requested,std::string& error,const std::string& target)
{
    error.clear();
    auto url=requested; LLStringUtil::trim(url);
    if (!mGuidebookOpen || !mBrowserCommand) { error="Native media browser service is unavailable"; return false; }
    if (url.empty() || url.find('\0')!=url.npos) { error="Native media browser URL is invalid"; return false; }
    if (!target.empty() && target!="_blank")
        for (auto& [browser,dialog] : mWebDialogs)
            if (dialog.target==target && dialog.floater->visible())
            {
                if (!mBrowserCommand(browser,"Navigate",url,error)) return false;
                mActiveFloater=dialog.floater.get();
                return dialog.floater->open(error);
            }
    std::erase_if(mWebDialogs,[](const auto& entry) { return !entry.second.floater->visible(); });
    const auto limit=mTree.setting("WebContentWindowLimit").value_or(LLSD(0)).asInteger();
    if (limit>0 && mWebDialogs.size()>=static_cast<std::size_t>(limit))
    {
        auto oldest=mWebDialogs.begin();
        if (!oldest->second.floater->close(error)) return false;
        mWebDialogs.erase(oldest);
    }
    const auto documents=mSkin->read("xui","floater_web_content.xml",LLVKSkinFiles::Policy::Current,error);
    if (!documents) return false;
    std::vector<std::string_view> views; for (const auto& document : *documents) views.push_back(document);
    const auto merged=LLVKXmlLayers::merge(views,error);
    if (!merged) return false;
    std::string xml;
    try
    {
        boost::property_tree::ptree document; std::istringstream input(*merged);
        boost::property_tree::read_xml(input,document);
        for (auto& [tag,panel] : document.get_child("floater.layout_stack"))
            if (tag=="layout_panel" && panel.get<std::string>("<xmlattr>.name","")=="external_controls")
                panel.put("<xmlattr>.height",600);
        std::ostringstream output; boost::property_tree::write_xml(output,document); xml=output.str();
    }
    catch (const std::exception& exception) { error=exception.what(); return false; }
    auto owner=std::make_shared<LLVKWidgetTree::Id>(0);
    auto factory=*mDialogFactory;
    for (const auto action : {"Back","Forward","Reload","Stop","EnterAddress","PopExternal","TestURL"})
        factory.bindAction(std::string("WebContent.")+action,[this,owner,action=std::string(action)](auto,const LLSD& parameter)
        { webBrowserAction(*owner,action,parameter.asString()); });
    auto floater=LLVKFloater::createXml(mTree,factory,mRoot,xml,error);
    if (!floater) return false;
    auto fields=preferenceFields(mTree,floater->id());
    const auto browser=fields.at("webbrowser"); *owner=browser;
    mTree.setVisible(fields.at("plugin_fail_text"),true);
    mTree.setVisible(browser,false);
    mTree.setVisible(fields.at("reload"),false);
    mTree.setVisible(fields.at("statusbarprogress"),false);
    if (!mTree.prepareLayoutStacks(floater->id(),0,error) || !floater->open(error)) return false;
    if (!mGuidebookOpen(browser,url,error)) return false;
    floater->onClose([this,browser] { if (mGuidebookClose) mGuidebookClose(browser); });
    mActiveFloater=floater.get();
    mWebDialogs.emplace(browser,WebDialog{std::move(floater),std::move(fields),{},target});
    webBrowserEvent(browser,"Address",url,false,false);
    return true;
}

void LLVKViewerUi::webBrowserAction(LLVKWidgetTree::Id browser,const std::string& action,const std::string& parameter)
{
    const auto found=mWebDialogs.find(browser);
    if (found==mWebDialogs.end() || !found->second.floater->visible()) return;
    auto& dialog=found->second;
    if (action=="PopExternal") { if (mOpenUrl) mOpenUrl(dialog.url); return; }
    std::string url=parameter;
    auto command=action;
    if (action=="EnterAddress")
    {
        const auto* combo=mTree.get(dialog.fields.at("address"));
        url=mTree.value(combo->combo->editor ? combo->combo->editor : dialog.fields.at("address")).asString();
        LLStringUtil::trim(url); if (url.empty()) return;
        command="Navigate";
    }
    else if (action=="TestURL") command="Navigate";
    if (!mBrowserCommand || !mBrowserCommand(browser,command,url,mDialogError)) return;
    if (action=="Stop")
    { mTree.setVisible(dialog.fields.at("reload"),true); mTree.setVisible(dialog.fields.at("stop"),false); }
}

void LLVKViewerUi::webBrowserEvent(LLVKWidgetTree::Id browser,const std::string& kind,const std::string& text,bool back,bool forward)
{
    if (browser==mHelpBrowser && mHelp && mHelp->visible())
    {
        if (kind=="Address")
        {
            mHelpCurrentUrl=text;
            if (!text.empty() && text!="about:blank")
            {
                const LLURI address(text);
                const auto simplified=address.scheme()+"://"+address.authority()+address.path();
                std::erase(mHelpHistory,simplified);
                mHelpHistory.insert(mHelpHistory.begin(),simplified);
                if (mHelpHistory.size()>10) mHelpHistory.resize(10);
            }
        }
        else if (kind=="LoadStart" || kind=="LoadEnd")
        {
            const auto* panel=mTree.get(mHelp->id());
            const auto& strings=panel->panel->params.strings;
            const auto found=strings.find(kind=="LoadStart" ? "loading_text" : "done_text");
            mTree.setValue(mHelpFields.at("status_text"),LLSD(found==strings.end() ? "" : found->second));
            mTree.setVisible(browser,true);
        }
        else if (kind=="LoadError")
        {
            const auto fallback=mTree.setting("GenericErrorPageURL").value_or(LLSD()).asString();
            if (!fallback.empty() && !mHelpErrorPageUsed)
            {
                mHelpErrorPageUsed=true;
                mBrowserCommand(browser,"Navigate",fallback,mDialogError);
            }
        }
        else if (kind=="Closed") mHelp->close(mDialogError);
        return;
    }
    const auto found=mWebDialogs.find(browser);
    if (found==mWebDialogs.end() || !found->second.floater->visible()) return;
    auto& dialog=found->second; const auto& fields=dialog.fields;
    mTree.setEnabled(fields.at("back"),back); mTree.setEnabled(fields.at("forward"),forward);
    if (kind=="Address" && !text.empty())
    {
        dialog.url=text;
        auto items=mTree.get(fields.at("address"))->combo->params->items;
        std::erase_if(items,[&](const auto& item) { return item.value.asString()==text; });
        items.insert(items.begin(),{text,LLSD(text)}); if (items.size()>100) items.resize(100);
        mTree.replaceComboItems(fields.at("address"),std::move(items),mDialogError);
        mTree.setComboValue(fields.at("address"),LLSD(text),mDialogError);
        mTree.setVisible(fields.at("media_secure_lock_flag"),LLURI(text).scheme()=="https");
        mTree.setValue(fields.at("statusbartext"),LLSD(text));
    }
    else if (kind=="Title") mTree.setValue(find("floater_title",dialog.floater->id()),LLSD(text.empty() ? dialog.url : text));
    else if (kind=="Status") mTree.setValue(fields.at("statusbartext"),LLSD(text));
    else if (kind=="LoadStart" || kind=="LoadEnd")
    {
        const bool loading=kind=="LoadStart";
        mTree.setVisible(browser,true); mTree.setVisible(fields.at("plugin_fail_text"),false);
        mTree.setVisible(fields.at("reload"),!loading); mTree.setVisible(fields.at("stop"),loading);
        mTree.setVisible(fields.at("statusbarprogress"),loading);
        mTree.setValue(fields.at("statusbarprogress"),LLSD(loading ? 0. : 100.));
        if (!loading) mTree.setValue(fields.at("statusbartext"),LLSD(""));
    }
    else if (kind=="Closed") dialog.floater->close(mDialogError);
}

void LLVKViewerUi::setGuidebookService(GuidebookOpen open,std::function<void(LLVKWidgetTree::Id)> close)
{
    mGuidebookOpen=std::move(open);
    mGuidebookClose=std::move(close);
    mMenu->bind("Help.ToggleHowTo",[this](const auto&,const auto&) { toggleGuidebook(mDialogError); });
}

LLVKWidgetTree::Id LLVKViewerUi::guidebook() const
{ return mGuidebook && mGuidebook->visible() ? mGuidebook->id() : 0; }

void LLVKViewerUi::recordGuidebookState(bool visible)
{
    if (!mGuidebook) return;
    const auto rect=mTree.get(mGuidebook->id())->params.rect;
    const auto root=mTree.get(mRoot)->params.rect;
    const auto relative=[](int position,int extent,int available)
    {
        if (position<0) return -.5+double(position)/(2.*std::max(1,extent-16));
        if (position+extent>available) return .5+double(position-(available-extent))/(2.*std::max(1,extent-16));
        return available==extent ? 0. : double(position)/(available-extent)-.5;
    };
    mGuidebookChanges={{"floater_vis_guidebook",visible},
        {"floater_pos_guidebook_x",std::clamp(relative(rect.left,rect.right-rect.left,root.right-root.left),-1.,1.)},
        {"floater_pos_guidebook_y",std::clamp(relative(rect.bottom,rect.top-rect.bottom,root.top-root.bottom),-1.,1.)}};
    for (const auto& [name,value] : mGuidebookChanges) mTree.updateSetting(name,value);
}

bool LLVKViewerUi::toggleGuidebook(std::string& error)
{
    error.clear();
    if (guidebook())
        return activeFloater()==guidebook() ? mGuidebook->close(error) : mGuidebook->open(error);
    if (!mGuidebookOpen) { error="Native Guidebook browser service is unavailable"; return false; }
    const auto url=mTree.setting("GuidebookURL").value_or(LLSD()).asString();
    const LLURI address(url);
    if ((address.scheme()!="http" && address.scheme()!="https") || address.hostName().empty() || url.find('\0')!=url.npos)
    { error="Native Guidebook requires an HTTP or HTTPS URL"; return false; }
    if (mActiveFloater==mGuidebook.get()) mActiveFloater=nullptr;
    mGuidebook.reset();
    std::vector<std::string> layers;
    for (const auto file : {"floater_web_content.xml","floater_how_to.xml"})
    {
        const auto documents=mSkin->read("xui",file,LLVKSkinFiles::Policy::Current,error);
        if (!documents) return false;
        std::vector<std::string_view> views;
        for (const auto& document : *documents) views.push_back(document);
        const auto merged=LLVKXmlLayers::merge(views,error);
        if (!merged) return false;
        layers.push_back(*merged);
    }
    std::string declaration;
    try
    {
        boost::property_tree::ptree document,override;
        std::istringstream input(layers.front()),guidebookInput(layers.back());
        boost::property_tree::read_xml(input,document);
        boost::property_tree::read_xml(guidebookInput,override);
        auto& root=document.get_child("floater");
        for (const auto& [name,value] : override.get_child("floater.<xmlattr>")) root.put("<xmlattr>."+name,value.data());
        for (const auto& [attribute,setting] : {std::pair{"rel_x","floater_pos_guidebook_x"},
            std::pair{"rel_y","floater_pos_guidebook_y"}})
        {
            const auto value=mTree.setting(setting).value_or(LLSD(10.)).asReal();
            if (std::isfinite(value) && value>=-1. && value<=1.) root.put(std::string("<xmlattr>.")+attribute,value);
        }
        root.get_child("<xmlattr>").erase("filename");
        auto& stack=root.get_child("layout_stack");
        stack.put("<xmlattr>.width",300);
        stack.put("<xmlattr>.bottom",525);
        for (auto& [tag,panel] : stack)
        {
            if (tag!="layout_panel") continue;
            const auto name=panel.get<std::string>("<xmlattr>.name","");
            if (name=="external_controls")
            {
                panel.put("<xmlattr>.height",505);
                panel.put("<xmlattr>.width",300);
                panel.get_child("<xmlattr>").erase("top_delta");
                panel.get_child("<xmlattr>").erase("left_delta");
                panel.put("<xmlattr>.top",0);
                panel.put("<xmlattr>.left",0);
                panel.put("web_browser.<xmlattr>.width",300);
                continue;
            }
            if (name!="nav_controls" && name!="debug_controls" && name!="status_bar") continue;
            auto attributes=panel.get_child("<xmlattr>");
            attributes.put("visible",false);
            panel.clear(); panel.add_child("<xmlattr>",attributes);
        }
        std::ostringstream output;
        boost::property_tree::write_xml(output,document);
        declaration=output.str();
    }
    catch (const std::exception& exception) { error=exception.what(); return false; }
    auto floater=LLVKFloater::createXml(mTree,*mDialogFactory,mRoot,declaration,error);
    if (!floater) return false;
    const auto browser=find("webbrowser",floater->id());
    const auto stack=find("stack1",floater->id());
    if (!browser || !stack) { error="Original Guidebook browser hierarchy is missing"; return false; }
    for (const auto name : {"nav_controls","debug_controls","status_bar","plugin_fail_text"})
        mTree.setVisible(find(name,floater->id()),false);
    if (!mTree.reshape(stack,300,505,error) || !mTree.prepareLayoutStacks(floater->id(),0,error)) return false;
    if (!mGuidebookOpen(browser,url,error)) return false;
    floater->onClose([this,browser]
    {
        recordGuidebookState(mApplicationQuitting);
        if (mGuidebookClose) mGuidebookClose(browser);
    });
    mGuidebook=std::move(floater);
    mActiveFloater=mGuidebook.get();
    if (!mGuidebook->open(error)) return false;
    recordGuidebookState(true);
    return true;
}

void LLVKViewerUi::setAboutInfo(std::string info)
{ mAboutInfo=std::move(info); if (mAboutBody) updateAboutText(mDialogError); }
bool LLVKViewerUi::reportProblem(std::string& error)
{
    error.clear();
    if (!mOpenUrl) { error="Native report browser service is unavailable"; return false; }
    if (!mDiagnosticInfo.isMap()) { error="Native report system information is unavailable"; return false; }
    auto url=mTree.setting("ReportBugURL").value_or(LLSD()).asString();
    if (url.empty()) { error="Native report URL is unavailable"; return false; }
    std::string environment;
    for (const auto& [label,key] : {std::pair{"Viewer","VIEWER_VERSION_TEXT"},
        {"Channel","CHANNEL"},{"Build date","BUILD_DATE"},{"Build time","BUILD_TIME"},
        {"Build type","BUILD_TYPE"},{"Address size","ADDRESS_SIZE"},{"SIMD","SIMD"},
        {"Compiler","COMPILER"},{"Compiler version","COMPILER_VERSION"},
        {"Location","REGION"},{"Host","HOSTNAME"},{"Server version","SERVER_VERSION"},
        {"CPU","CPU"},{"Memory MB","MEMORY_MB"},{"Used RAM MB","USED_RAM"},{"OS","OS_VERSION"},
        {"Graphics vendor","GRAPHICS_CARD_VENDOR"},{"Graphics card","GRAPHICS_CARD"},
        {"VRAM MB","GRAPHICS_CARD_MEMORY"},{"Detected VRAM MB","GRAPHICS_CARD_MEMORY_DETECTED"},
        {"VRAM budget","VRAM_BUDGET"},{"Graphics driver","GRAPHICS_DRIVER_VERSION"},
        {"Rendering API","RENDERING_API"},{"Rendering API version","RENDERING_API_VERSION"},
        {"libcurl","LIBCURL_VERSION"},{"J2C decoder","J2C_VERSION"},{"Audio driver","AUDIO_DRIVER_VERSION"},
        {"CEF","LIBCEF_VERSION"},{"LibVLC","LIBVLC_VERSION"},{"Voice","VOICE_VERSION"},
        {"Packets lost","PACKETS_LOST"},{"RLVa","RLV_VERSION"},{"Mode","MODE"},{"Skin","SKIN"},{"Theme","THEME"},
        {"Window width","WINDOW_WIDTH"},{"Window height","WINDOW_HEIGHT"},{"Font","FONT"},
        {"Font size adjustment","FONT_SIZE"},{"Font DPI","FONT_SCREEN_DPI"},{"UI scale","UI_SCALE_FACTOR"},
        {"Draw distance","DRAW_DISTANCE"},{"Bandwidth","BANDWIDTH"},{"LOD","LOD"},
        {"Render quality","RENDERQUALITY"},{"Disk cache","DISK_CACHE_INFO"}})
    {
        environment+=std::string(label)+": "+(mDiagnosticInfo.has(key) ? mDiagnosticInfo[key].asString() : "Unavailable")+"\n";
    }
    LLStringUtil::format_map_t substitutions;
    substitutions["[ENVIRONMENT]"]=LLURI::escape(environment);
    substitutions["[LOCATION]"]=LLURI::escape(mDiagnosticInfo["LOCATION_URL"].asString());
    LLStringUtil::format(url,substitutions);
    const LLURI parsed(url);
    if ((parsed.scheme()!="https" && parsed.scheme()!="http") || parsed.hostName().empty() || url.find('\0')!=std::string::npos)
    { error="Native report URL must be HTTP or HTTPS"; return false; }
    mOpenUrl(url);
    return true;
}
bool LLVKViewerUi::setAboutInfo(const LLSD& info,std::string& error)
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
    mDiagnosticInfo=info;
    for (const auto& [name,value] : arguments) mDiagnosticInfo[name]=value;
    std::string version="Vulkanstorm";
    for (LLSD::Integer index=0; index<info["VIEWER_VERSION"].size(); ++index)
        version+=(index ? "." : " ")+info["VIEWER_VERSION"][index].asString();
    mDiagnosticInfo["VIEWER_VERSION_TEXT"]=version;
    mAboutInfo=std::move(text);
    return !mAboutBody || updateAboutText(error);
}
bool LLVKViewerUi::updateAboutText(std::string& error)
{
    return mTree.setTextEditorText(mAboutBody,mAboutInfo,error);
}
bool LLVKViewerUi::showColorPicker(LLVKWidgetTree::Id swatch,bool takeFocus,std::string& error)
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
    const bool opening=!found->second->visible();
    if (!found->second->open(error)) return false;
    if (opening)
    {
        auto parent=source->parent;
        while (mTree.get(parent) && !mTree.get(parent)->floater) parent=mTree.get(parent)->parent;
        if (const auto* owner=mTree.get(parent); owner && owner->floater)
        {
            found->second->setSnapTarget(parent);
            auto base=owner->params.rect;
            const auto expanded=LLVKWidgetTree::Rect{base.left-10,base.bottom-10,base.right+10,base.top+10};
            for (const auto& [otherSwatch,picker] : mColorPickers)
            {
                if (otherSwatch==swatch || !picker->visible()) continue;
                auto ancestor=mTree.get(otherSwatch) ? mTree.get(otherSwatch)->parent : 0;
                while (mTree.get(ancestor) && !mTree.get(ancestor)->floater) ancestor=mTree.get(ancestor)->parent;
                const auto rectangle=mTree.get(picker->id())->params.rect;
                if (ancestor==parent && rectangle.left<expanded.right && rectangle.right>expanded.left &&
                    rectangle.bottom<expanded.top && rectangle.top>expanded.bottom)
                {
                    base.left=std::min(base.left,rectangle.left); base.right=std::max(base.right,rectangle.right);
                    base.bottom=std::min(base.bottom,rectangle.bottom); base.top=std::max(base.top,rectangle.top);
                }
            }
            auto rectangle=mTree.get(found->second->id())->params.rect;
            const auto width=rectangle.right-rectangle.left,height=rectangle.top-rectangle.bottom;
            const auto root=mTree.get(mRoot)->params.rect;
            auto leftMargin=std::max(0,base.left),rightMargin=std::max(0,root.right-root.left-base.right);
            auto bottomMargin=std::max(0,base.bottom),topMargin=std::max(0,root.top-root.bottom-mNoticeMenuHeight-base.top);
            for (unsigned attempt=0; attempt<5; ++attempt)
            {
                int left=0,bottom=0;
                if (rightMargin>width) { left=base.right; bottom=base.top-height; }
                else if (leftMargin>width) { left=base.left-width; bottom=base.top-height; }
                else if (bottomMargin>height) { left=base.left; bottom=base.bottom-height; }
                else if (topMargin>height) { left=base.left; bottom=base.top; }
                else { leftMargin+=20; rightMargin+=20; bottomMargin+=20; topMargin+=20; continue; }
                if (!mTree.setShape(found->second->id(),{left,bottom,left+width,bottom+height},error)) return false;
                break;
            }
        }
    }
    mActiveFloater=found->second.get();
    const auto& fields=mTree.get(found->second->id())->colorPicker->fields;
    if (takeFocus || opening) return mTree.requestControlFocus(fields.at("select_btn"),true,error);
    return true;
}

std::vector<LLVKFloater*> LLVKViewerUi::floaters() const
{
    std::vector<LLVKFloater*> result{mPreferences.get(),mAbout.get(),mAutoReplace.get(),mSpellCheck.get(),mSpellImport.get(),mTranslation.get(),mKeyCapture.get(),mJoystick.get(),mProxy.get(),mMediaLists.get(),mBlockObjectName.get(),mBlockList.get()};
    result.push_back(mDefaultPermissions.get());
    result.push_back(mWhitelist.get());
    result.push_back(mUiPreview.get());
    for (const auto& preview : mPreviewFloaters) result.push_back(preview.get());
    result.push_back(mWindowSize.get());
    result.push_back(mDebugSettings.get());
    result.push_back(mColorSettings.get());
    for (const auto& [name,dialog] : mUiTests) result.push_back(dialog.get());
    for (const auto& [item,dialog] : mTornMenus) result.push_back(dialog.floater.get());
    result.push_back(mGuidebook.get());
    result.push_back(mHelp.get());
    for (const auto& [browser,dialog] : mWebDialogs) result.push_back(dialog.floater.get());
    result.push_back(mBeamColor.get());
    result.push_back(mBeamShape.get());
    for (const auto& [action,dialog] : mGraphicPresetDialogs) result.push_back(dialog.get());
    for (const auto& [swatch,picker] : mColorPickers) result.push_back(picker.get());
    return result;
}

LLVKWidgetTree::Id LLVKViewerUi::activeFloater() const
{
    for (const auto child : mTree.get(mRoot)->children)
        for (const auto* floater : floaters())
            if (floater && floater->id() == child && floater->visible()) return child;
    return 0;
}
bool LLVKViewerUi::pointOverFloater(int x,int y) const
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
bool LLVKViewerUi::closeFloater(std::string& error)
{
    const auto active=activeFloater();
    for (auto* floater : floaters())
        if (floater && floater->id()==active) return floater->close(error);
    return false;
}
bool LLVKViewerUi::canCloseMenuWindow() const
{
    if (mNoticePanel || keyCaptureDialog()) return false;
    for (const auto child : mTree.get(mRoot)->children)
        for (const auto* floater : floaters())
            if (floater && floater->id()==child && floater->visible() && mTree.get(child)->floater->canClose) return true;
    return false;
}
bool LLVKViewerUi::closeMenuWindow(std::string& error)
{
    error.clear();
    if (!canCloseMenuWindow()) return false;
    for (const auto child : mTree.get(mRoot)->children)
        for (auto* floater : floaters())
            if (floater && floater->id()==child && floater->visible() && mTree.get(child)->floater->canClose)
            { mMenu->dismiss(); return floater->close(error); }
    return false;
}
LLVKViewerUi::ShutdownStatus LLVKViewerUi::prepareShutdown(std::string& error,const std::map<std::string,LLSD>& applicationSettings)
{
    error.clear();
    if (mShutdownPrepared) return ShutdownStatus::Ready;
    if (mNoticePanel || !mNotices.empty()) return ShutdownStatus::Pending;
    if (!mApplicationQuitting)
    {
        mShutdownSnapshot=mPreferenceSnapshot.settings;
        mShutdownWarnings=mWarningSnapshot;
        if (const auto last=mTree.setting("LastPrefTab")) mShutdownSnapshot["LastPrefTab"]=*last;
        mApplicationQuitting=true;
        ++mPreferenceGeneration;
    }
    mMenu->dismiss();
    auto dialogs=floaters();
    for (auto iterator=dialogs.rbegin(); iterator!=dialogs.rend(); ++iterator)
    {
        auto* dialog=*iterator;
        if (!dialog || !dialog->visible()) continue;
        if (!dialog->close(error)) return ShutdownStatus::Failed;
        if (!mDialogError.empty()) { error=std::exchange(mDialogError,{}); return ShutdownStatus::Failed; }
    }
    if (mVoiceDeviceServices.tune && !mVoiceDeviceServices.tune(false,1.f,error)) return ShutdownStatus::Failed;
    if (mSaveSettingsOnExit)
    {
        std::map<std::string,LLSD> changed=applicationSettings,account;
        changed.insert(mGuidebookChanges.begin(),mGuidebookChanges.end());
        for (const auto& [name,previous] : mDebugChanges)
        {
            auto control=mDebugControls.at(name);
            if (!control->isPersisted()) continue;
            if (mDebugAccountNames.contains(name)) { if (mAccountSettingsLoaded) account[name]=control->getValue(); }
            else changed[name]=control->getValue();
        }
        for (const auto& [name,previous] : mShutdownSnapshot)
        {
            if (name=="RenderBackendPending") continue;
            const auto current=mTree.setting(name);
            if (!current || llsd_equals(*current,previous)) continue;
            if (mAccountSettingNames.contains(name))
            {
                if (mAccountSettingsLoaded) account[name]=*current;
            }
            else changed[name]=*current;
        }
        if (!changed.empty() && (!mSavePreferences || !mSavePreferences(changed,error)))
        { if (error.empty()) error="Native exit preference persistence is unavailable"; return ShutdownStatus::Failed; }
        if (!account.empty() && (!mSaveAccountPreferences || !mSaveAccountPreferences(account,error)))
        { if (error.empty()) error="Native account exit persistence is unavailable"; return ShutdownStatus::Failed; }
        std::map<std::string,LLSD> warnings;
        for (const auto& [name,previous] : mShutdownWarnings)
            if (const auto control=mWarningSettings->getControl(name); control && !llsd_equals(control->getValue(),previous))
                warnings[name]=control->getValue();
        if (!warnings.empty() && (!mSaveWarningPreferences || !mSaveWarningPreferences(warnings,error)))
        { if (error.empty()) error="Native warning exit persistence is unavailable"; return ShutdownStatus::Failed; }
        for (const auto& [name,value] : warnings) mWarningSettings->getControl(name)->setValue(value,true);
        if (!mUserColorsFile.empty() && !mColors->saveUserFile(mUserColorsFile,error)) return ShutdownStatus::Failed;
    }
    mShutdownPrepared=true;
    return ShutdownStatus::Ready;
}
bool LLVKViewerUi::floaterPointer(const LLVKWidgetTree::PointerEvent& event,std::string& error)
{
    if (mTree.mouseCapture())
    {
        for (auto* floater : floaters())
            if (floater && floater->id()==mTree.mouseCapture())
            {
                const auto before=mTree.get(floater->id())->params.rect;
                const auto handled=floater->pointer(event,error);
                const auto after=mTree.get(floater->id())->params.rect;
                const auto deltaX=after.left-before.left,deltaY=after.bottom-before.bottom;
                if (deltaX || deltaY)
                {
                    floater->setSnapTarget(0);
                    for (auto& [swatch,picker] : mColorPickers)
                    {
                        if (!picker->visible() || picker->snapTarget()!=floater->id()) continue;
                        auto rectangle=mTree.get(picker->id())->params.rect;
                        rectangle.left+=deltaX; rectangle.right+=deltaX;
                        rectangle.bottom+=deltaY; rectangle.top+=deltaY;
                        if (!mTree.setShape(picker->id(),rectangle,error)) return false;
                    }
                }
                if (before.left!=after.left && before.bottom!=after.bottom)
                    for (auto& [item,dialog] : mTornMenus)
                        if (dialog.floater.get()==floater) dialog.view->dismiss();
                return handled;
            }
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
bool LLVKViewerUi::floaterWheel(int x,int y,int clicks,std::string& error)
{
    const auto active=activeFloater();
    if (!active) return false;
    const auto rect=mTree.screenRect(active,error);
    if (!rect || x<rect->left || x>=rect->right || y<rect->bottom || y>=rect->top) return false;
    return mTree.routeWheel(active,x,y,clicks,false,error);
}