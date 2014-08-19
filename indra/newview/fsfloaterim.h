/** 
 * @file fsfloaterim.h
 * @brief LLIMFloater class definition
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

// Original file: llimfloater.h

#ifndef FS_FLOATERIM_H
#define FS_FLOATERIM_H

#include "llinstantmessage.h"
#include "lllogchat.h"
#include "lltooldraganddrop.h"
#include "lltransientdockablefloater.h"
#include "llvoicechannel.h"
#include "lllayoutstack.h"

class LLAvatarName;
class LLButton;		// support sysinfo button -Zi
class LLChatEntry;
class LLTextEditor;
class FSPanelChatControlPanel;
class FSChatHistory;
class LLInventoryItem;
class LLInventoryCategory;

typedef boost::signals2::signal<void(const LLUUID& session_id)> floater_showed_signal_t;

/**
 * Individual IM window that appears at the bottom of the screen,
 * optionally "docked" to the bottom tray.
 */
class FSFloaterIM : public LLTransientDockableFloater, LLVoiceClientStatusObserver, LLFriendObserver
{
	LOG_CLASS(FSFloaterIM);
public:
	FSFloaterIM(const LLUUID& session_id);

	virtual ~FSFloaterIM();
	
	// LLView overrides
	/*virtual*/ BOOL postBuild();
	/*virtual*/ void setVisible(BOOL visible);
	/*virtual*/ BOOL getVisible();
	// Check typing timeout timer.
	/*virtual*/ void draw();

	// LLFloater overrides
	/*virtual*/ void onClose(bool app_quitting);
	/*virtual*/ void setDocked(bool docked, bool pop_on_undock = true);

	// Make IM conversion visible and update the message history
	static FSFloaterIM* show(const LLUUID& session_id);

	// Toggle panel specified by session_id
	// Returns true iff panel became visible
	static bool toggle(const LLUUID& session_id);

	static FSFloaterIM* findInstance(const LLUUID& session_id);

	static FSFloaterIM* getInstance(const LLUUID& session_id);

	void sessionInitReplyReceived(const LLUUID& im_session_id);

	// get new messages from LLIMModel
	void updateMessages();
	void reloadMessages(bool clean_messages = false);
	static void onSendMsg( LLUICtrl*, void*);
	void sendMsgFromInputEditor();
	void sendMsg(const std::string& msg);

	// callback for LLIMModel on new messages
	// route to specific floater if it is visible
	static void newIMCallback(const LLSD& data);
	
	//AO: Callbacks for voice handling formerly in llPanelImControlPanel
	void onVoiceChannelStateChanged(const LLVoiceChannel::EState& old_state, const LLVoiceChannel::EState& new_state);
	void onChange(EStatusType status, const std::string &channelURI, bool proximal);
	void updateButtons(bool is_call_started);
	void updateCallButton();
	void changed(U32 mask);
	// ## Zi: overridden to fix the IM focus bug - FIRE-3989 etc.
	BOOL focusFirstItem(BOOL prefer_text_fields = FALSE, BOOL focus_flash = TRUE );

	// called when docked floater's position has been set by chiclet
	// void setPositioned(bool b) { mPositioned = b; };		// dead code -Zi

	void onVisibilityChange(BOOL new_visibility);
	void processIMTyping(const LLIMInfo* im_info, BOOL typing);
	void processAgentListUpdates(const LLSD& body);
	void processSessionUpdate(const LLSD& session_update);

	void updateChatHistoryStyle();
	static void processChatHistoryStyleUpdate(const LLSD& newvalue);
	
	static void clearAllOpenHistories();	// <FS:CR> FIRE-11734

	BOOL handleDragAndDrop(S32 x, S32 y, MASK mask,
							   BOOL drop, EDragAndDropType cargo_type,
							   void *cargo_data, EAcceptance *accept,
							   std::string& tooltip_msg);

	/**
	 * Returns true if chat is displayed in multi tabbed floater
	 *         false if chat is displayed in multiple windows
	 */
	static bool isChatMultiTab();

	void initIMSession(const LLUUID& session_id);
	static void initIMFloater();

	//used as a callback on receiving new IM message
	static void sRemoveTypingIndicator(const LLSD& data);

	static void onNewIMReceived(const LLUUID& session_id);

