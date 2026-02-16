/**
 * @file fsnearbychatcontrol.cpp
 * @brief Nearby chat input control implementation
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
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "fsnearbychatcontrol.h"
#include "fsnearbychathub.h"
#include "lfsimfeaturehandler.h"
#include "llagent.h"        // gAgent
#include "llagentcamera.h"  // gAgentCamera
#include "llautoreplace.h"
#include "llchatmentionhelper.h"
#include "llemojihelper.h"
#include "llfloaterchatmentionpicker.h"
#include "llfloaterimnearbychathandler.h"
#include "llscrollcontainer.h"
#include "llworld.h"
#include "rlvactions.h"

static LLDefaultChildRegistry::Register<FSNearbyChatControl> r("fs_nearby_chat_control");

FSNearbyChatControl::FSNearbyChatControl(const FSNearbyChatControl::Params& p) :
    LLChatEntry(p),
    mDefault(p.is_default),
    mTextPadLeft(p.text_pad_left),
    mTextPadRight(p.text_pad_right)
{
    //<FS:TS> FIRE-11373: Autoreplace doesn't work in nearby chat bar
    setAutoreplaceCallback(boost::bind(&LLAutoReplace::autoreplaceCallback, LLAutoReplace::getInstance(), _1, _2, _3, _4, _5));
    setKeystrokeCallback([this](LLTextEditor* caller) { onKeystroke(caller); });
    FSNearbyChat::instance().registerChatBar(this);

    setCommitOnFocusLost(false);
    setPassDelete(true);
    setFont(LLViewerChat::getChatFont());
    enableSingleLineMode(true);

    setShowChatMentionPicker(!RlvActions::isRlvEnabled() || RlvActions::canShowName(RlvActions::SNC_DEFAULT));
    mRlvBehaviorCallbackConnection = gRlvHandler.setBehaviourToggleCallback(
        boost::bind(&FSNearbyChatControl::updateRlvRestrictions, this, _1));

    setShowEmojiHelper(gSavedSettings.getBOOL("FSEnableEmojiWindowPopupWhileTyping"));
    mEmojiHelperSettingConnection =
        gSavedSettings.getControl("FSEnableEmojiWindowPopupWhileTyping")->getSignal()->connect(
            boost::bind(&FSNearbyChatControl::updateEmojiHelperSetting, this, _2));

    // Register for font change notifications
    LLViewerChat::setFontChangedCallback(boost::bind(&FSNearbyChatControl::setFont, this, _1));
}

FSNearbyChatControl::~FSNearbyChatControl()
{
    if (mRlvBehaviorCallbackConnection.connected())
    {
        mRlvBehaviorCallbackConnection.disconnect();
    }
    if (mEmojiHelperSettingConnection.connected())
    {
        mEmojiHelperSettingConnection.disconnect();
    }
    LLFloaterChatMentionPicker::removeParticipantSource(this);
}

void FSNearbyChatControl::onKeystroke(LLTextEditor* caller)
{
    FSNearbyChat::handleChatBarKeystroke(caller);
}

// send our focus status to the LLNearbyChat hub
void FSNearbyChatControl::onFocusReceived()
{
    FSNearbyChat::instance().setFocusedInputEditor(this, true);
    LLFloaterChatMentionPicker::updateParticipantSource(this);
    LLChatEntry::onFocusReceived();
}

void FSNearbyChatControl::onFocusLost()
{
    FSNearbyChat::instance().setFocusedInputEditor(this, false);
    LLFloaterChatMentionPicker::removeParticipantSource(this);
    LLChatEntry::onFocusLost();
}

void FSNearbyChatControl::setFocus(bool focus)
{
    FSNearbyChat::instance().setFocusedInputEditor(this, focus);
    if (focus)
    {
        LLFloaterChatMentionPicker::updateParticipantSource(this);
    }
    else
    {
        LLFloaterChatMentionPicker::removeParticipantSource(this);
    }
    LLChatEntry::setFocus(focus);
}

void FSNearbyChatControl::draw()
{
    applyTextPadding();
    LLChatEntry::draw();
}

void FSNearbyChatControl::setTextPadding(S32 left, S32 right)
{
    mTextPadLeft = left;
    mTextPadRight = right;
    applyTextPadding();
}

void FSNearbyChatControl::applyTextPadding()
{
    LLRect base_rect = mScroller ? mScroller->getContentWindowRect() : getLocalRect();
    base_rect.mLeft = llmin(base_rect.mRight, base_rect.mLeft + mTextPadLeft);
    base_rect.mRight = llmax(base_rect.mLeft, base_rect.mRight - mTextPadRight);

    if (base_rect != mVisibleTextRect)
    {
        mVisibleTextRect = base_rect;
        needsReflow();
    }
}

void FSNearbyChatControl::autohide()
{
    if (isDefault())
    {
        if (gSavedSettings.getBOOL("CloseChatOnReturn"))
        {
            setFocus(false);
        }

        if (gAgentCamera.cameraMouselook() || gSavedSettings.getBOOL("AutohideChatBar"))
        {
            FSNearbyChat::instance().showDefaultChatBar(false);
        }
    }
}

// handle ESC key here
bool FSNearbyChatControl::handleKeyHere(KEY key, MASK mask)
{
    bool handled = false;
    EChatType type = CHAT_TYPE_NORMAL;

    if (LLChatMentionHelper::instance().isActive(this) ||
        LLEmojiHelper::instance().isActive(this))
    {
        return LLChatEntry::handleKeyHere(key, mask);
    }

    // autohide the chat bar if escape key was pressed and we're the default chat bar
    if (key == KEY_ESCAPE && mask == MASK_NONE)
    {
        // we let ESC key go through to the rest of the UI code, so don't set handled = true
        autohide();
        gAgent.stopTyping();
    }
    else if (KEY_RETURN == key)
    {
        if (mask == MASK_CONTROL && gSavedSettings.getBOOL("FSUseCtrlShout"))
        {
            // shout
            type = CHAT_TYPE_SHOUT;
            handled = true;
        }
        else if (mask == MASK_SHIFT && gSavedSettings.getBOOL("FSUseShiftWhisper"))
        {
            // whisper
            type = CHAT_TYPE_WHISPER;
            handled = true;
        }
        else if (mask == MASK_ALT && gSavedSettings.getBOOL("FSUseAltOOC"))
        {
            // OOC
            type = CHAT_TYPE_OOC;
            handled = true;
        }
        else if (mask == (MASK_SHIFT | MASK_CONTROL))
        {
            // linefeed
            addChar(llwchar(182));
            return true;
        }
        else
        {
            // say
            type = CHAT_TYPE_NORMAL;
            handled = true;
        }
    }

    if (handled)
    {
        // save current line in the history buffer
        updateHistory();

        // send chat to nearby chat hub
        FSNearbyChat::instance().sendChat(getConvertedText(), type);

        setText(LLStringExplicit(""));
        autohide();
        return true;
    }

    // let the line editor handle everything we don't handle
    return LLChatEntry::handleKeyHere(key, mask);
}

uuid_vec_t FSNearbyChatControl::getSessionParticipants() const
{
    if (!isAgentAvatarValid() || !LLWorld::instanceExists() || !LFSimFeatureHandler::instanceExists())
    {
        return {};
    }

    uuid_vec_t avatar_ids;
    LLWorld::instance().getAvatars(&avatar_ids, nullptr, gAgent.getPositionGlobal(), (F32)LFSimFeatureHandler::instance().sayRange());
    return avatar_ids;
}

void FSNearbyChatControl::updateRlvRestrictions(ERlvBehaviour behavior)
{
    if (behavior != RLV_BHVR_SHOWNAMES)
    {
        return;
    }

    setShowChatMentionPicker(!RlvActions::isRlvEnabled() || RlvActions::canShowName(RlvActions::SNC_DEFAULT));
}

void FSNearbyChatControl::updateEmojiHelperSetting(const LLSD& data)
{
    setShowEmojiHelper(data.asBoolean());
}
