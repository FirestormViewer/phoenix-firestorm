/**
 * @file fspanelprofile.h
 * @brief Legacy Profile Floater
 *
 * $LicenseInfo:firstyear=2012&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2012, Kadah Coba
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

#ifndef FS_PANELPROFILE_H
#define FS_PANELPROFILE_H

#include "llfloater.h"
#include "llavatarpropertiesprocessor.h"
#include "llcallingcard.h"
#include "llvoiceclient.h"
#include "llmediactrl.h"
#include "llremoteparcelrequest.h"
#include "rlvhandler.h"

class LLAvatarName;
class LLCheckBoxCtrl;
class LLTabContainer;
class LLTextBox;
class LLTextureCtrl;
class LLMediaCtrl;
class LLGroupList;
class LLTextBase;
class LLMenuButton;
class LLLineEditor;
class LLTextEditor;
class FSPanelClassifieds;

/**
* Base class for any Profile View or My Profile Panel.
*/
class FSPanelProfileTab
	: public LLPanel
	, public LLAvatarPropertiesObserver
{
public:

	/**
	 * Sets avatar ID, sets panel as observer of avatar related info replies from server.
	 */
	virtual void setAvatarId(const LLUUID& id);

	/**
	 * Sends update data request to server.
	 */
	virtual void updateData() { }

	/**
	 * Processes data received from server.
	 */
	virtual void processProperties(void* data, EAvatarProcessorType type) = 0;

	/**
	 * Returns avatar ID.
	 */
	const LLUUID& getAvatarId() { return mAvatarId; }

	/**
	 * Clears panel data if viewing avatar info for first time and sends update data request.
	 */
	virtual void onOpen(const LLSD& key);

	/*virtual*/ ~FSPanelProfileTab();

	void setEmbedded(bool embedded) { mEmbedded = embedded; }

protected:

	FSPanelProfileTab();

	virtual void enableControls();

	// mLoading: false: Initial state, can request
	//           true:  Data requested, skip duplicate requests (happens due to LLUI's habit of repeated callbacks)
	// mLoaded:  false: Initial state, show loading indicator
	//           true:  Data recieved, which comes in a single message, hide indicator
	bool getIsLoading() { return mLoading; }
	void setIsLoading() { mLoading = true; }
	bool getIsLoaded() { return mLoaded; }
	void resetLoading() { mLoading = false; mLoaded = false; }
	
	const bool getEmbedded() const { return mEmbedded; }
	
	const bool getSelfProfile() const { return mSelfProfile; }

	void setApplyProgress(bool started);

private:

	LLUUID	mAvatarId;
	bool	mLoading;
	bool	mLoaded;
	bool	mEmbedded;
	bool	mSelfProfile;
};


