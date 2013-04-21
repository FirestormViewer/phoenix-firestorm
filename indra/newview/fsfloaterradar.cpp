/**
 * @file fsfloaterradar.cpp
 * @brief Firestorm radar floater implementation
 *
 * $LicenseInfo:firstyear=2013&license=viewerlgpl$
 * Copyright (c) 2013 Ansariel Hiller @ Second Life
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
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "fsfloaterradar.h"

#include "llnotificationsutil.h"
#include "llfiltereditor.h"
#include "llavataractions.h"
#include "rlvhandler.h"


FSFloaterRadar::FSFloaterRadar(const LLSD& key)
	:	LLFloater(key),
		mFilterSubString(LLStringUtil::null),
		mFilterSubStringOrig(LLStringUtil::null),
		mFilterEditor(NULL),
		mRadarPanel(NULL),
		mRadarChangeConnection()
{
	mCommitCallbackRegistrar.add("People.addFriend", boost::bind(&FSFloaterRadar::onAddFriendButtonClicked, this));
}

FSFloaterRadar::~FSFloaterRadar()
{
	if (LLVoiceClient::instanceExists())
	{
		LLVoiceClient::getInstance()->removeObserver(this);
	}

	if (mRadarChangeConnection.connected())
	{
		mRadarChangeConnection.disconnect();
	}
}

BOOL FSFloaterRadar::postBuild()
{
	mFilterEditor = getChild<LLFilterEditor>("filter_input");
	mFilterEditor->setCommitCallback(boost::bind(&FSFloaterRadar::onFilterEdit, this, _2));

	mProfileButton = getChild<LLButton>("view_profile_btn");
	mIMButton = getChild<LLButton>("im_btn");
	mCallButton = getChild<LLButton>("call_btn");
	mTeleportButton = getChild<LLButton>("teleport_btn");		
	mShareButton = getChild<LLButton>("share_btn");

	mProfileButton->setCommitCallback(boost::bind(&FSFloaterRadar::onViewProfileButtonClicked, this));
	mIMButton->setCommitCallback(boost::bind(&FSFloaterRadar::onImButtonClicked, this));
	mCallButton->setCommitCallback(boost::bind(&FSFloaterRadar::onCallButtonClicked, this));
	mTeleportButton->setCommitCallback(boost::bind(&FSFloaterRadar::onTeleportButtonClicked, this));
	mShareButton->setCommitCallback(boost::bind(&FSFloaterRadar::onShareButtonClicked, this));

	mRadarPanel = findChild<FSPanelRadar>("panel_radar");
	if (!mRadarPanel)
	{
		return FALSE;
	}
	mRadarChangeConnection = mRadarPanel->setChangeCallback(boost::bind(&FSFloaterRadar::updateButtons, this));

	LLVoiceClient::getInstance()->addObserver(this);
	
	// call this method in case some list is empty and buttons can be in inconsistent state
	updateButtons();

	return TRUE;
}

// virtual
void FSFloaterRadar::onOpen(const LLSD& key)
{
	// Fill radar with most recent data so we don't have a blank window until next radar update
	mRadarPanel->requestUpdate();
	LLFloater::onOpen(key);
}

// virtual
void FSFloaterRadar::onChange(EStatusType status, const std::string &channelURI, bool proximal)
{
	if (status == STATUS_JOINING || status == STATUS_LEFT_CHANNEL)
	{
		return;
	}
	
	updateButtons();
}

void FSFloaterRadar::updateButtons()
{
	LLUUID selected_id;

	uuid_vec_t selected_uuids;
	mRadarPanel->getCurrentItemIDs(selected_uuids);
	
	bool item_selected = (selected_uuids.size() == 1);
	bool multiple_selected = (selected_uuids.size() >= 1);
	bool enable_calls = LLVoiceClient::getInstance()->isVoiceWorking() && LLVoiceClient::getInstance()->voiceEnabled();

// [RLVa:KB] - Checked: 2010-06-04 (RLVa-1.2.2a) | Modified: RLVa-1.2.0d
	if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
	{
		item_selected = multiple_selected = false;
	}
// [/RLBa:KB]

	mProfileButton->setEnabled(item_selected);
	mShareButton->setEnabled(item_selected);
	mIMButton->setEnabled(multiple_selected); // allow starting the friends conference for multiple selection
	mCallButton->setEnabled(multiple_selected && enable_calls);
	mTeleportButton->setEnabled(multiple_selected /* && LLAvatarActions::canOfferTeleport(selected_uuids) */ ); // LO - Dont block the TP button at all.
}

void FSFloaterRadar::onFilterEdit(const std::string& search_string)
{
	mFilterSubStringOrig = search_string;
	LLStringUtil::trimHead(mFilterSubStringOrig);
	// Searches are case-insensitive
	std::string search_upper = mFilterSubStringOrig;
	LLStringUtil::toUpper(search_upper);

	if (mFilterSubString == search_upper)
	{
		return;
	}

	mFilterSubString = search_upper;

	// Apply new filter.
	mRadarPanel->setFilter(mFilterSubStringOrig);
}

void FSFloaterRadar::onViewProfileButtonClicked()
{
	LLUUID id = mRadarPanel->getCurrentItemID();
	LLAvatarActions::showProfile(id);
}

void FSFloaterRadar::onAddFriendButtonClicked()
{
	LLUUID id = mRadarPanel->getCurrentItemID();
	if (id.notNull())
	{
		LLAvatarActions::requestFriendshipDialog(id);
	}
}

void FSFloaterRadar::onImButtonClicked()
{
	uuid_vec_t selected_uuids;
	mRadarPanel->getCurrentItemIDs(selected_uuids);
	if ( selected_uuids.size() == 1 )
	{
		// if selected only one person then start up IM
		LLAvatarActions::startIM(selected_uuids.at(0));
	}
	else if ( selected_uuids.size() > 1 )
	{
		// for multiple selection start up friends conference
		LLAvatarActions::startConference(selected_uuids);
	}
}

void FSFloaterRadar::onCallButtonClicked()
{
	uuid_vec_t selected_uuids;
	mRadarPanel->getCurrentItemIDs(selected_uuids);

	if (selected_uuids.size() == 1)
	{
		// initiate a P2P voice chat with the selected user
		LLAvatarActions::startCall(mRadarPanel->getCurrentItemID());
	}
	else if (selected_uuids.size() > 1)
	{
		// initiate an ad-hoc voice chat with multiple users
		LLAvatarActions::startAdhocCall(selected_uuids);
	}
}

void FSFloaterRadar::onTeleportButtonClicked()
{
	uuid_vec_t selected_uuids;
	mRadarPanel->getCurrentItemIDs(selected_uuids);
	LLAvatarActions::offerTeleport(LLAvatarActions::canOfferTeleport(selected_uuids));
}

void FSFloaterRadar::onShareButtonClicked()
{
	LLAvatarActions::share(mRadarPanel->getCurrentItemID());
}
