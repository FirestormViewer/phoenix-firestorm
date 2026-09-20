#include "llvkviewerui.h"
#include "llerrorcontrol.h"
#include "llvkxmllayers.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <fstream>
#include <sstream>

namespace
{
    using ShellXml=boost::property_tree::ptree;

    std::optional<ShellXml> shellDocument(const LLVKSkinFiles& skin,const std::string& directory,
        const std::string& filename,LLVKSkinFiles::Policy policy,std::string& error)
    {
        const auto files=skin.read(directory,filename,policy,error);
        if (!files) return {};
        std::vector<std::string_view> layers;
        for (const auto& file : *files) layers.push_back(file);
        const auto merged=LLVKXmlLayers::merge(layers,error);
        if (!merged) return {};
        ShellXml result;
        std::istringstream input(*merged);
        boost::property_tree::read_xml(input,result,boost::property_tree::xml_parser::no_comments);
        return result;
    }

    const ShellXml* shellNamed(const ShellXml& node,std::string_view name)
    {
        if (node.get<std::string>("<xmlattr>.name","")==name) return &node;
        for (const auto& [kind,child] : node)
            if (kind!="<xmlattr>") if (const auto found=shellNamed(child,name)) return found;
        return nullptr;
    }

    ShellXml shellNavigation(const ShellXml& node)
    {
        ShellXml result;
        result.data()=node.data();
        for (const auto& [sourceKind,source] : node)
        {
            auto kind=sourceKind;
            auto control=source;
            if (kind=="<xmlattr>") { result.add_child(kind,control); continue; }
            // Navigation services are unavailable: retain the disabled visible
            // control, but do not construct an inactive world-navigation popup.
            if (kind.find("callback")!=std::string::npos || kind=="chevron_button" || kind=="combo_list")
                continue;
            if (kind=="pull_button")
            {
                kind="button";
                control.get_child("<xmlattr>").erase("direction");
            }
            if (kind=="location_input" || kind=="search_combo_box")
            {
                kind="combo_box";
                control.put("<xmlattr>.allow_text_entry",true);
                control.put("<xmlattr>.allow_new_values",false);
                if (const auto alignment=control.get_optional<std::string>("<xmlattr>.halign"))
                {
                    control.put("combo_button.<xmlattr>.halign",*alignment);
                    control.get_child("<xmlattr>").erase("halign");
                }
            }
            if (kind=="favorites_bar")
            {
                kind="panel";
                control.get_child("<xmlattr>").erase("image_drag_indication");
                control.put("<xmlattr>.mouse_opaque",false);
            }
            if (kind=="label") kind="text";
            if (kind=="button" || kind=="combo_box") control.put("<xmlattr>.enabled",false);
            result.add_child(kind,shellNavigation(control));
        }
        return result;
    }
}