/**
* Panel for displaying Avatar's second life related info.
*/
class FSPanelProfileSecondLife
	: public FSPanelProfileTab
	, public LLFriendObserver
	, public LLVoiceClientStatusObserver
{
public:
	FSPanelProfileSecondLife();
	/*virtual*/ ~FSPanelProfileSecondLife();

	/*virtual*/ void onOpen(const LLSD& key);

	/**
	 * Saves changes.
	 */
	void apply(LLAvatarData* data);

	/**
	 * LLFriendObserver trigger
	 */
	virtual void changed(U32 mask);

	// Implements LLVoiceClientStatusObserver::onChange() to enable the call
	// button when voice is available
	/*virtual*/ void onChange(EStatusType status, const std::string &channelURI, bool proximal);

	/*virtual*/ void setAvatarId(const LLUUID& id);

	/*virtual*/ BOOL postBuild();

	/*virtual*/ void processProperties(void* data, EAvatarProcessorType type);
	
	void resetData();

	/**
	 * Sends update data request to server.
	 */
	/*virtual*/ void updateData();

	void onAvatarNameCache(const LLUUID& agent_id, const LLAvatarName& av_name);

protected:
	/**
	 * Process profile related data received from server.
	 */
	virtual void processProfileProperties(const LLAvatarData* avatar_data);

	/**
	 * Processes group related data received from server.
	 */
	virtual void processGroupProperties(const LLAvatarGroups* avatar_groups);

	/**
	 * Fills common for Avatar profile and My Profile fields.
	 */
	virtual void fillCommonData(const LLAvatarData* avatar_data);

	/**
	 * Fills partner data.
	 */
	virtual void fillPartnerData(const LLAvatarData* avatar_data);

	/**
	 * Fills account status.
	 */
	virtual void fillAccountStatus(const LLAvatarData* avatar_data);

	void onMapButtonClick();

	/**
	 * Opens "Pay Resident" dialog.
	 */
	void pay();

	/**
	 * Add/remove resident to/from your block list.
	 */
	void toggleBlock();

	void updateButtons();

	void onAddFriendButtonClick();
	void onIMButtonClick();
	void onTeleportButtonClick();

	void onCopyToClipboard();
	void onCopyURI();
	void onGroupInvite();

	bool isGrantedToSeeOnlineStatus();

	/**
	 * Displays avatar's online status if possible.
	 *
	 * Requirements from EXT-3880:
	 * For friends:
	 * - Online when online and privacy settings allow to show
	 * - Offline when offline and privacy settings allow to show
	 * - Else: nothing
	 * For other avatars:
	 *  - Online when online and was not set in Preferences/"Only Friends & Groups can see when I am online"
	 *  - Else: Offline
	 */
	void updateOnlineStatus();
	void processOnlineStatus(bool online);

	virtual void enableControls();

private:
	void onClickSetName();
	void onAvatarNameCacheSetName(const LLUUID& id, const LLAvatarName& av_name);

private:
	typedef std::map<std::string, LLUUID> group_map_t;
	group_map_t			mGroups;
	void				openGroupProfile();

	LLTextBox*			mStatusText;
	LLGroupList*		mGroupList;
	LLCheckBoxCtrl*		mShowInSearchCheckbox;
	LLTextureCtrl*		mSecondLifePic;
	LLTextBase*			mDescriptionEdit;
	LLButton*			mTeleportButton;
	LLButton*			mShowOnMapButton;
	LLButton*			mBlockButton;
	LLButton*			mUnblockButton;
	LLButton*			mDisplayNameButton;
	LLButton*			mAddFriendButton;
	LLButton*			mGroupInviteButton;
	LLButton*			mPayButton;
	LLButton*			mIMButton;
	LLMenuButton*		mOverflowButton;

	bool				mVoiceStatus;

	boost::signals2::connection mRlvBehaviorCallbackConnection;
	void updateRlvRestrictions(ERlvBehaviour behavior);
};


/**
* Panel for displaying Avatar's web profile and home page.
*/
class FSPanelProfileWeb
	: public FSPanelProfileTab
	, public LLViewerMediaObserver
{
public:
	FSPanelProfileWeb();
	/*virtual*/ ~FSPanelProfileWeb();

	/*virtual*/ void onOpen(const LLSD& key);

	/*virtual*/ BOOL postBuild();

	/*virtual*/ void processProperties(void* data, EAvatarProcessorType type);
	
	void resetData();

	/**
	 * Saves changes.
	 */
	void apply(LLAvatarData* data);

	/**
	 * Loads web profile.
	 */
	/*virtual*/ void updateData();

	/*virtual*/ void handleMediaEvent(LLPluginClassMedia* self, EMediaEvent event);

	void onAvatarNameCache(const LLUUID& agent_id, const LLAvatarName& av_name);

	virtual void enableControls();

protected:
	void onCommitLoad(LLUICtrl* ctrl);

private:
	std::string			mURLHome;
	std::string			mURLWebProfile;
	LLMediaCtrl*		mWebBrowser;

	LLFrameTimer		mPerformanceTimer;
	bool				mFirstNavigate;
};

/**
* Panel for displaying Avatar's interests.
*/
class FSPanelProfileInterests
	: public FSPanelProfileTab
{
public:
	FSPanelProfileInterests();
	/*virtual*/ ~FSPanelProfileInterests();

	/*virtual*/ void onOpen(const LLSD& key);

	/*virtual*/ BOOL postBuild();

	/*virtual*/ void processProperties(void* data, EAvatarProcessorType type);
	
	void resetData();

	/**
	 * Saves changes.
	 */
	virtual void apply();

protected:
	virtual void enableControls();

private:
	LLCheckBoxCtrl*	mWantChecks[8];
	LLCheckBoxCtrl*	mSkillChecks[6];
	LLLineEditor*	mWantToEditor;
	LLLineEditor*	mSkillsEditor;
	LLLineEditor*	mLanguagesEditor;
};


