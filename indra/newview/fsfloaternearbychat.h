 /**
 * @file fsfloaternearbychat.h
 * @brief Nearby chat history scrolling panel implementation
 *
 * $LicenseInfo:firstyear=2004&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * Copyright (C) 2012, Zi Ree @ Second Life
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

// Original file: llfloaternearbychat.h

#ifndef FS_FLOATERNEARBYCHAT_H
#define FS_FLOATERNEARBYCHAT_H

#include "llfloater.h"
#include "llviewerchat.h"
#include "fschatparticipants.h"

class FSChatHistory;
class LLChatEntry;
class LLComboBox;
class LLLayoutStack;
class LLLayoutPanel;
class LLPanelEmojiComplete;
class LLResizeBar;
class LLTextBox;


class FSFloaterNearbyChat: public LLFloater, FSChatParticipants
{
public:
    FSFloaterNearbyChat(const LLSD& key);
    ~FSFloaterNearbyChat();

    bool    postBuild();

    /** @param archive true - to save a message to the chat history log */
    void    addMessage(const LLChat& message,bool archive = true, const LLSD &args = LLSD());

    /*virtual*/ void onOpen(const LLSD& key);
    /*virtual*/ void setVisible(bool visible);
    /*virtual*/ void setMinimized(bool b);

    void    openFloater(const LLSD& key);

    void clearChatHistory();
    virtual void updateChatHistoryStyle();

    static void processChatHistoryStyleUpdate(const LLSD& newvalue);

    void loadHistory();
    void reloadMessages(bool clean_messages = false);

    static FSFloaterNearbyChat* findInstance();
    static FSFloaterNearbyChat* getInstance();

    void removeScreenChat();

    static bool isChatMultiTab();

    bool getVisible();

    void onHistoryButtonClicked();

    void onSearchButtonClicked();

    // overridden to fix the multitab focus bug -Zi
    bool focusFirstItem(bool prefer_text_fields = false, bool focus_flash = true );

    void updateFSUseNearbyChatConsole(const LLSD &data);
    static bool isWordsName(const std::string& name);

    LLChatEntry* getChatBox() { return mInputEditor; }

    S32 getMessageArchiveLength() { return static_cast<S32>(mMessageArchive.size()); }

    virtual bool handleKeyHere( KEY key, MASK mask );

    static void stopChat();

    void updateUnreadMessageNotification(S32 unread_messages, bool muted_history);
    void updateShowMutedChatHistory(const LLSD &data);

    void handleMinimized(bool minimized);

    uuid_vec_t getSessionParticipants() const;

protected:
    void onChatBoxKeystroke();
    void onChatBoxFocusLost();
    void onChatBoxFocusReceived();

    void sendChat( EChatType type );
    void sendChatFromViewer(const std::string& utf8text, EChatType type, bool animate);
    void onChatBoxCommit();
    void onChatTypeChanged();

    void reshapeChatLayoutPanel();

    static S32 sLastSpecialChatChannel;

private:
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

    void onFocusLost();
    void onFocusReceived();

    FSChatHistory*      mChatHistory;
    FSChatHistory*      mChatHistoryMuted;
    LLChatEntry*        mInputEditor;

    // chat type selector and send chat buttons
    LLButton*           mEmojiRecentPanelToggleBtn;
    LLButton*           mEmojiPickerToggleBtn;
    LLLayoutPanel*      mEmojiRecentPanel;
    LLTextBox*          mEmojiRecentEmptyText;
    LLPanelEmojiComplete* mEmojiRecentIconsCtrl;
    LLButton*           mSendChatButton;
    LLComboBox*         mChatTypeCombo;

    LLLayoutPanel*      mChatLayoutPanel;
    LLLayoutStack*      mInputPanels;

    LLLayoutPanel*      mUnreadMessagesNotificationPanel;
    LLTextBox*          mUnreadMessagesNotificationTextBox;
    S32                 mUnreadMessages;
    S32                 mUnreadMessagesMuted;

    S32 mInputEditorPad;
    S32 mChatLayoutPanelHeight;
    S32 mFloaterHeight;

    std::vector<LLChat> mMessageArchive;

    bool FSUseNearbyChatConsole;

    boost::signals2::connection mRecentEmojisUpdatedCallbackConnection{};
    boost::signals2::connection mEmojiCloseConn{};
    U32 mEmojiHelperLastCallbackFrame{ 0 };
};

#endif // FS_FLOATERNEARBYCHAT_H
