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
#include "llnotificationsutil.h"
#include "llscrolllistctrl.h"
#include "llsidetray.h"
#include "llstartup.h"
#include "llviewercontrol.h"
#include "llvoiceclient.h"

//Maximum number of people you can select to do an operation on at once.
#define MAX_FRIEND_SELECT 20
#define DEFAULT_PERIOD 5.0
#define RIGHTS_CHANGE_TIMEOUT 5.0
#define OBSERVER_TIMEOUT 0.5

#define ONLINE_SIP_ICON_NAME "slim_icon_16_viewer.tga"

static const std::string FRIENDS_TAB_NAME	= "friends_panel";
static const std::string GROUP_TAB_NAME		= "groups_panel";

/** Compares avatar items by online status, then by name */
// class LLAvatarItemStatusComparator : public LLAvatarItemComparator
// {
// public:
	// LLAvatarItemStatusComparator() {};

// protected:
	// /**
	 // * @return true if item1 < item2, false otherwise
	 // */
	// virtual bool doCompare(const LLAvatarListItem* item1, const LLAvatarListItem* item2) const
	// {
		// LLAvatarTracker& at = LLAvatarTracker::instance();
		// bool online1 = at.isBuddyOnline(item1->getAvatarId());
		// bool online2 = at.isBuddyOnline(item2->getAvatarId());

		// if (online1 == online2)
		// {
			// std::string name1 = item1->getAvatarName();
			// std::string name2 = item2->getAvatarName();

			// if (item1->getShowingBothNames() &&
				// gSavedSettings.getBOOL("FSSortContactsByUserName"))
			// {
				// // The tooltip happens to have the username
				// //  in it, so we'll just use that. Why store
				// //  it twice or parse it back out? -- TS
				// name1 = item1->getAvatarToolTip();
				// name2 = item2->getAvatarToolTip();
			// }

			// LLStringUtil::toUpper(name1);
			// LLStringUtil::toUpper(name2);

			// return name1 < name2;
		// }
		
		// return online1 > online2; 
	// }
// };

// static const LLAvatarItemStatusComparator STATUS_COMPARATOR;


// simple class to observe the calling cards.
class LLLocalFriendsObserver : public LLFriendObserver, public LLEventTimer
{
public: 
	LLLocalFriendsObserver(FSFloaterContacts* floater) : mFloater(floater), LLEventTimer(OBSERVER_TIMEOUT)
	{
		mEventTimer.stop();
	}
	virtual ~LLLocalFriendsObserver()
	{
		mFloater = NULL;
	}
	virtual void changed(U32 mask)
	{
		// events can arrive quickly in bulk - we need not process EVERY one of them -
		// so we wait a short while to let others pile-in, and process them in aggregate.
		mEventTimer.start();

		// save-up all the mask-bits which have come-in
		mMask |= mask;
	}
	virtual BOOL tick()
	{
		mFloater->updateFriends(mMask);

		mEventTimer.stop();
		mMask = 0;

		return FALSE;
	}
	
protected:
	FSFloaterContacts* mFloater;
	U32 mMask;
};

//
// FSFloaterContacts
//

FSFloaterContacts::FSFloaterContacts(const LLSD& seed)
	: LLFloater(seed),
	LLEventTimer(DEFAULT_PERIOD),
	mTabContainer(NULL),
	mObserver(NULL),
	// mFriendList(NULL),
	mGroupList(NULL),
	mAllowRightsChange(TRUE),
	mNumRightsChanged(0)
{
	mEventTimer.stop();
	mObserver = new LLLocalFriendsObserver(this);
	LLAvatarTracker::instance().addObserver(mObserver);
	// For notification when SIP online status changes.
	LLVoiceClient::getInstance()->addObserver(mObserver);
}

FSFloaterContacts::~FSFloaterContacts()
{
	// For notification when SIP online status changes.
	LLVoiceClient::getInstance()->removeObserver(mObserver);
	LLAvatarTracker::instance().removeObserver(mObserver);
	delete mObserver;
}

BOOL FSFloaterContacts::tick()
{
	mEventTimer.stop();
	mPeriod = DEFAULT_PERIOD;
	mAllowRightsChange = TRUE;
	updateFriends(LLFriendObserver::ADD);
	return FALSE;
}

