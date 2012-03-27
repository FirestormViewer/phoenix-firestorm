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
#include "llavatarnamecache.h"
#include "llcallingcard.h"			// for LLAvatarTracker
#include "llfloateravatarpicker.h"
#include "llfriendcard.h"
#include "llgroupactions.h"
#include "llgrouplist.h"
#include "llimfloatercontainer.h"
#include "llnotificationsutil.h"
#include "llscrolllistctrl.h"
#include "llfloatersidepanelcontainer.h"
#include "llstartup.h"
#include "llviewercontrol.h"
#include "llvoiceclient.h"
#include "lggcontactsetsfloater.h"

//Maximum number of people you can select to do an operation on at once.
#define MAX_FRIEND_SELECT 20
#define DEFAULT_PERIOD 5.0
#define RIGHTS_CHANGE_TIMEOUT 5.0
#define OBSERVER_TIMEOUT 0.5

static const std::string FRIENDS_TAB_NAME	= "friends_panel";
static const std::string GROUP_TAB_NAME		= "groups_panel";


// simple class to observe the calling cards.
class LLLocalFriendsObserver : public LLFriendObserver
{
public: 
	LLLocalFriendsObserver(FSFloaterContacts* floater) : mFloater(floater) {}
	virtual ~LLLocalFriendsObserver()
	{
		mFloater = NULL;
	}
	virtual void changed(U32 mask)
	{
		mFloater->onFriendListUpdate(mask);
	}
protected:
	FSFloaterContacts* mFloater;
};

//
// FSFloaterContacts
//

FSFloaterContacts::FSFloaterContacts(const LLSD& seed)
	: LLFloater(seed),
	mTabContainer(NULL),
	mObserver(NULL),
	mFriendsList(NULL),
	mGroupList(NULL),
	mAllowRightsChange(TRUE),
	mNumRightsChanged(0),
	mSortByUserName(LLCachedControl<bool>(gSavedSettings,"FSSortContactsByUserName", FALSE))
{
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

BOOL FSFloaterContacts::postBuild()
{
	mTabContainer = getChild<LLTabContainer>("friends_and_groups");
	mFriendsTab = getChild<LLPanel>(FRIENDS_TAB_NAME);

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
	if (mSortByUserName)
	{
		mFriendsList->getColumn(LIST_FRIEND_NAME)->mSortingColumn = mFriendsList->getColumn(LIST_FRIEND_USER_NAME)->mName;
		mFriendsList->sortByColumn(std::string("user_name"), TRUE);
	}
	else
	{
		mFriendsList->getColumn(LIST_FRIEND_NAME)->mSortingColumn = mFriendsList->getColumn(LIST_FRIEND_NAME)->mName;
		mFriendsList->sortByColumn(std::string("full_name"), TRUE);
	}
	mFriendsList->sortByColumn(std::string("icon_online_status"), FALSE);
	
	mFriendsTab->childSetAction("im_btn", boost::bind(&FSFloaterContacts::onImButtonClicked, this));
	mFriendsTab->childSetAction("profile_btn", boost::bind(&FSFloaterContacts::onViewProfileButtonClicked, this));
	mFriendsTab->childSetAction("offer_teleport_btn", boost::bind(&FSFloaterContacts::onTeleportButtonClicked, this));
	mFriendsTab->childSetAction("pay_btn", boost::bind(&FSFloaterContacts::onPayButtonClicked, this));
	
	mFriendsTab->childSetAction("remove_btn", boost::bind(&FSFloaterContacts::onDeleteFriendButtonClicked, this));
	mFriendsTab->childSetAction("add_btn", boost::bind(&FSFloaterContacts::onAddFriendWizButtonClicked, this));
	
	mFriendsTab->childSetAction("lgg_fg_openFG", boost::bind(&FSFloaterContacts::onContactSetsButtonClicked, this));

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
	mGroupsTab->childSetAction("titles_btn",	boost::bind(&FSFloaterContacts::onGroupTitlesButtonClicked,	this));
	
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
	}
	else
	{
		floater_container->addFloater(this, TRUE);
	}

	openTab(key.asString());
}

