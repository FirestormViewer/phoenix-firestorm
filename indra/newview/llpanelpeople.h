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
#include "llvoiceclient.h"
#include "llavatarnamecache.h"
#include "llscrolllistctrl.h"
#include "fsradarlistctrl.h"
#include <map>
#include <time.h>

class LLAvatarList;
class LLAvatarName;
class LLFilterEditor;
class LLGroupList;
class LLTabContainer;
class LLMenuButton;
class LLMenuGL;

const U32	MAX_AVATARS_PER_ALERT = 6; // maximum number of UUIDs we can cram into a single channel radar alert message
const U32	COARSE_OFFSET_INTERVAL = 7; // seconds after which we query the bridge for a coarse location adjustment
const U32   MAX_OFFSET_REQUESTS = 60; // 2048 / UUID size, leaving overhead space
const U32	NAMEFORMAT_DISPLAYNAME = 0;
const U32	RADAR_CHAT_MIN_SPACING = 6; //minimum delay between radar chat messages

const U32	NAMEFORMAT_USERNAME = 1;
const U32	NAMEFORMAT_DISPLAYNAME_USERNAME = 2;
const U32	NAMEFORMAT_USERNAME_DISPLAYNAME = 3;

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
	void	teleportToAvatar(LLUUID targetAv);
	void	requestRadarChannelAlertSync();
	// Implements LLVoiceClientStatusObserver::onChange() to enable call buttons
	// when voice is available
	/*virtual*/ void onChange(EStatusType status, const std::string &channelURI, bool proximal);

// [RLVa:KB] - Checked: 2010-04-05 (RLVa-1.2.0d) | Added: RLVa-1.2.0d
	LLAvatarList* getNearbyList() { return mNearbyList; }
// [/RLVa:KB]

	void	startTracking(const LLUUID& avatar_id);

	// internals
	class Updater;

