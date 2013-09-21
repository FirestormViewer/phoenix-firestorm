/** 
 * @file fscontactsfloater.h
 * @brief Legacy contacts floater header file
 *
 * $LicenseInfo:firstyear=2011&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2013, The Phoenix Firestorm Project, Inc.
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
 
 
#ifndef FS_CONTACTSFLOATER_H
#define FS_CONTACTSFLOATER_H

#include "llfloater.h"
#include "lleventtimer.h"
#include "llcallingcard.h"

class LLAvatarList;
class LLAvatarName;
class LLAvatarTracker;
class LLFriendObserver;
class LLScrollListCtrl;
class LLGroupList;
class LLRelationship;
class LLPanel;
class LLTabContainer;

class FSFloaterContacts : public LLFloater
{
public:
	FSFloaterContacts(const LLSD& seed);
	virtual ~FSFloaterContacts();

	/** 
	 * @brief This method is called in response to the LLAvatarTracker
	 * sending out a changed() message.
	 */
	void onFriendListUpdate(U32 changed_mask);

	/*virtual*/ BOOL postBuild();
	/*virtual*/ void onOpen(const LLSD& key);

	static FSFloaterContacts* getInstance();
	static FSFloaterContacts* findInstance();

	void openTab(const std::string& name);

	void					sortFriendList();

	LLPanel*				mFriendsTab;
	LLScrollListCtrl*		mFriendsList;
	LLPanel*				mGroupsTab;
	LLGroupList*			mGroupList;

private:
	std::string				getActiveTabName() const;
	LLUUID					getCurrentItemID() const;
	void					getCurrentItemIDs(uuid_vec_t& selected_uuids) const;
	void					onAvatarListDoubleClicked(LLUICtrl* ctrl);

	enum FRIENDS_COLUMN_ORDER
	{
		LIST_ONLINE_STATUS,
		LIST_FRIEND_USER_NAME,
		LIST_FRIEND_NAME,
		LIST_VISIBLE_ONLINE,
		LIST_VISIBLE_MAP,
		LIST_EDIT_MINE,
		LIST_VISIBLE_MAP_THEIRS,
		LIST_EDIT_THEIRS,
		LIST_FRIEND_UPDATE_GEN
	};

	typedef std::map<LLUUID, S32> rights_map_t;
	void					refreshRightsChangeList();
	void					refreshUI();
	void					onSelectName();
	void					applyRightsToFriends();
	void					addFriend(const LLUUID& agent_id);	
	void					updateFriendItem(const LLUUID& agent_id, const LLRelationship* relationship);

	typedef enum 
	{
		GRANT,
		REVOKE
	} EGrantRevoke;
	void					confirmModifyRights(rights_map_t& ids, EGrantRevoke command);
	void					sendRightsGrant(rights_map_t& ids);
	bool					modifyRightsConfirmation(const LLSD& notification, const LLSD& response, rights_map_t* rights);


	bool					isItemsFreeOfFriends(const uuid_vec_t& uuids);
	
	// misc callbacks
	static void				onAvatarPicked(const uuid_vec_t& ids, const std::vector<LLAvatarName> names);
	
	// friend buttons
	void					onViewProfileButtonClicked();
	void					onImButtonClicked();
	void					onTeleportButtonClicked();
	void					onPayButtonClicked();
	void					onDeleteFriendButtonClicked();
	void					onAddFriendWizButtonClicked();
	void					onContactSetsButtonClicked();
	
	// group buttons
	void					onGroupChatButtonClicked();
	void					onGroupInfoButtonClicked();
	void					onGroupActivateButtonClicked();
	void					onGroupLeaveButtonClicked();
	void					onGroupCreateButtonClicked();
	void					onGroupSearchButtonClicked();
	void					onGroupTitlesButtonClicked();
	void					onGroupInviteButtonClicked();
	void					updateGroupButtons();

	LLTabContainer*			mTabContainer;

	LLFriendObserver*		mObserver;
	BOOL					mAllowRightsChange;
	S32						mNumRightsChanged;
	LLCachedControl<bool>	mSortByUserName;
};


#endif // FS_CONTACTSFLOATER_H
