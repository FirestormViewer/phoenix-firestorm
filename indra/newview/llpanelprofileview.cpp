/** 
* @file llpanelprofileview.cpp
* @brief Side tray "Profile View" panel
*
* $LicenseInfo:firstyear=2009&license=viewerlgpl$
* Second Life Viewer Source Code
* Copyright (C) 2010, Linden Research, Inc.
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
* Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
* $/LicenseInfo$
*/

#include "llviewerprecompiledheaders.h"

#include "llpanelprofileview.h"

#include "llavatarconstants.h"
#include "llavatarnamecache.h"	// IDEVO
#include "llavataractions.h"
#include "llclipboard.h"
#include "lluserrelations.h"

#include "llavatarpropertiesprocessor.h"
#include "llcallingcard.h"
// [SL:KB] - Patch : UI-ProfileGroupFloater 
#include "llfloater.h"
// [/SL:KB]
#include "llpanelavatar.h"
#include "llpanelpicks.h"
#include "llpanelprofile.h"
#include "llsidetraypanelcontainer.h"

static LLRegisterPanelClassWrapper<LLPanelProfileView> t_panel_target_profile("panel_profile_view");

static std::string PANEL_NOTES = "panel_notes";
static const std::string PANEL_PROFILE = "panel_profile";
static const std::string PANEL_PICKS = "panel_picks";
static std::string PANEL_FIRST = "panel_profile_firstlife";


class AvatarStatusObserver : public LLAvatarPropertiesObserver
{
public:
	AvatarStatusObserver(LLPanelProfileView* profile_view)
	{
		mProfileView = profile_view;
	}

// [SL:KB] - Patch : UI-ProfileGroupFloater | Checked: 2010-11-28 (Catznip-2.4.0g) | Added: Catznip-2.4.0g
	~AvatarStatusObserver()
	{
		if (mAvatarId.notNull())
			LLAvatarPropertiesProcessor::instance().removeObserver(mAvatarId, this);
	}
// [/SL:KB]

	void processProperties(void* data, EAvatarProcessorType type)
	{
		if(APT_PROPERTIES != type) return;
		const LLAvatarData* avatar_data = static_cast<const LLAvatarData*>(data);
		if(avatar_data && mProfileView->getAvatarId() == avatar_data->avatar_id)
		{
			llinfos << "processing online status in the AvatarStatusObserver, will remove it." << llendl;
			mProfileView->processOnlineStatus(avatar_data->flags & AVATAR_ONLINE);
//			LLAvatarPropertiesProcessor::instance().removeObserver(mProfileView->getAvatarId(), this);
		}

// [SL:KB] - Patch : UI-ProfileGroupFloater | Checked: 2010-11-28 (Catznip-2.4.0g) | Added: Catznip-2.4.0g
		// Profile view may have switched to a new avatar already so this needs to be outside the check above
		LLAvatarPropertiesProcessor::instance().removeObserver(mAvatarId, this);
		mAvatarId.setNull();
// [/SL:KB]
	}

	void subscribe()
	{
//		LLAvatarPropertiesProcessor::instance().addObserver(mProfileView->getAvatarId(), this);
// [SL:KB] - Patch : UI-ProfileGroupFloater | Checked: 2010-11-28 (Catznip-2.4.0g) | Added: Catznip-2.4.0g
		if (mAvatarId.notNull())
			LLAvatarPropertiesProcessor::instance().removeObserver(mProfileView->getAvatarId(), this);
		mAvatarId = mProfileView->getAvatarId();
		LLAvatarPropertiesProcessor::instance().addObserver(mAvatarId, this);
// [/SL:KB]
	}

private:
	LLPanelProfileView* mProfileView;
// [SL:KB] - Patch : UI-ProfileGroupFloater | Checked: 2010-11-28 (Catznip-2.4.0g) | Added: Catznip-2.4.0g
	LLUUID				mAvatarId;
// [/SL:KB]
};

LLPanelProfileView::LLPanelProfileView()
:	LLPanelProfile()
,	mStatusText(NULL)
,	mAvatarStatusObserver(NULL)
{
	mAvatarStatusObserver = new AvatarStatusObserver(this);
}

LLPanelProfileView::~LLPanelProfileView(void)
{
	delete mAvatarStatusObserver;
}

