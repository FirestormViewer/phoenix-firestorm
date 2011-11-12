/** 
 * @file llnearbychatbar.cpp
 * @brief LLNearbyChatBar class implementation
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
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

#include "llviewerprecompiledheaders.h"

#include "message.h"

#include "llappviewer.h"
#include "llfloaterreg.h"
#include "lltrans.h"

#include "llfirstuse.h"
#include "llnearbychatbar.h"
#include "llagent.h"
#include "llgesturemgr.h"
#include "llmultigesture.h"
#include "llkeyboard.h"
#include "llanimationstates.h"
#include "llviewerstats.h"
#include "llcommandhandler.h"
#include "llviewercontrol.h"
#include "llnavigationbar.h"
#include "llwindow.h"
#include "llviewerwindow.h"
#include "llrootview.h"
#include "llviewerchat.h"
#include "llnearbychat.h"
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-10-26 (Catznip-3.2.0a)
#include "llimfloater.h"
#include "llimfloatercontainer.h"
#include "llnearbychatbarmulti.h"
// [/SL:KB]

#include "llresizehandle.h"

//S32 LLNearbyChatBar::sLastSpecialChatChannel = 0;
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-08-20 (Catznip-3.2.0a) | Added: Catznip-2.8.0a
S32 LLNearbyChatBarBase::sLastSpecialChatChannel = 0;
// [/SL:KB]

const S32 EXPANDED_HEIGHT = 300;
const S32 COLLAPSED_HEIGHT = 60;
const S32 EXPANDED_MIN_HEIGHT = 150;

// legacy callback glue
void send_chat_from_viewer(const std::string& utf8_out_text, EChatType type, S32 channel);

struct LLChatTypeTrigger {
	std::string name;
	EChatType type;
};

static LLChatTypeTrigger sChatTypeTriggers[] = {
	{ "/whisper"	, CHAT_TYPE_WHISPER},
	{ "/shout"	, CHAT_TYPE_SHOUT}
};

// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-10-26 (Catznip-3.2.0a) | Added: Catznip-3.2.0a
LLNearbyChatBarSingle::LLNearbyChatBarSingle() 
	: LLPanel()
	, mChatBox(NULL)
	, mOutputMonitor(NULL)
	, mSpeakerMgr(LLLocalSpeakerMgr::getInstance())
{
}
// [/SL:KB]

LLNearbyChatBar::LLNearbyChatBar(const LLSD& key)
	: LLFloater(key),
//	mChatBox(NULL)
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-10-26 (Catznip-3.2.0a) | Added: Catznip-3.2.0a
	mChatBarImpl(NULL)
// [/SL:KB]
{
//	mSpeakerMgr = LLLocalSpeakerMgr::getInstance();
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-10-26 (Catznip-3.2.0a) | Modified: Catznip-3.2.0a
	mFactoryMap["panel_chat_bar_single"] = LLCallbackMap(createChatBarSingle, NULL);
	mFactoryMap["panel_chat_bar_multi"] = LLCallbackMap(createChatBarMulti, NULL);
// [/SL:KB]
}

//virtual
//BOOL LLNearbyChatBar::postBuild()
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-10-26 (Catznip-3.2.0a) | Added: Catznip-3.2.0a
BOOL LLNearbyChatBarSingle::postBuild()
// [/SL:KB]
{
	mChatBox = getChild<LLLineEditor>("chat_box");

//	mChatBox->setCommitCallback(boost::bind(&LLNearbyChatBar::onChatBoxCommit, this));
//	mChatBox->setKeystrokeCallback(&onChatBoxKeystroke, this);
//	mChatBox->setFocusLostCallback(boost::bind(&onChatBoxFocusLost, _1, this));
//	mChatBox->setFocusReceivedCallback(boost::bind(&LLNearbyChatBar::onChatBoxFocusReceived, this));
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-10-26 (Catznip-3.2.0a) | Modified: Catznip-3.2.0a
	mChatBox->setCommitCallback(boost::bind(&LLNearbyChatBarSingle::onChatBoxCommit, this));
	mChatBox->setKeystrokeCallback(boost::bind(&LLNearbyChatBarSingle::onChatBoxKeystroke, this), NULL);
	mChatBox->setFocusLostCallback(boost::bind(&LLNearbyChatBarSingle::onChatBoxFocusLost, this));
	mChatBox->setFocusReceivedCallback(boost::bind(&LLNearbyChatBarSingle::onChatBoxFocusReceived, this));
// [/SL:KB]

	mChatBox->setIgnoreArrowKeys( FALSE ); 
	mChatBox->setCommitOnFocusLost( FALSE );
	mChatBox->setRevertOnEsc( FALSE );
	mChatBox->setIgnoreTab(TRUE);
	mChatBox->setPassDelete(TRUE);
	mChatBox->setReplaceNewlinesWithSpaces(FALSE);
	mChatBox->setEnableLineHistory(TRUE);
	mChatBox->setFont(LLViewerChat::getChatFont());

//	LLUICtrl* show_btn = getChild<LLUICtrl>("show_nearby_chat");
//	show_btn->setCommitCallback(boost::bind(&LLNearbyChatBar::onToggleNearbyChatPanel, this));

	mOutputMonitor = getChild<LLOutputMonitorCtrl>("chat_zone_indicator");
	mOutputMonitor->setVisible(FALSE);

	// Register for font change notifications
//	LLViewerChat::setFontChangedCallback(boost::bind(&LLNearbyChatBar::onChatFontChange, this, _1));
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-10-26 (Catznip-3.2.0a) | Added: Catznip-3.2.0a
	LLViewerChat::setFontChangedCallback(boost::bind(&LLNearbyChatBarSingle::onChatFontChange, this, _1));

	return TRUE;
}
// [/SL:KB]

// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-10-26 (Catznip-3.2.0a) | Added: Catznip-3.2.0a
BOOL LLNearbyChatBar::postBuild()
{
	mChatBarImpl = (hasChild("panel_chat_bar_multi")) ? findChild<LLNearbyChatBarBase>("panel_chat_bar_multi") : findChild<LLNearbyChatBarBase>("panel_chat_bar_single");

	LLUICtrl* show_btn = getChild<LLUICtrl>("show_nearby_chat");
	show_btn->setCommitCallback(boost::bind(&LLNearbyChatBar::onToggleNearbyChatPanel, this));
// [/SL:KB]

	mExpandedHeight = COLLAPSED_HEIGHT + EXPANDED_HEIGHT;

	enableResizeCtrls(true, true, false);

// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-10-26 (Catznip-3.2.0a) | Modified: Catznip-3.2.0a
	// Initalize certain parameters depending on default vs embedded state
	bool fTabbedNearbyChat = isTabbedNearbyChat();
	setCanClose(!fTabbedNearbyChat);
	setCanMinimize(fTabbedNearbyChat);
	setCanTearOff(fTabbedNearbyChat);

	if (fTabbedNearbyChat)
	{
		LLIMFloaterContainer* pConvFloater = LLIMFloaterContainer::getInstance();
		if (pConvFloater)
		{
			if (!getChildView("nearby_chat")->getVisible())
				onToggleNearbyChatPanel();
			pConvFloater->LLMultiFloater::addFloater(this, TRUE, LLTabContainer::START);
		}
	}
// [/SL:KB]

	return TRUE;
}

bool LLNearbyChatBar::applyRectControl()
{
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-10-26 (Catznip-3.2.0a) | Added: Catznip-3.2.0a
	if (isTabbedNearbyChat())
	{
		return true;
	}
// [/SL:KB]

	bool rect_controlled = LLFloater::applyRectControl();

	LLView* nearby_chat = getChildView("nearby_chat");	
	if (!nearby_chat->getVisible())
	{
		reshape(getRect().getWidth(), getMinHeight());
		enableResizeCtrls(true, true, false);
	}
	else
	{
		enableResizeCtrls(true);
		setResizeLimits(getMinWidth(), EXPANDED_MIN_HEIGHT);
	}
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-10-26 (Catznip-3.2.0a) | Added: Catznip-3.2.0a
	getChildView("panel_nearby_chat")->setVisible(mExpandedHeight > COLLAPSED_HEIGHT);
// [/SL:KB]
	
	return rect_controlled;
}

//void LLNearbyChatBar::onChatFontChange(LLFontGL* fontp)
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-10-26 (Catznip-3.2.0a) | Added: Catznip-3.2.0a
void LLNearbyChatBarSingle::onChatFontChange(LLFontGL* fontp)
// [/SL:KB]
{
	// Update things with the new font whohoo
	if (mChatBox)
	{
		mChatBox->setFont(fontp);
	}
}

//static
LLNearbyChatBar* LLNearbyChatBar::getInstance()
{
	return LLFloaterReg::getTypedInstance<LLNearbyChatBar>("chat_bar");
}

void LLNearbyChatBar::showHistory()
{
	openFloater();

	if (!getChildView("nearby_chat")->getVisible())
	{
		onToggleNearbyChatPanel();
	}
}

//void LLNearbyChatBar::draw()
//{
//	displaySpeakingIndicator();
//	LLFloater::draw();
//}
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-10-26 (Catznip-3.2.0a) | Added: Catznip-3.2.0a
void LLNearbyChatBarSingle::draw()
{
	displaySpeakingIndicator();
	LLPanel::draw();
}
// [/SL:KB]

//std::string LLNearbyChatBar::getCurrentChat()
//{
//	return mChatBox ? mChatBox->getText() : LLStringUtil::null;
//}

// virtual
BOOL LLNearbyChatBar::handleKeyHere( KEY key, MASK mask )
{
	BOOL handled = FALSE;

	if( KEY_RETURN == key && mask == MASK_CONTROL)
	{
		// shout
//		sendChat(CHAT_TYPE_SHOUT);
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-10-26 (Catznip-3.2.0a) | Modified: Catznip-3.2.0a
		mChatBarImpl->sendChat(CHAT_TYPE_SHOUT);
// [/SL:KB]
		handled = TRUE;
	}
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-08-20 (Catznip-3.2.0a) | Added: Catznip-2.8.0a
	if ( (KEY_RETURN == key) && (mask == MASK_SHIFT) )
	{
		// Whisper
		mChatBarImpl->sendChat(CHAT_TYPE_WHISPER);
		handled = TRUE;
	}
// [/SL:KB]

	return handled;
}

//BOOL LLNearbyChatBar::matchChatTypeTrigger(const std::string& in_str, std::string* out_str)
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-08-20 (Catznip-3.2.0a) | Added: Catznip-2.8.0a
BOOL LLNearbyChatBarBase::matchChatTypeTrigger(const std::string& in_str, std::string* out_str)
// [/SL:KB]
{
	U32 in_len = in_str.length();
	S32 cnt = sizeof(sChatTypeTriggers) / sizeof(*sChatTypeTriggers);
	
	for (S32 n = 0; n < cnt; n++)
	{
		if (in_len > sChatTypeTriggers[n].name.length())
			continue;

		std::string trigger_trunc = sChatTypeTriggers[n].name;
		LLStringUtil::truncate(trigger_trunc, in_len);

		if (!LLStringUtil::compareInsensitive(in_str, trigger_trunc))
		{
			*out_str = sChatTypeTriggers[n].name;
			return TRUE;
		}
	}

	return FALSE;
}

//void LLNearbyChatBar::onChatBoxKeystroke(LLLineEditor* caller, void* userdata)
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-10-26 (Catznip-3.2.0a) | Added: Catznip-3.2.0a
void LLNearbyChatBarBase::onChatBoxKeystroke()
// [/SL:KB]
{
	LLFirstUse::otherAvatarChatFirst(false);

//	LLNearbyChatBar* self = (LLNearbyChatBar *)userdata;
//
//	LLWString raw_text = self->mChatBox->getWText();
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-10-26 (Catznip-3.2.0a) | Added: Catznip-3.2.0a
	LLLineEditor* pLineEditor = dynamic_cast<LLLineEditor*>(getChatBoxCtrl());
	LLTextEditor* pTextEditor = dynamic_cast<LLTextEditor*>(getChatBoxCtrl());

	LLWString raw_text = (pLineEditor) ? pLineEditor->getWText() : pTextEditor->getWText();
// [/SL:KB]

	// Can't trim the end, because that will cause autocompletion
	// to eat trailing spaces that might be part of a gesture.
	LLWStringUtil::trimHead(raw_text);

	S32 length = raw_text.length();

	if( (length > 0) && (raw_text[0] != '/') )  // forward slash is used for escape (eg. emote) sequences
	{
		gAgent.startTyping();
	}
	else
	{
		gAgent.stopTyping();
	}

	/* Doesn't work -- can't tell the difference between a backspace
	   that killed the selection vs. backspace at the end of line.
	if (length > 1 
		&& text[0] == '/'
		&& key == KEY_BACKSPACE)
	{
		// the selection will already be deleted, but we need to trim
		// off the character before
		std::string new_text = raw_text.substr(0, length-1);
		self->mInputEditor->setText( new_text );
		self->mInputEditor->setCursorToEnd();
		length = length - 1;
	}
	*/

	KEY key = gKeyboard->currentKey();

	// Ignore "special" keys, like backspace, arrows, etc.
	if (length > 1 
		&& raw_text[0] == '/'
		&& key < KEY_SPECIAL)
	{
		// we're starting a gesture, attempt to autocomplete

		std::string utf8_trigger = wstring_to_utf8str(raw_text);
		std::string utf8_out_str(utf8_trigger);

		if (LLGestureMgr::instance().matchPrefix(utf8_trigger, &utf8_out_str))
		{
			std::string rest_of_match = utf8_out_str.substr(utf8_trigger.size());
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-10-26 (Catznip-3.2.0a) | Added: Catznip-3.2.0a
			if (pLineEditor)
			{
				pLineEditor->setText(utf8_trigger + rest_of_match);
				pLineEditor->setSelection(length, pLineEditor->getLength());
			}
			else
			{
				pTextEditor->setText(utf8_trigger + rest_of_match);
				pTextEditor->setSelection(length, pTextEditor->getLength());
			}
// [/SL:KB]
//			self->mChatBox->setText(utf8_trigger + rest_of_match); // keep original capitalization for user-entered part
//			S32 outlength = self->mChatBox->getLength(); // in characters
//
//			// Select to end of line, starting from the character
//			// after the last one the user typed.
//			self->mChatBox->setSelection(length, outlength);
		}
		else if (matchChatTypeTrigger(utf8_trigger, &utf8_out_str))
		{
			std::string rest_of_match = utf8_out_str.substr(utf8_trigger.size());
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-10-26 (Catznip-3.2.0a) | Added: Catznip-3.2.0a
			if (pLineEditor)
			{
				pLineEditor->setText(utf8_trigger + rest_of_match + " ");
				pLineEditor->setCursorToEnd();
			}
			else
			{
				pTextEditor->setText(utf8_trigger + rest_of_match + " ");
				pTextEditor->endOfDoc();
			}
// [/SL:KB]
//			self->mChatBox->setText(utf8_trigger + rest_of_match + " "); // keep original capitalization for user-entered part
//			self->mChatBox->setCursorToEnd();
		}

		//llinfos << "GESTUREDEBUG " << trigger 
		//	<< " len " << length
		//	<< " outlen " << out_str.getLength()
		//	<< llendl;
	}
}

