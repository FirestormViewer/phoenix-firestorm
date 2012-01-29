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

// LLCommon
#include "llavatarconstants.h" //AVATAR_ONLINE
#include "llavatarnamecache.h"

// LLUI
#include "lltextbox.h"
#include "llgrouplist.h"

// Newview
#include "llagent.h" //gAgent
#include "llavatarpropertiesprocessor.h"
#include "llcallingcard.h"

class AvatarStatusObserver : public LLAvatarPropertiesObserver
{
public:
	AvatarStatusObserver(FSFloaterProfile* profile_floater)
	{
		mProfileFloater = profile_floater;
        LLAvatarPropertiesProcessor::instance().addObserver(mProfileFloater->getAvatarId(), this);
	}

	~AvatarStatusObserver()
	{
		LLAvatarPropertiesProcessor::instance().removeObserver(mProfileFloater->getAvatarId(), this);
	}

	void processProperties(void* data, EAvatarProcessorType type)
	{
        if(APT_PROPERTIES == type)
        {
            const LLAvatarData* avatar_data = static_cast<const LLAvatarData*>(data);
            if(avatar_data && mProfileFloater->getAvatarId() == avatar_data->avatar_id)
            {
                mProfileFloater->processProfileProperties(avatar_data);
            }
        }
        else if(APT_GROUPS == type)
        {
            LLAvatarGroups* avatar_groups = static_cast<LLAvatarGroups*>(data);
            if(avatar_groups && mProfileFloater->getAvatarId() == avatar_groups->avatar_id)
            {
                mProfileFloater->processGroupProperties(avatar_groups);
            }
        }
	}

private:
	FSFloaterProfile* mProfileFloater;
};

FSFloaterProfile::FSFloaterProfile(const LLSD& key)
 : LLFloater(key)
 , mAvatarId(LLUUID::null)
 , mStatusText(NULL)
 , mAvatarStatusObserver(NULL)
 , mIsFriend(FALSE)
{
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

	if(!id.notNull()) return;

    setAvatarId(id);
    

	// Update the avatar name.
	LLAvatarNameCache::get(getAvatarId(), boost::bind(&FSFloaterProfile::onAvatarNameCache, this, _1, _2));

	// LLPanelProfile::onOpen(key);
    //process tab open cmd here
    
    updateData();
}

void FSFloaterProfile::setAvatarId(const LLUUID& avatar_id)
{
    mAvatarId = avatar_id;
    
    const LLRelationship* relationship = LLAvatarTracker::instance().getBuddyInfo(getAvatarId());
	if (NULL != relationship)
    {
        mIsFriend = TRUE;
        updateOnlineStatus();
    }

    mAvatarStatusObserver = new AvatarStatusObserver(this);
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

void FSFloaterProfile::updateData()
{
	if (getAvatarId().notNull())
	{
		LLAvatarPropertiesProcessor::getInstance()->
			sendAvatarPropertiesRequest(getAvatarId());
		LLAvatarPropertiesProcessor::getInstance()->
			sendAvatarGroupsRequest(getAvatarId());
	}
}

void FSFloaterProfile::processProfileProperties(const LLAvatarData* avatar_data)
{
	if (!mIsFriend)
	{
		// this is non-friend avatar. Status will be updated from LLAvatarPropertiesProcessor.
		// in FSFloaterProfile::processOnlineStatus()

		// subscribe observer to get online status. Request will be sent by LLPanelAvatarProfile itself.
		// do not subscribe for friend avatar because online status can be wrong overridden
		// via LLAvatarData::flags if Preferences: "Only Friends & Groups can see when I am online" is set.
		processOnlineStatus(avatar_data->flags & AVATAR_ONLINE);
	}

	// fillCommonData(avatar_data);

	// fillPartnerData(avatar_data);

	// fillAccountStatus(avatar_data);
}

void FSFloaterProfile::processGroupProperties(const LLAvatarGroups* avatar_groups)
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

bool FSFloaterProfile::isGrantedToSeeOnlineStatus(bool online)
{
    // set text box visible to show online status for non-friends who has not set in Preferences
    // "Only Friends & Groups can see when I am online"
	if (!mIsFriend)
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
void FSFloaterProfile::updateOnlineStatus()
{
	if (!mIsFriend) return;
	// For friend let check if he allowed me to see his status
	const LLRelationship* relationship = LLAvatarTracker::instance().getBuddyInfo(getAvatarId());
	bool online = relationship->isOnline();
	processOnlineStatus(online);
}

void FSFloaterProfile::processOnlineStatus(bool online)
{
    mStatusText->setVisible(isGrantedToSeeOnlineStatus(online));

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
