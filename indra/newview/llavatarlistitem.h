/** 
 * @file llavatarlistitem.h
 * @brief avatar list item header file
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

#ifndef LL_LLAVATARLISTITEM_H
#define LL_LLAVATARLISTITEM_H

#include "llpanel.h"
#include "lloutputmonitorctrl.h"
#include "llbutton.h"
#include "lltextbox.h"
#include "llstyle.h"

#include "llcallingcard.h" // for LLFriendObserver
#include "llavatarpropertiesprocessor.h"

class LLAvatarIconCtrl;
class LLAvatarName;
class LLIconCtrl;
class LLUICtrl;

class LLAvatarListItem : public LLPanel, public LLFriendObserver, public LLAvatarPropertiesObserver
{
public:
	struct Params : public LLInitParam::Block<Params, LLPanel::Params>
	{
		Optional<LLStyle::Params>	default_style,
									voice_call_invited_style,
									voice_call_joined_style,
									voice_call_left_style,
									online_style,
									offline_style,
									group_moderator_style;


		Optional<S32>				name_right_pad;

		Params();
	};

	typedef enum e_item_state_type {
		IS_DEFAULT,
		IS_VOICE_INVITED,
		IS_VOICE_JOINED,
		IS_VOICE_LEFT,
		IS_ONLINE,
		IS_OFFLINE,
		IS_GROUPMOD,
	} EItemState;

	/**
	 * Creates an instance of LLAvatarListItem.
	 *
	 * It is not registered with LLDefaultChildRegistry. It is built via LLUICtrlFactory::buildPanel
	 * or via registered LLCallbackMap depend on passed parameter.
	 * 
	 * @param not_from_ui_factory if true instance will be build with LLUICtrlFactory::buildPanel 
	 * otherwise it should be registered via LLCallbackMap before creating.
	 */
	LLAvatarListItem(bool not_from_ui_factory = true);
	virtual ~LLAvatarListItem();

	virtual BOOL postBuild();

	/**
	 * Processes notification from speaker indicator to update children when indicator's visibility is changed.
	 */
	virtual S32	notifyParent(const LLSD& info);
	virtual void onMouseLeave(S32 x, S32 y, MASK mask);
	virtual void onMouseEnter(S32 x, S32 y, MASK mask);
	virtual void setValue(const LLSD& value);
	virtual void changed(U32 mask); // from LLFriendObserver

	void setOnline(bool online);
	void updateAvatarName(); // re-query the name cache
	void updateAvatarProperties(); // (re)query the avatar's server properties
	void setAvatarName(const std::string& name);
	void setAvatarToolTip(const std::string& tooltip);
	void setHighlight(const std::string& highlight);
	void setState(EItemState item_style);
	void setAvatarId(const LLUUID& id, const LLUUID& session_id, bool ignore_status_changes = false, bool is_resident = true);
	void setLastInteractionTime(U32 secs_since);
	//Show/hide profile/info btn, translating speaker indicator and avatar name coordinates accordingly
	void setShowProfileBtn(bool show);
	void setShowInfoBtn(bool show);
	void setShowVoiceVolume(bool show);
	void showSpeakingIndicator(bool show);
	void showRange(bool show);
	void setRange(F32 distance);
	F32  getRange();
	void setPosition(LLVector3d globalPos);
	LLVector3d getPosition();
	void setAvStatus(S32 statusFlags);
	S32  getAvStatus();
	void setFirstSeen(time_t seenTime);
	time_t	 getFirstSeen();
	void setLastZOffsetTime(time_t oTime);
	time_t	getLastZOffsetTime();
	void setZOffset(F32 offset);
	F32  getZOffset();
	void showDisplayName(bool show, bool updateName = true);
	void showFirstSeen(bool show);
	void showStatusFlags(bool show);
	void showAvatarAge(bool show);
	std::string getAvatarAge();
	std::string getSeen();
	void showPaymentStatus(bool show);
	void updateFirstSeen();
	void showUsername(bool show, bool updateName = true);
	void setShowPermissions(bool show);
	void showLastInteractionTime(bool show);
	void setAvatarIconVisible(bool visible);
// [RLVa:KB] - Checked: 2010-04-05 (RLVa-1.2.2a) | Added: RLVa-1.2.0d
	void setRlvCheckShowNames(bool fRlvCheckShowNames) { mRlvCheckShowNames = fRlvCheckShowNames; }
// [/RLVa:KB]
	
	const LLUUID& getAvatarId() const;
	std::string getAvatarName() const;
	std::string getUserName() const { return mUserName; }
	std::string getAvatarToolTip() const;
	bool getShowingBothNames() const;

	void onInfoBtnClick();
	void onVolumeChange(const LLSD& data);
	void onProfileBtnClick();
	void onPermissionOnlineClick();
	void onPermissionEditMineClick();
	void onPermissionMapClick();
	
	//Radar state-specific
	// [Ansariel: Colorful radar]
	void setUseRangeColors(bool UseRangeColors);
	void setShoutRangeColor(const LLUIColor& shoutRangeColor);
	void setBeyondShoutRangeColor(const LLUIColor& beyondShoutRangeColor);
	// [/Ansariel: Colorful radar]

	/*virtual*/ BOOL handleDoubleClick(S32 x, S32 y, MASK mask);
// [SL:KB] - Patch: UI-AvatarListDndShare | Checked: 2011-06-19 (Catznip-2.6.0c) | Added: Catznip-2.6.0c
	/*virtual*/ BOOL handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop, EDragAndDropType cargo_type, void *cargo_data, 
	                                   EAcceptance *accept, std::string& tooltip_msg);