bool LLVKViewerUi::initializeConnectedShell(std::string& error)
{
    error.clear();
    if (mConnectedShell) return true;
    try
    {
        const auto main=shellDocument(*mSkin,"xui","main_view.xml",LLVKSkinFiles::Policy::Current,error);
        const auto navigation=shellDocument(*mSkin,"xui","panel_navigation_bar.xml",LLVKSkinFiles::Policy::Current,error);
        const auto location=shellDocument(*mSkin,"xui","widgets/location_input.xml",LLVKSkinFiles::Policy::All,error);
        if (!main || !navigation || !location) return false;
        const auto placement=shellNamed(*main,"navigation_bar");
        if (!placement) { error="Native shell is missing navigation placement"; return false; }
        ShellXml shell,document;
        shell.put_child("<xmlattr>",main->get_child("panel.<xmlattr>"));
        shell.put("<xmlattr>.name","native_connected_shell");
        shell.put("<xmlattr>.visible",false);
        auto bar=navigation->get_child("panel");
        for (const auto& [name,value] : placement->get_child("<xmlattr>"))
            if (name!="filename" && name!="class") bar.put("<xmlattr>."+name,value.data());
        bar.put("<xmlattr>.visible",true);
        shell.add_child("panel",bar);
        shell=shellNavigation(shell);
        const auto injectLocation=[&](auto&& self,ShellXml& node) -> void
        {
            if (node.get<std::string>("<xmlattr>.name","")=="location_combo")
            {
                const auto& defaults=location->get_child("location_input");
                node.put("<xmlattr>.font",defaults.get<std::string>("<xmlattr>.font"));
                if (const auto editor=defaults.get_child_optional("combo_editor")) node.put_child("combo_editor",*editor);
                if (const auto button=defaults.get_child_optional("combo_button"))
                {
                    auto combined=*button;
                    if (const auto alignment=node.get_optional<std::string>("combo_button.<xmlattr>.halign"))
                        combined.put("<xmlattr>.halign",*alignment);
                    node.put_child("combo_button",combined);
                }
            }
            for (auto& [kind,child] : node) if (kind!="<xmlattr>") self(self,child);
        };
        injectLocation(injectLocation,shell);
        document.add_child("panel",shell);
        std::ostringstream output;
        boost::property_tree::write_xml(output,document);
        const auto created=mDialogFactory->construct(mTree,output.str(),0,error);
        if (!created) return false;
        const auto navigationId=find("navigation_bar",*created),locationId=find("location_combo",*created);
        if (!navigationId || !locationId || !mTree.get(locationId)->combo ||
            !mTree.reparent(*created,mRoot,true,0,error))
        {
            std::string ignored;
            mTree.erase(*created,ignored);
            if (error.empty()) error="Native shell navigation declaration is incomplete";
            return false;
        }
        mConnectedShell=*created;
        mShellNavigation=navigationId;
        mShellLocation=locationId;
        mShellBlockers={"Location: parcel, coordinates, maturity, history and teleport services are not bound",
            "Favorites: inventory/landmark snapshot, readiness, overflow and actions are not bound",
            "Navigation: land, lighting and search services are not bound",
            "Status: native clock/statistics/parcel/media producers are not integrated",
            "Toolbars: account arrangement, side bars, drag, wrapping, overflow and nearby chatbar are not integrated"};
        if (!initializeShellToolbar(error))
        {
            std::string ignored;
            mTree.erase(mConnectedShell,ignored);
            mConnectedShell=mShellNavigation=mShellLocation=mShellToolbar=0;
            mShellCommands.clear();
            return false;
        }
        return true;
    }
    catch (const boost::property_tree::ptree_error& failure) { error=failure.what(); return false; }
}

