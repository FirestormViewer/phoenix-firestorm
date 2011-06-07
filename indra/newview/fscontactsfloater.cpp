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

// libs
#include "llagent.h"
#include "llavatarname.h"
#include "llfloaterreg.h"
#include "llfloater.h"
#include "lltabcontainer.h"

#include "llavataractions.h"
#include "llavatarlist.h"
#include "llavatarlistitem.h"
#include "llcallingcard.h"			// for LLAvatarTracker
#include "llfloateravatarpicker.h"
#include "llfriendcard.h"
#include "llgroupactions.h"
#include "llgrouplist.h"
#include "llimfloatercontainer.h"
#include "llsidetray.h"
#include "llstartup.h"
#include "llviewercontrol.h"

static const std::string FRIENDS_TAB_NAME	= "friends_panel";
static const std::string GROUP_TAB_NAME		= "groups_panel";

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

			if (item1->getShowingBothNames() &&
				gSavedSettings.getBOOL("FSSortContactsByUserName"))
			{
				// The tooltip happens to have the username
				//  in it, so we'll just use that. Why store
				//  it twice or parse it back out? -- TS
				name1 = item1->getAvatarToolTip();
				name2 = item2->getAvatarToolTip();
			}

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
	mTabContainer(NULL),
	mFriendList(NULL),
	mGroupList(NULL)
{
}

FSFloaterContacts::~FSFloaterContacts()
{
}

BOOL FSFloaterContacts::postBuild()
{
	mTabContainer = getChild<LLTabContainer>("friends_and_groups");

	mFriendsTab = getChild<LLPanel>(FRIENDS_TAB_NAME);
	mFriendList = mFriendsTab->getChild<LLAvatarList>("avatars_all");
	mFriendList->setNoItemsCommentText(getString("no_friends"));
	mFriendList->setShowIcons("FriendsListShowIcons");
	mFriendList->showPermissions(TRUE);
	mFriendList->setComparator(&STATUS_COMPARATOR);
	
	mFriendList->setRefreshCompleteCallback(boost::bind(&FSFloaterContacts::sortFriendList, this));
	mFriendList->setItemDoubleClickCallback(boost::bind(&FSFloaterContacts::onAvatarListDoubleClicked, this, _1));
	mFriendList->setReturnCallback(boost::bind(&FSFloaterContacts::onImButtonClicked, this));
	
	mFriendsTab->childSetAction("im_btn", boost::bind(&FSFloaterContacts::onImButtonClicked, this));
	mFriendsTab->childSetAction("profile_btn", boost::bind(&FSFloaterContacts::onViewProfileButtonClicked, this));
	mFriendsTab->childSetAction("offer_teleport_btn", boost::bind(&FSFloaterContacts::onTeleportButtonClicked, this));
	mFriendsTab->childSetAction("pay_btn", boost::bind(&FSFloaterContacts::onPayButtonClicked, this));
	
	mFriendsTab->childSetAction("remove_btn", boost::bind(&FSFloaterContacts::onDeleteFriendButtonClicked, this));
	mFriendsTab->childSetAction("add_btn", boost::bind(&FSFloaterContacts::onAddFriendWizButtonClicked, this));
	
	mGroupsTab = getChild<LLPanel>(GROUP_TAB_NAME);
	mGroupList = mGroupsTab->getChild<LLGroupList>("group_list");
	mGroupList->setNoItemsMsg(getString("no_groups_msg"));
	mGroupList->setNoFilteredItemsMsg(getString("no_filtered_groups_msg"));
	
	mGroupList->setDoubleClickCallback(boost::bind(&FSFloaterContacts::onGroupChatButtonClicked, this));
	mGroupList->setCommitCallback(boost::bind(&FSFloaterContacts::updateButtons, this));
	mGroupList->setReturnCallback(boost::bind(&FSFloaterContacts::onGroupChatButtonClicked, this));
	
	mGroupsTab->childSetAction("chat_btn", boost::bind(&FSFloaterContacts::onGroupChatButtonClicked,	this));
	mGroupsTab->childSetAction("info_btn", boost::bind(&FSFloaterContacts::onGroupInfoButtonClicked,	this));
	mGroupsTab->childSetAction("activate_btn", boost::bind(&FSFloaterContacts::onGroupActivateButtonClicked,	this));
	mGroupsTab->childSetAction("leave_btn",	boost::bind(&FSFloaterContacts::onGroupLeaveButtonClicked,	this));
	mGroupsTab->childSetAction("create_btn",	boost::bind(&FSFloaterContacts::onGroupCreateButtonClicked,	this));
	mGroupsTab->childSetAction("search_btn",	boost::bind(&FSFloaterContacts::onGroupSearchButtonClicked,	this));
	
	return TRUE;
}