	virtual LLTransientFloaterMgr::ETransientGroup getGroup() { return LLTransientFloaterMgr::IM; }

	// <FS:Ansariel> FIRE-3248: Disable add friend button on IM floater if friendship request accepted
	void setEnableAddFriendButton(BOOL enabled);
	
	static boost::signals2::connection setIMFloaterShowedCallback(const floater_showed_signal_t::slot_type& cb);
	static floater_showed_signal_t sIMFloaterShowedSignal;

	S32 getLastChatMessageIndex() {return mLastMessageIndex;}

	LLVoiceChannel* getVoiceChannel() { return mVoiceChannel; }

protected:
	/* virtual */
	void	onClickCloseBtn(bool app_quitting = false);

	// support sysinfo button -Zi
	void	onSysinfoButtonVisibilityChanged(const LLSD& yes);
	LLButton* mSysinfoButton;
	// support sysinfo button -Zi

	BOOL enableViewerVersionCallback(const LLSD& notification,const LLSD& response);		// <FS:Zi> Viewer version popup
	void reshapeFloater(bool collapse);
	void reshapeChatLayoutPanel();
private:
	// process focus events to set a currently active session
	/* virtual */ void onFocusLost();
	/* virtual */ void onFocusReceived();

	// Update the window title, input field help text, etc.
	void updateSessionName(const std::string& ui_title, const std::string& ui_label);
	
	// For display name lookups for IM window titles
	void onAvatarNameCache(const LLUUID& agent_id, const LLAvatarName& av_name);
	void fetchAvatarName(LLUUID& agent_id);
	
	BOOL dropCallingCard(LLInventoryItem* item, BOOL drop);
	BOOL dropCategory(LLInventoryCategory* category, BOOL drop);

	BOOL isInviteAllowed() const;
	BOOL inviteToSession(const uuid_vec_t& agent_ids);
	
	void onInputEditorFocusReceived();
	void onInputEditorFocusLost();
	void onInputEditorKeystroke();

	void doToSelected(const LLSD& userdata);
	bool checkEnabled(const LLSD& userdata);

	// support sysinfo button -Zi
	void onSysinfoButtonClicked();
	BOOL onSendSysinfo(const LLSD& notification,const LLSD& response);
	// support sysinfo button -Zi

	// connection to voice channel state change signal
	boost::signals2::connection mVoiceChannelStateChangeConnection;
	
	void			setTyping(bool typing);
	void			onSlide();
	static void*	createPanelIMControl(void* userdata);
	static void*	createPanelGroupControl(void* userdata);
	static void* 	createPanelAdHocControl(void* userdata);

	// Add the "User is typing..." indicator.
	void addTypingIndicator(const LLIMInfo* im_info);

	// Remove the "User is typing..." indicator.
	void removeTypingIndicator(const LLIMInfo* im_info = NULL);

	static void closeHiddenIMToasts();

	static void confirmLeaveCallCallback(const LLSD& notification, const LLSD& response);
	
	void sendParticipantsAddedNotification(const uuid_vec_t& uuids);

	FSPanelChatControlPanel* mControlPanel;
	LLUUID mSessionID;
	S32 mLastMessageIndex;

	EInstantMessage mDialog;
	LLUUID mOtherParticipantUUID;
	FSChatHistory* mChatHistory;
	LLChatEntry* mInputEditor;
	LLLayoutPanel* mChatLayoutPanel;
	LLLayoutStack* mInputPanels;
	// bool mPositioned;		// dead code -Zi

	std::string mSavedTitle;
	LLUIString mTypingStart;
	bool mMeTyping;
	bool mOtherTyping;
	bool mShouldSendTypingState;
	LLFrameTimer mTypingTimer;
	LLFrameTimer mTypingTimeoutTimer;
	LLFrameTimer mMeTypingTimer;
	LLFrameTimer mOtherTypingTimer;

	bool mSessionInitialized;
	LLSD mQueuedMsgsForInit;

	LLVoiceChannel* mVoiceChannel;
	
	S32 mInputEditorPad;
	S32 mChatLayoutPanelHeight;
	S32 mFloaterHeight;
	
	boost::signals2::connection mAvatarNameCacheConnection;
};


#endif  // FS_FLOATERIM_H
