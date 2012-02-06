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
#include "llmenubutton.h"
#include "lltextbox.h"
#include "lltexteditor.h"
#include "lltexturectrl.h"
#include "lltoggleablemenu.h"
#include "llgrouplist.h"

// Newview
#include "fsdata.h"
#include "llagent.h" //gAgent
#include "llavataractions.h"
#include "llavatarpropertiesprocessor.h"
#include "llcallingcard.h"
#include "llfloaterreg.h"
#include "llnotificationsutil.h"
#include "llnotificationsutil.h"
#include "lltrans.h"
#include "llvoiceclient.h"
#include "llgroupactions.h"
#include "llviewercontrol.h"
#include "llviewernetwork.h" //LLGridManager

static LLRegisterPanelClassWrapper<FSPanelProfile> t_panel_profile("panel_profile_secondlife");
static LLRegisterPanelClassWrapper<FSPanelProfileWeb> t_panel_web("panel_profile_web");
static LLRegisterPanelClassWrapper<FSPanelProfileFirstLife> t_panel_firstlife("panel_profile_firstlife");
static LLRegisterPanelClassWrapper<FSPanelAvatarNotes> t_panel_notes("panel_profile_notes");

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
}

void FSPanelProfileTab::onMapButtonClick()
{
	LLAvatarActions::showOnMap(getAvatarId());
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
// [SL:KB] - Patch : UI-ProfileGroupFloater | Checked: 2010-11-26 (Catznip-2.4.0f) | Added: Catznip-2.4.0f
	if(LLVoiceClient::instanceExists())
	{
		LLVoiceClient::getInstance()->removeObserver((LLVoiceClientStatusObserver*)this);
	}
// [/SL:KB]
}

BOOL FSPanelProfile::postBuild()
{
	mStatusText = getChild<LLTextBox>("status");
	mStatusText->setVisible(false);

	LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
	registrar.add("Profile.AddFriend",  boost::bind(&FSPanelProfile::onAddFriendButtonClick, this));
	registrar.add("Profile.IM",  boost::bind(&FSPanelProfile::onIMButtonClick, this));
	registrar.add("Profile.Call",  boost::bind(&FSPanelProfile::onCallButtonClick, this));
	registrar.add("Profile.Teleport",  boost::bind(&FSPanelProfile::onTeleportButtonClick, this));
	registrar.add("Profile.ShowOnMap",  boost::bind(&FSPanelProfile::onMapButtonClick, this));
	registrar.add("Profile.Pay",  boost::bind(&FSPanelProfile::pay, this));
	registrar.add("Profile.Share", boost::bind(&FSPanelProfile::share, this));
	registrar.add("Profile.BlockUnblock", boost::bind(&FSPanelProfile::toggleBlock, this));
	registrar.add("Profile.Kick", boost::bind(&FSPanelProfile::kick, this));
	registrar.add("Profile.Freeze", boost::bind(&FSPanelProfile::freeze, this));
	registrar.add("Profile.Unfreeze", boost::bind(&FSPanelProfile::unfreeze, this));
	registrar.add("Profile.CSR", boost::bind(&FSPanelProfile::csr, this));
	registrar.add("Profile.CopyNameToClipboard", boost::bind(&FSPanelProfile::onCopyToClipboard, this));
	registrar.add("Profile.CopyURI", boost::bind(&FSPanelProfile::onCopyURI, this));

	LLUICtrl::EnableCallbackRegistry::ScopedRegistrar enable;
	enable.add("Profile.EnableAddFriend", boost::bind(&FSPanelProfile::enableAddFriend, this));
	enable.add("Profile.EnableCall", boost::bind(&FSPanelProfile::enableCall, this));
	enable.add("Profile.EnableShowOnMap", boost::bind(&FSPanelProfile::enableShowOnMap, this));
	enable.add("Profile.EnableTeleport", boost::bind(&FSPanelProfile::enableTeleport, this));
	enable.add("Profile.EnableGod", boost::bind(&enable_god));
	enable.add("Profile.EnableBlock", boost::bind(&FSPanelProfile::enableBlock, this));
	enable.add("Profile.EnableUnblock", boost::bind(&FSPanelProfile::enableUnblock, this));

	LLGroupList* group_list = getChild<LLGroupList>("group_list");
	group_list->setDoubleClickCallback(boost::bind(&FSPanelProfile::openGroupProfile, this));
	group_list->setReturnCallback(boost::bind(&FSPanelProfile::openGroupProfile, this));

	LLToggleableMenu* profile_menu = LLUICtrlFactory::getInstance()->createFromFile<LLToggleableMenu>("menu_profile_overflow.xml", gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance());
        getChild<LLMenuButton>("overflow_btn")->setMenu(profile_menu, LLMenuButton::MP_TOP_RIGHT);

	LLVoiceClient::getInstance()->addObserver((LLVoiceClientStatusObserver*)this);

	return TRUE;
}

