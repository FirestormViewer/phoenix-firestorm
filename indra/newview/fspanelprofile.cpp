/**
 * @file fspanelprofile.cpp
 * @brief Legacy Profile Floater
 *
 * $LicenseInfo:firstyear=2012&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2012, Kadah Coba
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * The Phoenix Viewer Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.phoenixviewer.com
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"
#include "fspanelprofile.h"

// Common
#include "llavatarconstants.h" //AVATAR_ONLINE
#include "llavatarnamecache.h"
#include "llslurl.h"
#include "lldateutil.h" //ageFromDate

// UI
#include "llavatariconctrl.h"
#include "llclipboard.h" //gClipboard
#include "llcheckboxctrl.h"
#include "lllineeditor.h"
#include "llloadingindicator.h"
#include "llmenubutton.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lltexteditor.h"
#include "lltexturectrl.h"
#include "lltoggleablemenu.h"
#include "llgrouplist.h"

// Newview
#include "fsdata.h"
#include "llagent.h" //gAgent
#include "llagentpicksinfo.h"
#include "llavataractions.h"
#include "llavatarpropertiesprocessor.h"
#include "llcallingcard.h"
#include "llfloaterreg.h"
#include "llfirstuse.h"
#include "llnotificationsutil.h"
#include "llnotificationsutil.h"
#include "lltrans.h"
#include "llvoiceclient.h"
#include "llgroupactions.h"
#include "lltooldraganddrop.h"
#include "llviewercontrol.h"
#include "llviewernetwork.h" //LLGridManager
#include "llfloaterworldmap.h"
#include "llparcel.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llworldmap.h"

static LLRegisterPanelClassWrapper<FSPanelProfile> t_panel_profile("panel_profile_secondlife");
static LLRegisterPanelClassWrapper<FSPanelProfileWeb> t_panel_web("panel_profile_web");
static LLRegisterPanelClassWrapper<FSPanelProfileInterests> t_panel_interests("panel_profile_interests");
static LLRegisterPanelClassWrapper<FSPanelProfilePicks> t_panel_picks("panel_profile_picks");
static LLRegisterPanelClassWrapper<FSPanelProfileFirstLife> t_panel_firstlife("panel_profile_firstlife");
static LLRegisterPanelClassWrapper<FSPanelAvatarNotes> t_panel_notes("panel_profile_notes");


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class FSDropTarget
//
// This handy class is a simple way to drop something on another
// view. It handles drop events, always setting itself to the size of
// its parent.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class FSDropTarget : public LLView
{
public:
    struct Params : public LLInitParam::Block<Params, LLView::Params>
    {
        Optional<LLUUID> agent_id;
        Params()
        :    agent_id("agent_id")
        {
            changeDefault(mouse_opaque, false);
            changeDefault(follows.flags, FOLLOWS_ALL);
        }
    };

    FSDropTarget(const Params&);
    ~FSDropTarget();

    void doDrop(EDragAndDropType cargo_type, void* cargo_data);

    //
    // LLView functionality
    virtual BOOL handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
                                   EDragAndDropType cargo_type,
                                   void* cargo_data,
                                   EAcceptance* accept,
                                   std::string& tooltip_msg);
    void setAgentID(const LLUUID &agent_id)        { mAgentID = agent_id; }
protected:
    LLUUID mAgentID;
};

FSDropTarget::FSDropTarget(const FSDropTarget::Params& p)
:    LLView(p),
    mAgentID(p.agent_id)
{}

FSDropTarget::~FSDropTarget()
{}

void FSDropTarget::doDrop(EDragAndDropType cargo_type, void* cargo_data)
{
    llinfos << "FSDropTarget::doDrop()" << llendl;
}

BOOL FSDropTarget::handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
                                     EDragAndDropType cargo_type,
                                     void* cargo_data,
                                     EAcceptance* accept,
                                     std::string& tooltip_msg)
{
    if(getParent())
    {
        LLToolDragAndDrop::handleGiveDragAndDrop(mAgentID, LLUUID::null, drop,
                                                 cargo_type, cargo_data, accept);

        return TRUE;
    }

    return FALSE;
}

static LLDefaultChildRegistry::Register<FSDropTarget> r("profile_drop_target");

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

FSPanelProfileTab::FSPanelProfileTab()
: LLPanel()
, mAvatarId(LLUUID::null)
{
}

FSPanelProfileTab::~FSPanelProfileTab()
{
    if(getAvatarId().notNull())
    {
        LLAvatarPropertiesProcessor::getInstance()->removeObserver(getAvatarId(),this);
    }
}

void FSPanelProfileTab::setAvatarId(const LLUUID& id)
{
    if(id.notNull())
    {
        if(getAvatarId().notNull())
        {
            LLAvatarPropertiesProcessor::getInstance()->removeObserver(mAvatarId,this);
        }
        mAvatarId = id;
        LLAvatarPropertiesProcessor::getInstance()->addObserver(getAvatarId(),this);
    }
}

void FSPanelProfileTab::onOpen(const LLSD& key)
{
    // Update data even if we are viewing same avatar profile as some data might been changed.
    setAvatarId(key.asUUID());

    setApplyProgress(true);
}

void FSPanelProfileTab::enableControls()
{
    setApplyProgress(false);

    mLoaded = TRUE;
}

void FSPanelProfileTab::setApplyProgress(bool started)
{
    LLLoadingIndicator* indicator = getChild<LLLoadingIndicator>("progress_indicator");

    indicator->setVisible(started);

    if (started)
    {
        indicator->start();
    }
    else
    {
        indicator->stop();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool enable_god()
{
    return gAgent.isGodlike();
}

FSPanelProfile::FSPanelProfile()
 : FSPanelProfileTab()
 , mStatusText(NULL)
{
}

FSPanelProfile::~FSPanelProfile()
{
    if(getAvatarId().notNull())
    {
        LLAvatarTracker::instance().removeParticularFriendObserver(getAvatarId(), this);
    }

    if(LLVoiceClient::instanceExists())
    {
        LLVoiceClient::getInstance()->removeObserver((LLVoiceClientStatusObserver*)this);
    }
}

BOOL FSPanelProfile::postBuild()
{
    mStatusText = getChild<LLTextBox>("status");
    mStatusText->setVisible(false);

    LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
    registrar.add("Profile.Call",                   boost::bind(&FSPanelProfile::onCallButtonClick, this));
    registrar.add("Profile.Share",                  boost::bind(&FSPanelProfile::share, this));
    registrar.add("Profile.Kick",                   boost::bind(&FSPanelProfile::kick, this));
    registrar.add("Profile.Freeze",                 boost::bind(&FSPanelProfile::freeze, this));
    registrar.add("Profile.Unfreeze",               boost::bind(&FSPanelProfile::unfreeze, this));
    registrar.add("Profile.CSR",                    boost::bind(&FSPanelProfile::csr, this));
    registrar.add("Profile.CopyNameToClipboard",    boost::bind(&FSPanelProfile::onCopyToClipboard, this));
    registrar.add("Profile.CopyURI",                boost::bind(&FSPanelProfile::onCopyURI, this));
    registrar.add("Profile.CopyKey",                boost::bind(&FSPanelProfile::onCopyKey, this));

    childSetCommitCallback("add_friend",            boost::bind(&FSPanelProfile::onAddFriendButtonClick, this),NULL);
    childSetCommitCallback("im",                    boost::bind(&FSPanelProfile::onIMButtonClick, this), NULL);
    childSetCommitCallback("teleport",              boost::bind(&FSPanelProfile::onTeleportButtonClick, this), NULL);
    childSetCommitCallback("show_on_map_btn",       boost::bind(&FSPanelProfile::onMapButtonClick, this), NULL);
    childSetCommitCallback("pay",                   boost::bind(&FSPanelProfile::pay, this), NULL);
    childSetCommitCallback("block",                 boost::bind(&FSPanelProfile::toggleBlock,this),NULL);
    childSetCommitCallback("unblock",               boost::bind(&FSPanelProfile::toggleBlock,this),NULL);
    childSetCommitCallback("group_invite",          boost::bind(&FSPanelProfile::onGroupInvite,this),NULL);
    childSetCommitCallback("set_name",              boost::bind(&FSPanelProfile::onClickSetName, this), NULL);

    LLUICtrl::EnableCallbackRegistry::ScopedRegistrar enable;
    enable.add("Profile.EnableCall",                boost::bind(&FSPanelProfile::enableCall, this));
    enable.add("Profile.EnableGod",                 boost::bind(&enable_god));

    LLGroupList* group_list = getChild<LLGroupList>("group_list");
    group_list->setDoubleClickCallback(boost::bind(&FSPanelProfile::openGroupProfile, this));
    group_list->setReturnCallback(boost::bind(&FSPanelProfile::openGroupProfile, this));

    LLToggleableMenu* profile_menu = LLUICtrlFactory::getInstance()->createFromFile<LLToggleableMenu>("menu_profile_overflow.xml", gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance());
        getChild<LLMenuButton>("overflow_btn")->setMenu(profile_menu, LLMenuButton::MP_TOP_RIGHT);

    // allow skins to have copy buttons for name and avatar URI -Zi
    LLButton* copy_uri_button=findChild<LLButton>("copy_uri_button");
    LLButton* copy_name_button=findChild<LLButton>("copy_name_button");

    if(copy_uri_button)
        copy_uri_button->setCommitCallback(boost::bind(&FSPanelProfile::onCopyURI, this));

    if(copy_name_button)
        copy_name_button->setCommitCallback(boost::bind(&FSPanelProfile::onCopyToClipboard, this));
    // allow skins to have copy buttons for name and avatar URI -Zi

    LLVoiceClient::getInstance()->addObserver((LLVoiceClientStatusObserver*)this);

    return TRUE;
}

void FSPanelProfile::onOpen(const LLSD& key)
{
    FSPanelProfileTab::onOpen(key);

    LLAvatarPropertiesProcessor::getInstance()->addObserver(getAvatarId(),this);

    if (getAvatarId() == gAgent.getID())
    {
        getChild<LLUICtrl>("group_invite")->setVisible( false );
        getChild<LLUICtrl>("show_on_map_btn")->setVisible( false );
        getChild<LLUICtrl>("pay")->setVisible( false );
        getChild<LLUICtrl>("teleport")->setVisible( false );
        getChild<LLUICtrl>("im")->setVisible( false );
        getChild<LLUICtrl>("add_friend")->setVisible( false );
        getChild<LLUICtrl>("block")->setVisible( false );
        getChild<LLUICtrl>("unblock")->setVisible( false );
        getChild<LLUICtrl>("overflow_btn")->setVisible( false );

        LLGroupList* group_list = getChild<LLGroupList>("group_list");
        group_list->setShowNone(false);
        group_list->enableForAgent(false);

        if (LLAvatarNameCache::useDisplayNames())
        {
            getChild<LLUICtrl>("set_name")->setVisible( true );
            getChild<LLUICtrl>("set_name")->setEnabled( true );
        }

        getChild<LLTextBase>("sl_description_edit")->setParseHTML(false);

        updateButtons();

        FSDropTarget* drop_target = getChild<FSDropTarget> ("drop_target");
        drop_target->setVisible( false );
        drop_target->setEnabled( false );
    }
    else
    {
        //Disable "Add Friend" button for friends.
        getChildView("add_friend")->setEnabled(!LLAvatarActions::isFriend(getAvatarId()));

        mVoiceStatus = LLAvatarActions::canCall();

        updateOnlineStatus();
        updateButtons();

        FSDropTarget* drop_target = getChild<FSDropTarget> ("drop_target");
        drop_target->setAgentID( getAvatarId() );
    }

    getChild<LLUICtrl>("user_key")->setValue( getAvatarId().asString() );

    updateData();
}

void FSPanelProfile::apply(LLAvatarData* data)
{
    if (getIsLoaded() && (getAvatarId() == gAgent.getID()))
    {
        data->image_id = getChild<LLTextureCtrl>("2nd_life_pic")->getImageAssetID();
        data->about_text = getChild<LLUICtrl>("sl_description_edit")->getValue().asString();
        data->allow_publish = getChild<LLUICtrl>("show_in_search_checkbox")->getValue();

        LLAvatarPropertiesProcessor::getInstance()->sendAvatarPropertiesUpdate(data);
    }
}

void FSPanelProfile::updateData()
{
    if (getAvatarId().notNull())
    {
        LLAvatarPropertiesProcessor::getInstance()->sendAvatarPropertiesRequest(getAvatarId());
        if (getAvatarId() != gAgent.getID())
        {
            LLAvatarPropertiesProcessor::getInstance()->sendAvatarGroupsRequest(getAvatarId());
        }
    }
}

void FSPanelProfile::processProperties(void* data, EAvatarProcessorType type)
{
    if(APT_PROPERTIES == type)
    {
        const LLAvatarData* avatar_data = static_cast<const LLAvatarData*>(data);
        if(avatar_data && getAvatarId() == avatar_data->avatar_id)
        {
            processProfileProperties(avatar_data);
            enableControls();
        }
    }
    else if(APT_GROUPS == type)
    {
        LLAvatarGroups* avatar_groups = static_cast<LLAvatarGroups*>(data);
        if(avatar_groups && getAvatarId() == avatar_groups->avatar_id)
        {
            processGroupProperties(avatar_groups);
        }
    }
}

void FSPanelProfile::processProfileProperties(const LLAvatarData* avatar_data)
{
    if (!LLAvatarActions::isFriend(getAvatarId()) && (getAvatarId() != gAgent.getID()))
    {
        // this is non-friend avatar. Status will be updated from LLAvatarPropertiesProcessor.
        // in FSPanelProfile::processOnlineStatus()

        // subscribe observer to get online status. Request will be sent by FSPanelProfile itself.
        // do not subscribe for friend avatar because online status can be wrong overridden
        // via LLAvatarData::flags if Preferences: "Only Friends & Groups can see when I am online" is set.
        processOnlineStatus(avatar_data->flags & AVATAR_ONLINE);
    }

    fillCommonData(avatar_data);

    fillPartnerData(avatar_data);

    fillAccountStatus(avatar_data);
}

void FSPanelProfile::processGroupProperties(const LLAvatarGroups* avatar_groups)
{
    //KC: the group_list ctrl can handle all this for us on our own profile
    if (getAvatarId() == gAgent.getID())
        return;

    LLGroupList* group_list = getChild<LLGroupList>("group_list");

    // *NOTE dzaporozhan
    // Group properties may arrive in two callbacks, we need to save them across
    // different calls. We can't do that in textbox as textbox may change the text.

    LLAvatarGroups::group_list_t::const_iterator it = avatar_groups->group_list.begin();
    const LLAvatarGroups::group_list_t::const_iterator it_end = avatar_groups->group_list.end();

    for(; it_end != it; ++it)
    {
        LLAvatarGroups::LLGroupData group_data = *it;
        mGroups[group_data.group_name] = group_data.group_id;
    }

    group_list->setGroups(mGroups);
}

void FSPanelProfile::openGroupProfile()
{
    LLUUID group_id = getChild<LLGroupList>("group_list")->getSelectedUUID();
    LLGroupActions::show(group_id);
}

void FSPanelProfile::onAvatarNameCache(const LLUUID& agent_id, const LLAvatarName& av_name)
{
    getChild<LLUICtrl>("complete_name")->setValue( av_name.getCompleteName() );

    //[ADD: FIRE-2266: SJ] make sure username is always filled even when Displaynames are not enabled
    std::string username = av_name.mUsername;
    if (username.empty())
    {
        username = LLCacheName::buildUsername(av_name.mDisplayName);
    }

    //[ADD: FIRE-2266: SJ] Adding link to webprofiles on profile which opens Web Profiles in browser
    std::string url;
    if (LLGridManager::getInstance()->isInSLMain())
    {
        url = gSavedSettings.getString("WebProfileURL");
    }
    else if (LLGridManager::getInstance()->isInSLBeta())
    {
        url = gSavedSettings.getString("WebProfileNonProductionURL");
    }
    else
    {
        //OpenSimFIXME: get from grid - but how?
        // possibilities:     * grid_info  (profiles accessible outside the grid)
        //             * login message (profiles only within the grid)
        //            * capability (better for decentaliced environment)
    }
    LLSD subs;
    subs["AGENT_NAME"] = username;
    url = LLWeb::expandURLSubstitutions(url,subs);
    LLStringUtil::toLower(url);
    getChild<LLUICtrl>("web_profile_text")->setValue( url );
}

void FSPanelProfile::fillCommonData(const LLAvatarData* avatar_data)
{
    //remove avatar id from cache to get fresh info
    LLAvatarIconIDCache::getInstance()->remove(avatar_data->avatar_id);

    LLStringUtil::format_map_t args;
    {
        std::string birth_date = LLTrans::getString("AvatarBirthDateFormat");
        LLStringUtil::format(birth_date, LLSD().with("datetime", (S32) avatar_data->born_on.secondsSinceEpoch()));
        args["[REG_DATE]"] = birth_date;
    }

    args["[AGE]"] = LLDateUtil::ageFromDate( avatar_data->born_on, LLDate::now());
    args["[AGEDAYS]"] = LLSD((S32) (LLDate::now().secondsSinceEpoch()-avatar_data->born_on.secondsSinceEpoch())/86400).asString();
    std::string register_date = getString("RegisterDateFormat", args);
    getChild<LLUICtrl>("register_date")->setValue(register_date );
    getChild<LLUICtrl>("sl_description_edit")->setValue(avatar_data->about_text);
    // <FS:LO> Force profile picture boost level up so the full image loads
    LLViewerFetchedTexture* tx=LLViewerTextureManager::getFetchedTexture(avatar_data->image_id);
    tx->setBoostLevel(LLViewerFetchedTexture::BOOST_UI);
    tx->forceImmediateUpdate();
    // </FS:LO>
    getChild<LLUICtrl>("2nd_life_pic")->setValue(avatar_data->image_id);

    if (getAvatarId() == gAgent.getID())
    {
        getChild<LLUICtrl>("show_in_search_checkbox")->setValue((BOOL)(avatar_data->flags & AVATAR_ALLOW_PUBLISH));
    }

    // Hide home page textbox if no page was set to fix "homepage URL appears clickable without URL - EXT-4734"
    getChildView("homepage_edit")->setVisible( !avatar_data->profile_url.empty());
}

void FSPanelProfile::fillPartnerData(const LLAvatarData* avatar_data)
{
    LLTextBox* partner_text = getChild<LLTextBox>("partner_text");
    if (avatar_data->partner_id.notNull())
    {
        partner_text->setText(LLSLURL("agent", avatar_data->partner_id, "inspect").getSLURLString());
    }
    else
    {
        partner_text->setText(getString("no_partner_text"));
    }
}

void FSPanelProfile::fillAccountStatus(const LLAvatarData* avatar_data)
{
    LLStringUtil::format_map_t args;
    args["[ACCTTYPE]"] = LLAvatarPropertiesProcessor::accountType(avatar_data);
    args["[PAYMENTINFO]"] = LLAvatarPropertiesProcessor::paymentInfo(avatar_data);
    // *NOTE: AVATAR_AGEVERIFIED not currently getting set in
    // dataserver/lldataavatar.cpp for privacy considerations
    //...so why didn't they comment this out too and take it out of
    //   the XML, too? Argh. -- TS
    // args["[AGEVERIFICATION]"] = "";

    FSDataAgent* agent = FSData::getInstance()->getAgent(avatar_data->avatar_id);
    if (agent)
    {
        args["[FIRESTORM]"] = "Firestorm";
        if (agent->developer)
        {
            std::string dev = getString("FSDev");
            args["[FSDEV]"] = dev;
        }
        else
        {
            args["[FSDEV]"] = "";
        }
        if (agent->support)
        {
            std::string supp = getString("FSSupp");
            if (agent->developer)
            {
                supp = " /" + supp;
            }
            args["[FSSUPP]"] = supp;
        }
        else
        {
            args["[FSSUPP]"] = "";
        }
    }
    else
    {
        args["[FIRESTORM]"] = "";
        args["[FSSUPP]"] = "";
        args["[FSDEV]"] = "";
    }

    std::string caption_text = getString("CaptionTextAcctInfo", args);
    getChild<LLUICtrl>("acc_status_text")->setValue(caption_text);
}

void FSPanelProfile::onMapButtonClick()
{
    LLAvatarActions::showOnMap(getAvatarId());
}

void FSPanelProfile::pay()
{
    LLAvatarActions::pay(getAvatarId());
}

void FSPanelProfile::share()
{
    LLAvatarActions::share(getAvatarId());
}

void FSPanelProfile::toggleBlock()
{
    LLAvatarActions::toggleBlock(getAvatarId());

    updateButtons();
}

bool FSPanelProfile::enableCall()
{
    return mVoiceStatus;
}

void FSPanelProfile::kick()
{
    LLAvatarActions::kick(getAvatarId());
}

void FSPanelProfile::freeze()
{
    LLAvatarActions::freeze(getAvatarId());
}

void FSPanelProfile::unfreeze()
{
    LLAvatarActions::unfreeze(getAvatarId());
}

void FSPanelProfile::csr()
{
    std::string name;
    gCacheName->getFullName(getAvatarId(), name);
    LLAvatarActions::csr(getAvatarId(), name);
}

void FSPanelProfile::onAddFriendButtonClick()
{
    LLAvatarActions::requestFriendshipDialog(getAvatarId());
}

void FSPanelProfile::onIMButtonClick()
{
    LLAvatarActions::startIM(getAvatarId());
}

void FSPanelProfile::onTeleportButtonClick()
{
    LLAvatarActions::offerTeleport(getAvatarId());
}

void FSPanelProfile::onCallButtonClick()
{
    LLAvatarActions::startCall(getAvatarId());
}

void FSPanelProfile::onCopyToClipboard()
{
    std::string name = getChild<LLUICtrl>("complete_name")->getValue().asString();
    LLClipboard::instance().copyToClipboard(utf8str_to_wstring(name), 0, name.size() );
}

void FSPanelProfile::onCopyURI()
{
    std::string name = "secondlife:///app/agent/"+getChild<LLUICtrl>("user_key")->getValue().asString()+"/about";
    LLClipboard::instance().copyToClipboard(utf8str_to_wstring(name), 0, name.size() );
}

void FSPanelProfile::onCopyKey()
{
    LLClipboard::instance().copyToClipboard(utf8str_to_wstring(getAvatarId().asString()), 0, getAvatarId().asString().size() );
}

void FSPanelProfile::onGroupInvite()
{
    LLAvatarActions::inviteToGroup(getAvatarId());
}

// virtual, called by LLAvatarTracker
void FSPanelProfile::changed(U32 mask)
{
    updateOnlineStatus();
    updateButtons();
}

// virtual, called by LLVoiceClient
void FSPanelProfile::onChange(EStatusType status, const std::string &channelURI, bool proximal)
{
    if(status == STATUS_JOINING || status == STATUS_LEFT_CHANNEL)
    {
        return;
    }

    mVoiceStatus = LLAvatarActions::canCall();
}

void FSPanelProfile::setAvatarId(const LLUUID& id)
{
    if(id.notNull())
    {
        if(getAvatarId().notNull())
        {
            LLAvatarTracker::instance().removeParticularFriendObserver(getAvatarId(), this);
        }

        FSPanelProfileTab::setAvatarId(id);

        if (LLAvatarActions::isFriend(getAvatarId()))
        {
            LLAvatarTracker::instance().addParticularFriendObserver(getAvatarId(), this);
        }
    }
}

bool FSPanelProfile::isGrantedToSeeOnlineStatus()
{
    // set text box visible to show online status for non-friends who has not set in Preferences
    // "Only Friends & Groups can see when I am online"
    if (!LLAvatarActions::isFriend(getAvatarId()))
    {
        return true;
    }

    // *NOTE: GRANT_ONLINE_STATUS is always set to false while changing any other status.
    // When avatar disallow me to see her online status processOfflineNotification Message is received by the viewer
    // see comments for ChangeUserRights template message. EXT-453.
    // If GRANT_ONLINE_STATUS flag is changed it will be applied when viewer restarts. EXT-3880
    const LLRelationship* relationship = LLAvatarTracker::instance().getBuddyInfo(getAvatarId());
    return relationship->isRightGrantedFrom(LLRelationship::GRANT_ONLINE_STATUS);
}

// method was disabled according to EXT-2022. Re-enabled & improved according to EXT-3880
void FSPanelProfile::updateOnlineStatus()
{
    if (!LLAvatarActions::isFriend(getAvatarId())) return;
    // For friend let check if he allowed me to see his status
    const LLRelationship* relationship = LLAvatarTracker::instance().getBuddyInfo(getAvatarId());
    bool online = relationship->isOnline();
    processOnlineStatus(online);
}

void FSPanelProfile::processOnlineStatus(bool online)
{
    mStatusText->setVisible(isGrantedToSeeOnlineStatus());

    std::string status = getString(online ? "status_online" : "status_offline");

    mStatusText->setValue(status);
    mStatusText->setColor(online ?
        LLUIColorTable::instance().getColor("StatusUserOnline") :
        LLUIColorTable::instance().getColor("StatusUserOffline"));
}

void FSPanelProfile::enableControls()
{
    FSPanelProfileTab::enableControls();

    if (getAvatarId() == gAgent.getID())
    {
        getChild<LLUICtrl>("show_in_search_checkbox")->setVisible( true );
        getChild<LLUICtrl>("show_in_search_checkbox")->setEnabled( true );
        getChild<LLUICtrl>("sl_description_edit")->setEnabled( true );
        getChild<LLUICtrl>("2nd_life_pic")->setEnabled( true );
    }
}

void FSPanelProfile::updateButtons()
{
    bool is_buddy_online = LLAvatarTracker::instance().isBuddyOnline(getAvatarId());

    if(LLAvatarActions::isFriend(getAvatarId()))
    {
        getChildView("teleport")->setEnabled(is_buddy_online);
    }
    else
    {
        getChildView("teleport")->setEnabled(true);
    }

    bool enable_map_btn = (is_buddy_online && is_agent_mappable(getAvatarId())) || gAgent.isGodlike();
    getChildView("show_on_map_btn")->setEnabled(enable_map_btn);

    bool enable_block_btn = LLAvatarActions::canBlock(getAvatarId()) && !LLAvatarActions::isBlocked(getAvatarId());
    getChildView("block")->setVisible(enable_block_btn);

    bool enable_unblock_btn = LLAvatarActions::isBlocked(getAvatarId());
    getChildView("unblock")->setVisible(enable_unblock_btn);
}

void FSPanelProfile::onClickSetName()
{	
	LLAvatarNameCache::get(getAvatarId(), 
			boost::bind(&FSPanelProfile::onAvatarNameCacheSetName,
				this, _1, _2));	

	LLFirstUse::setDisplayName(false);
}

void FSPanelProfile::onAvatarNameCacheSetName(const LLUUID& agent_id, const LLAvatarName& av_name)
{
	if (av_name.mDisplayName.empty())
	{
		// something is wrong, tell user to try again later
		LLNotificationsUtil::add("SetDisplayNameFailedGeneric");
		return;		
	}

	llinfos << "name-change now " << LLDate::now() << " next_update "
		<< LLDate(av_name.mNextUpdate) << llendl;
	F64 now_secs = LLDate::now().secondsSinceEpoch();

	if (now_secs < av_name.mNextUpdate)
	{
		// if the update time is more than a year in the future, it means updates have been blocked
		// show a more general message
        const int YEAR = 60*60*24*365; 
		if (now_secs + YEAR < av_name.mNextUpdate)
		{
			LLNotificationsUtil::add("SetDisplayNameBlocked");
			return;
		}
	}
	
	LLFloaterReg::showInstance("display_name");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

FSPanelProfileWeb::FSPanelProfileWeb()
 : FSPanelProfileTab()
 , mWebBrowser(NULL)
{
}

FSPanelProfileWeb::~FSPanelProfileWeb()
{
}

void FSPanelProfileWeb::onOpen(const LLSD& key)
{
    FSPanelProfileTab::onOpen(key);
}

BOOL FSPanelProfileWeb::postBuild()
{
    getChild<LLUICtrl>("load")->setCommitCallback(boost::bind(&FSPanelProfileWeb::onCommitLoad, this, _1));

    getChild<LLUICtrl>("web_profile")->setCommitCallback(boost::bind(&FSPanelProfileWeb::onCommitWebProfile, this, _1));
    childSetVisible("web_profile", LLGridManager::getInstance()->isInSLMain() || LLGridManager::getInstance()->isInSLBeta());

    mWebBrowser = getChild<LLMediaCtrl>("profile_html");
    mWebBrowser->addObserver(this);

    getChild<LLLineEditor>("url_edit")->setEnabled( FALSE );

    return TRUE;
}

void FSPanelProfileWeb::processProperties(void* data, EAvatarProcessorType type)
{
    if(APT_PROPERTIES == type)
    {
        const LLAvatarData* avatar_data = static_cast<const LLAvatarData*>(data);
        if(avatar_data && getAvatarId() == avatar_data->avatar_id)
        {
            mURLHome = avatar_data->profile_url;
            getChild<LLUICtrl>("url_edit")->setValue(mURLHome);
            childSetEnabled("load", mURLHome.length() > 0);
            enableControls();
        }
    }
}

void FSPanelProfileWeb::apply(LLAvatarData* data)
{
    data->profile_url = getChild<LLUICtrl>("url_edit")->getValue().asString();
}

void FSPanelProfileWeb::onAvatarNameCache(const LLUUID& agent_id, const LLAvatarName& av_name)
{
    //[ADD: FIRE-2266: SJ] make sure username is always filled even when Displaynames are not enabled
    std::string username = av_name.mUsername;
    if (username.empty())
    {
        username = LLCacheName::buildUsername(av_name.mDisplayName);
    }

    //[ADD: FIRE-2266: SJ] Adding link to webprofiles on profile which opens Web Profiles in browser
    std::string url;
    if (LLGridManager::getInstance()->isInSLMain())
    {
        url = gSavedSettings.getString("WebProfileURL");
    }
    else if (LLGridManager::getInstance()->isInSLBeta())
    {
        url = gSavedSettings.getString("WebProfileNonProductionURL");
    }
    else
    {
        //OpenSimFIXME: get from grid - but how?
        // possibilities:     * grid_info  (profiles accessible outside the grid)
        //             * login message (profiles only within the grid)
        //            * capability (better for decentaliced environment)
        return;
    }
    LLSD subs;
    subs["AGENT_NAME"] = username;
    mURLWebProfile = LLWeb::expandURLSubstitutions(url,subs);

    childSetEnabled("web_profile", true);
    childSetVisible("profile_html",true);

    mPerformanceTimer.start();
    mWebBrowser->navigateTo( mURLWebProfile, "text/html" );
}

void FSPanelProfileWeb::onCommitLoad(LLUICtrl* ctrl)
{
    if (!mURLHome.empty())
    {
        LLSD::String valstr = ctrl->getValue().asString();
        if (valstr == "")
        {
            childSetVisible("profile_html",true);
            mPerformanceTimer.start();
            mWebBrowser->navigateTo( mURLHome, "text/html" );
        }
        else if (valstr == "popout")
        {
            // open in viewer's browser, new window
            LLWeb::loadURLInternal(mURLHome);
        }
        else if (valstr == "external")
        {
            // open in external browser
            LLWeb::loadURLExternal(mURLHome);
        }
    }
}

void FSPanelProfileWeb::onCommitWebProfile(LLUICtrl* ctrl)
{
    if (!mURLWebProfile.empty())
    {
        LLSD::String valstr = ctrl->getValue().asString();
        if (valstr == "")
        {
            childSetVisible("profile_html",true);
            mPerformanceTimer.start();
            mWebBrowser->navigateTo( mURLWebProfile, "text/html" );
        }
        else if (valstr == "popout")
        {
            // open the web profile floater
            LLAvatarActions::showProfileWeb(getAvatarId());
        }
        else if (valstr == "external")
        {
            // open in external browser
            LLWeb::loadURLExternal(mURLWebProfile);
        }
    }
}

void FSPanelProfileWeb::handleMediaEvent(LLPluginClassMedia* self, EMediaEvent event)
{
    switch(event)
    {
        case MEDIA_EVENT_STATUS_TEXT_CHANGED:
            childSetText("status_text", self->getStatusText() );
        break;

        case MEDIA_EVENT_LOCATION_CHANGED:
            // don't set this or user will set there url to profile url
            // when clicking ok on there own profile.
            // childSetText("url_edit", self->getLocation() );
        break;

        case MEDIA_EVENT_NAVIGATE_BEGIN:
        {
            if (mFirstNavigate)
            {
                mFirstNavigate = false;
            }
            else
            {
                mPerformanceTimer.start();
            }
        }
        break;

        case MEDIA_EVENT_NAVIGATE_COMPLETE:
        {
			LLStringUtil::format_map_t args;
			args["[TIME]"] = llformat("%.2f", mPerformanceTimer.getElapsedTimeF32());
            childSetText("status_text", getString("LoadTime", args));
        }
        break;

        default:
            // Having a default case makes the compiler happy.
        break;
    }
}

void FSPanelProfileWeb::enableControls()
{
    FSPanelProfileTab::enableControls();

    if (getAvatarId() == gAgent.getID())
    {
        getChild<LLLineEditor>("url_edit")->setEnabled( TRUE );
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const S32 WANT_CHECKS = 8;
static const S32 SKILL_CHECKS = 6;

FSPanelProfileInterests::FSPanelProfileInterests()
 : FSPanelProfileTab()
{
}

FSPanelProfileInterests::~FSPanelProfileInterests()
{
}

void FSPanelProfileInterests::onOpen(const LLSD& key)
{
    FSPanelProfileTab::onOpen(key);
}

BOOL FSPanelProfileInterests::postBuild()
{
    for (S32 i=0; i < WANT_CHECKS; ++i)
    {
        std::string check_name = llformat("chk%d", i);
        mWantChecks[i] = getChild<LLCheckBoxCtrl>(check_name);
    }

    for (S32 i=0; i < SKILL_CHECKS; ++i)
    {
        std::string check_name = llformat("schk%d", i);
        mSkillChecks[i] = getChild<LLCheckBoxCtrl>(check_name);
    }

    //FS:KC - Due to a bug with LLLineEditor, it cannot be disabled from XUI
    // It won't properly enable from code if it is.
    getChild<LLLineEditor>("want_to_edit")->setEnabled( FALSE );
    getChild<LLLineEditor>("skills_edit")->setEnabled( FALSE );
    getChild<LLLineEditor>("languages_edit")->setEnabled( FALSE );

    return TRUE;
}


void FSPanelProfileInterests::processProperties(void* data, EAvatarProcessorType type)
{
    if(APT_INTERESTS_INFO == type)
    {
        const FSInterestsData* interests_data = static_cast<const FSInterestsData*>(data);
        if (interests_data && getAvatarId() == interests_data->avatar_id)
        {
            for (S32 i=0; i < WANT_CHECKS; i++)
            {
                if (interests_data->want_to_mask & (1<<i))
                {
                    mWantChecks[i]->setValue(TRUE);
                }
                else
                {
                    mWantChecks[i]->setValue(FALSE);
                }
            }

            for (S32 i=0; i < SKILL_CHECKS; i++)
            {
                if (interests_data->skills_mask & (1<<i))
                {
                    mSkillChecks[i]->setValue(TRUE);
                }
                else
                {
                    mSkillChecks[i]->setValue(FALSE);
                }
            }

            childSetText("want_to_edit",    interests_data->want_to_text);
            childSetText("skills_edit",     interests_data->skills_text);
            childSetText("languages_edit",  interests_data->languages_text);

            enableControls();
        }
    }
}

void FSPanelProfileInterests::apply()
{
    if (getIsLoaded() && (getAvatarId() == gAgent.getID()))
    {
        FSInterestsData interests_data = FSInterestsData();

        interests_data.want_to_mask = 0;
        for (S32 i=0; i < WANT_CHECKS; i++)
        {
            if (mWantChecks[i]->getValue().asBoolean())
            {
                interests_data.want_to_mask |= (1<<i);
            }
        }

        interests_data.skills_mask = 0;
        for (S32 i=0; i < SKILL_CHECKS; i++)
        {
            if (mSkillChecks[i]->getValue().asBoolean())
            {
                interests_data.skills_mask |= (1<<i);
            }
        }

        interests_data.want_to_text = getChild<LLUICtrl>("want_to_edit")->getValue().asString();
        interests_data.skills_text = getChild<LLUICtrl>("skills_edit")->getValue().asString();
        interests_data.languages_text = getChild<LLUICtrl>("languages_edit")->getValue().asString();

        LLAvatarPropertiesProcessor::getInstance()->sendInterestsInfoUpdate(&interests_data);
    }

}

void FSPanelProfileInterests::enableControls()
{
    FSPanelProfileTab::enableControls();

    if (getAvatarId() == gAgent.getID())
    {
        getChild<LLLineEditor>("want_to_edit")->setEnabled( TRUE );
        getChild<LLLineEditor>("skills_edit")->setEnabled( TRUE );
        getChild<LLLineEditor>("languages_edit")->setEnabled( TRUE );

        for (S32 i=0; i < WANT_CHECKS; ++i)
        {
            mWantChecks[i]->setEnabled( TRUE );
        }

        for (S32 i=0; i < SKILL_CHECKS; ++i)
        {
            mSkillChecks[i]->setEnabled( TRUE );
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

FSPanelPick::FSPanelPick()
 : FSPanelProfileTab()
 , LLRemoteParcelInfoObserver()
 , mSnapshotCtrl(NULL)
 , mPickId(LLUUID::null)
 , mParcelId(LLUUID::null)
 , mRequestedId(LLUUID::null)
 , mLocationChanged(false)
 , mNewPick(false)
{
}

//static
FSPanelPick* FSPanelPick::create()
{
    FSPanelPick* panel = new FSPanelPick();
    panel->buildFromFile("panel_profile_pick.xml");
    return panel;
}

FSPanelPick::~FSPanelPick()
{
    if (mParcelId.notNull())
    {
        LLRemoteParcelInfoProcessor::getInstance()->removeObserver(mParcelId, this);
    }
}

void FSPanelPick::setAvatarId(const LLUUID& avatar_id)
{
    if(avatar_id.isNull())
    {
        return;
    }
    FSPanelProfileTab::setAvatarId(avatar_id);

    // creating new Pick
    if(getPickId().isNull())
    {
        mNewPick = true;

        setPosGlobal(gAgent.getPositionGlobal());

        LLUUID parcel_id = LLUUID::null, snapshot_id = LLUUID::null;
        std::string pick_name, pick_desc, region_name;

        LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
        if(parcel)
        {
            parcel_id = parcel->getID();
            pick_name = parcel->getName();
            pick_desc = parcel->getDesc();
            snapshot_id = parcel->getSnapshotID();
        }

        LLViewerRegion* region = gAgent.getRegion();
        if(region)
        {
            region_name = region->getName();
        }

        setParcelID(parcel_id);
        setPickName(pick_name.empty() ? region_name : pick_name);
        setPickDesc(pick_desc);
        setSnapshotId(snapshot_id);
        setPickLocation(createLocationText(getLocationNotice(), pick_name, region_name, getPosGlobal()));

        enableSaveButton(true);
    }
    else
    {
        LLAvatarPropertiesProcessor::getInstance()->sendPickInfoRequest(getAvatarId(), getPickId());

        LLAvatarPropertiesProcessor::instance().sendPickInfoRequest(getAvatarId(), getPickId());

        enableSaveButton(false);
    }

    resetDirty();

    if (getAvatarId() == gAgent.getID())
    {
        getChild<LLLineEditor>("pick_name")->setEnabled( TRUE );
        getChild<LLUICtrl>("pick_desc")->setEnabled( TRUE );
        getChild<LLUICtrl>("set_to_curr_location_btn")->setVisible( TRUE );
    }
	/*else
	{
		mSnapshotCtrl->setEnabled(FALSE);
	}*/
}

