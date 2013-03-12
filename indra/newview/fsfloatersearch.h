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
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#ifndef FS_FLOATERSEARCH_H
#define FS_FLOATERSEARCH_H

#include "llfloater.h"
#include "lliconctrl.h"
#include "lltexteditor.h"
#include "lltexturectrl.h"
#include "llremoteparcelrequest.h"
#include "llavatarpropertiesprocessor.h"
#include "llgroupmgr.h"
#include "llavatarnamecache.h"
#include "llmediactrl.h"

class LLRemoteParcelInfoObserver;
class LLAvatarPropertiesObserver;
class LLGroupMgrObserver;

class FSFloaterSearch
:	public LLFloater
{
	LOG_CLASS(FSFloaterSearch);
public:
	struct SearchQuery : public LLInitParam::Block<SearchQuery>
	{
		Optional<std::string> category;
		Optional<std::string> query;
		
		SearchQuery();
	};
	
	struct _Params : public LLInitParam::Block<_Params, LLFloater::Params>
	{
		Optional<SearchQuery> search;
	};
	
	typedef LLSDParamAdapter<_Params> Params;
	
	FSFloaterSearch(const Params& key);
	virtual ~FSFloaterSearch();
	virtual void onOpen(const LLSD& key);
	BOOL postBuild();
	
	void avatarNameUpdatedCallback(const LLUUID& id, const LLAvatarName& av_name);
	void groupNameUpdatedCallback(const LLUUID& id, const std::string& name, bool is_group);
	void onSelectedItem(const LLUUID& selected_item, int type);
	void onSelectedEvent(const S32 selected_event);
	void displayParcelDetails(const LLParcelData& parcel_data);
	void displayClassifiedDetails(LLAvatarClassifiedInfo*& c_info);
	void displayAvatarDetails(LLAvatarData*& avatar_data);
	void displayGroupDetails(LLGroupMgrGroupData*& group_data);
	void displayEventDetails(U32 eventId,
							 F64 eventEpoch,
							 const std::string& eventDateStr,
							 const std::string &eventName,
							 const std::string &eventDesc,
							 const std::string &simName,
							 U32 eventDuration,
							 U32 eventFlags,
							 U32 eventCover,
							 LLVector3d eventGlobalPos);
	void setLoadingProgress(bool started);
	static std::string filterShortWords(std::string query_string);
private:
	virtual void onClose(bool app_quitting);
	const LLUUID& getSelectedID() { return mSelectedID; }
	LLVector3d	mParcelGlobal;
	LLUUID		mSelectedID;
	U32			mEventID;
	bool		mHasSelection;
	
	void resetVerbs();
	void flushDetails();
	void onTabChange();
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
	
	LLPanel*		mDetailsPanel;
	LLTextEditor*	mDetailTitle;
	LLTextEditor*	mDetailDesc;
	LLTextEditor*	mDetailAux1;
	LLTextEditor*	mDetailAux2;
	LLTextEditor*	mDetailLocation;
	LLTextureCtrl*	mDetailSnapshot;
	LLIconCtrl*		mDetailMaturity;
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
	void editKeystroke(class LLLineEditor* caller, void* user_data);
	
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
	void editKeystroke(class LLLineEditor* caller, void* user_data);
	
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
	void editKeystroke(class LLLineEditor* caller, void* user_data);
	
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
	void editKeystroke(class LLLineEditor* caller, void* user_data);
	
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

class FSPanelSearchWeb : public LLPanel, public LLViewerMediaObserver
{
	LOG_CLASS(FSFloaterSearch);
public:
	FSPanelSearchWeb();
	/*virtual*/ BOOL postBuild();
	void loadURL(const FSFloaterSearch::SearchQuery &query);
private:
	~FSPanelSearchWeb();
	
	LLMediaCtrl*	mWebBrowser;
	LLSD			mCategoryPaths;
};

#endif // FS_FLOATERSEARCH_H