bool LLVKViewerUi::prepareConnectedShell(float frameDelta,std::string& error)
{
    error.clear();
    if (!mLoginMenu)
    {
        const bool debug=mTree.setting("UseDebugMenus").value_or(LLSD(false)).asBoolean();
        mMenu->setVisible("Debug",debug);
        mMenu->setEnabled("Debug",debug);
    }
    else refreshShellMenuBindings();
    if (!mConnectedShell) return true;
    mTree.setVisible(mConnectedShell,mConnectedView);
    if (!mConnectedView) return true;
    const auto viewport=mTree.get(mRoot)->params.rect;
    if (!mTree.setShape(mConnectedShell,{0,0,viewport.right-viewport.left,viewport.top-viewport.bottom},error) ||
        !mTree.prepareLayoutStacks(mConnectedShell,frameDelta,error)) return false;
    const auto region=shellSessionCurrent() && mCommunicationContext && mCommunicationContext->tag==mSessionSnapshot.tag ?
        mCommunicationContext->regionName : std::string{};
    const auto editor=mTree.get(mShellLocation)->combo->editor;
    if (!mTree.setValue(editor,LLSD(region))) { error="Native shell location update failed"; return false; }
    if (mShellToolbar)
    {
        const int width=viewport.right-viewport.left;
        if (!mTree.setShape(mShellToolbar,{0,0,width,mShellToolbarHeight},error)) return false;
        const auto count=static_cast<int>(mShellCommands.size());
        const int available=std::max(0,width-2*mShellToolbarPad-std::max(0,count-1)*mShellToolbarGap);
        int left=mShellToolbarPad;
        for (const auto& [button,command] : mShellCommands)
        {
            const auto rectangle=mTree.get(button)->params.rect;
            const int buttonWidth=mShellToolbarFill && count ? available/count : rectangle.right-rectangle.left;
            const int height=rectangle.top-rectangle.bottom;
            if (!mTree.setShape(button,{left,mShellToolbarHeight-mShellToolbarPad-height,
                left+buttonWidth,mShellToolbarHeight-mShellToolbarPad},error)) return false;
            left+=buttonWidth+mShellToolbarGap;
            mTree.setEnabled(button,shellCommandAvailable(command));
            LLVKControl::Callback callback;
            callback.function=[this,command,owner=mSessionOwner,tag=mSessionSnapshot.tag,incarnation=mShellMenuIncarnation](auto,const LLSD&)
            {
                if (owner==mSessionOwner && tag==mSessionSnapshot.tag && incarnation==mShellMenuIncarnation)
                    activateShellCommand(command,mDialogError);
            };
            mTree.setControlCommit(button,std::move(callback));
            const bool visible=command=="chat" ? mConversationsFloater && mConversationsFloater->visible() :
                command=="people" ? mPeopleFloater && mPeopleFloater->visible() :
                command=="preferences" ? mPreferences && mPreferences->visible() :
                command=="howto" && mGuidebook && mGuidebook->visible();
            mTree.setValue(button,LLSD(visible));
        }
    }
    mMenu->setVisible("Advanced",mTree.setting("UseDebugMenus").value_or(LLSD(false)).asBoolean());
    return true;
}

bool LLVKViewerUi::shellSessionCurrent() const
{
    if (!mConnectedView || !mLoginMenu || !mSessionOwner) return false;
    const auto snapshot=mSessionOwner->snapshot();
    return snapshot.state==LLVKSessionOwner::State::Connected && snapshot.tag==mSessionSnapshot.tag;
}

bool LLVKViewerUi::shellCommandAvailable(const std::string& command) const
{
    if (!shellSessionCurrent() || mLifecycleScreen || mNoticePanel) return false;
    if (command=="chat" || command=="conversations" || command=="people" || command=="contacts" || command=="nearby" ||
        command=="friends_panel" || command=="groups_panel" || command=="contact_sets_panel" || command=="nearby_panel")
    {
        if (!mCommunications.context || !mCommunicationContext || mCommunicationContext->tag!=mSessionSnapshot.tag) return false;
        const bool peopleTabs=mTree.setting("FSUseV2Friends").value_or(LLSD(false)).asBoolean() &&
            mTree.setting("FSInternalSkinCurrent").value_or(LLSD("")).asString()!="Vintage";
        if ((command=="contacts" || (!peopleTabs && (command=="friends_panel" || command=="groups_panel" || command=="contact_sets_panel"))) &&
            mTree.setting("ContactsTornOff").value_or(LLSD(false)).asBoolean()) return false;
        return true;
    }
    if (command=="preferences" || command=="sl_about" || command=="fs_whitelist_floater") return true;
    if (command=="window_size") return bool(mWindowSizeService);
    if (command=="howto") return bool(mGuidebookOpen);
    if (command=="close") return canCloseMenuWindow();
    if (command=="quit") return bool(mQuitRequest);
    return false;
}

bool LLVKViewerUi::shellFloaterVisible(const std::string& name) const
{
    if (name=="preferences") return mPreferences && mPreferences->visible();
    if (name=="fs_im_container") return mConversationsFloater && mConversationsFloater->visible();
    if (name=="people") return mPeopleFloater && mPeopleFloater->visible();
    if (name=="guidebook") return mGuidebook && mGuidebook->visible();
    if (name!="imcontacts" && name!="fs_nearby_chat") return false;
    if (!mConversationsFloater || !mConversationsFloater->visible() || mConversationsFloater->minimized()) return false;
    const auto page=find(name=="imcontacts" ? "imcontacts" : "nearby_chat",mCommunicationPanel);
    for (auto ancestor=page; ancestor;)
    {
        const auto* node=mTree.get(ancestor);
        if (!node || !node->params.visible) return false;
        if (ancestor==mConversationsFloater->id()) return true;
        ancestor=node->parent;
    }
    return false;
}