/**
* Panel for displaying Avatar's picks.
*/

class FSPanelPick
	: public FSPanelProfileTab
	, public LLRemoteParcelInfoObserver
{
public:

	// Creates new panel
	static FSPanelPick* create();

	/*virtual*/ ~FSPanelPick();

	/*virtual*/ BOOL postBuild();

	void setAvatarId(const LLUUID& avatar_id);

	void setPickId(const LLUUID& id) { mPickId = id; }
	virtual LLUUID& getPickId() { return mPickId; }

	virtual void setPickName(const std::string& name);
	const std::string getPickName();

	/*virtual*/ void processProperties(void* data, EAvatarProcessorType type);

	/**
	 * Saves changes.
	 */
	virtual void apply();

	void updateTabLabel(const std::string& title);

	//This stuff we got from LLRemoteParcelObserver, in the last one we intentionally do nothing
	/*virtual*/ void processParcelInfo(const LLParcelData& parcel_data);
	/*virtual*/ void setParcelID(const LLUUID& parcel_id) { mParcelId = parcel_id; }
	/*virtual*/ void setErrorStatus(S32 status, const std::string& reason) {};

protected:

	FSPanelPick();

	/**
	 * Sends remote parcel info request to resolve parcel name from its ID.
	 */
	void sendParcelInfoRequest();

	/**
	* "Location text" is actually the owner name, the original
	* name that owner gave the parcel, and the location.
	*/
	static std::string createLocationText(
		const std::string& owner_name,
		const std::string& original_name,
		const std::string& sim_name,
		const LLVector3d& pos_global);

	/**
	 * Sets snapshot id.
	 *
	 * Will mark snapshot control as valid if id is not null.
	 * Will mark snapshot control as invalid if id is null. If null id is a valid value,
	 * you have to manually mark snapshot is valid.
	 */
	virtual void setSnapshotId(const LLUUID& id);
	virtual void setPickDesc(const std::string& desc);
	virtual void setPickLocation(const std::string& location);

	virtual void setPosGlobal(const LLVector3d& pos) { mPosGlobal = pos; }
	virtual LLVector3d& getPosGlobal() { return mPosGlobal; }

	/**
	 * Callback for "Map" button, opens Map
	 */
	void onClickMap();

	/**
	 * Callback for "Teleport" button, teleports user to Pick location.
	 */
	void onClickTeleport();

	/**
	 * Enables/disables "Save" button
	 */
	void enableSaveButton(BOOL enable);

	/**
	 * Called when snapshot image changes.
	 */
	void onSnapshotChanged();

	/**
	 * Callback for Pick snapshot, name and description changed event.
	 */
	void onPickChanged(LLUICtrl* ctrl);

	/**
	 * Resets panel and all cantrols to unedited state
	 */
	/*virtual*/ void resetDirty();

	/**
	 * Returns true if any of Pick properties was changed by user.
	 */
	/*virtual*/ BOOL isDirty() const;

	/**
	 * Callback for "Set Location" button click
	 */
	void onClickSetLocation();

	/**
	 * Callback for "Save" button click
	 */
	void onClickSave();

	std::string getLocationNotice();

	/**
	 * Sends Pick properties to server.
	 */
	void sendUpdate();

protected:

	LLTextureCtrl*		mSnapshotCtrl;
	LLLineEditor*		mPickName;
	LLTextEditor*		mPickDescription;
	LLButton*			mSetCurrentLocationButton;
	LLButton*			mSaveButton;

	LLVector3d			mPosGlobal;
	LLUUID				mParcelId;
	LLUUID				mPickId;
	LLUUID				mRequestedId;

	bool				mLocationChanged;
	bool				mNewPick;
	bool				mIsEditing;

	std::string mCurrentPickDescription;

	void onDescriptionFocusReceived();
};