void FSPanelProfile::onOpen(const LLSD& key)
{
	FSPanelProfileTab::onOpen(key);

	LLAvatarPropertiesProcessor::getInstance()->addObserver(getAvatarId(),this);

	//Disable "Add Friend" button for friends.
	getChildView("add_friend")->setEnabled(!LLAvatarActions::isFriend(getAvatarId()));
    
    updateData();
}

void FSPanelProfile::updateData()
{
	if (getAvatarId().notNull())
	{
		LLAvatarPropertiesProcessor::getInstance()->sendAvatarPropertiesRequest(getAvatarId());
		LLAvatarPropertiesProcessor::getInstance()->sendAvatarGroupsRequest(getAvatarId());
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
	if (!LLAvatarActions::isFriend(getAvatarId()))
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

    mGroups.clear();
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
	// getChild<LLUICtrl>("display_name")->setValue( av_name.mDisplayName );
	// getChild<LLUICtrl>("user_name")->setValue( av_name.mUsername );
	getChild<LLUICtrl>("user_key")->setValue( agent_id.asString() );
	// getChild<LLUICtrl>("copy_uri")->setEnabled( true );

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
		// possibilities: 	* grid_info  (profiles accessible outside the grid)
		// 			* login message (profiles only within the grid)
		//			* capability (better for decentaliced environment)
	}
	LLSD subs;
	subs["AGENT_NAME"] = username;
	url = LLWeb::expandURLSubstitutions(url,subs);
	LLStringUtil::toLower(url);
	getChild<LLUICtrl>("web_profile_text")->setValue( url );

	if (LLAvatarNameCache::useDisplayNames())
	{
		getChild<LLUICtrl>("user_label")->setVisible( true );
		getChild<LLUICtrl>("user_name")->setVisible( true );
		getChild<LLUICtrl>("display_name_label")->setVisible( true );
		getChild<LLUICtrl>("copy_to_clipboard")->setVisible( true );
		getChild<LLUICtrl>("copy_to_clipboard")->setEnabled( true );
		getChild<LLUICtrl>("solo_username_label")->setVisible( false );
	}
	else
	{
		getChild<LLUICtrl>("user_label")->setVisible( false );
		getChild<LLUICtrl>("user_name")->setVisible( false );
		getChild<LLUICtrl>("display_name_label")->setVisible( false );
		getChild<LLUICtrl>("copy_to_clipboard")->setVisible( false );
		getChild<LLUICtrl>("copy_to_clipboard")->setEnabled( false );
		getChild<LLUICtrl>("solo_username_label")->setVisible( true );
	}

    //KC: This is very hacky, but getParent() would not work
    // LLFloater* pParentView = LLFloaterReg::getInstance("floater_profile", LLSD().with("id", agent_id));
	// if (pParentView)
		// pParentView->setTitle(av_name.getCompleteName());
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
	std::string register_date = getString("RegisterDateFormat", args);
	getChild<LLUICtrl>("register_date")->setValue(register_date );
	getChild<LLUICtrl>("sl_description_edit")->setValue(avatar_data->about_text);
	getChild<LLUICtrl>("2nd_life_pic")->setValue(avatar_data->image_id);

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
	
	bool enable_block_btn = LLAvatarActions::canBlock(getAvatarId()) && !LLAvatarActions::isBlocked(getAvatarId());
	getChildView("block")->setVisible(enable_block_btn);
	
	bool enable_unblock_btn = LLAvatarActions::isBlocked(getAvatarId());
	getChildView("unblock")->setVisible(enable_unblock_btn);
}

bool FSPanelProfile::enableAddFriend()
{
	return !LLAvatarTracker::instance().isBuddyOnline(getAvatarId());
}

bool FSPanelProfile::enableCall()
{
	return mVoiceStatus == TRUE;
}

bool FSPanelProfile::enableShowOnMap()
{
	bool is_buddy_online = LLAvatarTracker::instance().isBuddyOnline(getAvatarId());

	bool enable_map_btn = (is_buddy_online && is_agent_mappable(getAvatarId()))
		|| gAgent.isGodlike();
	return enable_map_btn;
}

