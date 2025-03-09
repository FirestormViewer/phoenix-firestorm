/**
 * @file fsfloaternearbychat.cpp
 * @brief Nearby chat history scrolling panel implementation
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
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

// Original file: LLFloaterNearbyChat.cpp

#include "llviewerprecompiledheaders.h"

#include "fsfloaternearbychat.h"

#include "chatbar_as_cmdline.h"
#include "fschathistory.h"
#include "fschatoptionsmenu.h"
#include "fscommon.h"
#include "fsfloaterim.h"
#include "fsfloaterimcontainer.h"
#include "fsnearbychathub.h"
#include "llagent.h"            // gAgent
#include "llanimationstates.h"  // ANIM_AGENT_WHISPER, ANIM_AGENT_TALK, ANIM_AGENT_SHOUT
#include "llautoreplace.h"
#include "llavatarnamecache.h"
#include "llbutton.h"
#include "llchannelmanager.h"
#include "llchatentry.h"
#include "llcombobox.h"
#include "llcommandhandler.h"
#include "llconsole.h"
#include "lldraghandle.h"
#include "llfloateremojipicker.h"
#include "llfloaterreg.h"
#include "llfloatersearchreplace.h"
#include "llfocusmgr.h"
#include "llgesturemgr.h"
#include "lliconctrl.h"
#include "llkeyboard.h"
#include "lllayoutstack.h"
#include "lllogchat.h"
#include "llmenugl.h"
#include "llmultigesture.h"
#include "llpanelemojicomplete.h"
#include "llresizebar.h"
#include "llresizehandle.h"
#include "llrootview.h"
#include "llspinctrl.h"
#include "llstylemap.h"
#include "lltextbox.h"
#include "lltrans.h"
#include "llviewercontrol.h"
#include "llviewermenu.h"//for gMenuHolder
#include "llviewerstats.h"
#include "llviewerwindow.h"
#include "llworld.h"
#include "rlvhandler.h"
// <FS:TS> FIRE-23123: Don't log newline spam even from own objects
#include "NACLantispam.h"
// </FS:TS> FIRE-23123

S32 FSFloaterNearbyChat::sLastSpecialChatChannel = 0;


FSFloaterNearbyChat::FSFloaterNearbyChat(const LLSD& key)
    : LLFloater(key)
    ,mChatHistory(NULL)
    ,mChatHistoryMuted(NULL)
    ,mInputEditor(NULL)
    ,mChatLayoutPanel(NULL)
    ,mInputPanels(NULL)
    ,mChatLayoutPanelHeight(0)
    ,mUnreadMessagesNotificationPanel(NULL)
    ,mUnreadMessagesNotificationTextBox(NULL)
    ,mUnreadMessages(0)
    ,mUnreadMessagesMuted(0)
{
    //menu
    mEnableCallbackRegistrar.add("ChatOptions.Check", boost::bind(&FSFloaterNearbyChat::onChatOptionsCheckContextMenuItem, this, _2));
    mCommitCallbackRegistrar.add("ChatOptions.Action", boost::bind(&FSFloaterNearbyChat::onChatOptionsContextMenuItemClicked, this, _2));
    mEnableCallbackRegistrar.add("ChatOptions.Visible", boost::bind(&FSFloaterNearbyChat::onChatOptionsVisibleContextMenuItem, this, _2));
    mEnableCallbackRegistrar.add("ChatOptions.Enable", boost::bind(&FSFloaterNearbyChat::onChatOptionsEnableContextMenuItem, this, _2));
}

FSFloaterNearbyChat::~FSFloaterNearbyChat()
{
    if (mRecentEmojisUpdatedCallbackConnection.connected())
    {
        mRecentEmojisUpdatedCallbackConnection.disconnect();
    }
}

void FSFloaterNearbyChat::updateFSUseNearbyChatConsole(const LLSD &data)
{
    FSUseNearbyChatConsole = data.asBoolean();

    if (FSUseNearbyChatConsole)
    {
        removeScreenChat();
        gConsole->setVisible(true);
    }
    else
    {
        gConsole->setVisible(false);
    }
}


bool FSFloaterNearbyChat::postBuild()
{
    setIsSingleInstance(true);

    mInputEditor = getChild<LLChatEntry>("chat_box");
    mInputEditor->setAutoreplaceCallback(boost::bind(&LLAutoReplace::autoreplaceCallback, LLAutoReplace::getInstance(), _1, _2, _3, _4, _5));
    mInputEditor->setCommitCallback(boost::bind(&FSFloaterNearbyChat::onChatBoxCommit, this));
    mInputEditor->setKeystrokeCallback(boost::bind(&FSFloaterNearbyChat::onChatBoxKeystroke, this));
    mInputEditor->setFocusLostCallback(boost::bind(&FSFloaterNearbyChat::onChatBoxFocusLost, this));
    mInputEditor->setFocusReceivedCallback(boost::bind(&FSFloaterNearbyChat::onChatBoxFocusReceived, this));
    mInputEditor->setTextExpandedCallback(boost::bind(&FSFloaterNearbyChat::reshapeChatLayoutPanel, this));
    mInputEditor->setPassDelete(true);
    mInputEditor->setFont(LLViewerChat::getChatFont());
    mInputEditor->setLabel(getString("chatbox_label"));
    mInputEditor->enableSingleLineMode(gSavedSettings.getBOOL("FSUseSingleLineChatEntry"));

    mChatLayoutPanel = getChild<LLLayoutPanel>("chat_layout_panel");
    mInputPanels = getChild<LLLayoutStack>("input_panels");
    mChatLayoutPanelHeight = mChatLayoutPanel->getRect().getHeight();
    mInputEditorPad = mChatLayoutPanelHeight - mInputEditor->getRect().getHeight();

    mEmojiRecentPanelToggleBtn = getChild<LLButton>("emoji_recent_panel_toggle_btn");
    mEmojiRecentPanelToggleBtn->setClickedCallback([this](LLUICtrl*, const LLSD&) { onEmojiRecentPanelToggleBtnClicked(); });

    mEmojiRecentPanel = getChild<LLLayoutPanel>("emoji_recent_layout_panel");
    mEmojiRecentPanel->setVisible(false);

    mEmojiRecentEmptyText = getChild<LLTextBox>("emoji_recent_empty_text");
    mEmojiRecentEmptyText->setToolTip(mEmojiRecentEmptyText->getText());
    mEmojiRecentEmptyText->setVisible(false);

    mEmojiRecentIconsCtrl = getChild<LLPanelEmojiComplete>("emoji_recent_icons_ctrl");
    mEmojiRecentIconsCtrl->setCommitCallback([this](LLUICtrl*, const LLSD& value) { onRecentEmojiPicked(value); });
    mEmojiRecentIconsCtrl->setVisible(false);

    static bool usePrettyEmojiButton = gSavedSettings.getBOOL( "FSUsePrettyEmojiButton" );
    static bool useBWEmojis = gSavedSettings.getBOOL( "FSUseBWEmojis" );
    mEmojiPickerToggleBtn = getChild<LLButton>("emoji_picker_toggle_btn");
    if (usePrettyEmojiButton)
    {
        static auto emoji_btn_char = gSavedSettings.getU32("FSPrettyEmojiButtonCode");
        mEmojiPickerToggleBtn->setImageOverlay(LLUUID::null);
        mEmojiPickerToggleBtn->setFont(LLFontGL::getFontEmojiLarge(useBWEmojis));
        mEmojiPickerToggleBtn->setLabel(LLUIString(LLWString(1, emoji_btn_char)));
    }
    else
    {
        mEmojiPickerToggleBtn->setLabel(LLUIString(""));
        mEmojiPickerToggleBtn->setImageOverlay("Emoji_Picker_Icon");
    }
    mEmojiPickerToggleBtn->setClickedCallback([this](LLUICtrl*, const LLSD&) { onEmojiPickerToggleBtnClicked(); });

    mRecentEmojisUpdatedCallbackConnection = LLFloaterEmojiPicker::setRecentEmojisUpdatedCallback([this](const std::list<llwchar>& recent_emojis_list) { initEmojiRecentPanel(); });

    getChild<LLButton>("chat_history_btn")->setCommitCallback(boost::bind(&FSFloaterNearbyChat::onHistoryButtonClicked, this));
    getChild<LLButton>("chat_search_btn")->setCommitCallback(boost::bind(&FSFloaterNearbyChat::onSearchButtonClicked, this));

    // chat type selector and send chat button
    mChatTypeCombo = getChild<LLComboBox>("chat_type");
    mChatTypeCombo->selectByValue("say");
    mChatTypeCombo->setCommitCallback(boost::bind(&FSFloaterNearbyChat::onChatTypeChanged, this));
    mSendChatButton = getChild<LLButton>("send_chat");
    mSendChatButton->setCommitCallback(boost::bind(&FSFloaterNearbyChat::onChatBoxCommit, this));
    onChatTypeChanged();

    mChatHistory = getChild<FSChatHistory>("chat_history");
    mChatHistoryMuted = getChild<FSChatHistory>("chat_history_muted");

    mUnreadMessagesNotificationPanel = getChild<LLLayoutPanel>("unread_messages_holder");
    mUnreadMessagesNotificationTextBox = getChild<LLTextBox>("unread_messages_text");
    mChatHistory->setUnreadMessagesUpdateCallback(boost::bind(&FSFloaterNearbyChat::updateUnreadMessageNotification, this, _1, false));
    mChatHistoryMuted->setUnreadMessagesUpdateCallback(boost::bind(&FSFloaterNearbyChat::updateUnreadMessageNotification, this, _1, true));

    FSUseNearbyChatConsole = gSavedSettings.getBOOL("FSUseNearbyChatConsole");
    gSavedSettings.getControl("FSUseNearbyChatConsole")->getSignal()->connect(boost::bind(&FSFloaterNearbyChat::updateFSUseNearbyChatConsole, this, _2));

    gSavedSettings.getControl("FSShowMutedChatHistory")->getSignal()->connect(boost::bind(&FSFloaterNearbyChat::updateShowMutedChatHistory, this, _2));

    return LLFloater::postBuild();
}

std::string appendTime()
{
    time_t utc_time = time_corrected();
    std::string timeStr ="[" + LLTrans::getString("TimeHour") + "]:[" + LLTrans::getString("TimeMin") + "]";
    if (gSavedSettings.getBOOL("FSSecondsinChatTimestamps"))
    {
        timeStr += ":[" + LLTrans::getString("TimeSec") + "]";
    }

    LLSD substitution;

    substitution["datetime"] = (S32) utc_time;
    LLStringUtil::format (timeStr, substitution);

    return timeStr;
}

void FSFloaterNearbyChat::addMessage(const LLChat& chat,bool archive,const LLSD &args)
{
    LLChat& tmp_chat = const_cast<LLChat&>(chat);
    bool use_plain_text_chat_history = gSavedSettings.getBOOL("PlainTextChatHistory");
    bool show_timestamps_nearby_chat = gSavedSettings.getBOOL("FSShowTimestampsNearbyChat");
    // [FIRE-1641 : SJ]: Option to hide timestamps in nearby chat - add Timestamp when show_timestamps_nearby_chat is true
    if (show_timestamps_nearby_chat)
    {
        if (tmp_chat.mTimeStr.empty())
        {
            tmp_chat.mTimeStr = appendTime();
        }
    }

    tmp_chat.mFromName = chat.mFromName;
    LLSD chat_args = args;
    chat_args["use_plain_text_chat_history"] = use_plain_text_chat_history;
    chat_args["show_time"] = show_timestamps_nearby_chat;
    chat_args["is_local"] = true;
    static const LLStyle::Params input_append_params = LLStyle::Params();
    mChatHistoryMuted->appendMessage(chat, chat_args, input_append_params);
    if (!chat.mMuted)
    {
        mChatHistory->appendMessage(chat, chat_args, input_append_params);
    }

    if (archive)
    {
        mMessageArchive.push_back(chat);
        if (mMessageArchive.size() > 200)
        {
            mMessageArchive.erase(mMessageArchive.begin());
        }
    }

    if (args["do_not_log"].asBoolean() || chat.mMuted)
    {
        return;
    }

    // AO: IF tab mode active, flash our tab
    if (isChatMultiTab())
    {
        LLMultiFloater* hostp = getHost();
        // KC: Don't flash tab on system messages
        if (!isInVisibleChain() && hostp && (chat.mSourceType == CHAT_SOURCE_AGENT || chat.mSourceType == CHAT_SOURCE_OBJECT))
        {
            hostp->setFloaterFlashing(this, true);
        }
    }

    if (gSavedPerAccountSettings.getBOOL("LogNearbyChat"))
    {
        std::string from_name = chat.mFromName;

        if (chat.mSourceType == CHAT_SOURCE_AGENT)
        {
            // if the chat is coming from an agent, log the complete name
            LLAvatarName av_name;
            LLAvatarNameCache::get(chat.mFromID, &av_name);

            if (!av_name.isDisplayNameDefault() && (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES) || chat.mChatType == CHAT_TYPE_IM || chat.mChatType == CHAT_TYPE_IM_GROUP))
            {
                from_name = av_name.getCompleteName();
            }

            // Ansariel: Handle IMs in nearby chat
            // FS:LO FIRE-5230 - Chat Console Improvement: Replacing the "IM" in front of group chat messages with the actual group name
            if (gSavedSettings.getBOOL("FSShowIMInChatHistory"))
            {
                if (chat.mChatType == CHAT_TYPE_IM_GROUP && !chat.mFromNameGroup.empty())
                {
                    from_name = "IM: " + chat.mFromNameGroup + from_name;
                }
                else if (chat.mChatType == CHAT_TYPE_IM)
                {
                    from_name = "IM: " + from_name;
                }

                // <FS:LO> hack to prevent chat logs from containing lines like "TIMESTANMP IM:: friend is on/offline" (aka the double ":" )
                if (from_name == "IM:")
                {
                    from_name = "IM";
                }
                // </FS:LO>
            }
            // FS:LO FIRE-5230 - Chat Console Improvement: Replacing the "IM" in front of group chat messages with the actual group name
        }
        // <FS:LO> Make logging IMs to the chat history file toggleable again
        if (!(chat.mChatType == CHAT_TYPE_IM || chat.mChatType == CHAT_TYPE_IM_GROUP) ||
            (chat.mChatType == CHAT_TYPE_IM && chat.mSourceType == CHAT_SOURCE_OBJECT) ||
            gSavedSettings.getBOOL("FSLogIMInChatHistory"))
        {
            // <FS:TS> FIRE-23123: Don't log newline flood even from own objects
            static LLCachedControl<bool> useAntiSpam(gSavedSettings, "UseAntiSpam");
            if (useAntiSpam && NACLAntiSpamRegistry::instance().checkNewlineFlood(ANTISPAM_QUEUE_CHAT, chat.mFromID, chat.mText))
            {
                return;
            }
            // </FS:TS> FIRE-23123
            LLLogChat::saveHistory("chat", from_name, chat.mFromID, chat.mText);
        }
    }
}

// virtual
bool FSFloaterNearbyChat::focusFirstItem(bool prefer_text_fields, bool focus_flash)
{
    mInputEditor->setFocus(true);
    onTabInto();
    if (focus_flash)
    {
        gFocusMgr.triggerFocusFlash();
    }
    return true;
}

void FSFloaterNearbyChat::onHistoryButtonClicked()
{
    if (gSavedSettings.getBOOL("FSUseBuiltInHistory"))
    {
        LLFloaterReg::showInstance("preview_conversation", LLSD(LLUUID::null), true);
    }
    else
    {
        gViewerWindow->getWindow()->openFile(LLLogChat::makeLogFileName("chat"));
    }
}

void FSFloaterNearbyChat::onSearchButtonClicked()
{
    if (mChatHistory->getVisible())
    {
        LLFloaterSearchReplace::show(mChatHistory);
    }
    else if (mChatHistoryMuted->getVisible())
    {
        LLFloaterSearchReplace::show(mChatHistoryMuted);
    }
}

void FSFloaterNearbyChat::onChatOptionsContextMenuItemClicked(const LLSD& userdata)
{
    FSChatOptionsMenu::onMenuItemClick(userdata, this);
}

bool FSFloaterNearbyChat::onChatOptionsCheckContextMenuItem(const LLSD& userdata)
{
    return FSChatOptionsMenu::onMenuItemCheck(userdata, this);
}

bool FSFloaterNearbyChat::onChatOptionsVisibleContextMenuItem(const LLSD& userdata)
{
    return FSChatOptionsMenu::onMenuItemVisible(userdata, this);
}

bool FSFloaterNearbyChat::onChatOptionsEnableContextMenuItem(const LLSD& userdata)
{
    return FSChatOptionsMenu::onMenuItemEnable(userdata, this);
}

void FSFloaterNearbyChat::openFloater(const LLSD& key)
{
    // We override this to put nearbychat in the IM floater. -AO
    if (isChatMultiTab())
    {
        // <FS:Ansariel> [FS communication UI]
        //LLIMFloaterContainer* floater_container = LLIMFloaterContainer::getInstance();
        FSFloaterIMContainer* floater_container = FSFloaterIMContainer::getInstance();
        // </FS:Ansariel> [FS communication UI]
        // only show the floater container if we are actually attached -Zi
        if (floater_container && !gSavedSettings.getBOOL("ChatHistoryTornOff"))
        {
            floater_container->showFloater(this, LLTabContainer::START);
        }
        setVisible(true);
        LLFloater::openFloater(key);
    }
}

void FSFloaterNearbyChat::removeScreenChat()
{
    LLNotificationsUI::LLScreenChannelBase* chat_channel = LLNotificationsUI::LLChannelManager::getInstance()->findChannelByID(LLNotificationsUI::NEARBY_CHAT_CHANNEL_UUID);
    if (chat_channel)
    {
        chat_channel->removeToastsFromChannel();
    }
}

void FSFloaterNearbyChat::setVisible(bool visible)
{
    if (visible)
    {
        removeScreenChat();
    }
    LLFloater::setVisible(visible);

    bool is_minimized = visible && isChatMultiTab()
        ? FSFloaterIMContainer::getInstance()->isMinimized()
        : !visible;

    if (!is_minimized && mChatHistory && mInputEditor)
    {
        //only if floater was construced and initialized from xml
        FSFloaterIMContainer* im_container = FSFloaterIMContainer::getInstance();

        //prevent stealing focus when opening a background IM tab (EXT-5387, checking focus for EXT-6781)
        // If this is docked, is the selected tab, and the im container has focus, put focus in the input ctrl -KC
        bool is_active = im_container->getActiveFloater() == this && im_container->hasFocus();
        if (!isChatMultiTab() || is_active || hasFocus())
        {
            mInputEditor->setFocus(true);
        }
    }

    if (visible && isInVisibleChain())
    {
        gConsole->addSession(LLUUID::null);
    }
    else
    {
        gConsole->removeSession(LLUUID::null);
    }
}

void FSFloaterNearbyChat::setMinimized(bool b)
{
    handleMinimized(b);

    LLFloater::setMinimized(b);
}

void FSFloaterNearbyChat::onOpen(const LLSD& key )
{
    FSFloaterIMContainer* floater_container = FSFloaterIMContainer::getInstance();
    if (floater_container)
    {
        if (gSavedSettings.getBOOL("ChatHistoryTornOff"))
        {
            // first set the tear-off host to this floater
            setHost(floater_container);
            // clear the tear-off host right after, the "last host used" will still stick
            setHost(NULL);
            // reparent the floater to the main view
            gFloaterView->addChild(this);
        }
        else
        {
            floater_container->addFloater(this, false);
        }
    }

    // We override this to put nearbychat in the IM floater. -AO
    if(isChatMultiTab() && ! isVisible(this))
    {
        // only show the floater container if we are actually attached -Zi
        if (floater_container && !gSavedSettings.getBOOL("ChatHistoryTornOff"))
        {
            // make sure to show our parent floater, too
            floater_container->setVisible(true);
            floater_container->showFloater(this, LLTabContainer::START);
        }
        setVisible(true);
    }

    LLFloater::onOpen(key);
}

// exported here for "clrchat" command line -Zi
void FSFloaterNearbyChat::clearChatHistory()
{
    mChatHistory->clear();
    mChatHistoryMuted->clear();
}

void FSFloaterNearbyChat::updateChatHistoryStyle()
{
    clearChatHistory();

    LLSD do_not_log;
    do_not_log["do_not_log"] = true;
    for(std::vector<LLChat>::iterator it = mMessageArchive.begin();it!=mMessageArchive.end();++it)
    {
        // Update the messages without re-writing them to a log file.
        addMessage(*it,false, do_not_log);
    }
}

//static
void FSFloaterNearbyChat::processChatHistoryStyleUpdate(const LLSD& newvalue)
{
    FSFloaterNearbyChat* nearby_chat = LLFloaterReg::getTypedInstance<FSFloaterNearbyChat>("fs_nearby_chat", LLSD());
    if (nearby_chat)
    {
        nearby_chat->updateChatHistoryStyle();
        nearby_chat->mInputEditor->setFont(LLViewerChat::getChatFont());

        // Re-set the current text to make style update instant
        std::string text = nearby_chat->mInputEditor->getText();
        nearby_chat->mInputEditor->clear();
        nearby_chat->mInputEditor->setText(text);
    }
}

//static
bool FSFloaterNearbyChat::isWordsName(const std::string& name)
{
    // checking to see if it's display name plus username in parentheses
    auto open_paren = name.find(" (", 0);
    auto close_paren = name.find(')', 0);

    if (open_paren != std::string::npos &&
        close_paren == name.length() - 1)
    {
        return true;
    }
    else
    {
        //checking for a single space
        auto pos = name.find(' ', 0);
        return std::string::npos != pos && name.rfind(' ', name.length()) == pos && 0 != pos && name.length()-1 != pos;
    }
}

void FSFloaterNearbyChat::reloadMessages(bool clean_messages/* = false*/)
{
    if (clean_messages)
    {
        mMessageArchive.clear();
        loadHistory();
    }

    mChatHistory->clear();
    mChatHistoryMuted->clear();

    LLSD do_not_log;
    do_not_log["do_not_log"] = true;
    for(std::vector<LLChat>::iterator it = mMessageArchive.begin();it!=mMessageArchive.end();++it)
    {
        // Update the messages without re-writing them to a log file.
        addMessage(*it,false, do_not_log);
    }
}