// static
//void LLNearbyChatBar::onChatBoxFocusLost(LLFocusableElement* caller, void* userdata)
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-08-20 (Catznip-3.2.0a) | Added: Catznip-2.8.0a
void LLNearbyChatBarBase::onChatBoxFocusLost()
// [/SL:KB]
{
	// stop typing animation
	gAgent.stopTyping();
}

//void LLNearbyChatBar::onChatBoxFocusReceived()
//{
//	mChatBox->setEnabled(!gDisconnected);
//}
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-08-20 (Catznip-3.2.0a) | Added: Catznip-2.8.0a
void LLNearbyChatBarBase::onChatBoxFocusReceived()
{
	getChatBoxCtrl()->setEnabled(!gDisconnected);
}
// [/SL:KB]

//EChatType LLNearbyChatBar::processChatTypeTriggers(EChatType type, std::string &str)
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-08-20 (Catznip-3.2.0a) | Added: Catznip-2.8.0a
EChatType LLNearbyChatBarBase::processChatTypeTriggers(EChatType type, std::string &str)
// [/SL:KB]
{
	U32 length = str.length();
	S32 cnt = sizeof(sChatTypeTriggers) / sizeof(*sChatTypeTriggers);
	
	for (S32 n = 0; n < cnt; n++)
	{
		if (length >= sChatTypeTriggers[n].name.length())
		{
			std::string trigger = str.substr(0, sChatTypeTriggers[n].name.length());

			if (!LLStringUtil::compareInsensitive(trigger, sChatTypeTriggers[n].name))
			{
				U32 trigger_length = sChatTypeTriggers[n].name.length();

				// It's to remove space after trigger name
				if (length > trigger_length && str[trigger_length] == ' ')
					trigger_length++;

				str = str.substr(trigger_length, length);

				if (CHAT_TYPE_NORMAL == type)
					return sChatTypeTriggers[n].type;
				else
					break;
			}
		}
	}

	return type;
}