// [/SL:KB]
 
protected:
	/**
	 * Contains indicator to show voice activity. 
	 */
	LLOutputMonitorCtrl* mSpeakingIndicator;

	LLAvatarIconCtrl* mAvatarIcon;

	/// Indicator for permission to see me online.
	LLButton* mBtnPermissionOnline;
	/// Indicator for permission to see my position on the map.
	LLButton* mBtnPermissionMap;
	/// Indicator for permission to edit my objects.
	LLButton* mBtnPermissionEditMine;
	/// Indicator for permission to edit their objects.
	LLIconCtrl* mIconPermissionEditTheirs;
	void confirmModifyRights(bool grant, S32 rights);
	void rightsConfirmationCallback(const LLSD& notification,
									const LLSD& response, S32 rights);
	
	//radar_specific
	LLTextBox* mNearbyRange;
	bool mShowDisplayName;
	bool mShowUsername;
	bool mShowFirstSeen;
	
	// [Ansariel: Colorful radar]
	bool mUseRangeColors;
	LLUIColor mShoutRangeColor;
	LLUIColor mBeyondShoutRangeColor;
	// [/Ansariel: Colorful radar]

private:

	typedef enum e_online_status {
		E_OFFLINE,
		E_ONLINE,
		E_UNKNOWN,
	} EOnlineStatus;

	/**
	 * Enumeration of item elements in order from right to left.
	 * 
	 * updateChildren() assumes that indexes are in the such order to process avatar icon easier.
	 * assume that child indexes are in back order: the first in Enum is the last (right) in the item
	 *
	 * @see updateChildren()
	 */
	typedef enum e_avatar_item_child {
		ALIC_SPEAKER_INDICATOR,
		ALIC_PROFILE_BUTTON,
		ALIC_INFO_BUTTON,
		ALIC_VOLUME_SLIDER,
		ALIC_PERMISSION_ONLINE,
		ALIC_PERMISSION_MAP,
		ALIC_PERMISSION_EDIT_MINE,
		ALIC_PERMISSION_EDIT_THEIRS,
		ALIC_INTERACTION_TIME,
		ALIC_NAME,
		ALIC_ICON,
		ALIC_COUNT,
	} EAvatarListItemChildIndex;

	void setNameInternal(const std::string& name, const std::string& highlight);
	void onAvatarNameCache(const LLAvatarName& av_name);
	void processProperties(void* data, EAvatarProcessorType type);


	std::string formatSeconds(U32 secs);

	typedef std::map<EItemState, LLColor4> icon_color_map_t;
	static icon_color_map_t& getItemIconColorMap();

	/**
	 * Initializes widths of all children to use them while changing visibility of any of them.
	 *
	 * @see updateChildren()
	 */
	static void initChildrenWidths(LLAvatarListItem* self);

	/**
	 * Updates position and rectangle of visible children to fit all available item's width.
	 */
	void updateChildren();

	/**
	 * Update visibility of active permissions icons.
	 *
	 * Need to call updateChildren() afterwards to sort out their layout.
	 */
	bool showPermissions(bool visible);

	/**
	 * Gets child view specified by index.
	 *
	 * This method implemented via switch by all EAvatarListItemChildIndex values.
	 * It is used to not store children in array or vector to avoid of increasing memory usage.
	 */
	LLView* getItemChildView(EAvatarListItemChildIndex child_index);

	LLTextBox* mAvatarName;
	LLTextBox* mLastInteractionTime;
	LLTextBox* mFirstSeenDisplay;
	LLTextBox* mAvatarAgeDisplay;
	LLIconCtrl* mPaymentStatus;
	LLStyle::Params mAvatarNameStyle;
	
	LLButton* mInfoBtn;
	LLButton* mProfileBtn;
	LLUICtrl* mVoiceSlider;

	LLUUID	mAvatarId;
	time_t	mFirstSeen;
	time_t	mLastZOffsetTime;
	F32	mZOffset;
	S32	mAvStatus;
	LLVector3d mAvPosition;
	S32	mAvatarAge;
	F32 	mDistance;
	
	std::string mHighlihtSubstring; // substring to highlight
	EOnlineStatus mOnlineStatus;
	//Flag indicating that info/profile button shouldn't be shown at all.
	//Speaker indicator and avatar name coords are translated accordingly
	bool mShowInfoBtn;
	bool mShowVoiceVolume;
	bool mShowProfileBtn;
// [RLVa:KB] - Checked: 2010-04-05 (RLVa-1.2.2a) | Added: RLVa-1.2.0d
	bool mRlvCheckShowNames;
// [/RLVa:KB]
	std::string mUserName; //KC - username cache used for sorting

	/// indicates whether to show icons representing permissions granted
	bool mShowPermissions;
	bool mShowStatusFlags;
	bool mShowAvatarAge;
	bool mShowPaymentStatus;
	
	/// true when the mouse pointer is hovering over this item
	bool mHovered;

	static bool	sStaticInitialized; // this variable is introduced to improve code readability
	static S32  sLeftPadding; // padding to first left visible child (icon or name)
	static S32  sNameRightPadding; // right padding from name to next visible child

	/**
	 * Contains widths of each child specified by EAvatarListItemChildIndex
	 * including padding to the next right one.
	 *
	 * @see initChildrenWidths()
	 */
	static S32 sChildrenWidths[ALIC_COUNT];

};

#endif //LL_LLAVATARLISTITEM_H
