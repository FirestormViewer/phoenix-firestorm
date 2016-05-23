/** 
 * @file llpanelpeople.h
 * @brief Side tray "People" panel
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

#ifndef LL_LLPANELPEOPLE_H
#define LL_LLPANELPEOPLE_H

#include <llpanel.h>

#include "llcallingcard.h" // for avatar tracker
#include "llfloaterwebcontent.h"
#include "llvoiceclient.h"

#include "llfloater.h"

// [FS:CR] Contact sets
#include "lggcontactsets.h"
#include <boost/signals2.hpp>

class LLAvatarList;
class LLAvatarName;
class LLFilterEditor;
class LLGroupList;
class LLMenuButton;
class LLTabContainer;
class LLNetMap;

// Firestorm declarations
class LLMenuGL;
class FSPanelRadar;

class LLPanelPeople 
	: public LLPanel
	, public LLVoiceClientStatusObserver
{
	LOG_CLASS(LLPanelPeople);
public:
	LLPanelPeople();
	virtual ~LLPanelPeople();

	/*virtual*/ BOOL 	postBuild();
	/*virtual*/ void	onOpen(const LLSD& key);
	/*virtual*/ bool	notifyChildren(const LLSD& info);
	// Implements LLVoiceClientStatusObserver::onChange() to enable call buttons
	// when voice is available
	/*virtual*/ void onChange(EStatusType status, const std::string &channelURI, bool proximal);

    bool mTryToConnectToFacebook;

	// internals
	class Updater;

private:

	typedef enum e_sort_oder {
		E_SORT_BY_NAME = 0,
		E_SORT_BY_STATUS = 1,
		E_SORT_BY_MOST_RECENT = 2,
		E_SORT_BY_DISTANCE = 3,
		E_SORT_BY_RECENT_SPEAKERS = 4,
		// <FS:Ansariel> FIRE-5283: Sort by username
		E_SORT_BY_USERNAME = 5,
	} ESortOrder;

    void				    removePicker();

	// methods indirectly called by the updaters
	void					updateFriendListHelpText();
	void					updateFriendList();
	bool					updateSuggestedFriendList();
	void					updateNearbyList();
	void					updateRecentList();
	void					updateFacebookList(bool visible);

	bool					isItemsFreeOfFriends(const uuid_vec_t& uuids);

	void					updateButtons();
	std::string				getActiveTabName() const;
	LLUUID					getCurrentItemID() const;
	void					getCurrentItemIDs(uuid_vec_t& selected_uuids) const;
	void					showGroupMenu(LLMenuGL* menu);
	void					setSortOrder(LLAvatarList* list, ESortOrder order, bool save = true);

	// UI callbacks
	void					onFilterEdit(const std::string& search_string);
	void					onGroupLimitInfo();
	void					onTabSelected(const LLSD& param);
	void					onAddFriendButtonClicked();
	void					onAddFriendWizButtonClicked();
	void					onDeleteFriendButtonClicked();
	void					onChatButtonClicked();
	void					onGearButtonClicked(LLUICtrl* btn);
	void					onImButtonClicked();
	void					onMoreButtonClicked();
	void					onAvatarListDoubleClicked(LLUICtrl* ctrl);
	void					onAvatarListCommitted(LLAvatarList* list);
	bool					onGroupPlusButtonValidate();
	void					onGroupMinusButtonClicked();
	void					onGroupPlusMenuItemClicked(const LLSD& userdata);

	void					onFriendsViewSortMenuItemClicked(const LLSD& userdata);
	void					onNearbyViewSortMenuItemClicked(const LLSD& userdata);
	void					onGroupsViewSortMenuItemClicked(const LLSD& userdata);
	void					onRecentViewSortMenuItemClicked(const LLSD& userdata);

	bool					onFriendsViewSortMenuItemCheck(const LLSD& userdata);
	bool					onRecentViewSortMenuItemCheck(const LLSD& userdata);
	bool					onNearbyViewSortMenuItemCheck(const LLSD& userdata);

	// misc callbacks
	static void				onAvatarPicked(const uuid_vec_t& ids, const std::vector<LLAvatarName> names);

	void					onFriendsAccordionExpandedCollapsed(LLUICtrl* ctrl, const LLSD& param, LLAvatarList* avatar_list);

	void					showAccordion(const std::string name, bool show);

	void					showFriendsAccordionsIfNeeded();

	void					onFriendListRefreshComplete(LLUICtrl*ctrl, const LLSD& param);

	bool					onConnectedToFacebook(const LLSD& data);

	void					setAccordionCollapsedByUser(LLUICtrl* acc_tab, bool collapsed);
	void					setAccordionCollapsedByUser(const std::string& name, bool collapsed);
	bool					isAccordionCollapsedByUser(LLUICtrl* acc_tab);
	bool					isAccordionCollapsedByUser(const std::string& name);

	LLTabContainer*			mTabContainer;
	LLAvatarList*			mOnlineFriendList;
	LLAvatarList*			mAllFriendList;
	LLAvatarList*			mSuggestedFriends;
	LLAvatarList*			mNearbyList;
	LLAvatarList*			mContactSetList;	// [FS:CR] Contact sets
	// <FS:Ansariel> Firestorm radar
	FSPanelRadar*			mRadarPanel;
	// </FS:Ansariel> Firestorm radar
	LLAvatarList*			mRecentList;
	LLGroupList*			mGroupList;
	LLNetMap*				mMiniMap;
	// <FS:Ansariel> FIRE-4740: Friend counter in people panel
	LLTabContainer*			mFriendsTabContainer;

	std::vector<std::string> mSavedOriginalFilters;
	std::vector<std::string> mSavedFilters;

	Updater*				mFriendListUpdater;
	// <FS:Ansariel> Firestorm radar
	//Updater*				mNearbyListUpdater;
	// </FS:Ansariel> Firestorm radar
	Updater*				mRecentListUpdater;
	Updater*				mButtonsUpdater;
    LLHandle< LLFloater >	mPicker;
	
	// [FS:CR] Contact sets
	bool					onContactSetsEnable(const LLSD& userdata);
	void					onContactSetsMenuItemClicked(const LLSD& userdata);
	void					handlePickerCallback(const uuid_vec_t& ids, const std::string& set);
	void					refreshContactSets();
	void					generateContactList(const std::string& contact_set);
	void					generateCurrentContactList();
	
	void					updateContactSets(LGGContactSets::EContactSetUpdate type);
	boost::signals2::connection mContactSetChangedConnection;
	LLComboBox* mContactSetCombo;
	// [/FS:CR]

	// <FS:Ansariel> FIRE-10839: Customizable radar columns (needed for Vintage skin)
	void					onColumnVisibilityChecked(const LLSD& userdata);
	bool					onEnableColumnVisibilityChecked(const LLSD& userdata);
	// </FS:Ansariel>
};

#endif //LL_LLPANELPEOPLE_H
