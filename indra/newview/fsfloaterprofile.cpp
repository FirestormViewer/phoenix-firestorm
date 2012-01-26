/** 
 * @file fsfloaterprofile.cpp
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

#include "fsfloaterprofile.h"

#include "llavatarconstants.h"
#include "llavatarnamecache.h"
#include "llavatarpropertiesprocessor.h"
#include "llcallingcard.h"
#include "lltextbox.h"


class AvatarStatusObserver : public LLAvatarPropertiesObserver
{
public:
	AvatarStatusObserver(FSFloaterProfile* profile_view)
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
	FSFloaterProfile* mProfileView;
// [SL:KB] - Patch : UI-ProfileGroupFloater | Checked: 2010-11-28 (Catznip-2.4.0g) | Added: Catznip-2.4.0g
	LLUUID				mAvatarId;
// [/SL:KB]
};

FSFloaterProfile::FSFloaterProfile(const LLSD& key)
 : LLFloater(key)
 , mAvatarId(LLUUID::null)
 , mStatusText(NULL)
 , mAvatarStatusObserver(NULL)
{
    mAvatarStatusObserver = new AvatarStatusObserver(this);
}

FSFloaterProfile::~FSFloaterProfile()
{
    delete mAvatarStatusObserver;
}

/*virtual*/ 
void FSFloaterProfile::onOpen(const LLSD& key)
{
    LLUUID id;
	if(key.has("id"))
	{
		id = key["id"];
	}

	if(id.notNull() && getAvatarId() != id)
	{
		setAvatarId(id);

		// clear name fields, which might have old data
		// getChild<LLUICtrl>("complete_name")->setValue( LLSD() );
		// getChild<LLUICtrl>("display_name")->setValue( LLSD() );
		// getChild<LLUICtrl>("user_name")->setValue( LLSD() );
		// getChild<LLUICtrl>("user_key")->setValue( LLSD() );
		//[ADD: FIRE-2266: SJ] Adding link to webprofiles on profile which opens Web Profiles in browser
		// getChild<LLUICtrl>("web_profile_text")->setValue( LLSD() );
	}

	// Update the avatar name.
	LLAvatarNameCache::get(getAvatarId(), boost::bind(&FSFloaterProfile::onAvatarNameCache, this, _1, _2));

	updateOnlineStatus();

	// LLPanelProfile::onOpen(key);
    //process tab open cmd here
}

BOOL FSFloaterProfile::postBuild()
{

	mStatusText = getChild<LLTextBox>("status");
	mStatusText->setVisible(false);

	// childSetCommitCallback("copy_to_clipboard",boost::bind(&FSFloaterProfile::onCopyToClipboard,this),NULL);

	// set up callback for copy URI button
	// childSetCommitCallback("copy_uri",boost::bind(&FSFloaterProfile::onCopyURI,this),NULL);

	// set up callback for invite to group button
	// childSetCommitCallback("group_invite",boost::bind(&FSFloaterProfile::onGroupInvite,this),NULL);

	// setTitle(getLabel());

	return TRUE;
}

bool FSFloaterProfile::isGrantedToSeeOnlineStatus()
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
void FSFloaterProfile::updateOnlineStatus()
{
	// set text box visible to show online status for non-friends who has not set in Preferences
	// "Only Friends & Groups can see when I am online"
	mStatusText->setVisible(TRUE);

	const LLRelationship* relationship = LLAvatarTracker::instance().getBuddyInfo(getAvatarId());
	if (NULL == relationship)
	{
		// this is non-friend avatar. Status will be updated from LLAvatarPropertiesProcessor.
		// in FSFloaterProfile::processOnlineStatus()

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

void FSFloaterProfile::processOnlineStatus(bool online)
{
	std::string status = getString(online ? "status_online" : "status_offline");

	mStatusText->setValue(status);
}

void FSFloaterProfile::onAvatarNameCache(const LLUUID& agent_id,
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

	setTitle(av_name.getCompleteName() + " - " + getLabel());
}

// eof