BOOL FSPanelPick::postBuild()
{
    mSnapshotCtrl = getChild<LLTextureCtrl>("pick_snapshot");
    mSnapshotCtrl->setCommitCallback(boost::bind(&FSPanelPick::onSnapshotChanged, this));

    childSetAction("teleport_btn", boost::bind(&FSPanelPick::onClickTeleport, this));
    childSetAction("show_on_map_btn", boost::bind(&FSPanelPick::onClickMap, this));

    childSetAction("set_to_curr_location_btn", boost::bind(&FSPanelPick::onClickSetLocation, this));
    childSetAction("save_changes_btn", boost::bind(&FSPanelPick::onClickSave, this));

    LLLineEditor* line_edit = getChild<LLLineEditor>("pick_name");
    line_edit->setKeystrokeCallback(boost::bind(&FSPanelPick::onPickChanged, this, _1), NULL);
    line_edit->setEnabled( FALSE );

    LLTextEditor* text_edit = getChild<LLTextEditor>("pick_desc");
    text_edit->setKeystrokeCallback(boost::bind(&FSPanelPick::onPickChanged, this, _1));

    return TRUE;
}

void FSPanelPick::processProperties(void* data, EAvatarProcessorType type)
{
    if(APT_PICK_INFO != type)
    {
        return;
    }
    LLPickData* pick_info = static_cast<LLPickData*>(data);
    if(!pick_info
        || pick_info->creator_id != getAvatarId()
        || pick_info->pick_id != getPickId())
    {
        return;
    }

    mParcelId = pick_info->parcel_id;
    setSnapshotId(pick_info->snapshot_id);
	if (getAvatarId() != gAgent.getID())
	{
		mSnapshotCtrl->setEnabled(FALSE);
	}
    setPickName(pick_info->name);
    setPickDesc(pick_info->desc);
    setPosGlobal(pick_info->pos_global);

    // Send remote parcel info request to get parcel name and sim (region) name.
    sendParcelInfoRequest();

    // *NOTE dzaporozhan
    // We want to keep listening to APT_PICK_INFO because user may
    // edit the Pick and we have to update Pick info panel.
    // revomeObserver is called from onClickBack

    enableControls();
}