void FSFloaterContacts::updateButtons()
{
	std::vector<LLPanel*> items;
	mGroupList->getItems(items);

	mGroupsTab->getChild<LLUICtrl>("groupcount")->setTextArg("[COUNT]", llformat("%d", gAgent.mGroups.count())); //  items.end()));//
	mGroupsTab->getChild<LLUICtrl>("groupcount")->setTextArg("[MAX]", llformat("%d", gMaxAgentGroups));
}

void FSFloaterContacts::onOpen(const LLSD& key)
{
	if (gSavedSettings.getBOOL("ContactsTornOff"))
	{
		LLIMFloaterContainer* floater_container = LLIMFloaterContainer::getInstance();
		// first set the tear-off host to the conversations container
		setHost(floater_container);
		// clear the tear-off host right after, the "last host used" will still stick
		setHost(NULL);
		// reparent to floater view
		gFloaterView->addChild(this);
		// and remember we are torn off
		setTornOff(TRUE);
	}

	if (key.asString() == "friends")
	{
		childShowTab("friends_and_groups", "friends_panel");
	}
	else if (key.asString() == "groups")
	{
		childShowTab("friends_and_groups", "groups_panel");
		mGroupsTab->getChild<LLUICtrl>("groupcount")->setTextArg("[COUNT]", llformat("%d", gAgent.mGroups.count())); //  items.end()));//
		mGroupsTab->getChild<LLUICtrl>("groupcount")->setTextArg("[MAX]", llformat("%d", gMaxAgentGroups));
	}
}

// as soon as the avatar list widget tells us all names are loaded, do
//  another sort on the list -Zi
void FSFloaterContacts::sortFriendList()
{
	mFriendList->sort();
}

//
// Friend actions
//

void FSFloaterContacts::onAvatarListDoubleClicked(LLUICtrl* ctrl)
{
	LLAvatarListItem* item = dynamic_cast<LLAvatarListItem*>(ctrl);
	if(!item)
	{
		return;
	}

	LLUUID clicked_id = item->getAvatarId();
	
#if 0 // SJB: Useful for testing, but not currently functional or to spec
	LLAvatarActions::showProfile(clicked_id);
#else // spec says open IM window
	LLAvatarActions::startIM(clicked_id);
#endif
}

