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
#include "llagent.h" 		// gAgent
#include "llagentcamera.h"	// gAgentCamera
#include "llautoreplace.h"
#include "llfloaterimnearbychathandler.h"

static LLDefaultChildRegistry::Register<FSNearbyChatControl> r("fs_nearby_chat_control");

FSNearbyChatControl::FSNearbyChatControl(const FSNearbyChatControl::Params& p) :
	LLLineEditor(p),
	mDefault(p.is_default)
{
	//<FS:TS> FIRE-11373: Autoreplace doesn't work in nearby chat bar
	setAutoreplaceCallback(boost::bind(&LLAutoReplace::autoreplaceCallback, LLAutoReplace::getInstance(), _1, _2, _3, _4, _5));
	setKeystrokeCallback(onKeystroke, this);
	FSNearbyChat::instance().registerChatBar(this);

	setEnableLineHistory(TRUE);
	setIgnoreArrowKeys(FALSE);
	setCommitOnFocusLost(FALSE);
	setRevertOnEsc(FALSE);
	setIgnoreTab(TRUE);
	setReplaceNewlinesWithSpaces(FALSE);
	setPassDelete(TRUE);
	setFont(LLViewerChat::getChatFont());

	// Register for font change notifications
	LLViewerChat::setFontChangedCallback(boost::bind(&FSNearbyChatControl::setFont, this, _1));
}

FSNearbyChatControl::~FSNearbyChatControl()
{
}

void FSNearbyChatControl::onKeystroke(LLLineEditor* caller, void* userdata)
{
	FSNearbyChat::handleChatBarKeystroke(caller);
}

// send our focus status to the LLNearbyChat hub
void FSNearbyChatControl::onFocusReceived()
{
	FSNearbyChat::instance().setFocusedInputEditor(this, TRUE);
	LLLineEditor::onFocusReceived();
}

void FSNearbyChatControl::onFocusLost()
{
	FSNearbyChat::instance().setFocusedInputEditor(this, FALSE);
	LLLineEditor::onFocusLost();
}

void FSNearbyChatControl::setFocus(BOOL focus)
{
	FSNearbyChat::instance().setFocusedInputEditor(this, focus);
	LLLineEditor::setFocus(focus);
}

void FSNearbyChatControl::autohide()
{
	if (isDefault())
	{
		if (gSavedSettings.getBOOL("CloseChatOnReturn"))
		{
			setFocus(FALSE);
		}

		if (gAgentCamera.cameraMouselook() || gSavedSettings.getBOOL("AutohideChatBar"))
		{
			FSNearbyChat::instance().showDefaultChatBar(FALSE);
		}
	}
}

// handle ESC key here
BOOL FSNearbyChatControl::handleKeyHere(KEY key, MASK mask)
{
	bool handled = false;
	EChatType type = CHAT_TYPE_NORMAL;

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
			return TRUE;
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
		LLLineEditor::onCommit();

		// send chat to nearby chat hub
		FSNearbyChat::instance().sendChat(getConvertedText(), type);

		setText(LLStringExplicit(""));
		autohide();
		return TRUE;
	}

	// let the line editor handle everything we don't handle
	return LLLineEditor::handleKeyHere(key, mask);
}