BOOL FSFloaterContacts::postBuild()
{
	mTabContainer = getChild<LLTabContainer>("friends_and_groups");

	mFriendsTab = getChild<LLPanel>(FRIENDS_TAB_NAME);
	// mFriendList = mFriendsTab->getChild<LLAvatarList>("avatars_all");
	// mFriendList->setNoItemsCommentText(getString("no_friends"));
	// mFriendList->setShowIcons("FriendsListShowIcons");
	// mFriendList->showPermissions(TRUE);
	// mFriendList->setComparator(&STATUS_COMPARATOR);
	
	// mFriendList->setRefreshCompleteCallback(boost::bind(&FSFloaterContacts::sortFriendList, this));
	// mFriendList->setItemDoubleClickCallback(boost::bind(&FSFloaterContacts::onAvatarListDoubleClicked, this, _1));
	// mFriendList->setReturnCallback(boost::bind(&FSFloaterContacts::onImButtonClicked, this));
	
	
	mFriendsList = mFriendsTab->getChild<LLScrollListCtrl>("friend_list");
	mFriendsList->setMaxSelectable(MAX_FRIEND_SELECT);
	//mFriendsList->setMaximumSelectCallback(boost::bind(&FSFloaterContacts::onMaximumSelect, this)); //TODO
	mFriendsList->setCommitOnSelectionChange(TRUE);
	//mFriendsList->setContextMenu(LLScrollListCtrl::MENU_AVATAR);
	mFriendsList->setCommitCallback(boost::bind(&FSFloaterContacts::onSelectName, this));
	mFriendsList->setDoubleClickCallback(boost::bind(&FSFloaterContacts::onImButtonClicked, this));
	//mFriendsList->setReturnCallback(boost::bind(&FSFloaterContacts::onImButtonClicked, this));
	mFriendsList->setContextMenu(LLScrollListCtrl::MENU_AVATAR);

	// primary sort = online status, secondary sort = name
	mFriendsList->sortByColumn(std::string("friend_name"), TRUE);
	mFriendsList->sortByColumn(std::string("icon_online_status"), FALSE);
	
	
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
	LLIMFloaterContainer* floater_container = LLIMFloaterContainer::getInstance();
	if (gSavedSettings.getBOOL("ContactsTornOff"))
	{
		// first set the tear-off host to the conversations container
		setHost(floater_container);
		// clear the tear-off host right after, the "last host used" will still stick
		setHost(NULL);
		// reparent to floater view
		gFloaterView->addChild(this);
		// and remember we are torn off
		setTornOff(TRUE);
	}
	else
	{
		floater_container->addFloater(this, TRUE);
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
	// mFriendList->sort();
	mFriendsList->updateSort();
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
	{
		// return mFriendList->getSelectedUUID();
		LLScrollListItem* selected = mFriendsList->getFirstSelected();
		if (selected)
			selected->getUUID();
		else
			return LLUUID::null;
	}
	if (cur_tab == GROUP_TAB_NAME)
		return mGroupList->getSelectedUUID();

	llassert(0 && "unknown tab selected");
	return LLUUID::null;
}

void FSFloaterContacts::getCurrentItemIDs(uuid_vec_t& selected_uuids) const
{
	std::string cur_tab = getActiveTabName();

	if (cur_tab == FRIENDS_TAB_NAME)
	{
		// mFriendList->getSelectedUUIDs(selected_uuids);
		std::vector<LLScrollListItem*> selected = mFriendsList->getAllSelected();
		for(std::vector<LLScrollListItem*>::iterator itr = selected.begin(); itr != selected.end(); ++itr)
		{
			selected_uuids.push_back((*itr)->getUUID());
		}
	}
	else if (cur_tab == GROUP_TAB_NAME)
		mGroupList->getSelectedUUIDs(selected_uuids);
	else
		llassert(0 && "unknown tab selected");

}


// void FSFloaterContacts::updateFriendList()
// {
	// if (!mFriendList)
		// return;

	// uuid_vec_t& contact_friendsp = mFriendList->getIDs();
	// contact_friendsp.clear();
	
	// Cause inv fails so very much, lets just load the contacts floater from
	// the avatar tracker instead of from calling cards, which are fail
	// This should resolve part or all of FIRE-565 -KC
	
	// const LLAvatarTracker& av_tracker = LLAvatarTracker::instance();
	// LLAvatarTracker::buddy_map_t all_buddies;
	// av_tracker.copyBuddyList(all_buddies);

	// LLAvatarTracker::buddy_map_t::const_iterator buddy_it = all_buddies.begin();
	// for (; buddy_it != all_buddies.end(); ++buddy_it)
	// {
		// LLUUID buddy_id = buddy_it->first;
		// contact_friendsp.push_back(buddy_id);
	// }

	// mFriendList->setDirty(true, !mFriendList->filterHasMatches());
// }


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




void FSFloaterContacts::updateFriends(U32 changed_mask)
{
	LLUUID selected_id;
	LLCtrlListInterface *friends_list = childGetListInterface("friend_list");
	if (!friends_list) return;
	LLCtrlScrollInterface *friends_scroll = childGetScrollInterface("friend_list");
	if (!friends_scroll) return;

	if(changed_mask & (LLFriendObserver::ADD | LLFriendObserver::REMOVE | LLFriendObserver::ONLINE))
	{
		refreshNames(changed_mask);
	}
	else if(changed_mask & LLFriendObserver::POWERS)
	{
		--mNumRightsChanged;
		if(mNumRightsChanged > 0)
		{
			mPeriod = RIGHTS_CHANGE_TIMEOUT;	
			mEventTimer.start();
			mAllowRightsChange = FALSE;
		}
		else
		{
			tick();
		}
	}

	uuid_vec_t selected_friends;
	getCurrentItemIDs(selected_friends);
	if(selected_friends.size() > 0)
	{
		// only non-null if friends was already found. This may fail,
		// but we don't really care here, because refreshUI() will
		// clean up the interface.
		friends_list->setCurrentByID(selected_id);
		for(std::vector<LLUUID>::iterator itr = selected_friends.begin(); itr != selected_friends.end(); ++itr)
		{
			friends_list->setSelectedByValue(*itr, true);
		}
	}

	refreshUI();
}

BOOL FSFloaterContacts::addFriend(const LLUUID& agent_id)
{
	LLAvatarTracker& at = LLAvatarTracker::instance();
	const LLRelationship* relationInfo = at.getBuddyInfo(agent_id);
	if(!relationInfo) return FALSE;

	bool isOnlineSIP = LLVoiceClient::getInstance()->isOnlineSIP(agent_id);
	bool isOnline = relationInfo->isOnline();

	std::string fullname;
	BOOL have_name = gCacheName->getFullName(agent_id, fullname);
	
	LLSD element;
	element["id"] = agent_id;
	LLSD& friend_column = element["columns"][LIST_FRIEND_NAME];
	friend_column["column"] = "friend_name";
	friend_column["value"] = fullname;
	friend_column["font"]["name"] = "SANSSERIF";
	friend_column["font"]["style"] = "NORMAL";	

	LLSD& online_status_column = element["columns"][LIST_ONLINE_STATUS];
	online_status_column["column"] = "icon_online_status";
	online_status_column["type"] = "icon";
	
	if (isOnline)
	{
		friend_column["font"]["style"] = "BOLD";	
		online_status_column["value"] = "legacy/icon_avatar_online.tga";
	}
	else if(isOnlineSIP)
	{
		friend_column["font"]["style"] = "BOLD";	
		online_status_column["value"] = ONLINE_SIP_ICON_NAME;
	}

	LLSD& online_column = element["columns"][LIST_VISIBLE_ONLINE];
	online_column["column"] = "icon_visible_online";
	online_column["type"] = "checkbox";
	online_column["value"] = relationInfo->isRightGrantedTo(LLRelationship::GRANT_ONLINE_STATUS);

	LLSD& visible_map_column = element["columns"][LIST_VISIBLE_MAP];
	visible_map_column["column"] = "icon_visible_map";
	visible_map_column["type"] = "checkbox";
	visible_map_column["value"] = relationInfo->isRightGrantedTo(LLRelationship::GRANT_MAP_LOCATION);

	LLSD& edit_my_object_column = element["columns"][LIST_EDIT_MINE];
	edit_my_object_column["column"] = "icon_edit_mine";
	edit_my_object_column["type"] = "checkbox";
	edit_my_object_column["value"] = relationInfo->isRightGrantedTo(LLRelationship::GRANT_MODIFY_OBJECTS);

	LLSD& visible_their_map_column = element["columns"][LIST_VISIBLE_MAP_THEIRS];
	visible_their_map_column["column"] = "icon_visible_map_theirs";
	visible_their_map_column["type"] = "checkbox";
	visible_their_map_column["enabled"] = "";
	visible_their_map_column["value"] = relationInfo->isRightGrantedFrom(LLRelationship::GRANT_MAP_LOCATION);
	
	LLSD& edit_their_object_column = element["columns"][LIST_EDIT_THEIRS];
	edit_their_object_column["column"] = "icon_edit_theirs";
	edit_their_object_column["type"] = "checkbox";
	edit_their_object_column["enabled"] = "";
	edit_their_object_column["value"] = relationInfo->isRightGrantedFrom(LLRelationship::GRANT_MODIFY_OBJECTS);

	LLSD& update_gen_column = element["columns"][LIST_FRIEND_UPDATE_GEN];
	update_gen_column["column"] = "friend_last_update_generation";
	update_gen_column["value"] = have_name ? relationInfo->getChangeSerialNum() : -1;

	mFriendsList->addElement(element, ADD_BOTTOM);
	return have_name;
}


// propagate actual relationship to UI.
// Does not resort the UI list because it can be called frequently. JC
BOOL FSFloaterContacts::updateFriendItem(const LLUUID& agent_id, const LLRelationship* info)
{
	if (!info) return FALSE;
	LLScrollListItem* itemp = mFriendsList->getItem(agent_id);
	if (!itemp) return FALSE;
	
	bool isOnlineSIP = LLVoiceClient::getInstance()->isOnlineSIP(itemp->getUUID());
	bool isOnline = info->isOnline();

	std::string fullname;
	BOOL have_name = gCacheName->getFullName(agent_id, fullname);
	
	// Name of the status icon to use
	std::string statusIcon;
	
	if(isOnline)
	{
		statusIcon = "legacy/icon_avatar_online.tga";
	}
	else if(isOnlineSIP)
	{
		statusIcon = ONLINE_SIP_ICON_NAME;
	}

	itemp->getColumn(LIST_ONLINE_STATUS)->setValue(statusIcon);
	
	itemp->getColumn(LIST_FRIEND_NAME)->setValue(fullname);
	//This might work, but would need to test if it changes with DN changes _KC
	//itemp->getColumn(LIST_FRIEND_NAME)->setValue( LLSLURL("agent", agent_id, "completename").getSLURLString());
	
	// render name of online friends in bold text
	((LLScrollListText*)itemp->getColumn(LIST_FRIEND_NAME))->setFontStyle((isOnline || isOnlineSIP) ? LLFontGL::BOLD : LLFontGL::NORMAL);	
	itemp->getColumn(LIST_VISIBLE_ONLINE)->setValue(info->isRightGrantedTo(LLRelationship::GRANT_ONLINE_STATUS));
	itemp->getColumn(LIST_VISIBLE_MAP)->setValue(info->isRightGrantedTo(LLRelationship::GRANT_MAP_LOCATION));
	itemp->getColumn(LIST_EDIT_MINE)->setValue(info->isRightGrantedTo(LLRelationship::GRANT_MODIFY_OBJECTS));
	itemp->getColumn(LIST_VISIBLE_MAP_THEIRS)->setValue(info->isRightGrantedFrom(LLRelationship::GRANT_MAP_LOCATION));
	itemp->getColumn(LIST_EDIT_THEIRS)->setValue(info->isRightGrantedFrom(LLRelationship::GRANT_MODIFY_OBJECTS));
	S32 change_generation = have_name ? info->getChangeSerialNum() : -1;
	// S32 change_generation = info->getChangeSerialNum();
	itemp->getColumn(LIST_FRIEND_UPDATE_GEN)->setValue(change_generation);

	// enable this item, in case it was disabled after user input
	itemp->setEnabled(TRUE);

	// Do not resort, this function can be called frequently.
	return have_name;
}

void FSFloaterContacts::refreshRightsChangeList()
{
	uuid_vec_t friends;
	getCurrentItemIDs(friends);

	S32 num_selected = friends.size();

	bool can_offer_teleport = num_selected >= 1;
	bool selected_friends_online = true;

	const LLRelationship* friend_status = NULL;
	for(std::vector<LLUUID>::iterator itr = friends.begin(); itr != friends.end(); ++itr)
	{
		friend_status = LLAvatarTracker::instance().getBuddyInfo(*itr);
		if (friend_status)
		{
			if(!friend_status->isOnline())
			{
				can_offer_teleport = false;
				selected_friends_online = false;
			}
		}
		else // missing buddy info, don't allow any operations
		{
			can_offer_teleport = false;
		}
	}
	
	if (num_selected == 0)  // nothing selected
	{
		childSetEnabled("im_btn", FALSE);
		childSetEnabled("offer_teleport_btn", FALSE);
	}
	else // we have at least one friend selected...
	{
		// only allow IMs to groups when everyone in the group is online
		// to be consistent with context menus in inventory and because otherwise
		// offline friends would be silently dropped from the session
		childSetEnabled("im_btn", selected_friends_online || num_selected == 1);
		childSetEnabled("offer_teleport_btn", can_offer_teleport);
	}
}

struct SortFriendsByID
{
	bool operator() (const LLScrollListItem* const a, const LLScrollListItem* const b) const
	{
		return a->getValue().asUUID() < b->getValue().asUUID();
	}
};

void FSFloaterContacts::refreshNames(U32 changed_mask)
{
	uuid_vec_t selected_ids;
	getCurrentItemIDs(selected_ids);
	
	S32 pos = mFriendsList->getScrollPos();	
	
	// get all buddies we know about
	LLAvatarTracker::buddy_map_t all_buddies;
	LLAvatarTracker::instance().copyBuddyList(all_buddies);

	BOOL have_names = TRUE;

	if(changed_mask & (LLFriendObserver::ADD | LLFriendObserver::REMOVE))
	{
		have_names &= refreshNamesSync(all_buddies);
	}

	if(changed_mask & LLFriendObserver::ONLINE)
	{
		have_names &= refreshNamesPresence(all_buddies);
	}

	if (!have_names)
	{
		mEventTimer.start();
	}
	// Changed item in place, need to request sort and update columns
	// because we might have changed data in a column on which the user
	// has already sorted. JC
	mFriendsList->updateSort();

	// re-select items
	mFriendsList->selectMultiple(selected_ids);
	mFriendsList->setScrollPos(pos);
}

BOOL FSFloaterContacts::refreshNamesSync(const LLAvatarTracker::buddy_map_t & all_buddies)
{
	mFriendsList->deleteAllItems();

	BOOL have_names = TRUE;
	LLAvatarTracker::buddy_map_t::const_iterator buddy_it = all_buddies.begin();

	for(; buddy_it != all_buddies.end(); ++buddy_it)
	{
		have_names &= addFriend(buddy_it->first);
	}

	return have_names;
}

BOOL FSFloaterContacts::refreshNamesPresence(const LLAvatarTracker::buddy_map_t & all_buddies)
{
	std::vector<LLScrollListItem*> items = mFriendsList->getAllData();
	std::sort(items.begin(), items.end(), SortFriendsByID());

	LLAvatarTracker::buddy_map_t::const_iterator buddy_it  = all_buddies.begin();
	std::vector<LLScrollListItem*>::const_iterator item_it = items.begin();
	BOOL have_names = TRUE;

	while(true)
	{
		if(item_it == items.end() || buddy_it == all_buddies.end())
		{
			break;
		}

		const LLUUID & buddy_uuid = buddy_it->first;
		const LLUUID & item_uuid  = (*item_it)->getValue().asUUID();
		if(item_uuid == buddy_uuid)
		{
			const LLRelationship* info = buddy_it->second;
			if (!info) 
			{	
				++item_it;
				continue;
			}
			
			S32 last_change_generation = (*item_it)->getColumn(LIST_FRIEND_UPDATE_GEN)->getValue().asInteger();
			if (last_change_generation < info->getChangeSerialNum())
			{
				// update existing item in UI
				have_names &= updateFriendItem(buddy_it->first, info);
			}

			++buddy_it;
			++item_it;
		}
		else if(item_uuid < buddy_uuid)
		{
			++item_it;
		}
		else //if(item_uuid > buddy_uuid)
		{
			++buddy_it;
		}
	}

	return have_names;
}

void FSFloaterContacts::refreshUI()
{	
	BOOL single_selected = FALSE;
	BOOL multiple_selected = FALSE;
	int num_selected = mFriendsList->getAllSelected().size();
	if(num_selected > 0)
	{
		single_selected = TRUE;
		if(num_selected > 1)
		{
			multiple_selected = TRUE;		
		}
	}


	//Options that can only be performed with one friend selected
	childSetEnabled("profile_btn", single_selected && !multiple_selected);
	childSetEnabled("pay_btn", single_selected && !multiple_selected);

	//Options that can be performed with up to MAX_FRIEND_SELECT friends selected
	//(single_selected will always be true in this situations)
	childSetEnabled("remove_btn", single_selected);
	childSetEnabled("im_btn", single_selected);
//	childSetEnabled("friend_rights", single_selected);

	refreshRightsChangeList();
}

void FSFloaterContacts::onSelectName()
{
	refreshUI();
	// check to see if rights have changed
	applyRightsToFriends();
}



void FSFloaterContacts::confirmModifyRights(rights_map_t& ids, EGrantRevoke command)
{
	if (ids.empty()) return;
	
	LLSD args;
	if(ids.size() > 0)
	{
		rights_map_t* rights = new rights_map_t(ids);

		// for single friend, show their name
		if(ids.size() == 1)
		{
			LLUUID agent_id = ids.begin()->first;
			std::string name;
			if(gCacheName->getFullName(agent_id, name))
			{
				args["NAME"] = name;	
			}
			if (command == GRANT)
			{
				LLNotificationsUtil::add("GrantModifyRights", 
					args, 
					LLSD(), 
					boost::bind(&FSFloaterContacts::modifyRightsConfirmation, this, _1, _2, rights));
			}
			else
			{
				LLNotificationsUtil::add("RevokeModifyRights", 
					args, 
					LLSD(), 
					boost::bind(&FSFloaterContacts::modifyRightsConfirmation, this, _1, _2, rights));
			}
		}
		else
		{
			if (command == GRANT)
			{
				LLNotificationsUtil::add("GrantModifyRightsMultiple", 
					args, 
					LLSD(), 
					boost::bind(&FSFloaterContacts::modifyRightsConfirmation, this, _1, _2, rights));
			}
			else
			{
				LLNotificationsUtil::add("RevokeModifyRightsMultiple", 
					args, 
					LLSD(), 
					boost::bind(&FSFloaterContacts::modifyRightsConfirmation, this, _1, _2, rights));
			}
		}
	}
}

bool FSFloaterContacts::modifyRightsConfirmation(const LLSD& notification, const LLSD& response, rights_map_t* rights)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if(0 == option)
	{
		sendRightsGrant(*rights);
	}
	else
	{
		// need to resync view with model, since user cancelled operation
		rights_map_t::iterator rights_it;
		for (rights_it = rights->begin(); rights_it != rights->end(); ++rights_it)
		{
			const LLRelationship* info = LLAvatarTracker::instance().getBuddyInfo(rights_it->first);
			updateFriendItem(rights_it->first, info);
		}
	}
	refreshUI();

	delete rights;
	return false;
}