void FSFloaterContacts::onImButtonClicked()
{
	uuid_vec_t selected_uuids;
	getCurrentItemIDs(selected_uuids);
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

void FSFloaterContacts::onViewProfileButtonClicked()
{
	LLUUID id = getCurrentItemID();
	LLAvatarActions::showProfile(id);
}

void FSFloaterContacts::onTeleportButtonClicked()
{
	uuid_vec_t selected_uuids;
	getCurrentItemIDs(selected_uuids);
	LLAvatarActions::offerTeleport(selected_uuids);
}

void FSFloaterContacts::onPayButtonClicked()
{
	LLUUID id = getCurrentItemID();
	if (id.notNull())
		LLAvatarActions::pay(id);
}

void FSFloaterContacts::onDeleteFriendButtonClicked()
{
	uuid_vec_t selected_uuids;
	getCurrentItemIDs(selected_uuids);

	if (selected_uuids.size() == 1)
	{
		LLAvatarActions::removeFriendDialog( selected_uuids.front() );
	}
	else if (selected_uuids.size() > 1)
	{
		LLAvatarActions::removeFriendsDialog( selected_uuids );
	}
}

bool FSFloaterContacts::isItemsFreeOfFriends(const uuid_vec_t& uuids)
{
	const LLAvatarTracker& av_tracker = LLAvatarTracker::instance();
	for ( uuid_vec_t::const_iterator
			  id = uuids.begin(),
			  id_end = uuids.end();
		  id != id_end; ++id )
	{
		if (av_tracker.isBuddy (*id))
		{
			return false;
		}
	}
	return true;
}

void FSFloaterContacts::onAddFriendWizButtonClicked()
{
	// Show add friend wizard.
	LLFloaterAvatarPicker* picker = LLFloaterAvatarPicker::show(boost::bind(&FSFloaterContacts::onAvatarPicked, _1, _2), FALSE, TRUE);
	// Need to disable 'ok' button when friend occurs in selection
	if (picker)	picker->setOkBtnEnableCb(boost::bind(&FSFloaterContacts::isItemsFreeOfFriends, this, _1));
	LLFloater* root_floater = gFloaterView->getParentFloater(this);
	if (root_floater)
	{
		root_floater->addDependentFloater(picker);
	}
}

//
// Group actions
//

void FSFloaterContacts::onGroupChatButtonClicked()
{
	LLUUID group_id = getCurrentItemID();
	if (group_id.notNull())
		LLGroupActions::startIM(group_id);
}

void FSFloaterContacts::onGroupInfoButtonClicked()
{
	LLGroupActions::show(getCurrentItemID());
}

void FSFloaterContacts::onGroupActivateButtonClicked()
{
	LLGroupActions::activate(mGroupList->getSelectedUUID());
}

void FSFloaterContacts::onGroupLeaveButtonClicked()
{
	LLUUID group_id = getCurrentItemID();
	if (group_id.notNull())
		LLGroupActions::leave(group_id);
}

void FSFloaterContacts::onGroupCreateButtonClicked()
{
	LLGroupActions::createGroup();
}

void FSFloaterContacts::onGroupSearchButtonClicked()
{
	LLGroupActions::search();
}



std::string FSFloaterContacts::getActiveTabName() const
{
	return mTabContainer->getCurrentPanel()->getName();
}

LLUUID FSFloaterContacts::getCurrentItemID() const
{
	std::string cur_tab = getActiveTabName();

	if (cur_tab == FRIENDS_TAB_NAME)
		return mFriendList->getSelectedUUID();

	if (cur_tab == GROUP_TAB_NAME)
		return mGroupList->getSelectedUUID();

	llassert(0 && "unknown tab selected");
	return LLUUID::null;
}

void FSFloaterContacts::getCurrentItemIDs(uuid_vec_t& selected_uuids) const
{
	std::string cur_tab = getActiveTabName();

	if (cur_tab == FRIENDS_TAB_NAME)
		mFriendList->getSelectedUUIDs(selected_uuids);
	else if (cur_tab == GROUP_TAB_NAME)
		mGroupList->getSelectedUUIDs(selected_uuids);
	else
		llassert(0 && "unknown tab selected");

}


void FSFloaterContacts::updateFriendList()
{
	if (!mFriendList)
		return;

	uuid_vec_t& contact_friendsp = mFriendList->getIDs();
	contact_friendsp.clear();
	
	// Cause inv fails so very much, lets just load the contacts floater from
	// the avatar tracker instead of from calling cards, which are fail
	// This should resolve part or all of FIRE-565 -KC
	
	const LLAvatarTracker& av_tracker = LLAvatarTracker::instance();
	LLAvatarTracker::buddy_map_t all_buddies;
	av_tracker.copyBuddyList(all_buddies);

	LLAvatarTracker::buddy_map_t::const_iterator buddy_it = all_buddies.begin();
	for (; buddy_it != all_buddies.end(); ++buddy_it)
	{
		LLUUID buddy_id = buddy_it->first;
		contact_friendsp.push_back(buddy_id);
	}

	mFriendList->setDirty(true, !mFriendList->filterHasMatches());
}


// static
void FSFloaterContacts::onAvatarPicked(const uuid_vec_t& ids, const std::vector<LLAvatarName> names)
{
	if (!names.empty() && !ids.empty())
		LLAvatarActions::requestFriendshipDialog(ids[0], names[0].getCompleteName());
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