void FSFloaterNearbyChat::loadHistory()
{
    LLSD do_not_log;
    do_not_log["do_not_log"] = true;

    std::list<LLSD> history;
    LLLogChat::loadChatHistory("chat", history);

    std::list<LLSD>::const_iterator it = history.begin();
    // FS:LO FIRE-5230 - Chat Console Improvement: Replacing the "IM" in front of group chat messages with the actual group name
    typedef enum e_im_type
    {
        IM_TYPE_NONE = 0,
        IM_TYPE_NORMAL,
        IM_TYPE_GROUP
    } EIMType;
    // FS:LO FIRE-5230 - Chat Console Improvement: Replacing the "IM" in front of group chat messages with the actual group name
    while (it != history.end())
    {
        EIMType im_type = IM_TYPE_NONE; // FS:LO FIRE-5230 - Chat Console Improvement: Replacing the "IM" in front of group chat messages with the actual group name
        const LLSD& msg = *it;

        std::string from = msg[LL_IM_FROM];
        std::string fromGroup = ""; // FS:LO FIRE-5230 - Chat Console Improvement: Replacing the "IM" in front of group chat messages with the actual group name
        LLUUID from_id;
        if (msg[LL_IM_FROM_ID].isDefined())
        {
            from_id = msg[LL_IM_FROM_ID].asUUID();
        }
        else
        {
            // Ansariel: Strip IM prefix so we can properly
            //           retrieve the UUID in case we got a
            //           saved IM in nearby chat history.
            std::string im_prefix = "IM: ";
            size_t im_prefix_found = from.find(im_prefix);
            if (im_prefix_found != std::string::npos)
            {
                from = from.substr(im_prefix.length());
                // FS:LO FIRE-5230 - Chat Console Improvement: Replacing the "IM" in front of group chat messages with the actual group name
                //im_type = true;
                size_t group_im_prefix_start = from.find("[");
                size_t group_im_prefix_end = from.find("] ");
                if((group_im_prefix_start != std::string::npos) && (group_im_prefix_end != std::string::npos))
                {
                    fromGroup = from.substr(group_im_prefix_start,group_im_prefix_end+2);
                    from = from.substr(fromGroup.length());
                    im_type = IM_TYPE_GROUP;
                }
                else
                {
                    im_type = IM_TYPE_NORMAL;
                }
                // FS:LO FIRE-5230 - Chat Console Improvement: Replacing the "IM" in front of group chat messages with the actual group name
            }

            std::string legacy_name = gCacheName->buildLegacyName(from);
            from_id = LLAvatarNameCache::getInstance()->findIdByName(legacy_name);
        }

        LLChat chat;
        chat.mFromName = from;
        chat.mFromID = from_id;
        chat.mText = msg[LL_IM_TEXT].asString();
        chat.mTimeStr = msg[LL_IM_TIME].asString();
        chat.mChatStyle = CHAT_STYLE_HISTORY;

        // FS:LO FIRE-5230 - Chat Console Improvement: Replacing the "IM" in front of group chat messages with the actual group name
        //if (im_type) chat.mChatType = CHAT_TYPE_IM;
        if (im_type == IM_TYPE_NORMAL)
        {
            chat.mChatType = CHAT_TYPE_IM;
        }
        else if(im_type == IM_TYPE_GROUP)
        {
            chat.mChatType = CHAT_TYPE_IM_GROUP;
            chat.mFromNameGroup = fromGroup;
        }
        // FS:LO FIRE-5230 - Chat Console Improvement: Replacing the "IM" in front of group chat messages with the actual group name

        chat.mSourceType = CHAT_SOURCE_AGENT;
        if (from_id.isNull() && SYSTEM_FROM == from)
        {
            chat.mSourceType = CHAT_SOURCE_SYSTEM;

        }
        else if (from_id.isNull())
        {
            chat.mSourceType = isWordsName(from) ? CHAT_SOURCE_UNKNOWN : CHAT_SOURCE_OBJECT;
        }

        addMessage(chat, true, do_not_log);

        it++;
    }
}