void FSFloaterContacts::applyRightsToFriends()
{
	BOOL rights_changed = FALSE;

	// store modify rights separately for confirmation
	rights_map_t rights_updates;

	BOOL need_confirmation = FALSE;
	EGrantRevoke confirmation_type = GRANT;

	// this assumes that changes only happened to selected items
	std::vector<LLScrollListItem*> selected = mFriendsList->getAllSelected();
	for(std::vector<LLScrollListItem*>::iterator itr = selected.begin(); itr != selected.end(); ++itr)
	{
		LLUUID id = (*itr)->getValue();
		const LLRelationship* buddy_relationship = LLAvatarTracker::instance().getBuddyInfo(id);
		if (buddy_relationship == NULL) continue;

		bool show_online_staus = (*itr)->getColumn(LIST_VISIBLE_ONLINE)->getValue().asBoolean();
		bool show_map_location = (*itr)->getColumn(LIST_VISIBLE_MAP)->getValue().asBoolean();
		bool allow_modify_objects = (*itr)->getColumn(LIST_EDIT_MINE)->getValue().asBoolean();

		S32 rights = buddy_relationship->getRightsGrantedTo();
		if(buddy_relationship->isRightGrantedTo(LLRelationship::GRANT_ONLINE_STATUS) != show_online_staus)
		{
			rights_changed = TRUE;
			if(show_online_staus) 
			{
				rights |= LLRelationship::GRANT_ONLINE_STATUS;
			}
			else 
			{
				// ONLINE_STATUS necessary for MAP_LOCATION
				rights &= ~LLRelationship::GRANT_ONLINE_STATUS;
				rights &= ~LLRelationship::GRANT_MAP_LOCATION;
				// propagate rights constraint to UI
				(*itr)->getColumn(LIST_VISIBLE_MAP)->setValue(FALSE);
			}
		}
		if(buddy_relationship->isRightGrantedTo(LLRelationship::GRANT_MAP_LOCATION) != show_map_location)
		{
			rights_changed = TRUE;
			if(show_map_location) 
			{
				// ONLINE_STATUS necessary for MAP_LOCATION
				rights |= LLRelationship::GRANT_MAP_LOCATION;
				rights |= LLRelationship::GRANT_ONLINE_STATUS;
				(*itr)->getColumn(LIST_VISIBLE_ONLINE)->setValue(TRUE);
			}
			else 
			{
				rights &= ~LLRelationship::GRANT_MAP_LOCATION;
			}
		}
		
		// now check for change in modify object rights, which requires confirmation
		if(buddy_relationship->isRightGrantedTo(LLRelationship::GRANT_MODIFY_OBJECTS) != allow_modify_objects)
		{
			rights_changed = TRUE;
			need_confirmation = TRUE;

			if(allow_modify_objects)
			{
				rights |= LLRelationship::GRANT_MODIFY_OBJECTS;
				confirmation_type = GRANT;
			}
			else
			{
				rights &= ~LLRelationship::GRANT_MODIFY_OBJECTS;
				confirmation_type = REVOKE;
			}
		}

		if (rights_changed)
		{
			rights_updates.insert(std::make_pair(id, rights));
			// disable these ui elements until response from server
			// to avoid race conditions
			(*itr)->setEnabled(FALSE);
		}
	}

	// separately confirm grant and revoke of modify rights
	if (need_confirmation)
	{
		confirmModifyRights(rights_updates, confirmation_type);
	}
	else
	{
		sendRightsGrant(rights_updates);
	}
}

void FSFloaterContacts::sendRightsGrant(rights_map_t& ids)
{
	if (ids.empty()) return;

	LLMessageSystem* msg = gMessageSystem;

	// setup message header
	msg->newMessageFast(_PREHASH_GrantUserRights);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUID(_PREHASH_AgentID, gAgent.getID());
	msg->addUUID(_PREHASH_SessionID, gAgent.getSessionID());

	rights_map_t::iterator id_it;
	rights_map_t::iterator end_it = ids.end();
	for(id_it = ids.begin(); id_it != end_it; ++id_it)
	{
		msg->nextBlockFast(_PREHASH_Rights);
		msg->addUUID(_PREHASH_AgentRelated, id_it->first);
		msg->addS32(_PREHASH_RelatedRights, id_it->second);
	}

	mNumRightsChanged = ids.size();
	gAgent.sendReliableMessage();
}



// EOF
