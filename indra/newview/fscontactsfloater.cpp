/** 
 * @file 
 * @brief 
 *
 * $LicenseInfo:firstyear=2011&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2011, The Phoenix Viewer Project, Inc.
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
 * $/LicenseInfo$
 */


#include "llviewerprecompiledheaders.h"

#include "fscontactsfloater.h"
#include "llfloaterreg.h"
#include "llfloater.h"

#include "llavatarlist.h"
#include "llavatarlistitem.h"
#include "llcallingcard.h"			// for LLAvatarTracker
#include "llfriendcard.h"
#include "llpanelpeople.h"
#include "llsidetray.h"


/** Compares avatar items by online status, then by name */
class LLAvatarItemStatusComparator : public LLAvatarItemComparator
{
public:
	LLAvatarItemStatusComparator() {};

protected:
	/**
	 * @return true if item1 < item2, false otherwise
	 */
	virtual bool doCompare(const LLAvatarListItem* item1, const LLAvatarListItem* item2) const
	{
		LLAvatarTracker& at = LLAvatarTracker::instance();
		bool online1 = at.isBuddyOnline(item1->getAvatarId());
		bool online2 = at.isBuddyOnline(item2->getAvatarId());

		if (online1 == online2)
		{
			std::string name1 = item1->getAvatarName();
			std::string name2 = item2->getAvatarName();

			LLStringUtil::toUpper(name1);
			LLStringUtil::toUpper(name2);

			return name1 < name2;
		}
		
		return online1 > online2; 
	}
};

static const LLAvatarItemStatusComparator STATUS_COMPARATOR;


//
// FSFloaterContacts
//

FSFloaterContacts::FSFloaterContacts(const LLSD& seed)
	: LLFloater(seed),
	mFriendList(NULL)
{
	//mFriendListUpdater = new LLPanelPeople::LLFriendListUpdater(boost::bind(&LLPanelPeople::updateFriendList,	this));

	//mFactoryMap["friends_panel"] = LLCallbackMap(FSFloaterContacts::createFriendsPanel, NULL);
	//mFactoryMap["groups_panel"] = LLCallbackMap(FSFloaterContacts::createGroupsPanel, NULL);
}

FSFloaterContacts::~FSFloaterContacts()
{
	//delete mFriendListUpdater;
}

BOOL FSFloaterContacts::postBuild()
{
	mFriendsTab = getChild<LLPanel>("friends_panel");
	// updater is active only if panel is visible to user.
	//friends_tab->setVisibleCallback(boost::bind(&LLPanelPeople::Updater::setActive, LLPanelPeople::mFriendListUpdater, _2));
	mFriendList = mFriendsTab->getChild<LLAvatarList>("avatars_all");
	mFriendList->setNoItemsCommentText(getString("no_friends"));
	mFriendList->setShowIcons("FriendsListShowIcons");
	mFriendList->showPermissions(TRUE);
	mFriendList->setComparator(&STATUS_COMPARATOR);
	mFriendList->sort();
	
	//LLPanelPeople* pPeoplePanel = dynamic_cast<LLPanelPeople*>(LLSideTray::getInstance()->getPanel("panel_people"));
	//pPeoplePanel->setContactsFriendList(friends_tab, mFriendList);
	
	//mFriendList->setRefreshCompleteCallback(boost::bind(&LLPanelPeople::onFriendListRefreshComplete, this, _1, _2));
	
	return TRUE;
}

void FSFloaterContacts::onOpen(const LLSD& key)
{
	if (key.asString() == "friends")
	{
		childShowTab("friends_and_groups", "friends_panel");
	}
	else if (key.asString() == "groups")
	{
		childShowTab("friends_and_groups", "groups_panel");
	}
}

//static
void* FSFloaterContacts::createFriendsPanel(void* data)
{
	//return new FSPanelFriends();
	return NULL;
}

//static
void* FSFloaterContacts::createGroupsPanel(void* data)
{
	//return new LLPanelGroups();
	return NULL;
}

//static
FSFloaterContacts* FSFloaterContacts::findInstance()
{
	return LLFloaterReg::findTypedInstance<FSFloaterContacts>("imcontacts");
}

FSFloaterContacts* FSFloaterContacts::getInstance()
{
	return LLFloaterReg::getTypedInstance<FSFloaterContacts>("imcontacts");
}

// EOF