bool LLVKViewerUi::activateShellCommand(const std::string& command,std::string& error)
{
    error.clear();
    if (!shellCommandAvailable(command))
    { error="Native shell command is not available in the current session"; return false; }
    const auto focused=[this](const LLVKFloater* floater)
    {
        if (!floater || !mCommunicationApplicationFocused || activeFloater()!=floater->id()) return false;
        for (auto ancestor=mTree.keyboardFocus(); mTree.get(ancestor); ancestor=mTree.get(ancestor)->parent)
            if (ancestor==floater->id()) return true;
        return false;
    };
    if (command=="chat")
        return mConversationsFloater && mConversationsFloater->visible() ? hideConversations(error) : showConversations(error);
    if (command=="conversations" || command=="nearby")
    {
        const bool shown=shellFloaterVisible(command=="nearby" ? "fs_nearby_chat" : "fs_im_container");
        if (shown && !mConversationsFloater->minimized() && focused(mConversationsFloater.get())) return hideConversations(error);
        return command=="nearby" ? showNearbyChat(error) : showConversations(error);
    }
    if (command=="contacts")
    {
        if (shellFloaterVisible("imcontacts")) return hideConversations(error);
        const auto* tabs=mTree.get(find("friends_and_groups",mCommunicationPanel));
        const auto* selected=tabs && tabs->tabContainer ? mTree.get(tabs->tabContainer->selected) : nullptr;
        const auto tab=selected ? selected->params.name : std::string{};
        return showContacts(tab=="groups_panel" ? "groups" : tab=="contact_sets_panel" ? "contact_sets" : "friends",error);
    }
    if (command=="friends_panel" || command=="groups_panel" || command=="contact_sets_panel" || command=="nearby_panel")
    {
        const bool peopleTabs=command=="nearby_panel" || (mTree.setting("FSUseV2Friends").value_or(LLSD(false)).asBoolean() &&
            mTree.setting("FSInternalSkinCurrent").value_or(LLSD("")).asString()!="Vintage");
        if (peopleTabs)
        {
            const auto* tabs=mPeopleFloater ? mTree.get(find("tabs",mPeopleFloater->id())) : nullptr;
            const auto* selected=tabs && tabs->tabContainer ? mTree.get(tabs->tabContainer->selected) : nullptr;
            if (mPeopleFloater && mPeopleFloater->visible() && !mPeopleFloater->minimized() &&
                selected && selected->params.name==command) return hidePeople(error);
            return showPeople(error,command);
        }
        return showContacts(command=="groups_panel" ? "groups" : command=="contact_sets_panel" ? "contact_sets" : "friends",error);
    }
    if (command=="people") return mPeopleFloater && mPeopleFloater->visible() ? hidePeople(error) : showPeople(error);
    if (command=="preferences")
        return mPreferences && mPreferences->visible() ? mPreferences->close(error) : showPreferences(error);
    if (command=="howto")
    {
        if (mGuidebook && mGuidebook->visible())
            return mGuidebook->minimized() || !focused(mGuidebook.get()) ? mGuidebook->open(error) : mGuidebook->close(error);
        return toggleGuidebook(error);
    }
    error="Native shell command service is not bound: "+command;
    return false;
}