void FSPanelPick::setSnapshotId(const LLUUID& id)
{
    mSnapshotCtrl->setImageAssetID(id);
    mSnapshotCtrl->setValid(TRUE);
}

void FSPanelPick::setPickName(const std::string& name)
{
    getChild<LLUICtrl>("pick_name")->setValue(name);
}

const std::string FSPanelPick::getPickName()
{
    return getChild<LLUICtrl>("pick_name")->getValue().asString();
}

void FSPanelPick::setPickDesc(const std::string& desc)
{
    getChild<LLUICtrl>("pick_desc")->setValue(desc);
}

void FSPanelPick::setPickLocation(const std::string& location)
{
    getChild<LLUICtrl>("pick_location")->setValue(location);
}

void FSPanelPick::onClickMap()
{
    LLFloaterWorldMap::getInstance()->trackLocation(getPosGlobal());
    LLFloaterReg::showInstance("world_map", "center");
}

void FSPanelPick::onClickTeleport()
{
    if (!getPosGlobal().isExactlyZero())
    {
        gAgent.teleportViaLocation(getPosGlobal());
        LLFloaterWorldMap::getInstance()->trackLocation(getPosGlobal());
    }
}

void FSPanelPick::enableSaveButton(bool enable)
{
    getChildView("save_changes_btn")->setEnabled(enable);
    getChild<LLUICtrl>("save_changes_btn")->setVisible(enable);
}