//void LLNearbyChatBar::sendChat( EChatType type )
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-10-26 (Catznip-3.2.0a) | Modified: Catznip-3.2.0a
void LLNearbyChatBarBase::sendChat(EChatType type)
// [/SL:KB]
{
//	if (mChatBox)
//	{
//		LLWString text = mChatBox->getConvertedText();
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-10-26 (Catznip-3.2.0a) | Modified: Catznip-3.2.0a
	LLLineEditor* pLineEditor = dynamic_cast<LLLineEditor*>(getChatBoxCtrl());
	LLTextEditor* pTextEditor = dynamic_cast<LLTextEditor*>(getChatBoxCtrl());
	if ( (pLineEditor) || (pTextEditor) )
	{
		LLWString text = getChatBoxText();
		LLWStringUtil::trim(text);
// [/SL:KB]
		if (!text.empty())
		{
			// store sent line in history, duplicates will get filtered
//			mChatBox->updateHistory();
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-08-20 (Catznip-3.2.0a) | Added: Catznip-2.8.0a
			// The multi-line chat bar history is updated in LLNearbyChatBarMulti::onChatBoxCommit()
			if (pLineEditor)
				pLineEditor->updateHistory();
// [/SL:KB]
			// Check if this is destined for another channel
			S32 channel = 0;
			stripChannelNumber(text, &channel);
			
			std::string utf8text = wstring_to_utf8str(text);
			// Try to trigger a gesture, if not chat to a script.
			std::string utf8_revised_text;
			if (0 == channel)
			{
				// discard returned "found" boolean
				LLGestureMgr::instance().triggerAndReviseString(utf8text, &utf8_revised_text);
			}
			else
			{
				utf8_revised_text = utf8text;
			}

			utf8_revised_text = utf8str_trim(utf8_revised_text);

			type = processChatTypeTriggers(type, utf8_revised_text);

			if (!utf8_revised_text.empty())
			{
				// Chat with animation
				sendChatFromViewer(utf8_revised_text, type, TRUE);
			}
		}

//		mChatBox->setText(LLStringExplicit(""));
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-08-20 (Catznip-3.2.0a) | Added: Catznip-2.8.0a
		setChatBoxText(LLStringExplicit(""));
// [/SL:KB]
	}

	gAgent.stopTyping();

	// If the user wants to stop chatting on hitting return, lose focus
	// and go out of chat mode.
	if (gSavedSettings.getBOOL("CloseChatOnReturn"))
	{
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-10-26 (Catznip-3.2.0a) | Modified: Catznip-3.2.0a
		getChatBoxCtrl()->setFocus(FALSE);
// [/SL:KB]
//		stopChat();
	}
}