bool LLVKViewerUi::initializeShellToolbar(std::string& error)
{
    const auto layout=shellDocument(*mSkin,"xui","panel_toolbar_view.xml",LLVKSkinFiles::Policy::Current,error);
    const auto defaults=shellDocument(*mSkin,"xui","widgets/toolbar.xml",LLVKSkinFiles::Policy::All,error);
    const auto arrangement=shellDocument(*mSkin,"","toolbars.xml",LLVKSkinFiles::Policy::Current,error);
    if (!layout || !defaults || !arrangement) return false;
    const auto placement=shellNamed(*layout,"toolbar_bottom");
    if (!placement) { error="Native shell toolbar placement is unavailable"; return false; }
    std::ifstream stream(mSkinBaseDirectory.parent_path()/"app_settings"/"commands.xml",std::ios::binary|std::ios::ate);
    if (!stream || stream.tellg()<0 || stream.tellg()>4*1024*1024)
    { error="Native shell command declaration is unavailable or exceeds byte budget"; return false; }
    stream.seekg(0);
    ShellXml commands;
    boost::property_tree::read_xml(stream,commands,boost::property_tree::xml_parser::no_comments);
    const auto& settings=arrangement->get_child("toolbars.bottom_toolbar");
    const auto& toolbar=defaults->get_child("toolbar");
    const auto mode=settings.get<std::string>("<xmlattr>.button_display_mode",
        placement->get<std::string>("<xmlattr>.button_display_mode","icons_with_text"));
    const auto styleName=mode=="icons_only" ? "button_icon" : mode=="icons_with_text" ? "button_icon_and_text" : "button";
    if (mode!="icons_only" && mode!="icons_with_text" && mode!="text_only")
    { error="Unsupported native toolbar button display mode: "+mode; return false; }
    const auto& style=toolbar.get_child(styleName);
    const auto& attributes=style.get_child("<xmlattr>");
    const auto attribute=[&](const char* name,int fallback)
    {
        const auto found=attributes.find(name);
        return found==attributes.not_found() ? fallback : found->second.get_value<int>();
    };
    const int height=attribute("desired_height",24),minimum=attribute("button_width.min",50);
    mShellToolbarHeight=placement->get<int>("<xmlattr>.height");
    mShellToolbarPad=toolbar.get<int>("<xmlattr>.pad_left",1);
    mShellToolbarGap=toolbar.get<int>("<xmlattr>.pad_between",1);
    mShellToolbarFill=settings.get<std::string>("<xmlattr>.button_layout_style","none")=="fill";
    if (height<1 || height>512 || minimum<1 || minimum>2048 || mShellToolbarHeight<1 || mShellToolbarHeight>512 ||
        mShellToolbarPad<0 || mShellToolbarPad>128 || mShellToolbarGap<0 || mShellToolbarGap>128)
    { error="Native shell toolbar dimensions are invalid"; return false; }
    ShellXml panel=toolbar.get_child("button_panel"),document;
    panel.put("<xmlattr>.name","toolbar_bottom");
    const auto& placementAttributes=placement->get_child("<xmlattr>");
    const auto background=placementAttributes.find("button_panel.bg_opaque_image");
    if (background!=placementAttributes.not_found()) panel.put("<xmlattr>.bg_opaque_image",background->second.data());
    panel.put("<xmlattr>.layout","topleft");
    panel.put("<xmlattr>.left",0);
    panel.put("<xmlattr>.bottom",-1);
    panel.put("<xmlattr>.width",mTree.get(mConnectedShell)->params.rect.right);
    panel.put("<xmlattr>.height",mShellToolbarHeight);
    panel.put("<xmlattr>.follows","left|right|bottom");
    panel.put("<xmlattr>.mouse_opaque",false);
    std::vector<std::string> names;
    for (const auto& [kind,entry] : settings)
    {
        if (kind!="command") continue;
        const auto name=entry.get<std::string>("<xmlattr>.name");
        if (names.size()>=64) { error="Native shell toolbar command budget exceeded"; return false; }
        const auto command=shellNamed(commands,name);
        if (!command)
        {
            mShellBlockers.push_back("Obsolete toolbar command has no declaration: "+name);
            continue;
        }
        ShellXml button=style;
        auto& buttonAttributes=button.get_child("<xmlattr>");
        for (const auto key : {"button_width.min","button_width.max","desired_height"}) buttonAttributes.erase(key);
        button.put("<xmlattr>.name","shell_command_"+name);
        button.put("<xmlattr>.label",mode=="icons_only" ? "" : errorString(command->get<std::string>("<xmlattr>.label_ref"),{}));
        button.put("<xmlattr>.tool_tip",errorString(command->get<std::string>("<xmlattr>.tooltip_ref"),{}));
        if (mode!="text_only") button.put("<xmlattr>.image_overlay",command->get<std::string>("<xmlattr>.icon"));
        button.put("<xmlattr>.layout","topleft");
        button.put("<xmlattr>.left",mShellToolbarPad+static_cast<int>(names.size())*(minimum+mShellToolbarGap));
        button.put("<xmlattr>.top",mShellToolbarPad);
        button.put("<xmlattr>.width",minimum);
        button.put("<xmlattr>.height",height);
        button.put("<xmlattr>.auto_resize",false);
        const bool bound=name=="chat" || name=="people" || name=="preferences" || name=="howto";
        button.put("<xmlattr>.enabled",false);
        if (!bound) mShellBlockers.push_back("Toolbar command service is not bound: "+name);
        panel.add_child("button",button);
        names.push_back(name);
    }
    document.add_child("panel",panel);
    std::ostringstream output;
    boost::property_tree::write_xml(output,document);
    const auto created=mDialogFactory->construct(mTree,output.str(),mConnectedShell,error);
    if (!created) return false;
    mShellToolbar=*created;
    for (const auto& name : names)
    {
        const auto id=find("shell_command_"+name,mShellToolbar);
        if (!id) { error="Native shell toolbar button is missing: "+name; return false; }
        mShellCommands.emplace_back(id,name);
    }
    return true;
}