bool FSPanelProfile::enableTeleport()
{
	bool is_buddy_online = LLAvatarTracker::instance().isBuddyOnline(getAvatarId());
	
	if(LLAvatarActions::isFriend(getAvatarId()))
	{
		return is_buddy_online;
	}
	else
	{
		return true;
	}
}

bool FSPanelProfile::enableBlock()
{
	return LLAvatarActions::canBlock(getAvatarId()) && !LLAvatarActions::isBlocked(getAvatarId());
}

bool FSPanelProfile::enableUnblock()
{
	return LLAvatarActions::isBlocked(getAvatarId());
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

void FSPanelProfile::onShareButtonClick()
{
	//*TODO not implemented
}

void FSPanelProfile::onCopyToClipboard()
{
	// std::string name = getChild<LLUICtrl>("display_name")->getValue().asString() + " (" + getChild<LLUICtrl>("user_name")->getValue().asString() + ")";
	std::string name = getChild<LLUICtrl>("complete_name")->getValue().asString();
	gClipboard.copyFromString(utf8str_to_wstring(name));
}

void FSPanelProfile::onCopyURI()
{
    std::string name = "secondlife:///app/agent/"+getChild<LLUICtrl>("user_key")->getValue().asString()+"/about";
    gClipboard.copyFromString(utf8str_to_wstring(name));
}

void FSPanelProfile::onGroupInvite()
{
    LLAvatarActions::inviteToGroup(getAvatarId());
}

// virtual, called by LLAvatarTracker
void FSPanelProfile::changed(U32 mask)
{
	updateOnlineStatus();
}

// virtual, called by LLVoiceClient
void FSPanelProfile::onChange(EStatusType status, const std::string &channelURI, bool proximal)
{
	if(status == STATUS_JOINING || status == STATUS_LEFT_CHANNEL)
	{
		return;
	}

	// getChildView("call")->setEnabled(LLVoiceClient::getInstance()->voiceEnabled() && LLVoiceClient::getInstance()->isVoiceWorking());
	mVoiceStatus == LLVoiceClient::getInstance()->voiceEnabled() && LLVoiceClient::getInstance()->isVoiceWorking();
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
		LLAvatarTracker::instance().addParticularFriendObserver(getAvatarId(), this);
	}
}

bool FSPanelProfile::isGrantedToSeeOnlineStatus(bool online)
{
    // set text box visible to show online status for non-friends who has not set in Preferences
    // "Only Friends & Groups can see when I am online"
	if (!LLAvatarActions::isFriend(getAvatarId()))
    {
        return online;
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
    mStatusText->setVisible(isGrantedToSeeOnlineStatus(online));

	std::string status = getString(online ? "status_online" : "status_offline");

	mStatusText->setValue(status);
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

BOOL FSPanelProfileWeb::postBuild()
{
    getChild<LLUICtrl>("load")->setCommitCallback(boost::bind(&FSPanelProfileWeb::onCommitLoad, this, _1));
    
    getChild<LLUICtrl>("web_profile")->setCommitCallback(boost::bind(&FSPanelProfileWeb::onCommitWebProfile, this, _1));
	childSetVisible("web_profile", LLGridManager::getInstance()->isInSLMain() || LLGridManager::getInstance()->isInSLBeta());

	mWebBrowser = getChild<LLMediaCtrl>("profile_html");
	mWebBrowser->addObserver(this);

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
		}
	}
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
		// possibilities: 	* grid_info  (profiles accessible outside the grid)
		// 			* login message (profiles only within the grid)
		//			* capability (better for decentaliced environment)
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
            // open in viewer's browser, new window
            LLWeb::loadURLInternal(mURLWebProfile);
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
			childSetText("status_text", llformat("Load Time: %.2f seconds", mPerformanceTimer.getElapsedTimeF32()));
		}
		break;
		
		default:
			// Having a default case makes the compiler happy.
		break;
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

void FSPanelProfileFirstLife::processProperties(void* data, EAvatarProcessorType type)
{
	if(APT_PROPERTIES == type)
	{
		const LLAvatarData* avatar_data = static_cast<const LLAvatarData*>(data);
		if(avatar_data && getAvatarId() == avatar_data->avatar_id)
		{
            getChild<LLUICtrl>("fl_description_edit")->setValue(avatar_data->fl_about_text);
            getChild<LLUICtrl>("real_world_pic")->setValue(avatar_data->fl_image_id);
		}
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