void LLNearbyChatBar::onToggleNearbyChatPanel()
{
//	LLView* nearby_chat = getChildView("nearby_chat");
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-10-26 (Catznip-3.2.0a) | Added: Catznip-3.2.0a
	LLView* nearby_chat = getChildView("panel_nearby_chat");
// [/SL:KB]

	if (nearby_chat->getVisible())
	{
		if (!isMinimized())
		{
			mExpandedHeight = getRect().getHeight();
		}
		setResizeLimits(getMinWidth(), COLLAPSED_HEIGHT);
		nearby_chat->setVisible(FALSE);
		reshape(getRect().getWidth(), COLLAPSED_HEIGHT);
		enableResizeCtrls(true, true, false);
		storeRectControl();
	}
	else
	{
		nearby_chat->setVisible(TRUE);
		setResizeLimits(getMinWidth(), EXPANDED_MIN_HEIGHT);
		reshape(getRect().getWidth(), mExpandedHeight);
		enableResizeCtrls(true);
		storeRectControl();
	}
}

void LLNearbyChatBar::setMinimized(BOOL b)
{
	LLNearbyChat* nearby_chat = getChild<LLNearbyChat>("nearby_chat");
	// when unminimizing with nearby chat visible, go ahead and kill off screen chats
	if (!b && nearby_chat->getVisible())
	{
		nearby_chat->removeScreenChat();
	}
		LLFloater::setMinimized(b);
}