//static
FSFloaterNearbyChat* FSFloaterNearbyChat::findInstance()
{
    return LLFloaterReg::findTypedInstance<FSFloaterNearbyChat>("fs_nearby_chat", LLSD());
}

//static
FSFloaterNearbyChat* FSFloaterNearbyChat::getInstance()
{
    return LLFloaterReg::getTypedInstance<FSFloaterNearbyChat>("fs_nearby_chat", LLSD());
}

bool FSFloaterNearbyChat::isChatMultiTab()
{
    // Restart is required in order to change chat window type.
    static bool is_single_window = gSavedSettings.getS32("FSChatWindow") == 1;
    return is_single_window;
}

bool FSFloaterNearbyChat::getVisible()
{
    FSFloaterIMContainer* im_container = FSFloaterIMContainer::getInstance();

    // Treat inactive floater as invisible.
    bool is_active = im_container->getActiveFloater() == this;

    //torn off floater is always inactive
    if (!is_active && getHost() != im_container)
    {
        return LLFloater::getVisible();
    }

    // getVisible() returns true when Tabbed IM window is minimized.
    return is_active && !im_container->isMinimized() && im_container->getVisible();
}

// virtual
bool FSFloaterNearbyChat::handleKeyHere( KEY key, MASK mask )
{
    bool handled = false;

    if (KEY_RETURN == key)
    {
        if (mask == MASK_CONTROL && gSavedSettings.getBOOL("FSUseCtrlShout"))
        {
            // shout
            mInputEditor->updateHistory();
            sendChat(CHAT_TYPE_SHOUT);
            handled = true;
        }
        else if (mask == MASK_SHIFT && gSavedSettings.getBOOL("FSUseShiftWhisper"))
        {
            // whisper
            mInputEditor->updateHistory();
            sendChat(CHAT_TYPE_WHISPER);
            handled = true;
        }
        else if (mask == MASK_ALT && gSavedSettings.getBOOL("FSUseAltOOC"))
        {
            // OOC
            mInputEditor->updateHistory();
            sendChat(CHAT_TYPE_OOC);
            handled = true;
        }
        else if (mask == (MASK_SHIFT | MASK_CONTROL))
        {
            // linefeed
            if (!gSavedSettings.getBOOL("FSUseSingleLineChatEntry"))
            {
                if ((wstring_utf8_length(mInputEditor->getWText()) + wchar_utf8_length('\n')) > mInputEditor->getMaxTextLength())
                {
                    LLUI::getInstance()->reportBadKeystroke();
                }
                else
                {
                    mInputEditor->insertLinefeed();
                }
            }
            else
            {
                if ((wstring_utf8_length(mInputEditor->getWText()) + wchar_utf8_length(llwchar(182))) > mInputEditor->getMaxTextLength())
                {
                    LLUI::getInstance()->reportBadKeystroke();
                }
                else
                {
                    LLWString line_break(1, llwchar(182));
                    mInputEditor->insertText(line_break);
                }
            }

            handled = true;
        }
    }

    return handled;
}