void LLVKViewerUi::refreshShellMenuBindings()
{
    const auto admitted=[this,owner=mSessionOwner,tag=mSessionSnapshot.tag,incarnation=mShellMenuIncarnation]
    {
        return owner==mSessionOwner && tag==mSessionSnapshot.tag && incarnation==mShellMenuIncarnation &&
            shellSessionCurrent() && !mLifecycleScreen && !mNoticePanel;
    };
    const auto bind=[&](const std::string& action,const std::string& parameter,const std::string& command,LLVKMenu::Handler handler)
    {
        mMenu->bindItem(action,parameter,shellCommandAvailable(command) ? std::move(handler) : LLVKMenu::Handler{});
    };
    const auto service=[&](const std::string& action,LLVKMenu::Handler handler,bool available=true)
    {
        mMenu->bind(action,admitted() && available ?
            LLVKMenu::Handler([admitted,handler=std::move(handler)](const auto& action,const auto& parameter)
            { if (admitted()) handler(action,parameter); }) : LLVKMenu::Handler{});
    };
    service("PromptShowURL",[this](const auto&,const std::string& parameter)
    {
        const auto separator=parameter.find(',');
        if (separator!=std::string::npos && mOpenUrl) mOpenUrl(parameter.substr(separator+1));
    },bool(mOpenUrl));
    service("Advanced.ShowURL",[this](const auto&,const std::string& url)
    { if (mOpenUrl) mOpenUrl(url); },bool(mOpenUrl));
    for (const auto action : {"Advanced.WebContentTest","Advanced.WebBrowserTest"})
        service(action,[this](const auto&,const std::string& url)
        { showMediaBrowser(url=="HOME_PAGE" ? mTree.setting("FSBrowserHomePage").value_or(LLSD()).asString() :
            url.empty() ? "about:blank" : url,mDialogError); });
    service("Advanced.ReportBug",[this](const auto&,const auto&) { reportProblem(mDialogError); },bool(mOpenUrl));
    service("Advanced.ShowDebugSettings",[this](const auto&,const auto&) { showDebugSettings(mDialogError); });
    for (const auto name : {"test_textbox","test_text_editor","font_test","test_widgets"})
        mMenu->bindItem("Floater.Show",name,admitted() ? LLVKMenu::Handler([this,admitted,name](const auto&,const auto&)
        { if (admitted()) showUiTest(name,mDialogError); }) : LLVKMenu::Handler{});
    mMenu->bindItem("Floater.Toggle","settings_color",admitted() ? LLVKMenu::Handler([this,admitted](const auto&,const auto&)
    {
        if (!admitted()) return;
        if (mColorSettings && mColorSettings->visible()) mColorSettings->close(mDialogError);
        else showColorSettings(mDialogError);
    }) : LLVKMenu::Handler{});
    mMenu->bindItem("Floater.Toggle","ui_preview",admitted() ? LLVKMenu::Handler([this,admitted](const auto&,const auto&)
    {
        if (!admitted()) return;
        if (mUiPreview && mUiPreview->visible()) mUiPreview->close(mDialogError);
        else showUiPreview(mDialogError);
    }) : LLVKMenu::Handler{});
    service("Develop.Fonts.Dump",[this](const auto&,const auto&)
    { LL_INFOS("NativeFonts") << fontDiagnostics() << LL_ENDL; });
    service("Develop.Fonts.DumpTextures",[this](const auto&,const auto&) { dumpFontTextures(mDialogError); });
    service("Develop.SetLoggingLevel",[](const auto&,const std::string& level)
    {
        if (level.size()==1 && level[0]>='0' && level[0]<='4')
            LLError::setDefaultLevel(static_cast<LLError::ELevel>(level[0]-'0'));
    });
    mMenu->bindPredicate("Develop.CheckLoggingLevel",[](const std::string& level)
    { return level.size()==1 && level[0]>='0' && level[0]<='4' && static_cast<int>(LLError::getDefaultLevel())==level[0]-'0'; });
    for (const std::string setting : {"UseDebugMenus","QAMode"})
        mMenu->bindItem("ToggleControl",setting,admitted() ? LLVKMenu::Handler([this,admitted,setting](const auto&,const auto&)
        { if (admitted()) mTree.updateSetting(setting,LLSD(!mTree.setting(setting).value_or(LLSD(false)).asBoolean())); }) : LLVKMenu::Handler{});
    mMenu->bindPredicate("CheckControl",[this](const std::string& name)
    { return mTree.setting(name).value_or(LLSD(false)).asBoolean(); });
    const bool develop=mTree.setting("QAMode").value_or(LLSD(false)).asBoolean();
    mMenu->setVisible("Develop",develop);
    mMenu->setEnabled("Develop",develop);
    bind("Floater.Toggle","preferences","preferences",[this,admitted](const auto&,const auto&)
    { if (admitted()) activateShellCommand("preferences",mDialogError); });
    for (const auto& [name,show] : std::map<std::string,bool (LLVKViewerUi::*)(std::string&)>{
        {"sl_about",&LLVKViewerUi::showAbout},{"fs_whitelist_floater",&LLVKViewerUi::showWhitelist},
        {"window_size",&LLVKViewerUi::showWindowSize}})
        bind("Floater.Show",name,name,[this,admitted,show,name](const auto&,const auto&)
        { if (admitted() && shellCommandAvailable(name)) (this->*show)(mDialogError); });
    bind("Floater.ToggleOrBringToFront","fs_im_container","conversations",[this,admitted](const auto&,const auto&)
    { if (admitted()) activateShellCommand("conversations",mDialogError); });
    bind("Floater.Toggle","people","people",[this,admitted](const auto&,const auto&)
    { if (admitted()) activateShellCommand("people",mDialogError); });
    bind("Floater.Toggle","imcontacts","contacts",[this,admitted](const auto&,const auto&)
    { if (admitted()) activateShellCommand("contacts",mDialogError); });
    bind("Floater.ToggleOrBringToFront","fs_nearby_chat","nearby",[this,admitted](const auto&,const auto&)
    { if (admitted()) activateShellCommand("nearby",mDialogError); });
    for (const std::string tab : {"friends_panel","groups_panel","contact_sets_panel","nearby_panel"})
        bind("SideTray.PanelPeopleTab",tab,tab,[this,admitted,tab](const auto&,const auto&)
        { if (admitted()) activateShellCommand(tab,mDialogError); });
    bind("File.CloseWindow","","close",[this,admitted](const auto&,const auto&)
    { if (admitted() && shellCommandAvailable("close")) closeMenuWindow(mDialogError); });
    bind("Help.ToggleHowTo","","howto",[this,admitted](const auto&,const auto&)
    { if (admitted()) activateShellCommand("howto",mDialogError); });
    bind("File.Quit","","quit",[this,admitted](const auto&,const auto&)
    {
        if (!admitted() || !shellCommandAvailable("quit")) return;
        queueNotice("ConfirmQuit",{},[this,owner=mSessionOwner,tag=mSessionSnapshot.tag,incarnation=mShellMenuIncarnation](int option,const LLSD&)
        {
            if (option==0 && owner==mSessionOwner && mSessionOwner && mQuitRequest &&
                incarnation==mShellMenuIncarnation && mSessionOwner->snapshot().tag==tag &&
                mSessionOwner->snapshot().state==LLVKSessionOwner::State::Connected) mQuitRequest();
        },mDialogError);
    });
    mMenu->bindPredicate("File.EnableCloseWindow",[this,admitted](const auto&)
    { return admitted() && shellCommandAvailable("close"); });
    mMenu->setTearOffHandler([this,admitted](auto item,auto rectangle)
    { if (admitted()) tearOffMenu(item,rectangle,mDialogError); });
}

