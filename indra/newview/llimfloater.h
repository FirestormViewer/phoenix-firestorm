/** 
 * @file llimfloater.h
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

#ifndef LL_IMFLOATER_H
#define LL_IMFLOATER_H

#include "llinstantmessage.h"
#include "lllogchat.h"
#include "lltooldraganddrop.h"
#include "lltransientdockablefloater.h"
#include "llvoicechannel.h"

class LLAvatarName;
class LLButton;		// support sysinfo button -Zi
class LLLineEditor;
class LLPanelChatControlPanel;
class LLChatHistory;
class LLInventoryItem;
class LLInventoryCategory;

/**
 * Individual IM window that appears at the bottom of the screen,
 * optionally "docked" to the bottom tray.
 */
class LLIMFloater : public LLTransientDockableFloater, LLVoiceClientStatusObserver, LLFriendObserver
{
	LOG_CLASS(LLIMFloater);
public:
	LLIMFloater(const LLUUID& session_id);

	virtual ~LLIMFloater();
	
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
	static LLIMFloater* show(const LLUUID& session_id);

	// Toggle panel specified by session_id
	// Returns true iff panel became visible
	static bool toggle(const LLUUID& session_id);

	static LLIMFloater* findInstance(const LLUUID& session_id);

	static LLIMFloater* getInstance(const LLUUID& session_id);

	void sessionInitReplyReceived(const LLUUID& im_session_id);

	// get new messages from LLIMModel
	void updateMessages();
	void reloadMessages();
	static void onSendMsg( LLUICtrl*, void*);
	void sendMsg();

	// callback for LLIMModel on new messages
	// route to specific floater if it is visible
	static void newIMCallback(const LLSD& data);
	
	//AO: Callbacks for voice handling formerly in llPanelImControlPanel
	void onCallButtonClicked();
	void onEndCallButtonClicked();
	void onOpenVoiceControlsClicked();
	void onVoiceChannelStateChanged(const LLVoiceChannel::EState& old_state, const LLVoiceChannel::EState& new_state);
	void onChange(EStatusType status, const std::string &channelURI, bool proximal);
	void updateButtons(bool is_call_started);
	void updateCallButton();
	void changed(U32 mask);
	// ## Zi: overridden to fix the IM focus bug - FIRE-3989 etc.
	BOOL focusFirstItem(BOOL prefer_text_fields = FALSE, BOOL focus_flash = TRUE );

	// called when docked floater's position has been set by chiclet
	void setPositioned(bool b) { mPositioned = b; };

	void onVisibilityChange(const LLSD& new_visibility);
	void processIMTyping(const LLIMInfo* im_info, BOOL typing);
	void processAgentListUpdates(const LLSD& body);
	void processSessionUpdate(const LLSD& session_update);

	void updateChatHistoryStyle();
	static void processChatHistoryStyleUpdate(const LLSD& newvalue);

	BOOL handleDragAndDrop(S32 x, S32 y, MASK mask,
							   BOOL drop, EDragAndDropType cargo_type,
							   void *cargo_data, EAcceptance *accept,
							   std::string& tooltip_msg);

	/**
	 * Returns true if chat is displayed in multi tabbed floater
	 *         false if chat is displayed in multiple windows
	 */
	static bool isChatMultiTab();

	static void initIMFloater();

	//used as a callback on receiving new IM message
	static void sRemoveTypingIndicator(const LLSD& data);

	static void onIMChicletCreated(const LLUUID& session_id);

	virtual LLTransientFloaterMgr::ETransientGroup getGroup() { return LLTransientFloaterMgr::IM; }

	// <FS:Ansariel> FIRE-3248: Disable add friend button on IM floater if friendship request accepted
	void setEnableAddFriendButton(BOOL enabled);

protected:
	/* virtual */
	void	onClickCloseBtn();

	// support sysinfo button -Zi
	void	onSysinfoButtonVisibilityChanged(const LLSD& yes);
	LLButton* mSysinfoButton;
	// support sysinfo button -Zi

	BOOL enableViewerVersionCallback(const LLSD& notification,const LLSD& response);		// <FS:Zi> Viewer version popup
private:
	// process focus events to set a currently active session
	/* virtual */ void onFocusLost();
	/* virtual */ void onFocusReceived();

	// Update the window title, input field help text, etc.
	void updateSessionName(const std::string& ui_title, const std::string& ui_label);
	
	// For display name lookups for IM window titles
	void onAvatarNameCache(const LLUUID& agent_id, const LLAvatarName& av_name);
	
	BOOL dropCallingCard(LLInventoryItem* item, BOOL drop);
	BOOL dropCategory(LLInventoryCategory* category, BOOL drop);

	BOOL isInviteAllowed() const;
	BOOL inviteToSession(const uuid_vec_t& agent_ids);
	
	static void		onInputEditorFocusReceived( LLFocusableElement* caller, void* userdata );
	static void		onInputEditorFocusLost(LLFocusableElement* caller, void* userdata);
	static void		onInputEditorKeystroke(LLLineEditor* caller, void* userdata);
	
	// AO, originally from llpaneChatControlPanel trees
	void onViewProfileButtonClicked();
	void onAddFriendButtonClicked();
	void onShareButtonClicked();
	void onTeleportButtonClicked();
	void onPayButtonClicked();
	void onGroupInfoButtonClicked();
	void onHistoryButtonClicked();

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

	LLPanelChatControlPanel* mControlPanel;
	LLUUID mSessionID;
	S32 mLastMessageIndex;

	EInstantMessage mDialog;
	LLUUID mOtherParticipantUUID;
	LLChatHistory* mChatHistory;
	LLLineEditor* mInputEditor;
	bool mPositioned;

	std::string mSavedTitle;
	LLUIString mTypingStart;
	bool mMeTyping;
	bool mOtherTyping;
	bool mShouldSendTypingState;
	LLFrameTimer mTypingTimer;
	LLFrameTimer mTypingTimeoutTimer;

	bool mSessionInitialized;
	LLSD mQueuedMsgsForInit;
};


#endif  // LL_IMFLOATER_H