class FSPanelProfilePicks
	: public FSPanelProfileTab
{
public:
	FSPanelProfilePicks();
	/*virtual*/ ~FSPanelProfilePicks();

	/*virtual*/ BOOL postBuild();

	/*virtual*/ void onOpen(const LLSD& key);

	void selectPick(const LLUUID& pick_id);

	/*virtual*/ void processProperties(void* data, EAvatarProcessorType type);
	
	void resetData();

	/**
	 * Saves changes.
	 */
	virtual void apply();

	/**
	 * Sends update data request to server.
	 */
	/*virtual*/ void updateData();

private:
	void onClickNewBtn();
	void onClickDelete();
	void callbackDeletePick(const LLSD& notification, const LLSD& response);

	boost::signals2::connection mRlvBehaviorCallbackConnection;
	void updateRlvRestrictions(ERlvBehaviour behavior, ERlvParamType type);

	bool canAddNewPick();
	bool canDeletePick();

	LLTabContainer*	mTabContainer;
	LLUICtrl*		mNoItemsLabel;
	LLButton*		mNewButton;
	LLButton*		mDeleteButton;

	LLUUID			mPickToSelectOnLoad;
};



/**
* Panel for displaying Avatar's first life related info.
*/
class FSPanelProfileFirstLife
	: public FSPanelProfileTab
{
public:
	FSPanelProfileFirstLife();
	/*virtual*/ ~FSPanelProfileFirstLife();

	/*virtual*/ void onOpen(const LLSD& key);

	/*virtual*/ BOOL postBuild();

	/*virtual*/ void processProperties(void* data, EAvatarProcessorType type);
	
	void resetData();

	/**
	 * Saves changes.
	 */
	void apply(LLAvatarData* data);

protected:
	virtual void enableControls();
	void onDescriptionFocusReceived();

	LLTextEditor*	mDescriptionEdit;
	LLTextureCtrl*	mPicture;

	bool			mIsEditing;
	std::string		mCurrentDescription;
};

/**
 * Panel for displaying Avatar's notes and modifying friend's rights.
 */
class FSPanelAvatarNotes
	: public FSPanelProfileTab
	, public LLFriendObserver
{
public:
	FSPanelAvatarNotes();
	/*virtual*/ ~FSPanelAvatarNotes();

	virtual void setAvatarId(const LLUUID& id);

	/**
	 * LLFriendObserver trigger
	 */
	virtual void changed(U32 mask);

	/*virtual*/ void onOpen(const LLSD& key);

	/*virtual*/ BOOL postBuild();

	/*virtual*/ void processProperties(void* data, EAvatarProcessorType type);
	
	void resetData();

	/*virtual*/ void updateData();

	/**
	 * Saves changes.
	 */
	virtual void apply();

protected:
	/**
	 * Fills rights data for friends.
	 */
	void fillRightsData();

	void rightsConfirmationCallback(const LLSD& notification, const LLSD& response, S32 rights);
	void confirmModifyRights(bool grant, S32 rights);
	void onCommitRights();
	void onCommitNotes();
	void enableCheckboxes(bool enable);

	LLCheckBoxCtrl*		mOnlineStatus;
	LLCheckBoxCtrl*		mMapRights;
	LLCheckBoxCtrl*		mEditObjectRights;
	LLTextEditor*		mNotesEditor;
};


/*
*
* Container panel for the profile tabs
*/
class FSPanelProfile
	: public FSPanelProfileTab
{
public:
	FSPanelProfile();
	/*virtual*/ ~FSPanelProfile();

	/*virtual*/ BOOL postBuild();

	/*virtual*/ void updateData();

	/*virtual*/ void processProperties(void* data, EAvatarProcessorType type);

	/*virtual*/ void onOpen(const LLSD& key);

	/**
	 * Saves changes.
	 */
	void apply();

	void showPick(const LLUUID& pick_id = LLUUID::null);
	bool isPickTabSelected();

	void showClassified(const LLUUID& classified_id = LLUUID::null, bool edit = false);

private:
	void onAvatarNameCache(const LLUUID& agent_id, const LLAvatarName& av_name);
	void onTabChange();
	
	FSPanelProfileSecondLife*	mPanelSecondlife;
	FSPanelProfileWeb*			mPanelWeb;
	FSPanelProfileInterests*	mPanelInterests;
	FSPanelProfilePicks*		mPanelPicks;
	FSPanelClassifieds*			mPanelClassifieds;
	FSPanelProfileFirstLife*	mPanelFirstlife;
	FSPanelAvatarNotes*			mPanelNotes;
	LLTabContainer*				mTabContainer;

	boost::signals2::connection	mAvatarNameCacheConnection;
};

#endif // FS_PANELPROFILE_H