private:

	typedef enum e_sort_oder {
		E_SORT_BY_NAME = 0,
		E_SORT_BY_STATUS = 1,
		E_SORT_BY_MOST_RECENT = 2,
		E_SORT_BY_DISTANCE = 3,
		E_SORT_BY_RECENT_SPEAKERS = 4,
	} ESortOrder;

	// methods indirectly called by the updaters
	void					updateFriendListHelpText();
	void					updateFriendList();
	void					updateNearbyList();
	void					updateRecentList();
	void					updateNearbyRange();
	void					updateTracking();
	void					checkTracking();
	
	bool					isItemsFreeOfFriends(const uuid_vec_t& uuids);

	void					updateButtons();
	std::string				getActiveTabName() const;
	LLUUID					getCurrentItemID() const;
	void					getCurrentItemIDs(uuid_vec_t& selected_uuids) const;
	void					buttonSetVisible(std::string btn_name, BOOL visible);
	void					buttonSetEnabled(const std::string& btn_name, bool enabled);
	void					buttonSetAction(const std::string& btn_name, const commit_signal_t::slot_type& cb);
	void					showGroupMenu(LLMenuGL* menu);
	void					setSortOrder(LLAvatarList* list, ESortOrder order, bool save = true);
	void					handleLimitRadarByRange(const LLSD& newalue);
	std::string				getRadarName(LLUUID avId);
	std::string				getRadarName(LLAvatarName avName);
	void					radarAlertMsg(const LLUUID& agent_id, const LLAvatarName& av_name,std::string postMsg);

	// UI callbacks
	void					onFilterEdit(const std::string& search_string);
	void					onTabSelected(const LLSD& param);
	void					onViewProfileButtonClicked();
	void					onAddFriendButtonClicked();
	void					onAddFriendWizButtonClicked();
	void					onGlobalVisToggleButtonClicked();
	void					onDeleteFriendButtonClicked();
	void					onGroupInfoButtonClicked();
	void					onChatButtonClicked();
	void					onImButtonClicked();
	void					onCallButtonClicked();
	void					onGroupCallButtonClicked();
	void					onTeleportButtonClicked();
	void					onShareButtonClicked();
	void					onMoreButtonClicked();
	void					onActivateButtonClicked();
	void					onRecentViewSortButtonClicked();
	void					onNearbyViewSortButtonClicked();
	void					onFriendsViewSortButtonClicked();
	void					onGroupsViewSortButtonClicked();
	void					onAvatarListDoubleClicked(LLUICtrl* ctrl);
	void					onNearbyListDoubleClicked(LLUICtrl* ctrl);
	void					onAvatarListCommitted(LLAvatarList* list);
	void					onRadarListDoubleClicked();
	void					onGroupPlusButtonClicked();
	void					onGroupMinusButtonClicked();
	void					onGroupPlusMenuItemClicked(const LLSD& userdata);

	void					onFriendsViewSortMenuItemClicked(const LLSD& userdata);
	void					onNearbyViewSortMenuItemClicked(const LLSD& userdata);
	void					onGroupsViewSortMenuItemClicked(const LLSD& userdata);
	void					onRecentViewSortMenuItemClicked(const LLSD& userdata);
	void					onRadarNameFmtClicked(const LLSD& userdata);
	bool					radarNameFmtCheck(const LLSD& userdata);
	void					onRadarReportToClicked(const LLSD& userdata);	// <FS:CR> Milkshake-style Radar Alerts
	bool					radarReportToCheck(const LLSD& userdata);	// <FS:CR> Milkshake-style Radar Alerts

	//returns false only if group is "none"
	bool					isRealGroup();
	bool					onFriendsViewSortMenuItemCheck(const LLSD& userdata);
	bool					onRecentViewSortMenuItemCheck(const LLSD& userdata);
	bool					onNearbyViewSortMenuItemCheck(const LLSD& userdata);

	// misc callbacks
	static void				onAvatarPicked(const uuid_vec_t& ids, const std::vector<LLAvatarName> names);

	void					onFriendsAccordionExpandedCollapsed(LLUICtrl* ctrl, const LLSD& param, LLAvatarList* avatar_list);

	void					showAccordion(const std::string name, bool show);

	void					showFriendsAccordionsIfNeeded();

	void					onFriendListRefreshComplete(LLUICtrl*ctrl, const LLSD& param);

	void					setAccordionCollapsedByUser(LLUICtrl* acc_tab, bool collapsed);
	void					setAccordionCollapsedByUser(const std::string& name, bool collapsed);
	bool					isAccordionCollapsedByUser(LLUICtrl* acc_tab);
	bool					isAccordionCollapsedByUser(const std::string& name);

	LLFilterEditor*			mFilterEditor;
	LLTabContainer*			mTabContainer;
	LLAvatarList*			mOnlineFriendList;
	LLAvatarList*			mAllFriendList;
	LLAvatarList*			mNearbyList;
	LLAvatarList*			mRecentList;
	LLGroupList*			mGroupList;
	LLRadarListCtrl*		mRadarList;
	LLNetMap*				mMiniMap;

	LLHandle<LLView>		mGroupPlusMenuHandle;
	LLHandle<LLView>		mNearbyViewSortMenuHandle;
	LLHandle<LLView>		mFriendsViewSortMenuHandle;
	LLHandle<LLView>		mGroupsViewSortMenuHandle;
	LLHandle<LLView>		mRecentViewSortMenuHandle;

	Updater*				mFriendListUpdater;
	Updater*				mNearbyListUpdater;
	Updater*				mRecentListUpdater;
	Updater*				mButtonsUpdater;

	LLMenuButton*			mNearbyGearButton;
	LLMenuButton*			mFriendsGearButton;
	LLMenuButton*			mGroupsGearButton;
	LLMenuButton*			mRecentGearButton;

	std::string				mFilterSubString;
	std::string				mFilterSubStringOrig;
	
	LLUIColor			mChatRangeColor;
	LLUIColor			mShoutRangeColor;
	LLUIColor			mFarRangeColor;
	
	struct radarFields 
	{
		std::string avName;
		F32			lastDistance;
		LLVector3d	lastGlobalPos;
		LLUUID		lastRegion;
		time_t		firstSeen;
		S32			lastStatus;
		U32			ZOffset;
		time_t		lastZOffsetTime;
		
	}; 
	std::multimap < LLUUID, radarFields > lastRadarSweep;
	std::vector <LLUUID>	mRadarEnterAlerts;
	std::vector <LLUUID>	mRadarLeaveAlerts;
	std::vector <LLUUID>	mRadarOffsetRequests;
	 	
	S32					mRadarFrameCount;
	bool				mRadarAlertRequest;
	F32					mRadarLastRequestTime;
	U32					mRadarLastBulkOffsetRequestTime;

	LLUUID				mTrackedAvatarId;
};

#endif //LL_LLPANELPEOPLE_H