void FSPanelPick::onSnapshotChanged()
{
    enableSaveButton(true);
}

void FSPanelPick::onPickChanged(LLUICtrl* ctrl)
{
    enableSaveButton(isDirty());
}

void FSPanelPick::resetDirty()
{
    LLPanel::resetDirty();

    getChild<LLLineEditor>("pick_name")->resetDirty();
    getChild<LLTextEditor>("pick_desc")->resetDirty();
    mSnapshotCtrl->resetDirty();
    mLocationChanged = false;
}

BOOL FSPanelPick::isDirty() const
{
    if( mNewPick
        || LLPanel::isDirty()
        || mLocationChanged
        || mSnapshotCtrl->isDirty()
        || getChild<LLLineEditor>("pick_name")->isDirty()
        || getChild<LLTextEditor>("pick_desc")->isDirty())
    {
        return TRUE;
    }
    return FALSE;
}

void FSPanelPick::onClickSetLocation()
{
    // Save location for later use.
    setPosGlobal(gAgent.getPositionGlobal());

    std::string parcel_name, region_name;

    LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
    if (parcel)
    {
        mParcelId = parcel->getID();
        parcel_name = parcel->getName();
    }

    LLViewerRegion* region = gAgent.getRegion();
    if(region)
    {
        region_name = region->getName();
    }

    setPickLocation(createLocationText(getLocationNotice(), parcel_name, region_name, getPosGlobal()));

    mLocationChanged = true;
    enableSaveButton(TRUE);
}

