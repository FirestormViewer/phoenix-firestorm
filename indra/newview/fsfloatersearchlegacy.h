/**
 * @file fsfloatersearch.h
 * @brief Firestorm search definitions
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
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.phoenixviewer.com
 * $/LicenseInfo$
 */

#ifndef FS_FSFLOATERSEARCH_H
#define FS_FSFLOATERSEARCH_H

#include "llfloater.h"
#include "lltexteditor.h"
#include "lltexturectrl.h"
#include "llremoteparcelrequest.h"
#include "llavatarpropertiesprocessor.h"
#include "llgroupmgr.h"
#include "llavatarnamecache.h"

class LLRemoteParcelInfoObserver;
class LLAvatarPropertiesObserver;
class LLGroupMgrObserver;

class FSFloaterSearch
:	public LLFloater
{
	LOG_CLASS(FSFloaterSearch);
public:
	FSFloaterSearch(const LLSD& key);
	virtual ~FSFloaterSearch();
	virtual void onOpen(const LLSD& key);
	BOOL postBuild();
	
	void avatarNameUpdatedCallback(const LLUUID& id, const LLAvatarName& av_name);
	void groupNameUpdatedCallback(const LLUUID& id, const std::string& name, bool is_group);
	void onSelectedItem(const LLUUID& selected_item, int type);
	void onSelectedEvent(const S32 selected_event);
	void sendParcelDetails(const LLParcelData& parcel_data);
	void sendClassifiedDetails(LLAvatarClassifiedInfo*& c_info);
	void sendAvatarDetails(LLAvatarData*& avatar_data);
	void sendGroupDetails(LLGroupMgrGroupData*& group_data);
	void sendEventDetails(U32 eventId,
						  F64 eventEpoch,
						  const std::string& eventDateStr,
						  const std::string &eventName,
						  const std::string &eventDesc,
						  U32 eventDuration,
						  U32 eventFlags,
						  LLVector3d eventGlobalPos);
	void setLoadingProgress(bool started);
private:
	const LLUUID& getSelectedID() { return mSelectedID; }
	LLVector3d	mParcelGlobal;
	LLUUID		mSelectedID;
	U32			mEventID;
	
	void resetVerbs();
	void onBtnPeopleProfile();
	void onBtnPeopleIM();
	void onBtnPeopleFriend();
	void onBtnGroupProfile();
	void onBtnGroupChat();
	void onBtnGroupJoin();
	void onBtnEventReminder();
	void onBtnTeleport();
	void onBtnMap();
	
	LLRemoteParcelInfoObserver* mRemoteParcelObserver;
	LLAvatarPropertiesObserver* mAvatarPropertiesObserver;
	LLGroupMgrObserver* mGroupPropertiesRequest;
	
	LLTextEditor*	mDetailTitle;
	LLTextEditor*	mDetailDesc;
	LLTextureCtrl*	mDetailSnapshot;
	LLPanel*		mDetailsPanel;
};

///////////////////////////////
//       Search Panels       //
///////////////////////////////

class FSPanelSearchPeople : public LLPanel
{
	LOG_CLASS(FSFloaterSearch);
public:
	FSPanelSearchPeople();
	void onSearchPanelOpen(FSFloaterSearch* parent);
	static void processSearchReply(LLMessageSystem* msg, void**);
protected:
private:
	/*virtual*/ BOOL postBuild();
	/*virtual*/ ~FSPanelSearchPeople();

	static FSPanelSearchPeople* sInstance;
	
	void onBtnFind();
	void onSelectItem();
	void onBtnNext();
	void onBtnBack();
	
	void find();
	void resetSearch();
	S32  showNextButton(S32);
	void setLoadingProgress(bool started);
	
	const LLUUID& getQueryID() const { return mQueryID; }
	
	int			mNumResultsReturned;
	S32			mStartSearch;
	S32			mResultsReceived;
	LLSD		mResultsContent;
	LLUUID		mQueryID;
	FSFloaterSearch* mParent;
};

class FSPanelSearchGroups : public LLPanel
{
	LOG_CLASS(FSFloaterSearch);
public:
	FSPanelSearchGroups();
	void onSearchPanelOpen(FSFloaterSearch* parent);
	static void processSearchReply(LLMessageSystem* msg, void**);
protected:
private:
	/*virtual*/ BOOL postBuild();
	/*virtual*/ ~FSPanelSearchGroups();
	static FSPanelSearchGroups* sInstance;
	
	void onBtnFind();
	void onSelectItem();
	void onBtnNext();
	void onBtnBack();
	
