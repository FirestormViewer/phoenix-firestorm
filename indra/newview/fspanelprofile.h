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
 * The Phoenix Viewer Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.phoenixviewer.com
 * $/LicenseInfo$
 */

#ifndef FS_FSPANELPROFILE_H
#define FS_FSPANELPROFILE_H

#include "llfloater.h"
#include "llavatarpropertiesprocessor.h"
#include "llcallingcard.h"
#include "llvoiceclient.h"
#include "llmediactrl.h"
#include "llremoteparcelrequest.h"

class LLAvatarName;
class LLTextBox;
class LLMediaCtrl;
class LLTabContainer;
class LLTextureCtrl;

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
	 * Processes data received from server.
	 */
	/*virtual*/ void processProperties(void* data, EAvatarProcessorType type) = 0;

	/**
	 * Returns avatar ID.
	 */
	virtual const LLUUID& getAvatarId() { return mAvatarId; }

	/**
	 * Clears panel data if viewing avatar info for first time and sends update data request.
	 */
	virtual void onOpen(const LLSD& key);

	/*virtual*/ ~FSPanelProfileTab();

protected:

	FSPanelProfileTab();

	virtual void onMapButtonClick();

private:

	LLUUID mAvatarId;
};


/**
* Panel for displaying Avatar's second life related info.
*/
class FSPanelProfile
	: public FSPanelProfileTab
	, public LLFriendObserver
	, public LLVoiceClientStatusObserver
{
public:
	FSPanelProfile();
	/*virtual*/ ~FSPanelProfile();

	/*virtual*/ void onOpen(const LLSD& key);

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

	/**
	 * Sends update data request to server.
	 */
    void updateData();

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

	/**
	 * Opens "Pay Resident" dialog.
	 */
	void pay();

	/**
	 * opens inventory and IM for sharing items
	 */
	void share();

	/**
	 * Add/remove resident to/from your block list.
	 */
	void toggleBlock();

	void kick();
	void freeze();
	void unfreeze();
	void csr();
	
    bool enableAddFriend();
    bool enableCall();
	bool enableShowOnMap();
	bool enableTeleport();
	bool enableBlock();
	bool enableUnblock();

	void onAddFriendButtonClick();
	void onIMButtonClick();
	void onCallButtonClick();
	void onTeleportButtonClick();
	void onShareButtonClick();

	void onCopyToClipboard();
	void onCopyURI();
	void onGroupInvite();

	bool isGrantedToSeeOnlineStatus(bool online);

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

private:
    typedef std::map<std::string,LLUUID>    group_map_t;
    group_map_t             mGroups;
    void                    openGroupProfile();

    LLTextBox*          mStatusText;
    
    bool                mVoiceStatus;
};


/**
* Panel for displaying Avatar's picks.
*/

class FSPanelPick : public LLPanel, public LLAvatarPropertiesObserver, LLRemoteParcelInfoObserver
{
public:

    // Creates new panel
    static FSPanelPick* create();
    
    /*virtual*/ ~FSPanelPick();

	/*virtual*/ BOOL postBuild();
    
	void setAvatarId(const LLUUID& avatar_id);

	void setPickId(const LLUUID& id) { mPickId = id; }
	
	virtual void setPickName(const std::string& name);
    
	/*virtual*/ void processProperties(void* data, EAvatarProcessorType type);
    
    
	//This stuff we got from LLRemoteParcelObserver, in the last one we intentionally do nothing
	/*virtual*/ void processParcelInfo(const LLParcelData& parcel_data);
	/*virtual*/ void setParcelID(const LLUUID& parcel_id) { mParcelId = parcel_id; }
	/*virtual*/ void setErrorStatus(U32 status, const std::string& reason) {};

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

	virtual LLUUID& getAvatarId() { return mAvatarId; }
    const LLUUID& getPickId() { return mPickId; }

	/**
	 * Sets snapshot id.
	 *
	 * Will mark snapshot control as valid if id is not null.
	 * Will mark snapshot control as invalid if id is null. If null id is a valid value,
	 * you have to manually mark snapshot is valid.
	 */
	// virtual void setSnapshotId(const LLUUID& id);
	
	// virtual void setPickDesc(const std::string& desc);
	
	// virtual void setPickLocation(const std::string& location);
	
	virtual void setPosGlobal(const LLVector3d& pos) { mPosGlobal = pos; }
	virtual LLVector3d& getPosGlobal() { return mPosGlobal; }

	/**
	 * Callback for "Map" button, opens Map
	 */
	// void onClickMap();

	/**
	 * Callback for "Teleport" button, teleports user to Pick location.
	 */
	// void onClickTeleport();

protected:

	LLTextureCtrl*			mSnapshotCtrl;

	LLUUID mAvatarId;
	LLVector3d mPosGlobal;
	LLUUID mParcelId;
	LLUUID mPickId;
	LLUUID mRequestedId;
};

class FSPanelProfilePicks
	: public FSPanelProfileTab
{
public:
	FSPanelProfilePicks();
	/*virtual*/ ~FSPanelProfilePicks();

    /*virtual*/ BOOL postBuild();

	/*virtual*/ void onOpen(const LLSD& key);

	/*virtual*/ void processProperties(void* data, EAvatarProcessorType type);

protected:
	/**
	 * Sends update data request to server.
	 */
    void updateData();
    
private:
    LLTabContainer* mTabContainer;
    LLUICtrl* mNoItemsLabel;
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

    /*virtual*/ BOOL postBuild();

	/*virtual*/ void processProperties(void* data, EAvatarProcessorType type);

    /*virtual*/ void handleMediaEvent(LLPluginClassMedia* self, EMediaEvent event);

    // void load(std::string url);
    // static void onURLKeystroke(LLLineEditor* editor, void* data);
    // static void onCommitLoad(LLUICtrl* ctrl, void* data);
    // static void onCommitURL(LLUICtrl* ctrl, void* data);
    // static void onClickWebProfileHelp(void *);

    void onAvatarNameCache(const LLUUID& agent_id, const LLAvatarName& av_name);

protected:
	void onCommitLoad(LLUICtrl* ctrl);
	void onCommitWebProfile(LLUICtrl* ctrl);
    
private:
    std::string         mURLHome;
    std::string         mURLWebProfile;
    LLMediaCtrl*        mWebBrowser;
    LLFrameTimer        mPerformanceTimer;
    bool                mFirstNavigate;
    bool                mCompleted;
};


/**
* Panel for displaying Avatar's first life related info.
*/
class FSPanelProfileFirstLife
	: public FSPanelProfileTab
{
public:
	FSPanelProfileFirstLife();
	/*virtual*/ ~FSPanelProfileFirstLife();;

	/*virtual*/ void processProperties(void* data, EAvatarProcessorType type);
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

	/*virtual*/ void updateData();

protected:
	/**
	 * Fills rights data for friends.
	 */
	void fillRightsData();

	void rightsConfirmationCallback(const LLSD& notification,
			const LLSD& response, S32 rights);
	void confirmModifyRights(bool grant, S32 rights);
	void onCommitRights();
	void onCommitNotes();
	void enableCheckboxes(bool enable);
};

#endif // FS_FSPANELPROFILE_H