void FSPanelPick::onClickSave()
{
    sendUpdate();

    mLocationChanged = false;
}

void FSPanelPick::apply()
{
    if ((mNewPick || getIsLoaded()) && isDirty())
    {
        sendUpdate();
    }
}

std::string FSPanelPick::getLocationNotice()
{
    static std::string notice = getString("location_notice");
    return notice;
}

void FSPanelPick::sendParcelInfoRequest()
{
    if (mParcelId != mRequestedId)
    {
        LLRemoteParcelInfoProcessor::getInstance()->addObserver(mParcelId, this);
        LLRemoteParcelInfoProcessor::getInstance()->sendParcelInfoRequest(mParcelId);

        mRequestedId = mParcelId;
    }
}

void FSPanelPick::processParcelInfo(const LLParcelData& parcel_data)
{
    setPickLocation(createLocationText(LLStringUtil::null, parcel_data.name, parcel_data.sim_name, getPosGlobal()));

    // We have received parcel info for the requested ID so clear it now.
    mRequestedId.setNull();

    if (mParcelId.notNull())
    {
        LLRemoteParcelInfoProcessor::getInstance()->removeObserver(mParcelId, this);
    }
}

void FSPanelPick::sendUpdate()
{
    LLPickData pick_data;

    // If we don't have a pick id yet, we'll need to generate one,
    // otherwise we'll keep overwriting pick_id 00000 in the database.
    if (getPickId().isNull())
    {
        getPickId().generate();
    }

    pick_data.agent_id = gAgent.getID();
    pick_data.session_id = gAgent.getSessionID();
    pick_data.pick_id = getPickId();
    pick_data.creator_id = gAgent.getID();;

    //legacy var  need to be deleted
    pick_data.top_pick = FALSE;
    pick_data.parcel_id = mParcelId;
    pick_data.name = getPickName();
    pick_data.desc = getChild<LLUICtrl>("pick_desc")->getValue().asString();
    pick_data.snapshot_id = mSnapshotCtrl->getImageAssetID();
    pick_data.pos_global = getPosGlobal();
    pick_data.sort_order = 0;
    pick_data.enabled = TRUE;

    LLAvatarPropertiesProcessor::instance().sendPickInfoUpdate(&pick_data);

    if(mNewPick)
    {
        // Assume a successful create pick operation, make new number of picks
        // available immediately. Actual number of picks will be requested in
        // LLAvatarPropertiesProcessor::sendPickInfoUpdate and updated upon server respond.
        LLAgentPicksInfo::getInstance()->incrementNumberOfPicks();
    }
}