/*virtual*/ 
void LLPanelProfileView::onOpen(const LLSD& key)
{
	llinfos << "opening profile" << llendl;

	LLUUID id;
	if(key.has("id"))
	{
		id = key["id"];
	}

	if(id.notNull() && getAvatarId() != id)
	{
		setAvatarId(id);

		// clear name fields, which might have old data
		getChild<LLUICtrl>("complete_name")->setValue( LLSD() );
		getChild<LLUICtrl>("display_name")->setValue( LLSD() );
		getChild<LLUICtrl>("user_name")->setValue( LLSD() );
		getChild<LLUICtrl>("user_key")->setValue( LLSD() );
		//[ADD: FIRE-2266: SJ] Adding link to webprofiles on profile which opens Web Profiles in browser
		getChild<LLUICtrl>("web_profile_text")->setValue( LLSD() );
	}

	// Update the avatar name.
	LLAvatarNameCache::get(getAvatarId(),
		boost::bind(&LLPanelProfileView::onAvatarNameCache, this, _1, _2));

	updateOnlineStatus();

	LLPanelProfile::onOpen(key);
}

BOOL LLPanelProfileView::postBuild()
{
	LLPanelProfile::postBuild();

	getTabContainer()[PANEL_NOTES] = findChild<LLPanelAvatarNotes>(PANEL_NOTES);
	getTabContainer()[PANEL_FIRST] = findChild<LLPanelAvatarFirst>(PANEL_FIRST);
	
	//*TODO remove this, according to style guide we don't use status combobox
	getTabContainer()[PANEL_PROFILE]->getChildView("online_me_status_text")->setVisible( FALSE);
	getTabContainer()[PANEL_PROFILE]->getChildView("status_combo")->setVisible( FALSE);

	mStatusText = getChild<LLTextBox>("status");
	mStatusText->setVisible(false);

//	childSetCommitCallback("back",boost::bind(&LLPanelProfileView::onBackBtnClick,this),NULL);
	childSetCommitCallback("copy_to_clipboard",boost::bind(&LLPanelProfileView::onCopyToClipboard,this),NULL);
// [SL:KB] - Patch : UI-ProfileGroupFloater 

	// set up callback for copy URI button
	childSetCommitCallback("copy_uri",boost::bind(&LLPanelProfileView::onCopyURI,this),NULL);

	// set up callback for invite to group button
	childSetCommitCallback("group_invite",boost::bind(&LLPanelProfileView::onGroupInvite,this),NULL);

	LLFloater* pParentView = dynamic_cast<LLFloater*>(getParent());
	if (!pParentView)
	{
		childSetCommitCallback("back", boost::bind(&LLPanelProfileView::onBackBtnClick,this),NULL);
	}
	else
	{
		pParentView->setTitle(getLabel());
		childSetVisible("back", false);
		// -TT - we might want to remove this. Maybe.
		// HACK-Catznip: we got rid of the back button so we want to line up the name controls with the rest
		LLUICtrl* pCtrls[] = 
			{
				getChild<LLUICtrl>("complete_name", FALSE),
				getChild<LLUICtrl>("display_name", FALSE),
				getChild<LLUICtrl>("user_name", FALSE),
				getChild<LLUICtrl>("user_key", FALSE),
				getChild<LLUICtrl>("display_name_label", FALSE),
				getChild<LLUICtrl>("solo_username_label", FALSE),
				getChild<LLUICtrl>("user_name_small", FALSE),
				getChild<LLUICtrl>("user_label", FALSE)
			};
		int dX = (NULL != pCtrls[0]) ? 10 - pCtrls[0]->getRect().mLeft : 0;
		for (int idxCtrl = 0; idxCtrl < sizeof(pCtrls) / sizeof(LLUICtrl*); idxCtrl++)
		{
			if (pCtrls[idxCtrl])
				pCtrls[idxCtrl]->translate(dX, 0);
		}
	}
// [/SL:KB]
		
	return TRUE;
}


//private

void LLPanelProfileView::onBackBtnClick()
{
	// Set dummy value to make picks panel dirty, 
	// This will make Picks reload on next open.
	getTabContainer()[PANEL_PICKS]->setValue(LLSD());

	LLSideTrayPanelContainer* parent = dynamic_cast<LLSideTrayPanelContainer*>(getParent());
	if(parent)
	{
		parent->openPreviousPanel();
	}
}

void LLPanelProfileView::onCopyToClipboard()
{
	std::string name = getChild<LLUICtrl>("display_name")->getValue().asString() + " (" + getChild<LLUICtrl>("user_name")->getValue().asString() + ")";
	gClipboard.copyFromString(utf8str_to_wstring(name));
}

