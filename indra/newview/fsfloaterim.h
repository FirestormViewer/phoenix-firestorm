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
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

// Original file: llimfloater.h

#ifndef FS_FLOATERIM_H
#define FS_FLOATERIM_H

#include "llchat.h"
#include "llinstantmessage.h"
#include "lllogchat.h"
#include "lltooldraganddrop.h"
#include "lltransientdockablefloater.h"
#include "llvoicechannel.h"
#include "fschatparticipants.h"

class FSChatHistory;
class FSFloaterIMTimer;
class FSPanelChatControlPanel;
class LLAvatarName;
class LLButton;     // support sysinfo button -Zi
class LLChatEntry;
class LLInventoryCategory;
class LLInventoryItem;
class LLLayoutPanel;
class LLLayoutStack;
class LLPanelEmojiComplete;
class LLTextBox;
class LLTextEditor;

typedef boost::signals2::signal<void(const LLUUID& session_id)> floater_showed_signal_t;

/**
 * Individual IM window that appears at the bottom of the screen,
 * optionally "docked" to the bottom tray.
 */
class FSFloaterIM : public LLTransientDockableFloater, LLVoiceClientStatusObserver, LLFriendObserver, LLEventTimer, FSChatParticipants
{
    LOG_CLASS(FSFloaterIM);
public:
    FSFloaterIM(const LLUUID& session_id);

    virtual ~FSFloaterIM();

    // LLView overrides
    /*virtual*/ bool postBuild();
    /*virtual*/ void setVisible(bool visible);
    /*virtual*/ bool getVisible();
    /*virtual*/ void setMinimized(bool b);

    // LLFloater overrides
    /*virtual*/ void onClose(bool app_quitting);
    /*virtual*/ void setDocked(bool docked, bool pop_on_undock = true);
    /*virtual*/ void onSnooze();

    /*virtual*/ bool tick();

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
    void sendMsgFromInputEditor(EChatType type);
    void sendMsg(const std::string& msg);

    // callback for LLIMModel on new messages
    // route to specific floater if it is visible
    static void newIMCallback(const LLSD& data);

    //AO: Callbacks for voice handling formerly in llPanelImControlPanel
    void onVoiceChannelStateChanged(const LLVoiceChannel::EState& old_state, const LLVoiceChannel::EState& new_state);
    void onChange(EStatusType status, const LLSD& channelInfo, bool proximal);
    void updateButtons(bool is_call_started);
    void updateCallButton();
    void changed(U32 mask);
    // ## Zi: overridden to fix the IM focus bug - FIRE-3989 etc.
    bool focusFirstItem(bool prefer_text_fields = false, bool focus_flash = true );

    void onVisibilityChange(bool new_visibility);
    void processIMTyping(const LLUUID& from_id, bool typing);
    void processAgentListUpdates(const LLSD& body);

    void updateChatHistoryStyle();
    static void processChatHistoryStyleUpdate(const LLSD& newvalue);

    static void clearAllOpenHistories();    // <FS:CR> FIRE-11734

    void onChatSearchButtonClicked();

    bool handleDragAndDrop(S32 x, S32 y, MASK mask,
                               bool drop, EDragAndDropType cargo_type,
                               void *cargo_data, EAcceptance *accept,
                               std::string& tooltip_msg);

    virtual bool handleKeyHere( KEY key, MASK mask );

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

    static boost::signals2::connection setIMFloaterShowedCallback(const floater_showed_signal_t::slot_type& cb);
    static floater_showed_signal_t sIMFloaterShowedSignal;

    S32 getLastChatMessageIndex() {return mLastMessageIndex;}

    LLVoiceChannel* getVoiceChannel() { return mVoiceChannel; }

    void updateUnreadMessageNotification(S32 unread_messages);

    void loadInitialInvitedIDs();

    bool isP2PChat() const { return mIsP2PChat; }

    void handleMinimized(bool minimized);

    void timedUpdate();

    uuid_vec_t getSessionParticipants() const;

    // <FS:TJ> [FIRE-35804] Allow the IM floater to have separate transparency
    F32 onGetChatEditorOpacityCallback(ETypeTransparency type, F32 alpha);
    // </FS:TJ>

protected:
    /* virtual */
    void    onClickCloseBtn(bool app_quitting = false);
    /*virtual*/ bool applyRectControl();

    // support sysinfo button -Zi
    void    onSysinfoButtonVisibilityChanged(const LLSD& yes);
    LLButton* mSysinfoButton;
    // support sysinfo button -Zi

    bool enableViewerVersionCallback(const LLSD& notification,const LLSD& response);        // <FS:Zi> Viewer version popup
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

    bool dropCallingCard(LLInventoryItem* item, bool drop);
    bool dropCategory(LLInventoryCategory* category, bool drop);
    bool dropPerson(LLUUID* person_id, bool drop);