//void LLNearbyChatBar::onChatBoxCommit()
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-10-26 (Catznip-3.2.0a) | Added: Catznip-3.2.0a
void LLNearbyChatBarSingle::onChatBoxCommit()
// [/SL:KB]
{
	if (mChatBox->getText().length() > 0)
	{
		sendChat(CHAT_TYPE_NORMAL);
	}

	gAgent.stopTyping();
}

//void LLNearbyChatBar::displaySpeakingIndicator()
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-10-26 (Catznip-3.2.0a) | Added: Catznip-3.2.0a
void LLNearbyChatBarSingle::displaySpeakingIndicator()
// [/SL:KB]
{
	LLSpeakerMgr::speaker_list_t speaker_list;
	LLUUID id;

	id.setNull();
	mSpeakerMgr->update(TRUE);
	mSpeakerMgr->getSpeakerList(&speaker_list, FALSE);

	for (LLSpeakerMgr::speaker_list_t::iterator i = speaker_list.begin(); i != speaker_list.end(); ++i)
	{
		LLPointer<LLSpeaker> s = *i;
		if (s->mSpeechVolume > 0 || s->mStatus == LLSpeaker::STATUS_SPEAKING)
		{
			id = s->mID;
			break;
		}
	}

	if (!id.isNull())
	{
		mOutputMonitor->setVisible(TRUE);
		mOutputMonitor->setSpeakerId(id);
	}
	else
	{
		mOutputMonitor->setVisible(FALSE);
	}
}

