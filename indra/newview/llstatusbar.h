/** 
 * @file llstatusbar.h
 * @brief LLStatusBar class definition
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
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

#ifndef LL_LLSTATUSBAR_H
#define LL_LLSTATUSBAR_H

#include "llpanel.h"

// <FS:Ansariel> Pathfinding support
#include "llpathfindingnavmesh.h"

// "Constants" loaded from settings.xml at start time
extern S32 STATUS_BAR_HEIGHT;

class LLButton;
class LLLineEditor;
class LLMessageSystem;
class LLTextBox;
class LLTextEditor;
class LLUICtrl;
class LLUUID;
class LLFrameTimer;
class LLStatGraph;
class LLPanelVolumePulldown;
class LLPanelNearByMedia;
class LLIconCtrl;
class LLParcelChangeObserver;
class LLPanel;

// <FS:Ansariel> Pathfinding support
class LLPathfindingNavMeshStatus;

class LLRegionDetails
{
public:
	LLRegionDetails() :
		mRegionName("Unknown"),
		mParcelName("Unknown"),
		mAccessString("Unknown"),
		mX(0),
		mY(0),
		mZ(0),
		mArea (0),
		mForSale(FALSE),
		mOwner("Unknown"),
		mTraffic(0),
		mBalance(0),
		mPing(0)
	{
	}
	std::string mRegionName;
	std::string	mParcelName;
	std::string	mAccessString;
	S32		mX;
	S32		mY;
	S32		mZ;
	S32		mArea;
	BOOL	mForSale;
	std::string	mOwner;
	F32		mTraffic;
	S32		mBalance;
	std::string mTime;
	U32		mPing;
};

class LLStatusBar
:	public LLPanel
{
public:
	LLStatusBar(const LLRect& rect );
	/*virtual*/ ~LLStatusBar();
	
	/*virtual*/ void draw();

	/*virtual*/ BOOL handleRightMouseDown(S32 x, S32 y, MASK mask);
	/*virtual*/ BOOL postBuild();

	// MANIPULATORS
	void		setBalance(S32 balance);
	void		debitBalance(S32 debit);
	void		creditBalance(S32 credit);

	// Request the latest currency balance from the server
	static void sendMoneyBalanceRequest();

	void		setHealth(S32 percent);

	void setLandCredit(S32 credit);
	void setLandCommitted(S32 committed);

	void		refresh();
	void setVisibleForMouselook(bool visible);
		// some elements should hide in mouselook

	/**
	 * Updates location and parcel icons on login complete
	 */
	void handleLoginComplete();

	// ACCESSORS
	S32			getBalance() const;
	S32			getHealth() const;

	BOOL isUserTiered() const;
	S32 getSquareMetersCredit() const;
	S32 getSquareMetersCommitted() const;
	S32 getSquareMetersLeft() const;
	LLRegionDetails mRegionDetails;

	LLPanelNearByMedia* getNearbyMediaPanel() { return mPanelNearByMedia; }
	BOOL getAudioStreamEnabled() const;
	
	void setBackgroundColor( const LLColor4& color );

	// <FS:Zi> External toggles for media and streams
	void toggleMedia(bool enable);
	void toggleStream(bool enable);
	// </FS:Zi>

private:
	
	void onClickBuyCurrency();
	void onVolumeChanged(const LLSD& newvalue);

	void onMouseEnterVolume();
	void onMouseEnterNearbyMedia();
	void onClickScreen(S32 x, S32 y);
	void onModeChange(const LLSD& original_value, const LLSD& new_value);
	void onModeChangeConfirm(const LLSD& original_value, const LLSD& new_value, const LLSD& notification, const LLSD& response);

	static void onClickStreamToggle(void* data);		// ## Zi: Media/Stream separation
	static void onClickMediaToggle(void* data);
	
	class LLParcelChangeObserver;

	friend class LLParcelChangeObserver;

	enum EParcelIcon
	{
		VOICE_ICON = 0,
		FLY_ICON,
		PUSH_ICON,
		BUILD_ICON,
		SCRIPTS_ICON,
		DAMAGE_ICON,
		SEE_AVATARS_ICON,
		// <FS:Ansariel> Pathfinding support
		PATHFINDING_DIRTY_ICON,
		PATHFINDING_DISABLED_ICON,
		// </FS:Ansariel> Pathfinding support
		ICON_COUNT
	};

	/**
	 * Initializes parcel icons controls. Called from the constructor.
	 */
	void initParcelIcons();

	/**
	 * Handles clicks on the parcel icons.
	 */
	void onParcelIconClick(EParcelIcon icon);

	/**
	 * Handles clicks on the info buttons.
	 */
	void onInfoButtonClicked();

	/**
	 * Handles clicks on the parcel wl info button.
	 */
	void onParcelWLClicked();

	/**
	 * Called when agent changes the parcel.
	 */
	void onAgentParcelChange();

	/**
	 * Called when context menu item is clicked.
	 */
	void onContextMenuItemClicked(const LLSD::String& userdata);

	/**
	 * Called when user checks/unchecks Show Paracel Properies menu item
	 */
	void onNavBarShowParcelPropertiesCtrlChanged();

	/**
	 * Called when user checks/unchecks Show Coordinates menu item.
	 */
	void onNavBarShowCoordinatesCtrlChanged();

	/**
	 * Shorthand to call updateParcelInfoText() and updateParcelIcons().
	 */
	void update();

	/**
	 * Updates parcel info text (mParcelInfoText).
	 */
	void updateParcelInfoText();

	/**
	 * Updates the visibility state of the parcel icons according to parcel properties
	 */
	void updateParcelIconVisibility();

	void onBuyLandClicked();

	// <FS:Ansariel> Pathfinding support
	void onRegionBoundaryCrossed();
	void onNavMeshStatusChange(const LLPathfindingNavMeshStatus &pNavMeshStatus);
	void createNavMeshStatusListenerForCurrentRegion();
	// </FS:Ansariel> Pathfinding support