void FSFloaterContacts::openTab(const std::string& name)
{
	BOOL visible=FALSE;

	if(name=="friends")
	{
		visible=TRUE;
		childShowTab("friends_and_groups", "friends_panel");
	}
	else if(name=="groups")
	{
		visible=TRUE;
		childShowTab("friends_and_groups", "groups_panel");
		mGroupsTab->getChild<LLUICtrl>("groupcount")->setTextArg("[COUNT]", llformat("%d", gAgent.mGroups.count())); //  items.end()));//
		mGroupsTab->getChild<LLUICtrl>("groupcount")->setTextArg("[MAX]", llformat("%d", gMaxAgentGroups));
	}

	if(visible)
	{
		LLIMFloaterContainer* floater_container = (LLIMFloaterContainer *) getHost();
		if(floater_container)
		{
			floater_container->setVisible(TRUE);
			floater_container->showFloater(this);
		}
		else
			setVisible(TRUE);
	}
}


//static
FSFloaterContacts* FSFloaterContacts::findInstance()
{
	return LLFloaterReg::findTypedInstance<FSFloaterContacts>("imcontacts");
}

//static
FSFloaterContacts* FSFloaterContacts::getInstance()
{
	return LLFloaterReg::getTypedInstance<FSFloaterContacts>("imcontacts");
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

// static
void FSFloaterContacts::onAvatarPicked(const uuid_vec_t& ids, const std::vector<LLAvatarName> names)
{
	if (!names.empty() && !ids.empty())
		LLAvatarActions::requestFriendshipDialog(ids[0], names[0].getCompleteName());
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

void FSFloaterContacts::onContactSetsButtonClicked()
{
	LLFloaterReg::toggleInstance("contactsets");
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

void FSFloaterContacts::onGroupTitlesButtonClicked()
{
	LLFloaterReg::toggleInstance("group_titles");
}

//
// Tab and list functions
//

std::string FSFloaterContacts::getActiveTabName() const
{
	return mTabContainer->getCurrentPanel()->getName();
}

LLUUID FSFloaterContacts::getCurrentItemID() const
{
	std::string cur_tab = getActiveTabName();

	if (cur_tab == FRIENDS_TAB_NAME)
	{
		LLScrollListItem* selected = mFriendsList->getFirstSelected();
		if (selected)
			return selected->getUUID();
		else
			return LLUUID::null;
	}
	if (cur_tab == GROUP_TAB_NAME)
	{
		return mGroupList->getSelectedUUID();
	}

	return LLUUID::null;
}

void FSFloaterContacts::getCurrentItemIDs(uuid_vec_t& selected_uuids) const
{
	std::string cur_tab = getActiveTabName();

	if (cur_tab == FRIENDS_TAB_NAME)
	{
		std::vector<LLScrollListItem*> selected = mFriendsList->getAllSelected();
		for(std::vector<LLScrollListItem*>::iterator itr = selected.begin(); itr != selected.end(); ++itr)
		{
			selected_uuids.push_back((*itr)->getUUID());
		}
	}
	else if (cur_tab == GROUP_TAB_NAME)
	{
		mGroupList->getSelectedUUIDs(selected_uuids);
	}
}

void FSFloaterContacts::sortFriendList()
{
	mFriendsList->updateLineHeight();
	mFriendsList->updateLayout();

	if (mSortByUserName)
		mFriendsList->getColumn(LIST_FRIEND_NAME)->mSortingColumn = mFriendsList->getColumn(LIST_FRIEND_USER_NAME)->mName;
	else
		mFriendsList->getColumn(LIST_FRIEND_NAME)->mSortingColumn = mFriendsList->getColumn(LIST_FRIEND_NAME)->mName;
	mFriendsList->setNeedsSort(true);
}




//
// Friends list
//

void FSFloaterContacts::onFriendListUpdate(U32 changed_mask)
{
	LLAvatarTracker& at = LLAvatarTracker::instance();

	switch(changed_mask) {
	case LLFriendObserver::ADD:
		{
			const std::set<LLUUID>& changed_items = at.getChangedIDs();
			std::set<LLUUID>::const_iterator id_it = changed_items.begin();
			std::set<LLUUID>::const_iterator id_end = changed_items.end();
			for (;id_it != id_end; ++id_it)
			{
				addFriend(*id_it);
			}
		}
		break;
	case LLFriendObserver::REMOVE:
		{
			const std::set<LLUUID>& changed_items = at.getChangedIDs();
			std::set<LLUUID>::const_iterator id_it = changed_items.begin();
			std::set<LLUUID>::const_iterator id_end = changed_items.end();
			for (;id_it != id_end; ++id_it)
			{
				mFriendsList->deleteSingleItem(mFriendsList->getItemIndex(*id_it));
			}
		}
		break;
	case LLFriendObserver::POWERS:
		{
			--mNumRightsChanged;
			if(mNumRightsChanged > 0)
				mAllowRightsChange = FALSE;
			else
				mAllowRightsChange = TRUE;
			
			const std::set<LLUUID>& changed_items = at.getChangedIDs();
			std::set<LLUUID>::const_iterator id_it = changed_items.begin();
			std::set<LLUUID>::const_iterator id_end = changed_items.end();
			for (;id_it != id_end; ++id_it)
			{
				const LLRelationship* info = at.getBuddyInfo(*id_it);
				updateFriendItem(*id_it, info);
			}
		}
		break;
	case LLFriendObserver::ONLINE:
		{
			const std::set<LLUUID>& changed_items = at.getChangedIDs();
			std::set<LLUUID>::const_iterator id_it = changed_items.begin();
			std::set<LLUUID>::const_iterator id_end = changed_items.end();
			for (;id_it != id_end; ++id_it)
			{
				const LLRelationship* info = at.getBuddyInfo(*id_it);
				updateFriendItem(*id_it, info);
			}
		}
	default:;
	}
	
	refreshUI();
}

void FSFloaterContacts::addFriend(const LLUUID& agent_id)
{
	LLAvatarTracker& at = LLAvatarTracker::instance();
	const LLRelationship* relationInfo = at.getBuddyInfo(agent_id);
	if(!relationInfo) return;

	bool isOnlineSIP = LLVoiceClient::getInstance()->isOnlineSIP(agent_id);
	bool isOnline = relationInfo->isOnline();

	LLAvatarName av_name;
	if (!LLAvatarNameCache::get(agent_id, &av_name))
	{
		const LLRelationship* info = LLAvatarTracker::instance().getBuddyInfo(agent_id);
		LLAvatarNameCache::get(agent_id, boost::bind(&FSFloaterContacts::updateFriendItem, this, agent_id, info));
	}

	LLSD element;
	element["id"] = agent_id;
	LLSD& username_column				= element["columns"][LIST_FRIEND_USER_NAME];
	username_column["column"]			= "user_name";
	username_column["value"]			= av_name.mUsername;
	
	LLSD& friend_column					= element["columns"][LIST_FRIEND_NAME];
	friend_column["column"]				= "full_name";
	friend_column["value"]				= av_name.getCompleteName();
	friend_column["font"]["name"]		= "SANSSERIF";
	friend_column["font"]["style"]		= "NORMAL";

	LLSD& online_status_column			= element["columns"][LIST_ONLINE_STATUS];
	online_status_column["column"]		= "icon_online_status";
	online_status_column["type"]		= "icon";
	
	if (isOnline)
	{	
		friend_column["font"]["style"]		= "BOLD";	
		online_status_column["value"]		= "icon_avatar_online";
	}
	else if(isOnlineSIP)
	{	
		friend_column["font"]["style"]		= "BOLD";	
		online_status_column["value"]		= "slim_icon_16_viewer";
	}

	LLSD& online_column						= element["columns"][LIST_VISIBLE_ONLINE];
	online_column["column"]					= "icon_visible_online";
	online_column["type"]					= "checkbox";
	online_column["value"]					= relationInfo->isRightGrantedTo(LLRelationship::GRANT_ONLINE_STATUS);

	LLSD& visible_map_column				= element["columns"][LIST_VISIBLE_MAP];
	visible_map_column["column"]			= "icon_visible_map";
	visible_map_column["type"]				= "checkbox";
	visible_map_column["value"]				= relationInfo->isRightGrantedTo(LLRelationship::GRANT_MAP_LOCATION);

	LLSD& edit_my_object_column				= element["columns"][LIST_EDIT_MINE];
	edit_my_object_column["column"]			= "icon_edit_mine";
	edit_my_object_column["type"]			= "checkbox";
	edit_my_object_column["value"]			= relationInfo->isRightGrantedTo(LLRelationship::GRANT_MODIFY_OBJECTS);

	LLSD& visible_their_map_column			= element["columns"][LIST_VISIBLE_MAP_THEIRS];
	visible_their_map_column["column"]		= "icon_visible_map_theirs";
	visible_their_map_column["type"]		= "checkbox";
	visible_their_map_column["enabled"]		= "";
	visible_their_map_column["value"]		= relationInfo->isRightGrantedFrom(LLRelationship::GRANT_MAP_LOCATION);
	
	LLSD& edit_their_object_column			= element["columns"][LIST_EDIT_THEIRS];
	edit_their_object_column["column"]		= "icon_edit_theirs";
	edit_their_object_column["type"]		= "checkbox";
	edit_their_object_column["enabled"]		= "";
	edit_their_object_column["value"]		= relationInfo->isRightGrantedFrom(LLRelationship::GRANT_MODIFY_OBJECTS);

	LLSD& update_gen_column					= element["columns"][LIST_FRIEND_UPDATE_GEN];
	update_gen_column["column"]				= "friend_last_update_generation";
	update_gen_column["value"]				= relationInfo->getChangeSerialNum();

	mFriendsList->addElement(element, ADD_BOTTOM);
}


// propagate actual relationship to UI.
// Does not resort the UI list because it can be called frequently. JC
void FSFloaterContacts::updateFriendItem(const LLUUID& agent_id, const LLRelationship* info)
{
	if (!info) return;
	LLScrollListItem* itemp = mFriendsList->getItem(agent_id);
	if (!itemp) return;
	
	bool isOnlineSIP = LLVoiceClient::getInstance()->isOnlineSIP(itemp->getUUID());
	bool isOnline = info->isOnline();

	LLAvatarName av_name;
	if (!LLAvatarNameCache::get(agent_id, &av_name))
	{
		LLAvatarNameCache::get(agent_id, boost::bind(&FSFloaterContacts::updateFriendItem, this, agent_id, info));
	}

	// Name of the status icon to use
	std::string statusIcon;
	if(isOnline)
	{
		statusIcon = "icon_avatar_online";
	}
	else if(isOnlineSIP)
	{
		statusIcon = "slim_icon_16_viewer";
	}

	itemp->getColumn(LIST_ONLINE_STATUS)->setValue(statusIcon);

	itemp->getColumn(LIST_FRIEND_USER_NAME)->setValue( av_name.mUsername );
	itemp->getColumn(LIST_FRIEND_NAME)->setValue( av_name.getCompleteName() );

	// render name of online friends in bold text
	((LLScrollListText*)itemp->getColumn(LIST_FRIEND_NAME))->setFontStyle((isOnline || isOnlineSIP) ? LLFontGL::BOLD : LLFontGL::NORMAL);

	itemp->getColumn(LIST_VISIBLE_ONLINE)->setValue(info->isRightGrantedTo(LLRelationship::GRANT_ONLINE_STATUS));
	itemp->getColumn(LIST_VISIBLE_MAP)->setValue(info->isRightGrantedTo(LLRelationship::GRANT_MAP_LOCATION));
	itemp->getColumn(LIST_EDIT_MINE)->setValue(info->isRightGrantedTo(LLRelationship::GRANT_MODIFY_OBJECTS));
	itemp->getColumn(LIST_VISIBLE_MAP_THEIRS)->setValue(info->isRightGrantedFrom(LLRelationship::GRANT_MAP_LOCATION));
	itemp->getColumn(LIST_EDIT_THEIRS)->setValue(info->isRightGrantedFrom(LLRelationship::GRANT_MODIFY_OBJECTS));
	S32 change_generation = info->getChangeSerialNum();
	itemp->getColumn(LIST_FRIEND_UPDATE_GEN)->setValue(change_generation);

	// enable this item, in case it was disabled after user input
	itemp->setEnabled(TRUE);
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

void FSFloaterContacts::refreshUI()
{
	sortFriendList();

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

	// Set friend count
	LLStringUtil::format_map_t args;
	args["[COUNT]"] = llformat("%d", mFriendsList->getItemCount());
	mFriendsTab->childSetText("friend_count", mFriendsTab->getString("FriendCount", args));

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