	void find();
	void resetSearch();
	S32  showNextButton(S32);
	void setLoadingProgress(bool started);
	
	const LLUUID& getQueryID() const { return mQueryID; }
	
	int			mNumResultsReturned;
	S32			mStartSearch;
	S32			mResultsReceived;
	LLSD		mResultsContent;
	LLUUID		mQueryID;
	FSFloaterSearch* mParent;
};

class FSPanelSearchPlaces : public LLPanel
{
	LOG_CLASS(FSFloaterSearch);
public:
	FSPanelSearchPlaces();
	void onSearchPanelOpen(FSFloaterSearch* parent);
	static void processSearchReply(LLMessageSystem* msg, void**);
protected:
private:
	/*virtual*/ BOOL postBuild();
	/*virtual*/ ~FSPanelSearchPlaces();
	static FSPanelSearchPlaces* sInstance;
	
	void onBtnFind();
	void onSelectItem();
	void onBtnNext();
	void onBtnBack();
	
	void find();
	void resetSearch();
	S32  showNextButton(S32);
	void setLoadingProgress(bool started);
	
	const LLUUID& getQueryID() const { return mQueryID; }
	
	int			mNumResultsReturned;
	S32			mStartSearch;
	S32			mResultsReceived;
	LLSD		mResultsContent;
	LLUUID		mQueryID;
	FSFloaterSearch* mParent;
};

class FSPanelSearchLand : public LLPanel
{
	LOG_CLASS(FSFloaterSearch);
public:
	FSPanelSearchLand();
	void onSearchPanelOpen(FSFloaterSearch* parent);
	static void processSearchReply(LLMessageSystem* msg, void**);
protected:
private:
	/*virtual*/ BOOL postBuild();
	/*virtual*/ ~FSPanelSearchLand();
	static FSPanelSearchLand* sInstance;
	
	void onBtnFind();
	void onSelectItem();
	void onBtnNext();
	void onBtnBack();
	
	void find();
	void resetSearch();
	S32  showNextButton(S32);
	void setLoadingProgress(bool started);
	
	const LLUUID& getQueryID() const { return mQueryID; }
	
	int			mNumResultsReturned;
	S32			mStartSearch;
	S32			mResultsReceived;
	LLSD		mResultsContent;
	LLUUID		mQueryID;
	FSFloaterSearch* mParent;
};

class FSPanelSearchClassifieds : public LLPanel
{
	LOG_CLASS(FSFloaterSearch);
public:
	FSPanelSearchClassifieds();
	void onSearchPanelOpen(FSFloaterSearch* parent);
	static void processSearchReply(LLMessageSystem* msg, void**);
protected:
private:
	/*virtual*/ BOOL postBuild();
	/*virtual*/ ~FSPanelSearchClassifieds();
	static FSPanelSearchClassifieds* sInstance;
	
	void onBtnFind();
	void onSelectItem();
	void onBtnNext();
	void onBtnBack();
	
	void find();
	void resetSearch();
	S32  showNextButton(S32);
	void setLoadingProgress(bool started);
	
	const LLUUID& getQueryID() const { return mQueryID; }
	
	int			mNumResultsReturned;
	S32			mStartSearch;
	S32			mResultsReceived;
	LLSD		mResultsContent;
	LLUUID		mQueryID;
	FSFloaterSearch* mParent;
};

class FSPanelSearchEvents : public LLPanel
{
	LOG_CLASS(FSFloaterSearch);
public:
	FSPanelSearchEvents();
	void onSearchPanelOpen(FSFloaterSearch* parent);
	static void processSearchReply(LLMessageSystem* msg, void**);
protected:
private:
	/*virtual*/ BOOL postBuild();
	/*virtual*/ ~FSPanelSearchEvents();
	static FSPanelSearchEvents* sInstance;
	
	void onBtnFind();
	void onSelectItem();
	void onBtnNext();
	void onBtnBack();
	void onBtnTomorrow();
	void onBtnYesterday();
	void onBtnToday();
	
	void find();
	void setDay(S32 day);
	void onSearchModeChanged();
	void resetSearch();
	S32  showNextButton(S32);
	void setLoadingProgress(bool started);
	
	const LLUUID& getQueryID() const { return mQueryID; }
	
	int			mNumResultsReturned;
	S32			mResultsReceived;
	S32			mStartSearch;
	S32			mDay;
	LLSD		mResultsContent;
	LLUUID		mQueryID;
	FSFloaterSearch* mParent;
};

#endif // FS_FSFLOATERSEARCH_H