    bool isInviteAllowed() const;
    bool inviteToSession(const uuid_vec_t& agent_ids);

    void onInputEditorFocusReceived();
    void onInputEditorFocusLost();
    void onInputEditorKeystroke();

    void doToSelected(const LLSD& userdata);
    bool checkEnabled(const LLSD& userdata);

    // support sysinfo button -Zi
    void onSysinfoButtonClicked();
    bool onSendSysinfo(const LLSD& notification,const LLSD& response);
    // support sysinfo button -Zi

    // connection to voice channel state change signal
    boost::signals2::connection mVoiceChannelStateChangeConnection;

    void            setTyping(bool typing);
    void            onSlide();
    static void*    createPanelIMControl(void* userdata);
    static void*    createPanelGroupControl(void* userdata);
    static void*    createPanelAdHocControl(void* userdata);

    // Add the "User is typing..." indicator.
    void addTypingIndicator(const LLUUID& from_id);

    // Remove the "User is typing..." indicator.
    void removeTypingIndicator(const LLUUID& from_id = LLUUID::null);

    static void closeHiddenIMToasts();

    static void confirmLeaveCallCallback(const LLSD& notification, const LLSD& response);

    void sendParticipantsAddedNotification(const uuid_vec_t& uuids);

    void confirmSnooze();
    void snoozeDurationCallback(const LLSD& notification, const LLSD& response);
    void snooze(S32 duration = -1);

    void onAddButtonClicked();
    bool canAddSelectedToChat(const uuid_vec_t& uuids);
    void addSessionParticipants(const uuid_vec_t& uuids);
    void addP2PSessionParticipants(const LLSD& notification, const LLSD& response, const uuid_vec_t& uuids);

    void onChatOptionsContextMenuItemClicked(const LLSD& userdata);
    bool onChatOptionsCheckContextMenuItem(const LLSD& userdata);
    bool onChatOptionsVisibleContextMenuItem(const LLSD& userdata);
    bool onChatOptionsEnableContextMenuItem(const LLSD& userdata);

    void onEmojiPickerToggleBtnClicked();
    void onEmojiPickerToggleBtnDown();
    void onEmojiRecentPanelToggleBtnClicked();
    void onEmojiPickerClosed();
    void initEmojiRecentPanel();
    void onRecentEmojiPicked(const LLSD& value);

    FSPanelChatControlPanel* mControlPanel;
    LLUUID mSessionID;
    S32 mLastMessageIndex;
    S32 mPendingMessages;

    EInstantMessage mDialog;
    LLUUID mOtherParticipantUUID;
    FSChatHistory* mChatHistory;
    LLChatEntry* mInputEditor;
    LLLayoutPanel* mChatLayoutPanel;
    LLLayoutStack* mInputPanels;
    LLLayoutPanel* mUnreadMessagesNotificationPanel;
    LLTextBox* mUnreadMessagesNotificationTextBox;
    LLButton* mEmojiRecentPanelToggleBtn;
    LLButton* mEmojiPickerToggleBtn;
    LLLayoutPanel* mEmojiRecentPanel;
    LLTextBox* mEmojiRecentEmptyText;
    LLPanelEmojiComplete* mEmojiRecentIconsCtrl;

    std::string mSavedTitle;
    LLUIString mTypingStart;
    bool mMeTyping;
    bool mOtherTyping;
    bool mShouldSendTypingState;
    LLFrameTimer mTypingTimer;
    LLFrameTimer mTypingTimeoutTimer;
    LLFrameTimer mMeTypingTimer;
    LLFrameTimer mOtherTypingTimer;
    LLFrameTimer mRefreshNameTimer;

    bool mSessionInitialized;
    LLSD mQueuedMsgsForInit;

    bool mIsP2PChat;

    LLVoiceChannel* mVoiceChannel;

    S32 mInputEditorPad;
    S32 mChatLayoutPanelHeight;
    S32 mFloaterHeight;

    uuid_vec_t mInvitedParticipants;
    uuid_vec_t mPendingParticipants;

    boost::signals2::connection mAvatarNameCacheConnection;

    bool mApplyRect;

    FSFloaterIMTimer*   mIMFloaterTimer;

    boost::signals2::connection mRecentEmojisUpdatedCallbackConnection{};
    boost::signals2::connection mEmojiCloseConn{};
    U32 mEmojiHelperLastCallbackFrame{ 0 };
};

class FSFloaterIMTimer : public LLEventTimer
{
public:
    typedef boost::function<void()> callback_t;

    FSFloaterIMTimer(callback_t callback);
    /*virtual*/ bool tick();

private:
    callback_t mCallback;
};

#endif  // FS_FLOATERIM_H
