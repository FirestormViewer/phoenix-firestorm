/** 
* @file fscontactlist.h
* @brief Firestorm contacts list control
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

#ifndef FS_FSCONTACTLIST_H
#define FS_FSCONTACTLIST_H

#include "llpanel.h"

#include "llcallingcard.h" // for avatar tracker
#include "llvoiceclient.h"
#include "llavatarnamecache.h"

// class FSContactListSet;
class LLAvatarList;
class LLAvatarName;
class LLAccordionCtrl;
class LLAccordionCtrlTab;
// class LLFilterEditor;
class LLMenuButton;
class LLToggleableMenu;
class LLMenuGL;
class LLTextBox;


typedef enum e_sort_oder
{
	E_SORT_BY_NAME = 0,
	E_SORT_BY_USERNAME = 1,
	E_SORT_BY_STATUS = 2,
	E_SORT_BY_STATUSUSERNAME = 3
} ESortOrder;



/// convenience class for holding keyframes mapped to sliders
class FSContactSet : public LLPanel
{
	LOG_CLASS(FSContactSet);
public:
	// FSContactSet(const Params& p = LLPanel::Params());
	FSContactSet();
	virtual ~FSContactSet();
	
	/*virtual*/ 			BOOL postBuild();
	
	void					update( const uuid_vec_t& buddies_uuids );
	void					update();
	
	LLAvatarList*			getList() { return mAvatarList; }
	
	void					setTab(LLAccordionCtrlTab* tab);
	LLAccordionCtrlTab*		getTab() { return mTab; }
	
	void					setName(std::string name);
	
	void					setHandleFriendsAll( bool handel_all ) { mHandleFriendsAll = handel_all; }
	
	void					setSortOrder( ESortOrder order );
	
protected:
	friend class LLUICtrlFactory;
	
	
	void					onViewOptionChecked(LLSD::String param);
	bool					isViewOptionChecked(LLSD::String param);
	bool					isViewOptionEnabled(LLSD::String param);
	void					forceUpdate();
	
	LLAccordionCtrlTab*		mTab;
	LLAvatarList*			mAvatarList;
	LLMenuButton*			mViewOptionMenuButton;
	LLToggleableMenu*		mViewOptionMenu;
	LLTextBox*				mInfoTextLabel;
	bool					mShowOffline;
	bool					mHandleFriendsAll;
	LLUUID					mFriendSubFolderID;
	std::string				mName;
	
	// this contains all friends for this set
	uuid_vec_t				mFriendsIDs;
};


class FSContactList : public LLPanel, public LLVoiceClientStatusObserver
{
	LOG_CLASS(FSContactList);
public:

	FSContactList(const Params& p = LLPanel::Params());
	virtual	~FSContactList();
	
	/*virtual*/ BOOL postBuild();
	
	bool isFriendsLoaded() { return mIsFriendsLoaded; }
	
	
private:
	
	
	
	typedef std::map<LLUUID, FSContactSet*> contact_set_map_t;
	contact_set_map_t mContactSets;
	
	void							onCreateContactSet();
	static void						onContactSetCreate(const LLSD& notification, const LLSD& response);
	
	FSContactSet*					addContactSet(const LLUUID& sub_folder_ID, const std::string& set_name);
	FSContactSet*					getConstactSet(const LLUUID& sub_folder_ID);
	FSContactSet*					getOrCreateConstactSet(const LLUUID& sub_folder_ID);
	
	FSContactSet*					mAllFriendList;
	
public:

	// Implements LLVoiceClientStatusObserver::onChange() to enable call buttons
	// when voice is available
	/*virtual*/ void onChange(EStatusType status, const std::string &channelURI, bool proximal);
	
	// internals
	class Updater;

private:

	

	// methods indirectly called by the updaters
	// void					updateFriendListHelpText();
	void					updateFriendList();
	void					forceUpdate();
	
	

	bool					isItemsFreeOfFriends(const uuid_vec_t& uuids);

	void					updateButtons();
	LLUUID					getCurrentItemID() const;
	void					getCurrentItemIDs(uuid_vec_t& selected_uuids) const;
	void					buttonSetEnabled(const std::string& btn_name, bool enabled);
	void					buttonSetAction(const std::string& btn_name, const commit_signal_t::slot_type& cb);
	
	
	void					onViewOptionChecked(LLSD::String param);
	bool					isViewOptionChecked(LLSD::String param);
	// bool					isViewOptionEnabled(LLSD::String param);
	void					setSortOrder(ESortOrder order, bool save = true);
	
private:
	// UI callbacks
	// void					onFilterEdit(const std::string& search_string);
	void					onViewProfileButtonClicked();
	void					onAddFriendWizButtonClicked();
	void					onGlobalVisToggleButtonClicked();
	void					onDeleteFriendButtonClicked();
	void					onImButtonClicked();
	void					onCallButtonClicked();
	void					onTeleportButtonClicked();
	void					onShareButtonClicked();
	void					onAvatarListDoubleClicked(LLUICtrl* ctrl);
	void					onAvatarListCommitted(LLAvatarList* list);
	
	void					showPermissions(bool visible);

	// misc callbacks
	static void				onAvatarPicked(const uuid_vec_t& ids, const std::vector<LLAvatarName> names);

	// void					onFriendsAccordionExpandedCollapsed(LLUICtrl* ctrl, const LLSD& param, LLAvatarList* avatar_list);

	// void					showAccordion(const std::string name, bool show);

	// void					showFriendsAccordionsIfNeeded();

	// void					onFriendListRefreshComplete(LLUICtrl*ctrl, const LLSD& param);

	// void					setAccordionCollapsedByUser(LLUICtrl* acc_tab, bool collapsed);
	// void					setAccordionCollapsedByUser(const std::string& name, bool collapsed);
	// bool					isAccordionCollapsedByUser(LLUICtrl* acc_tab);
	// bool					isAccordionCollapsedByUser(const std::string& name);
	
	void					onFriendsLoaded();

	LLHandle<LLView>		mFriendsViewSortMenuHandle;

	Updater*				mFriendListUpdater;
	Updater*				mButtonsUpdater;

 	LLMenuButton*			mFriendsGearButton;

	std::string				mFilterSubString;
	std::string				mFilterSubStringOrig;
	
	LLMenuButton*			mViewOptionMenuButton;
	LLToggleableMenu*		mViewOptionMenu;
	LLAccordionCtrl*		mAccordion;
	
	bool					mIsFriendsLoaded;
};


#endif // FS_FSCONTACTLIST_H