public:

	/**
	 * Updates parcel panel pos (mParcelPanel).
	 */
	void updateParcelPanel();

	/**
	 * Updates parcel icons (mParcelIcon[]).
	 */
	void updateParcelIcons();

	static void onClickBalance(void* data);

private:

	/**
	 * Updates health information (mDamageText).
	 */
	void updateHealth();

	/**
	 * Lays out all parcel icons starting from right edge of the mParcelInfoText + 11px
	 * (see screenshots in EXT-5808 for details).
	 */
	void layoutParcelIcons();

	/**
	 * Lays out a widget. Widget's rect mLeft becomes equal to the 'left' argument.
	 */
	S32 layoutWidget(LLUICtrl* ctrl, S32 left);

	/**
	 * Generates location string and returns it in the loc_str parameter.
	 */
	void buildLocationString(std::string& loc_str, bool show_coords);

	/**
	 * Sets new value to the mParcelInfoText and updates the size of the top bar.
	 */
	void setParcelInfoText(const std::string& new_text);

	void updateNetstatVisibility(const LLSD& data);

private:
	LLTextBox	*mTextBalance;
	LLTextBox	*mTextHealth;
	LLTextBox	*mTextTime;

	LLTextBox*	mTextParcelName;

	LLStatGraph *mSGBandwidth;
	LLStatGraph *mSGPacketLoss;

	LLView		*mBtnStats;
	LLButton	*mBtnVolume;
	LLTextBox	*mBoxBalance;
	LLButton	*mStreamToggle;		// ## Zi: Media/Stream separation
	LLButton	*mMediaToggle;
	LLView		*mScriptOut;
	LLFrameTimer	mClockUpdateTimer;

	S32				mBalance;
	S32				mHealth;
	S32				mSquareMetersCredit;
	S32				mSquareMetersCommitted;
	BOOL			mAudioStreamEnabled;
	BOOL			mShowParcelIcons;
	LLFrameTimer*	mBalanceTimer;
	LLFrameTimer*	mHealthTimer;
	LLPanelVolumePulldown* mPanelVolumePulldown;
	LLPanelNearByMedia*	mPanelNearByMedia;
	
	LLPanel* 				mParcelInfoPanel;
	LLButton* 				mInfoBtn;
	LLTextBox* 				mParcelInfoText;
	LLTextBox* 				mDamageText;
	LLIconCtrl*				mParcelIcon[ICON_COUNT];
	LLParcelChangeObserver*	mParcelChangedObserver;
	LLButton* 				mPWLBtn;
	LLPanel*				mBalancePanel;
	LLButton*				mBuyParcelBtn;
	LLPanel*				mTimeMediaPanel;

	boost::signals2::connection	mParcelPropsCtrlConnection;
	boost::signals2::connection	mShowCoordsCtrlConnection;
	boost::signals2::connection	mParcelMgrConnection;

	// <FS:Zi> Pathfinding rebake functions
	BOOL			rebakeRegionCallback(const LLSD& notification,const LLSD& response);

	LLFrameTimer	mRebakingTimer;
	BOOL			mPathfindingFlashOn;
	// </FS:Zi>
};

// *HACK: Status bar owns your cached money balance. JC
BOOL can_afford_transaction(S32 cost);

extern LLStatusBar *gStatusBar;

#endif
