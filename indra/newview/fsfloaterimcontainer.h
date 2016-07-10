/** 
 * @file fsfloaterimcontainer.h
 * @brief Multifloater containing active IM sessions in separate tab container tabs
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

// Original file: llimfloatercontainer.h


#ifndef FS_FLOATERIMCONTAINER_H
#define FS_FLOATERIMCONTAINER_H

#include "llmultifloater.h"
#include "llimview.h"

class LLTabContainer;

class FSFloaterIMContainer : public LLMultiFloater, public LLIMSessionObserver
{
public:
	FSFloaterIMContainer(const LLSD& seed);
	virtual ~FSFloaterIMContainer();
	
	/*virtual*/ BOOL postBuild();
	/*virtual*/ void onOpen(const LLSD& key);
	/*virtual*/ void onClose(bool app_quitting);
	void onCloseFloater(LLUUID& id);
	/*virtual*/ void draw();
	
	/*virtual*/ void addFloater(LLFloater* floaterp, 
								BOOL select_added_floater, 
								LLTabContainer::eInsertionPoint insertion_point = LLTabContainer::END);
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-12-11 (Catznip-3.2.0d) | Added: Catznip-3.2.0d
	/*virtual*/ void removeFloater(LLFloater* floaterp);
// [/SL:KB]
	bool hasFloater(LLFloater* floaterp);

	void addNewSession(LLFloater* floaterp);

	static FSFloaterIMContainer* findInstance();
	static FSFloaterIMContainer* getInstance();

	virtual void setVisible(BOOL b);
	/*virtual*/ void setMinimized(BOOL b);

	void onNewMessageReceived(const LLSD& data); // public so nearbychat can call it directly. TODO: handle via callback. -AO

	virtual void sessionAdded(const LLUUID& session_id, const std::string& name, const LLUUID& other_participant_id, BOOL has_offline_msg);
	virtual void sessionActivated(const LLUUID& session_id, const std::string& name, const LLUUID& other_participant_id) {};
	virtual void sessionVoiceOrIMStarted(const LLUUID& session_id) {};
	virtual void sessionRemoved(const LLUUID& session_id);
	virtual void sessionIDUpdated(const LLUUID& old_session_id, const LLUUID& new_session_id);

	static void reloadEmptyFloaters();
	void initTabs();

	void addFlashingSession(const LLUUID& session_id);

private:
	enum eVoiceState
	{
		VOICE_STATE_NONE,
		VOICE_STATE_UNKNOWN,
		VOICE_STATE_CONNECTED,
		VOICE_STATE_NOT_CONNECTED,
		VOICE_STATE_ERROR
	};

	LLFloater*	getCurrentVoiceFloater();
	void		onVoiceStateIndicatorChanged(const LLSD& data);

	LLFloater*	mActiveVoiceFloater;
	LLTimer		mActiveVoiceUpdateTimer;
	eVoiceState	mCurrentVoiceState;
	bool		mForceVoiceStateUpdate;

	typedef std::map<LLUUID, LLFloater*> avatarID_panel_map_t;
	avatarID_panel_map_t mSessions;
	boost::signals2::connection mNewMessageConnection;

	void checkFlashing();
	uuid_vec_t	mFlashingSessions;

	bool		mIsAddingNewSession;

// [SL:KB] - Patch: UI-TabRearrange | Checked: 2012-05-05 (Catznip-3.3.0)
protected:
	void onIMTabRearrange(S32 tab_index, LLPanel* tab_panel);
// [/SL:KB]

};

#endif // FS_FLOATERIMCONTAINER_H