bool LLPanelProfileView::isGrantedToSeeOnlineStatus()
{
	const LLRelationship* relationship = LLAvatarTracker::instance().getBuddyInfo(getAvatarId());
	if (NULL == relationship)
		return false;

	// *NOTE: GRANT_ONLINE_STATUS is always set to false while changing any other status.
	// When avatar disallow me to see her online status processOfflineNotification Message is received by the viewer
	// see comments for ChangeUserRights template message. EXT-453.
	// If GRANT_ONLINE_STATUS flag is changed it will be applied when viewer restarts. EXT-3880
	return relationship->isRightGrantedFrom(LLRelationship::GRANT_ONLINE_STATUS);
}

// method was disabled according to EXT-2022. Re-enabled & improved according to EXT-3880
void LLPanelProfileView::updateOnlineStatus()
{
	// set text box visible to show online status for non-friends who has not set in Preferences
	// "Only Friends & Groups can see when I am online"
	mStatusText->setVisible(TRUE);

	const LLRelationship* relationship = LLAvatarTracker::instance().getBuddyInfo(getAvatarId());
	if (NULL == relationship)
	{
		// this is non-friend avatar. Status will be updated from LLAvatarPropertiesProcessor.
		// in LLPanelProfileView::processOnlineStatus()

		// subscribe observer to get online status. Request will be sent by LLPanelAvatarProfile itself.
		// do not subscribe for friend avatar because online status can be wrong overridden
		// via LLAvatarData::flags if Preferences: "Only Friends & Groups can see when I am online" is set.
		llinfos << "subscribing to AvatarStatusObserver for the profile" << llendl;
		mAvatarStatusObserver->subscribe();
		return;
	}
	// For friend let check if he allowed me to see his status

	// status should only show if viewer has permission to view online/offline. EXT-453, EXT-3880
	mStatusText->setVisible(isGrantedToSeeOnlineStatus());

	bool online = relationship->isOnline();
	processOnlineStatus(online);
}

void LLPanelProfileView::processOnlineStatus(bool online)
{
	std::string status = getString(online ? "status_online" : "status_offline");

	mStatusText->setValue(status);
}

void LLPanelProfileView::onAvatarNameCache(const LLUUID& agent_id,
										   const LLAvatarName& av_name)
{
	getChild<LLUICtrl>("complete_name")->setValue( av_name.getCompleteName() );
	getChild<LLUICtrl>("display_name")->setValue( av_name.mDisplayName );
	getChild<LLUICtrl>("user_name")->setValue( av_name.mUsername );
	getChild<LLUICtrl>("user_key")->setValue( agent_id.asString() );
	getChild<LLUICtrl>("copy_uri")->setEnabled( true );
	//[ADD: FIRE-2266: SJ] make sure username is always filled even when Displaynames are not enabled
	std::string username = av_name.mUsername;
	if (username.empty())
	{
		username = LLCacheName::buildUsername(av_name.mDisplayName);
	}

	//[ADD: FIRE-2266: SJ] Adding link to webprofiles on profile which opens Web Profiles in browser
	getChild<LLUICtrl>("web_profile_text")->setValue( "https://my.secondlife.com/" + username );

#if 0
	getChild<LLUICtrl>("user_name")->setValue( av_name.mDisplayName );
	getChild<LLUICtrl>("user_name_small")->setValue( av_name.mDisplayName );
	getChild<LLUICtrl>("user_slid")->setValue( av_name.mUsername );

	// show smaller display name if too long to display in regular size
	if (getChild<LLTextBox>("user_name")->getTextPixelWidth() > getChild<LLTextBox>("user_name")->getRect().getWidth())
	{
		getChild<LLUICtrl>("user_name_small")->setVisible( true );
		getChild<LLUICtrl>("user_name")->setVisible( false );
	}
	else
	{
		getChild<LLUICtrl>("user_name_small")->setVisible( false );
		getChild<LLUICtrl>("user_name")->setVisible( true );
	}
#endif

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
// [SL:KB] - Patch : UI-ProfileGroupFloater 
	LLFloater* pParentView = dynamic_cast<LLFloater*>(getParent());
	if (pParentView)
		pParentView->setTitle(av_name.getCompleteName() + " - " + getLabel());
// [/SL:KB]
}

// Copy URI button callback
void LLPanelProfileView::onCopyURI()
{
    std::string name = "secondlife:///app/agent/"+getChild<LLUICtrl>("user_key")->getValue().asString()+"/about";
    gClipboard.copyFromString(utf8str_to_wstring(name));
}

// Group invite button callback
void LLPanelProfileView::onGroupInvite()
{
    LLAvatarActions::inviteToGroup(getAvatarId());
}

// EOF
