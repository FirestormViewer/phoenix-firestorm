/** 
 * @file llnavigationbar.h
 * @brief Navigation bar definition
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

#ifndef LL_LLNAVIGATIONBAR_H
#define LL_LLNAVIGATIONBAR_H

// <FS:Zi> Make navigation bar part of the UI
// #include "llpanel.h"
// </FS:Zi>
#include "llbutton.h"

class LLLocationInputCtrl;
class LLMenuGL;
class LLSearchEditor;
class LLSearchComboBox;

/**
 * This button is able to handle click-dragging mouse event.
 * It has appropriated signal for this event.
 * Dragging direction can be set from xml attribute called 'direction'
 * 
 * *TODO: move to llui?  
 */

class LLPullButton: public LLButton
{
	LOG_CLASS(LLPullButton);

public:
	struct Params: public LLInitParam::Block<Params, LLButton::Params>
	{
		Optional<std::string> direction; // left, right, down, up

		Params() 
		:	direction("direction", "down")
		{
		}
	};
	
	/*virtual*/ BOOL handleMouseDown(S32 x, S32 y, MASK mask);

	/*virtual*/ BOOL handleMouseUp(S32 x, S32 y, MASK mask);

	/*virtual*/ void onMouseLeave(S32 x, S32 y, MASK mask);

	boost::signals2::connection setClickDraggingCallback(const commit_signal_t::slot_type& cb);

protected:
	friend class LLUICtrlFactory;
	// convert string name into direction vector
	void setDirectionFromName(const std::string& name);
	LLPullButton(const LLPullButton::Params& params);

	commit_signal_t mClickDraggingSignal;
	LLVector2 mLastMouseDown;
	LLVector2 mDraggingDirection;
};

/**
 * Web browser-like navigation bar.
 */ 
class LLNavigationBar
// <FS:Zi> Make navigation bar part of the UI
	// :	public LLPanel, public LLSingleton<LLNavigationBar>, private LLDestroyClass<LLNavigationBar>
	:	public LLSingleton<LLNavigationBar>
// </FS:Zi>
{
	LOG_CLASS(LLNavigationBar);
	friend class LLDestroyClass<LLNavigationBar>;
	
public:
	LLNavigationBar();
	virtual ~LLNavigationBar();
	
	// <FS:Zi> Make navigation bar part of the UI
	// /*virtual*/ void	draw();
	// /*virtual*/ BOOL handleRightMouseDown(S32 x, S32 y, MASK mask);
	// /*virtual*/ BOOL	postBuild();
	// </FS:Zi>
//	/*virtual*/ void	setVisible(BOOL visible); // <FS:Zi> Is done inside XUI now, using visibility_control

	void handleLoginComplete();
	void clearHistoryCache();

// <FS:Zi> No size calculations in code please. XUI handles it all now with visibility_control
// 	int getDefNavBarHeight();
// 	int getDefFavBarHeight();
// </FS:Zi>
	
// [RLVa:KB] - Checked: 2014-03-23 (RLVa-1.4.10)
	void refreshLocationCtrl();
// [/RLVa:KB]
private:
	// the distance between navigation panel and favorites panel in pixels
	// const static S32 FAVBAR_TOP_PADDING = 10;	// <FS:Zi> No size calculations in code please. XUI handles it all now with visibility_control

	void rebuildTeleportHistoryMenu();
	void showTeleportHistoryMenu(LLUICtrl* btn_ctrl);
	void invokeSearch(std::string search_text);
	// callbacks
	void onTeleportHistoryMenuItemClicked(const LLSD& userdata);
	void onTeleportHistoryChanged();
	// [FS:CR] FIRE-12333
	//void onBackButtonClicked();
	void onBackButtonClicked(LLUICtrl* ctrl);
	void onBackOrForwardButtonHeldDown(LLUICtrl* ctrl, const LLSD& param);
	void onNavigationButtonHeldUp(LLButton* nav_button);
	// [FS:CR] FIRE-12333
	//void onForwardButtonClicked();
	//void onHomeButtonClicked();
	void onForwardButtonClicked(LLUICtrl* ctrl);
	void onHomeButtonClicked(LLUICtrl* ctrl);
	void onLocationSelection();
	void onLocationPrearrange(const LLSD& data);
	void onSearchCommit();
	void onTeleportFinished(const LLVector3d& global_agent_pos);
	void onTeleportFailed();
	void onRegionNameResponse(
			std::string typed_location,
			std::string region_name,
			LLVector3 local_coords,
			U64 region_handle, const std::string& url,
			const LLUUID& snapshot_id, bool teleport);

	void fillSearchComboBox();
	
	void onClickedSkyBtn();	// <FS:CR> FIRE-11847

	// <FS:Zi> Make navigation bar part of the UI
	// static void destroyClass()
	// {
	// 	if (LLNavigationBar::instanceExists())
	// 	{
	// 		LLNavigationBar::getInstance()->setEnabled(FALSE);
	// 	}
	// }
	// </FS:Zi>

	LLMenuGL*					mTeleportHistoryMenu;
	LLPullButton*				mBtnBack;
	LLPullButton*				mBtnForward;
	LLButton*					mBtnHome;
	LLSearchComboBox*			mSearchComboBox;
	LLLocationInputCtrl*		mCmbLocation;
	// <FS:Zi> No size calculations in code please. XUI handles it all now with visibility_control
	//LLRect						mDefaultNbRect;
	//LLRect						mDefaultFpRect;
	// </FS:Zi>
	boost::signals2::connection	mTeleportFailedConnection;
	boost::signals2::connection	mTeleportFinishConnection;
	boost::signals2::connection	mHistoryMenuConnection;
	// if true, save location to location history when teleport finishes
	bool						mSaveToLocationHistory;

// <FS:Zi> Make navigation bar part of the UI
public:
	void clearHistory();
	LLView* getView();		// for RLVa to disable "Home" button

protected:
	void onRightMouseDown(S32 x,S32 y,MASK mask);
	void setupPanel();

	LLView* mView;
// </FS:Zi>
};

#endif