void FSFloaterNearbyChat::onChatBoxKeystroke()
{
    S32 channel = 0;
    if (gSavedSettings.getBOOL("FSNearbyChatbar") &&
        gSavedSettings.getBOOL("FSShowChatChannel"))
    {
        channel = (S32)(FSFloaterNearbyChat::getInstance()->getChild<LLSpinCtrl>("ChatChannel")->get());
    }

    FSNearbyChat::handleChatBarKeystroke(mInputEditor, channel);
}

// static
void FSFloaterNearbyChat::onChatBoxFocusLost()
{
    // stop typing animation
    gAgent.stopTyping();
}

void FSFloaterNearbyChat::onChatBoxFocusReceived()
{
    mInputEditor->setEnabled(!gDisconnected && gSavedSettings.getBOOL("FSNearbyChatbar"));
}

void FSFloaterNearbyChat::reshapeChatLayoutPanel()
{
    mChatLayoutPanel->reshape(mChatLayoutPanel->getRect().getWidth(), mInputEditor->getRect().getHeight() + mInputEditorPad, false);
}

void FSFloaterNearbyChat::sendChat( EChatType type )
{
    if (mInputEditor)
    {
        LLWString text = mInputEditor->getWText();
        LLWStringUtil::trim(text);
        LLWStringUtil::replaceChar(text,182,'\n'); // Convert paragraph symbols back into newlines.
        if (!text.empty())
        {
            FSCommon::updateUsedEmojis(text);

            if(type == CHAT_TYPE_OOC)
            {
                std::string tempText = wstring_to_utf8str( text );
                tempText = gSavedSettings.getString("FSOOCPrefix") + " " + tempText + " " + gSavedSettings.getString("FSOOCPostfix");
                text = utf8str_to_wstring(tempText);
            }

            // Check if this is destined for another channel
            S32 channel = 0;
            bool is_set = false;
            FSNearbyChat::stripChannelNumber(text, &channel, &sLastSpecialChatChannel, &is_set);
            // If "/<number>" is not specified, see if a channel has been set in
            //  the spinner.
            if (!is_set &&
                gSavedSettings.getBOOL("FSNearbyChatbar") &&
                gSavedSettings.getBOOL("FSShowChatChannel"))
            {
                channel = (S32)(FSFloaterNearbyChat::getInstance()->getChild<LLSpinCtrl>("ChatChannel")->get());
            }

            std::string utf8text = wstring_to_utf8str(text);
            // Try to trigger a gesture, if not chat to a script.
            std::string utf8_revised_text;
            if (0 == channel)
            {
                // Convert OOC and MU* style poses
                utf8text = FSCommon::applyAutoCloseOoc(utf8text);
                utf8text = FSCommon::applyMuPose(utf8text);

                // discard returned "found" boolean
                if(!LLGestureMgr::instance().triggerAndReviseString(utf8text, &utf8_revised_text))
                {
                    utf8_revised_text = utf8text;
                }
            }
            else
            {
                utf8_revised_text = utf8text;
            }

            utf8_revised_text = utf8str_trim(utf8_revised_text);

            EChatType nType;
            if (type == CHAT_TYPE_OOC)
            {
                nType = CHAT_TYPE_NORMAL;
            }
            else
            {
                nType = type;
            }

            type = FSNearbyChat::processChatTypeTriggers(nType, utf8_revised_text);

            if (!utf8_revised_text.empty() && cmd_line_chat(utf8_revised_text, type))
            {
                // Chat with animation
                sendChatFromViewer(utf8_revised_text, type, gSavedSettings.getBOOL("PlayChatAnim"));
            }
        }

        mInputEditor->setText(LLStringExplicit(""));
    }

    gAgent.stopTyping();

    // If the user wants to stop chatting on hitting return, lose focus
    // and go out of chat mode.
    if (gSavedSettings.getBOOL("CloseChatOnReturn") && gSavedSettings.getBOOL("FSUnfocusChatHistoryOnReturn"))
    {
        stopChat();
    }
}