bool LLVKViewerUi::selectShellMenu(bool connected,std::string& error)
{
    error.clear();
    if (connected==bool(mLoginMenu)) return true;
    std::unique_ptr<LLVKMenu> viewerMenu;
    if (connected)
    {
        const auto files=mSkin->read("xui","menu_viewer.xml",LLVKSkinFiles::Policy::Current,error);
        if (!files) return false;
        std::vector<std::string_view> layers;
        for (const auto& file : *files) layers.push_back(file);
        const auto xml=LLVKXmlLayers::merge(layers,error);
        if (!xml) return false;
        const auto font=mFonts->resolve({"SansSerif","Small"},error);
        if (!font) return false;
        viewerMenu=LLVKMenu::create(*xml,font,mColors,mShellLabels,false,error);
        if (!viewerMenu) return false;
        for (const auto& item : viewerMenu->items())
        {
            for (const auto& predicate : {item.enableAction,item.visibleAction,item.checkAction})
                if (!predicate.empty()) viewerMenu->bindPredicate(predicate,[](const auto&) { return false; });
        }
        viewerMenu->bindPredicate("Floater.Visible",[this](const std::string& name)
        { return shellFloaterVisible(name); });
        viewerMenu->setVisible("Advanced",mTree.setting("UseDebugMenus").value_or(LLSD(false)).asBoolean());
        for (const auto name : {"Develop","Admin","Deprecated","RLVa Main","RLVa"})
        { viewerMenu->setVisible(name,false); viewerMenu->setEnabled(name,false); }
        for (const auto name : {"current_grid_help","current_grid_about","grid_help_seperator"}) viewerMenu->setVisible(name,false);
    }
    mMenu->dismiss();
    blockTooltips();
    if (!mTree.setTopControl(0,error) || !mTree.setMouseCapture(0,error)) return false;
    for (auto& [item,dialog] : mTornMenus)
    {
        if (!dialog.floater->close(error)) return false;
        if (mActiveFloater==dialog.floater.get()) mActiveFloater=nullptr;
    }
    mTornMenus.clear();
    ++mShellMenuIncarnation;
    if (connected)
    {
        mLoginMenu=std::move(mMenu);
        mMenu=std::move(viewerMenu);
    }
    else mMenu=std::move(mLoginMenu);
    return true;
}