// static
std::string FSPanelPick::createLocationText(const std::string& owner_name, const std::string& original_name, const std::string& sim_name, const LLVector3d& pos_global)
{
    std::string location_text;
    location_text.append(owner_name);
    if (!original_name.empty())
    {
        if (!location_text.empty()) location_text.append(", ");
        location_text.append(original_name);

    }
    if (!sim_name.empty())
    {
        if (!location_text.empty()) location_text.append(", ");
        location_text.append(sim_name);
    }

    if (!location_text.empty()) location_text.append(" ");

    if (!pos_global.isNull())
    {
        S32 region_x = llround((F32)pos_global.mdV[VX]) % REGION_WIDTH_UNITS;
        S32 region_y = llround((F32)pos_global.mdV[VY]) % REGION_WIDTH_UNITS;
        S32 region_z = llround((F32)pos_global.mdV[VZ]);
        location_text.append(llformat(" (%d, %d, %d)", region_x, region_y, region_z));
    }
    return location_text;
}


//////////////////////////////////////////////////////////////////////////

FSPanelProfilePicks::FSPanelProfilePicks()
 : FSPanelProfileTab()
{
}

FSPanelProfilePicks::~FSPanelProfilePicks()
{
}

void FSPanelProfilePicks::onOpen(const LLSD& key)
{
    FSPanelProfileTab::onOpen(key);

    if (getAvatarId() == gAgent.getID())
    {
        getChild<LLUICtrl>("new_btn")->setVisible( true );
        getChild<LLUICtrl>("new_btn")->setEnabled( true );

        getChild<LLUICtrl>("delete_btn")->setVisible( true );
        getChild<LLUICtrl>("delete_btn")->setEnabled( true );
    }

    updateData();
}