void FSFloaterNearbyChat::onChatBoxCommit()
{
    if (mInputEditor->getText().length() > 0)
    {
        EChatType type = CHAT_TYPE_NORMAL;
        if (gSavedSettings.getBOOL("FSShowChatType"))
        {
            const std::string typeString = mChatTypeCombo->getValue();
            if (typeString == "whisper")
            {
                type = CHAT_TYPE_WHISPER;
            }
            else if (typeString == "shout")
            {
                type = CHAT_TYPE_SHOUT;
            }
        }
        sendChat(type);
    }

    gAgent.stopTyping();
}

void FSFloaterNearbyChat::onChatTypeChanged()
{
    mSendChatButton->setLabel(mChatTypeCombo->getSelectedItemLabel());
}

void FSFloaterNearbyChat::sendChatFromViewer(const std::string &utf8text, EChatType type, bool animate)
{
    LLWString wtext = utf8string_to_wstring(utf8text);
    S32 channel = 0;
    bool is_set = false;
    LLWString out_text = FSNearbyChat::stripChannelNumber(wtext, &channel, &sLastSpecialChatChannel, &is_set);
    // If "/<number>" is not specified, see if a channel has been set in the spinner.
    if (!is_set &&
        gSavedSettings.getBOOL("FSNearbyChatbar") &&
        gSavedSettings.getBOOL("FSShowChatChannel"))
    {
        channel = (S32)(FSFloaterNearbyChat::getInstance()->getChild<LLSpinCtrl>("ChatChannel")->get());
    }

    FSNearbyChat::sendChatFromViewer(wtext, out_text, type, animate, channel);
}

