/**
 * @file fsfloaterplacedetails.h
 * @brief Class for the place details floater in Firestorm
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2012 Ansariel Hiller @ Second Life
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
 */

#ifndef FS_FLOATERPLACEDETAILS_H
#define FS_FLOATERPLACEDETAILS_H

#include "llfloater.h"
#include "llpointer.h"
#include "llsafehandle.h"
#include "llinventory.h"
#include "llinventorymodel.h"
#include "llparcelselection.h"
#include "llviewerinventory.h"

class LLLandmark;
class LLMenuButton;
class LLPanelLandmarkInfo;
class LLPanelPickEdit;
class LLPanelPlaceProfile;
struct LLParcelData;
class LLToggleableMenu;
class LLVector3d;
class FSPlaceDetailsInventoryObserver;
class FSPlaceDetailsRemoteParcelInfoObserver;
class FSPlaceDetailsPlacesParcelObserver;

class FSFloaterPlaceDetails : public LLFloater
{
public:
	FSFloaterPlaceDetails(const LLSD& seed);
	virtual ~FSFloaterPlaceDetails();

	/*virtual*/ BOOL postBuild();
	/*virtual*/ void onOpen(const LLSD& key);

	void showAddedLandmarkInfo(const uuid_vec_t& items);
	void changedGlobalPos(const LLVector3d& global_pos);
	void changedParcelSelection();
	void processParcelDetails(const LLParcelData& parcel_details);
	void togglePickPanel(BOOL visible);

private:
	enum ePlaceDisplayInfo
	{
		NONE,
		LANDMARK,
		CREATE_LANDMARK,
		REMOTE_PLACE,
		TELEPORT_HISTORY_ITEM,
		AGENT
	};

	void onLandmarkLoaded(LLLandmark* landmark);
	void onTeleportButtonClicked();
	void onShowOnMapButtonClicked();
	void onEditButtonClicked();
	void onCancelButtonClicked();
	void onSaveButtonClicked();
	void onCloseButtonClicked();
	void onOverflowButtonClicked();
	void onOverflowMenuItemClicked(const LLSD& param);
	bool onOverflowMenuItemEnable(const LLSD& param);

	void updateVerbs();
	void setItem(LLInventoryItem* item);
	void onSLURLBuilt(std::string& slurl);

	LLPanelPickEdit*						mPickPanel;
	LLPanelLandmarkInfo*					mPanelLandmarkInfo;
	LLPanelPlaceProfile*					mPanelPlaceInfo;

	LLSafeHandle<LLParcelSelection>			mParcel;
	LLPointer<LLInventoryItem>				mItem;
	FSPlaceDetailsInventoryObserver*		mInventoryObserver;
	FSPlaceDetailsRemoteParcelInfoObserver*	mRemoteParcelObserver;
	FSPlaceDetailsPlacesParcelObserver*		mParcelObserver;
	LLMenuButton*							mOverflowBtn;
	LLToggleableMenu*						mPlaceMenu;
	LLToggleableMenu*						mLandmarkMenu;
	LLTimer									mResetInfoTimer;

	bool				mIsInEditMode;
	bool				mIsInCreateMode;
	LLVector3d			mGlobalPos;
	ePlaceDisplayInfo	mDisplayInfo;

	boost::signals2::connection mAgentParcelChangedConnection;
};

#endif // FS_FLOATERPLACEDETAILS_H