BOOL FSPanelProfilePicks::postBuild()
{
    mTabContainer = getChild<LLTabContainer>("tab_picks");
    mNoItemsLabel = getChild<LLUICtrl>("picks_panel_text");

    childSetAction("new_btn", boost::bind(&FSPanelProfilePicks::onClickNewBtn, this));
    childSetAction("delete_btn", boost::bind(&FSPanelProfilePicks::onClickDelete, this));

    return TRUE;
}

void FSPanelProfilePicks::onClickNewBtn()
{
    mNoItemsLabel->setVisible(false);
    FSPanelPick* pick_panel = FSPanelPick::create();
    pick_panel->setAvatarId(getAvatarId());
    mTabContainer->addTabPanel(
        LLTabContainer::TabPanelParams().
        panel(pick_panel).
        select_tab(true));
}

void FSPanelProfilePicks::onClickDelete()
{
    FSPanelPick* pick_panel = (FSPanelPick*) mTabContainer->getCurrentPanel();
    if (pick_panel)
    {
        LLUUID pick_id = pick_panel->getPickId();
        if (!pick_id.isNull())
        {
            LLSD args;
            args["PICK"] = pick_panel->getPickName();
            LLSD payload;
            payload["pick_id"] = pick_id;
            LLNotificationsUtil::add("DeleteAvatarPick", args, payload, boost::bind(&FSPanelProfilePicks::callbackDeletePick, this, _1, _2));
            return;
        }
    }
}

bool FSPanelProfilePicks::callbackDeletePick(const LLSD& notification, const LLSD& response)
{
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);

    if (0 == option)
    {
        LLUUID pick_id = notification["payload"]["pick_id"].asUUID();
        LLAvatarPropertiesProcessor::instance().sendPickDelete(pick_id);
        getChildView("new_btn")->setEnabled(!LLAgentPicksInfo::getInstance()->isPickLimitReached());

        for (S32 tab_idx = 0; tab_idx < mTabContainer->getTabCount(); ++tab_idx)
        {
            FSPanelPick* pick_panel = (FSPanelPick*)mTabContainer->getPanelByIndex(tab_idx);
            if (pick_panel)
            {
                if (pick_id == pick_panel->getPickId())
                {
                    mTabContainer->removeTabPanel(pick_panel);
                }
            }
        }
    }

    return false;
}

void FSPanelProfilePicks::processProperties(void* data, EAvatarProcessorType type)
{
    if(APT_PICKS == type)
    {
        LLAvatarPicks* avatar_picks = static_cast<LLAvatarPicks*>(data);
        if(avatar_picks && getAvatarId() == avatar_picks->target_id)
        {

            LLUUID selected_id = LLUUID::null;
            if (mTabContainer->getTabCount() > 0)
            {
                FSPanelPick* active_pick_panel = (FSPanelPick*) mTabContainer->getCurrentPanel();
                if (active_pick_panel)
                {
                    selected_id = active_pick_panel->getPickId();
                }
            }

            mTabContainer->deleteAllTabs();

            LLAvatarPicks::picks_list_t::const_iterator it = avatar_picks->picks_list.begin();
            for(; avatar_picks->picks_list.end() != it; ++it)
            {
                LLUUID pick_id = it->first;
                std::string pick_name = it->second;

                FSPanelPick* pick_panel = FSPanelPick::create();

                pick_panel->setPickId(pick_id);
                pick_panel->setPickName(pick_name);
                pick_panel->setAvatarId(getAvatarId());

                mTabContainer->addTabPanel(
                    LLTabContainer::TabPanelParams().
                    panel(pick_panel).
                    select_tab(selected_id == pick_id).
                    label(pick_name));
            }

            getChildView("new_btn")->setEnabled(!LLAgentPicksInfo::getInstance()->isPickLimitReached());

            bool no_data = !mTabContainer->getTabCount();
            mNoItemsLabel->setVisible(no_data);
            if (no_data)
            {
                if(getAvatarId() == gAgent.getID())
                {
                    mNoItemsLabel->setValue(LLTrans::getString("NoPicksText"));
                }
                else
                {
                    mNoItemsLabel->setValue(LLTrans::getString("NoAvatarPicksText"));
                }
            }
            else if (selected_id.isNull())
            {
                mTabContainer->selectFirstTab();
            }

            enableControls();
        }
    }
}

void FSPanelProfilePicks::apply()
{
    if (getIsLoaded())
    {
        for (S32 tab_idx = 0; tab_idx < mTabContainer->getTabCount(); ++tab_idx)
        {
            FSPanelPick* pick_panel = (FSPanelPick*)mTabContainer->getPanelByIndex(tab_idx);
            if (pick_panel)
            {
                pick_panel->apply();
            }
        }
    }
}