//void LLNearbyChatBar::sendChatFromViewer(const std::string &utf8text, EChatType type, BOOL animate)
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-08-20 (Catznip-3.2.0a) | Added: Catznip-2.8.0a
void LLNearbyChatBarBase::sendChatFromViewer(const std::string &utf8text, EChatType type, BOOL animate)
// [/SL:KB]
{
	sendChatFromViewer(utf8str_to_wstring(utf8text), type, animate);
}

//void LLNearbyChatBar::sendChatFromViewer(const LLWString &wtext, EChatType type, BOOL animate)
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-08-20 (Catznip-3.2.0a) | Added: Catznip-2.8.0a
void LLNearbyChatBarBase::sendChatFromViewer(const LLWString &wtext, EChatType type, BOOL animate)
// [/SL:KB]
{
	// Look for "/20 foo" channel chats.
	S32 channel = 0;
	LLWString out_text = stripChannelNumber(wtext, &channel);
	std::string utf8_out_text = wstring_to_utf8str(out_text);
	std::string utf8_text = wstring_to_utf8str(wtext);

	utf8_text = utf8str_trim(utf8_text);
	if (!utf8_text.empty())
	{
		utf8_text = utf8str_truncate(utf8_text, MAX_STRING - 1);
	}

	// Don't animate for chats people can't hear (chat to scripts)
	if (animate && (channel == 0))
	{
		if (type == CHAT_TYPE_WHISPER)
		{
			lldebugs << "You whisper " << utf8_text << llendl;
			gAgent.sendAnimationRequest(ANIM_AGENT_WHISPER, ANIM_REQUEST_START);
		}
		else if (type == CHAT_TYPE_NORMAL)
		{
			lldebugs << "You say " << utf8_text << llendl;
			gAgent.sendAnimationRequest(ANIM_AGENT_TALK, ANIM_REQUEST_START);
		}
		else if (type == CHAT_TYPE_SHOUT)
		{
			lldebugs << "You shout " << utf8_text << llendl;
			gAgent.sendAnimationRequest(ANIM_AGENT_SHOUT, ANIM_REQUEST_START);
		}
		else
		{
			llinfos << "send_chat_from_viewer() - invalid volume" << llendl;
			return;
		}
	}
	else
	{
		if (type != CHAT_TYPE_START && type != CHAT_TYPE_STOP)
		{
			lldebugs << "Channel chat: " << utf8_text << llendl;
		}
	}

	send_chat_from_viewer(utf8_out_text, type, channel);
}