// Exit "chat mode" and do the appropriate focus changes
// static
void FSFloaterNearbyChat::stopChat()
{
    FSFloaterNearbyChat* nearby_chat = LLFloaterReg::getTypedInstance<FSFloaterNearbyChat>("fs_nearby_chat");
    if (nearby_chat)
    {
        nearby_chat->mInputEditor->setFocus(false);
        gAgent.stopTyping();
    }
}

void FSFloaterNearbyChat::updateUnreadMessageNotification(S32 unread_messages, bool muted_history)
{
    bool show_muted_history = gSavedSettings.getBOOL("FSShowMutedChatHistory");

    if (muted_history)
    {
        mUnreadMessagesMuted = unread_messages;
        if (!show_muted_history)
        {
            return;
        }
    }
    else
    {
        mUnreadMessages = unread_messages;
        if (show_muted_history)
        {
            return;
        }
    }

    if (unread_messages == 0 || !gSavedSettings.getBOOL("FSNotifyUnreadChatMessages"))
    {
        mUnreadMessagesNotificationPanel->setVisible(false);
    }
    else
    {
        mUnreadMessagesNotificationTextBox->setTextArg("[NUM]", llformat("%d", unread_messages));
        mUnreadMessagesNotificationPanel->setVisible(true);
    }
}