void FSPanelProfilePicks::updateData()
{
    if (getAvatarId().notNull())
    {
        mNoItemsLabel->setValue(LLTrans::getString("PicksClassifiedsLoadingText"));
        mNoItemsLabel->setVisible(TRUE);

        LLAvatarPropertiesProcessor::getInstance()->sendAvatarPicksRequest(getAvatarId());
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

FSPanelProfileFirstLife::FSPanelProfileFirstLife()
 : FSPanelProfileTab()
{
}

FSPanelProfileFirstLife::~FSPanelProfileFirstLife()
{
}

void FSPanelProfileFirstLife::onOpen(const LLSD& key)
{
    FSPanelProfileTab::onOpen(key);
}

void FSPanelProfileFirstLife::processProperties(void* data, EAvatarProcessorType type)
{
    if(APT_PROPERTIES == type)
    {
        const LLAvatarData* avatar_data = static_cast<const LLAvatarData*>(data);
        if(avatar_data && getAvatarId() == avatar_data->avatar_id)
        {
            getChild<LLUICtrl>("fl_description_edit")->setValue(avatar_data->fl_about_text);
            getChild<LLUICtrl>("real_world_pic")->setValue(avatar_data->fl_image_id);
            enableControls();
        }
    }
}

void FSPanelProfileFirstLife::apply(LLAvatarData* data)
{
    data->fl_image_id = getChild<LLTextureCtrl>("real_world_pic")->getImageAssetID();
    data->fl_about_text = getChild<LLUICtrl>("fl_description_edit")->getValue().asString();
}

void FSPanelProfileFirstLife::enableControls()
{
    FSPanelProfileTab::enableControls();

    if (getAvatarId() == gAgent.getID())
    {
        getChild<LLUICtrl>("fl_description_edit")->setEnabled( true );
        getChild<LLUICtrl>("real_world_pic")->setEnabled( true );
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

FSPanelAvatarNotes::FSPanelAvatarNotes()
: FSPanelProfileTab()
{

}

void FSPanelAvatarNotes::updateData()
{
    LLAvatarPropertiesProcessor::getInstance()->
        sendAvatarNotesRequest(getAvatarId());
}

BOOL FSPanelAvatarNotes::postBuild()
{
    childSetCommitCallback("status_check", boost::bind(&FSPanelAvatarNotes::onCommitRights, this), NULL);
    childSetCommitCallback("map_check", boost::bind(&FSPanelAvatarNotes::onCommitRights, this), NULL);
    childSetCommitCallback("objects_check", boost::bind(&FSPanelAvatarNotes::onCommitRights, this), NULL);

    LLTextEditor* te = getChild<LLTextEditor>("notes_edit");
    te->setCommitCallback(boost::bind(&FSPanelAvatarNotes::onCommitNotes,this));
    te->setCommitOnFocusLost(TRUE);

    return TRUE;
}

void FSPanelAvatarNotes::onOpen(const LLSD& key)
{
    FSPanelProfileTab::onOpen(key);

    fillRightsData();

    updateData();
}

void FSPanelAvatarNotes::apply()
{
    if (getIsLoaded())
    {
        //likely overkill
        onCommitNotes();
    }
}

void FSPanelAvatarNotes::fillRightsData()
{
    getChild<LLUICtrl>("status_check")->setValue(FALSE);
    getChild<LLUICtrl>("map_check")->setValue(FALSE);
    getChild<LLUICtrl>("objects_check")->setValue(FALSE);

    const LLRelationship* relation = LLAvatarTracker::instance().getBuddyInfo(getAvatarId());
    // If true - we are viewing friend's profile, enable check boxes and set values.
    if(relation)
    {
        S32 rights = relation->getRightsGrantedTo();

        getChild<LLUICtrl>("status_check")->setValue(LLRelationship::GRANT_ONLINE_STATUS & rights ? TRUE : FALSE);
        getChild<LLUICtrl>("map_check")->setValue(LLRelationship::GRANT_MAP_LOCATION & rights ? TRUE : FALSE);
        getChild<LLUICtrl>("objects_check")->setValue(LLRelationship::GRANT_MODIFY_OBJECTS & rights ? TRUE : FALSE);

    }

    enableCheckboxes(NULL != relation);
}

void FSPanelAvatarNotes::onCommitNotes()
{
    std::string notes = getChild<LLUICtrl>("notes_edit")->getValue().asString();
    LLAvatarPropertiesProcessor::getInstance()-> sendNotes(getAvatarId(),notes);
}

void FSPanelAvatarNotes::rightsConfirmationCallback(const LLSD& notification,
        const LLSD& response, S32 rights)
{
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    if (option == 0)
    {
        LLAvatarPropertiesProcessor::getInstance()->sendFriendRights(
                getAvatarId(), rights);
    }
    else
    {
        getChild<LLUICtrl>("objects_check")->setValue(
                getChild<LLUICtrl>("objects_check")->getValue().asBoolean() ? FALSE : TRUE);
    }
}

void FSPanelAvatarNotes::confirmModifyRights(bool grant, S32 rights)
// AO: If this is modified, also modify LLPanelAvatar::ConfirmModifyRights
{
    LLSD args;
    args["NAME"] = LLSLURL("agent", getAvatarId(), "displayname").getSLURLString();

    if (grant)
    {
        LLNotificationsUtil::add("GrantModifyRights", args, LLSD(),
                boost::bind(&FSPanelAvatarNotes::rightsConfirmationCallback, this,
                        _1, _2, rights));
    }
    else
    {
        LLNotificationsUtil::add("RevokeModifyRights", args, LLSD(),
                boost::bind(&FSPanelAvatarNotes::rightsConfirmationCallback, this,
                        _1, _2, rights));
    }
}

void FSPanelAvatarNotes::onCommitRights()
{
    const LLRelationship* buddy_relationship =
        LLAvatarTracker::instance().getBuddyInfo(getAvatarId());

    if (NULL == buddy_relationship)
    {
        // Lets have a warning log message instead of having a crash. EXT-4947.
        llwarns << "Trying to modify rights for non-friend avatar. Skipped." << llendl;
        return;
    }


    S32 rights = 0;

    if(getChild<LLUICtrl>("status_check")->getValue().asBoolean())
        rights |= LLRelationship::GRANT_ONLINE_STATUS;
    if(getChild<LLUICtrl>("map_check")->getValue().asBoolean())
        rights |= LLRelationship::GRANT_MAP_LOCATION;
    if(getChild<LLUICtrl>("objects_check")->getValue().asBoolean())
        rights |= LLRelationship::GRANT_MODIFY_OBJECTS;

    bool allow_modify_objects = getChild<LLUICtrl>("objects_check")->getValue().asBoolean();

    // if modify objects checkbox clicked
    if (buddy_relationship->isRightGrantedTo(
            LLRelationship::GRANT_MODIFY_OBJECTS) != allow_modify_objects)
    {
        confirmModifyRights(allow_modify_objects, rights);
    }
    // only one checkbox can trigger commit, so store the rest of rights
    else
    {
        LLAvatarPropertiesProcessor::getInstance()->sendFriendRights(
                        getAvatarId(), rights);
    }
}

void FSPanelAvatarNotes::processProperties(void* data, EAvatarProcessorType type)
{
    if(APT_NOTES == type)
    {
        LLAvatarNotes* avatar_notes = static_cast<LLAvatarNotes*>(data);
        if(avatar_notes && getAvatarId() == avatar_notes->target_id)
        {
            getChild<LLUICtrl>("notes_edit")->setValue(avatar_notes->notes);
            getChildView("notes edit")->setEnabled(true);
            enableControls();

            LLAvatarPropertiesProcessor::getInstance()->removeObserver(getAvatarId(),this);
        }
    }
}

void FSPanelAvatarNotes::enableCheckboxes(bool enable)
{
    getChildView("status_check")->setEnabled(enable);
    getChildView("map_check")->setEnabled(enable);
    getChildView("objects_check")->setEnabled(enable);
}

FSPanelAvatarNotes::~FSPanelAvatarNotes()
{
    if(getAvatarId().notNull())
    {
        LLAvatarTracker::instance().removeParticularFriendObserver(getAvatarId(), this);
    }
}

// virtual, called by LLAvatarTracker
void FSPanelAvatarNotes::changed(U32 mask)
{
    // update rights to avoid have checkboxes enabled when friendship is terminated. EXT-4947.
    fillRightsData();
}

void FSPanelAvatarNotes::setAvatarId(const LLUUID& id)
{
    if(id.notNull())
    {
        if(getAvatarId().notNull())
        {
            LLAvatarTracker::instance().removeParticularFriendObserver(getAvatarId(), this);
        }
        FSPanelProfileTab::setAvatarId(id);
        LLAvatarTracker::instance().addParticularFriendObserver(getAvatarId(), this);
    }
}

// eof