// static 
//void LLNearbyChatBar::startChat(const char* line)
//{
//	LLNearbyChatBar* cb = LLNearbyChatBar::getInstance();
//
//	if (!cb )
//		return;
//
//	cb->setVisible(TRUE);
//	cb->setFocus(TRUE);
//	cb->mChatBox->setFocus(TRUE);
//
//	if (line)
//	{
//		std::string line_string(line);
//		cb->mChatBox->setText(line_string);
//	}
//
//	cb->mChatBox->setCursorToEnd();
//}
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-10-26 (Catznip-3.2.0a) | Added: Catznip-3.2.0a
void LLNearbyChatBar::startChat(const char* line)
{
	LLNearbyChatBar* pSelf = getInstance();
	if (!pSelf)
		return;
	
	pSelf->setVisible(TRUE);
	pSelf->setFocus(TRUE);
	pSelf->mChatBarImpl->getChatBoxCtrl()->setFocus(TRUE);

	if (line)
	{
		LLStringExplicit line_string(line);
		pSelf->mChatBarImpl->setChatBoxText(line_string);
	}

	pSelf->mChatBarImpl->setChatBoxCursorToEnd();
}
// [/SL:KB]

// Exit "chat mode" and do the appropriate focus changes
// static
//void LLNearbyChatBar::stopChat()
//{
//	LLNearbyChatBar* cb = LLNearbyChatBar::getInstance();
//
//	if (!cb)
//		return;
//
//	cb->mChatBox->setFocus(FALSE);
//
// 	// stop typing animation
// 	gAgent.stopTyping();
//}
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-10-26 (Catznip-3.2.0a) | Modified: Catznip-3.2.0a
void LLNearbyChatBar::stopChat()
{
	LLNearbyChatBar* pSelf = getInstance();
	if (!pSelf)
		return;

	pSelf->getChatBarImpl()->getChatBoxCtrl()->setFocus(FALSE);

 	// stop typing animation
 	gAgent.stopTyping();
}
// [/SL:KB]

// If input of the form "/20foo" or "/20 foo", returns "foo" and channel 20.
// Otherwise returns input and channel 0.
//LLWString LLNearbyChatBar::stripChannelNumber(const LLWString &mesg, S32* channel)
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-08-20 (Catznip-3.2.0a) | Added: Catznip-2.8.0a
LLWString LLNearbyChatBarBase::stripChannelNumber(const LLWString &mesg, S32* channel)
// [/SL:KB]
{
	if (mesg[0] == '/'
		&& mesg[1] == '/')
	{
		// This is a "repeat channel send"
		*channel = sLastSpecialChatChannel;
		return mesg.substr(2, mesg.length() - 2);
	}
	else if (mesg[0] == '/'
			 && mesg[1]
			 && LLStringOps::isDigit(mesg[1]))
	{
		// This a special "/20" speak on a channel
		S32 pos = 0;

		// Copy the channel number into a string
		LLWString channel_string;
		llwchar c;
		do
		{
			c = mesg[pos+1];
			channel_string.push_back(c);
			pos++;
		}
		while(c && pos < 64 && LLStringOps::isDigit(c));
		
		// Move the pointer forward to the first non-whitespace char
		// Check isspace before looping, so we can handle "/33foo"
		// as well as "/33 foo"
		while(c && iswspace(c))
		{
			c = mesg[pos+1];
			pos++;
		}
		
		sLastSpecialChatChannel = strtol(wstring_to_utf8str(channel_string).c_str(), NULL, 10);
		*channel = sLastSpecialChatChannel;
		return mesg.substr(pos, mesg.length() - pos);
	}
	else
	{
		// This is normal chat.
		*channel = 0;
		return mesg;
	}
}

// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-10-26 (Catznip-3.2.0a) | Modified: Catznip-3.2.0a
void* LLNearbyChatBar::createChatBarSingle(void*)
{
	return new LLNearbyChatBarSingle();
}

void* LLNearbyChatBar::createChatBarMulti(void*)
{
	return new LLNearbyChatBarMulti();
}

const std::string& LLNearbyChatBar::getFloaterXMLFile()
{
	static std::string strFile;
	switch (gSavedSettings.getS32("NearbyChatFloaterBarType"))
	{
		case 2:		// Multi-line
			strFile = "floater_chat_bar_multi.xml";
			break;
		case 0:		// None (default)
		case 1:		// Single-line
		default:
			strFile = "floater_chat_bar.xml";
			break;
	}
	return strFile;
}

bool LLNearbyChatBar::isTabbedNearbyChat()
{
	return (LLIMFloater::isChatMultiTab()) && (gSavedSettings.getBOOL("NearbyChatFloaterWindow"));
}

void LLNearbyChatBar::processFloaterTypeChanged()
{
//	// We only need to do anything if an instance of the nearby chat floater already exists
//	LLNearbyChat* pNearbyChat = LLFloaterReg::findTypedInstance<LLNearbyChat>("nearby_chat", LLSD());
//	if (pNearbyChat)
//	{
//		bool fVisible = pNearbyChat->getVisible();
//		std::vector<LLChat> msgArchive = pNearbyChat->mMessageArchive;
//
//		// NOTE: * LLFloater::closeFloater() won't call LLFloater::destroy() since the nearby chat floater is single instaced
//		//       * we can't call LLFloater::destroy() since it will call LLMortician::die() which defers destruction until a later time
//		//   => we'll have created a new instance and the delayed destructor calling LLFloaterReg::removeInstance() will make all future
//		//      LLFloaterReg::getTypedInstance() calls return NULL so we need to destruct manually [see LLFloaterReg::destroyInstance()]
//		pNearbyChat->closeFloater();
//		LLFloaterReg::destroyInstance("nearby_chat", LLSD());
//
//		if ((pNearbyChat = LLFloaterReg::getTypedInstance<LLNearbyChat>("nearby_chat", LLSD())) != NULL)
//		{
//			pNearbyChat->mMessageArchive = msgArchive;
//			pNearbyChat->updateChatHistoryStyle();
//			if (fVisible)
//				pNearbyChat->openFloater(LLSD());
//		}
//	}
}
// [/SL:KB]

void send_chat_from_viewer(const std::string& utf8_out_text, EChatType type, S32 channel)
{
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_ChatFromViewer);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_ChatData);
	msg->addStringFast(_PREHASH_Message, utf8_out_text);
	msg->addU8Fast(_PREHASH_Type, type);
	msg->addS32("Channel", channel);

	gAgent.sendReliableMessage();

	LLViewerStats::getInstance()->incStat(LLViewerStats::ST_CHAT_COUNT);
}

class LLChatCommandHandler : public LLCommandHandler
{
public:
	// not allowed from outside the app
	LLChatCommandHandler() : LLCommandHandler("chat", UNTRUSTED_BLOCK) { }

    // Your code here
	bool handle(const LLSD& tokens, const LLSD& query_map,
				LLMediaCtrl* web)
	{
		bool retval = false;
		// Need at least 2 tokens to have a valid message.
		if (tokens.size() < 2)
		{
			retval = false;
		}
		else
		{
		S32 channel = tokens[0].asInteger();
			// VWR-19499 Restrict function to chat channels greater than 0.
			if ((channel > 0) && (channel < CHAT_CHANNEL_DEBUG))
			{
				retval = true;
		// Send unescaped message, see EXT-6353.
		std::string unescaped_mesg (LLURI::unescape(tokens[1].asString()));
		send_chat_from_viewer(unescaped_mesg, CHAT_TYPE_NORMAL, channel);
			}
			else
			{
				retval = false;
				// Tell us this is an unsupported SLurl.
			}
		}
		return retval;
	}
};

// Creating the object registers with the dispatcher.
LLChatCommandHandler gChatHandler;