void FSFloaterNearbyChat::updateShowMutedChatHistory(const LLSD &data)
{
    bool show_muted = data.asBoolean();
    updateUnreadMessageNotification((show_muted ? mUnreadMessagesMuted : mUnreadMessages), show_muted);
}

void FSFloaterNearbyChat::handleMinimized(bool minimized)
{
    if (minimized)
    {
        gConsole->removeSession(LLUUID::null);
    }
    else
    {
        gConsole->addSession(LLUUID::null);
    }
}

void FSFloaterNearbyChat::onEmojiRecentPanelToggleBtnClicked()
{
    bool show = mEmojiRecentPanel->getVisible() ? false : true;
    if (show)
    {
        initEmojiRecentPanel();
    }

    mEmojiRecentPanel->setVisible(show);
    mInputEditor->setFocus(true);
}

void FSFloaterNearbyChat::initEmojiRecentPanel()
{
    std::list<llwchar>& recentlyUsed = LLFloaterEmojiPicker::getRecentlyUsed();
    if (recentlyUsed.empty())
    {
        mEmojiRecentEmptyText->setVisible(true);
        mEmojiRecentIconsCtrl->setVisible(false);
    }
    else
    {
        LLWString emojis;
        for (llwchar emoji : recentlyUsed)
        {
            emojis += emoji;
        }
        mEmojiRecentIconsCtrl->setEmojis(emojis);
        mEmojiRecentEmptyText->setVisible(false);
        mEmojiRecentIconsCtrl->setVisible(true);
    }
}

void FSFloaterNearbyChat::onRecentEmojiPicked(const LLSD& value)
{
    LLSD::String str = value.asString();
    if (str.size())
    {
        LLWString wstr = utf8string_to_wstring(str);
        if (wstr.size())
        {
            llwchar emoji = wstr[0];
            mInputEditor->insertEmoji(emoji);
        }
    }
}

void FSFloaterNearbyChat::onEmojiPickerToggleBtnClicked()
{
    mInputEditor->setFocus(true);
    mInputEditor->showEmojiHelper();
}
