/**
 * @file fsfloatersearchlegacy.h
 * @brief Legacy search floater definitions
 *
 * $LicenseInfo:firstyear=2012&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2012, Cinder Roxley <cinder@cinderblocks.biz>
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

#ifndef FS_FSFLOATERSEARCHLEGACY_H
#define FS_FSFLOATERSEARCHLEGACY_H

#include "llfloater.h"
#include "lltexteditor.h"
#include "lltexturectrl.h"
#include "llavatarpropertiesprocessor.h"
#include "llgroupmgr.h"
#include "llavatarnamecache.h"

class LLRemoteParcelInfoObserver;
class LLGroupMgrObserver;

class FSFloaterSearchLegacy
:	public LLFloater,
	public LLAvatarPropertiesObserver
{
	LOG_CLASS(FSFloaterSearchLegacy);
public:
	FSFloaterSearchLegacy(const LLSD& key);
	virtual ~FSFloaterSearchLegacy();
	virtual void processProperties(void* data, EAvatarProcessorType type);
	
	BOOL postBuild();
	
	const LLUUID& getQueryID() const { return mQueryID; }
	static void sendSearchQuery(LLMessageSystem* msg, const LLUUID& query_id, const std::string& text, U32 flags, S32 query_start);
	static void sendPlacesSearchQuery(LLMessageSystem* msg, const LLUUID& query_id, const std::string& text, U32 flags, S8 category, S32 query_start);
	static void sendLandSearchQuery(LLMessageSystem* msg, const LLUUID& query_id, U32 flags, U32 category, S32 price, S32 area, S32 query_start);
	static void sendClassifiedsSearchQuery(LLMessageSystem* msg, const LLUUID& query_id, const std::string& text, U32 flags, U32 category, S32 query_start);
	static void processSearchPeopleReply(LLMessageSystem* msg, void**);
	static void processSearchGroupsReply(LLMessageSystem* msg, void**);
	static void processSearchPlacesReply(LLMessageSystem* msg, void**);
	static void processSearchLandReply(LLMessageSystem* msg, void**);
	static void processSearchEventsReply(LLMessageSystem* msg, void**);
	static void processSearchClassifiedsReply(LLMessageSystem* msg, void**);
	void sendParcelDetails(const LLVector3d &global_pos, const std::string& name, const std::string& desc, const LLUUID& snapshot_id);
	void avatarNameUpdatedCallback(const LLUUID& id, const LLAvatarName& av_name);
	void groupNameUpdatedCallback(const LLUUID& id, const std::string& name, bool is_group);
	void processGroupData();
	LLUUID& getSelectedID() { return mSelectedID; }
protected:
	S32 showNextButton(S32);
	void setAvatarID(const LLUUID& id);
	void setLoadingProgress(bool started);
	void setSelectionDetails(const std::string& title, const std::string& desc, const LLUUID& id);
private:
	static void	onModeSelect(LLUICtrl* ctrl, void *userdata);
	void refreshSearchJunk();
	void refreshActionButtons();
	void onSelectItem();
	
	void find();
	void setupSearch();
	void resetSearch();
	
	void onBtnFind();
	void onBtnNext();
	void onBtnBack();
	void onBtnPeopleProfile();
	void onBtnPeopleIM();
	void onBtnPeopleFriend();
	void onBtnGroupProfile();
	void onBtnGroupChat();
	void onBtnGroupJoin();
	void onBtnParcelProfile();
	void onBtnParcelTeleport();
	void onBtnParcelMap();
	
	BOOL isPeople();
	BOOL isGroups();
	BOOL isPlaces();
	BOOL isLand();
	BOOL isEvents();
	BOOL isClassifieds();
	
	LLUUID		mQueryID;
	int			mNumResultsReturned;
	S32			mStartSearch;
	S32			mResultsPerPage;
	S32			mResultsReceived;
	LLSD		mResultsContent;
	LLUUID		mSelectedID;
	LLVector3d	mParcelGlobal;
	std::set<LLUUID>	mParcelIDs;
	
	LLRemoteParcelInfoObserver* mRemoteParcelObserver;
	LLGroupMgrObserver* mGroupPropertiesRequest;
	
	LLTextEditor*	mDetailTitle;
	LLTextEditor*	mDetailDesc;
	LLTextureCtrl* mSnapshotCtrl;
	
	enum e_search_mode
	{
		SM_PEOPLE,
		SM_GROUPS,
		SM_PLACES,
		SM_LAND,
		SM_EVENTS,
		SM_CLASSIFIEDS
	} ESearchMode;
};

#endif // FS_FSFLOATERSEARCHLEGACY_H
