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
#include "llspinctrl.h"
#include "llnearbychat.h"
#include "llviewerwindow.h"
#include "llrootview.h"
#include "llviewerchat.h"
#include "llnearbychat.h"
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-10-26 (Catznip-3.2.0a)
#include "llevents.h"
#include "llimfloater.h"
#include "llimfloatercontainer.h"
#include "llmultifloater.h"
#include "llnearbychatbarmulti.h"
#include "lltranslate.h"
// [/SL:KB]

#include "llresizehandle.h"

// [RLVa:KB] - Checked: 2010-02-27 (RLVa-1.2.0b)
#include "rlvhandler.h"
// [/RLVa:KB]
#include "chatbar_as_cmdline.h"
#include "llviewerchat.h"

//S32 LLNearbyChatBar::sLastSpecialChatChannel = 0;
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-08-20 (Catznip-3.2.0a) | Added: Catznip-2.8.0a
S32 LLNearbyChatBarBase::sLastSpecialChatChannel = 0;
// [/SL:KB]

const S32 EXPANDED_HEIGHT = 300;
//const S32 COLLAPSED_HEIGHT = 60;
const S32 EXPANDED_MIN_HEIGHT = 150;
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-11-12 (Catznip-3.2.0a) | Added: Catznip-3.2.0a
S32 COLLAPSED_HEIGHT = 60;
// [/SL:KB]

// legacy callback glue
//void send_chat_from_viewer(const std::string& utf8_out_text, EChatType type, S32 channel);
// [RLVa:KB] - Checked: 2010-02-27 (RLVa-1.2.0b) | Modified: RLVa-0.2.2a
void send_chat_from_viewer(std::string utf8_out_text, EChatType type, S32 channel);
// [/RLVa:KB]

struct LLChatTypeTrigger {
	std::string name;
	EChatType type;
};

static LLChatTypeTrigger sChatTypeTriggers[] = {
	{ "/whisper"	, CHAT_TYPE_WHISPER},
	{ "/shout"	, CHAT_TYPE_SHOUT}
};

// <FS:ND>
LLNearbyChatBarBase::LLNearbyChatBarBase()
{
	FSPlayChatAnimation = gSavedSettings.getBOOL("FSPlayChatAnimation");
	gSavedSettings.getControl("FSPlayChatAnimation")->getSignal()->connect(boost::bind(&LLNearbyChatBarBase::updateFSPlayChatAnimation, this, _2));
}
// </FS:ND>

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
:	LLFloater(key),
//	mChatBox(NULL)
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-10-26 (Catznip-3.2.0a) | Added: Catznip-3.2.0a
	mNearbyChatContainer(NULL),
	mNearbyChat(NULL),
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

//	mNearbyChat = getChildView("nearby_chat");
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
	mNearbyChatContainer = findChild<LLPanel>("panel_nearby_chat");
	mNearbyChat = findChild<LLNearbyChat>("nearby_chat");

	mChatBarImpl = (hasChild("panel_chat_bar_multi", TRUE)) ? findChild<LLNearbyChatBarBase>("panel_chat_bar_multi", TRUE) : findChild<LLNearbyChatBarBase>("panel_chat_bar_single", TRUE);

	LLUICtrl* show_btn = getChild<LLUICtrl>("show_nearby_chat");
	show_btn->setCommitCallback(boost::bind(&LLNearbyChatBar::onToggleNearbyChatPanel, this));

	// The collpased height differs between single-line and multi-line so dynamically calculate it from the default sizes
	COLLAPSED_HEIGHT = getRect().getHeight() - mNearbyChatContainer->getRect().getHeight();
// [/SL:KB]

	mExpandedHeight = COLLAPSED_HEIGHT + EXPANDED_HEIGHT;
	enableResizeCtrls(true, true, false);

// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-10-26 (Catznip-3.2.0a) | Modified: Catznip-3.2.0a
	// Initalize certain parameters depending on default vs embedded state
	bool fTabbedNearbyChat = isTabbedNearbyChat();
//	setCanClose(!fTabbedNearbyChat);
//	setCanMinimize(fTabbedNearbyChat);
	setCanTearOff(fTabbedNearbyChat);

	if (fTabbedNearbyChat)
	{
		LLIMFloaterContainer* pConvFloater = LLIMFloaterContainer::getInstance();
		if (pConvFloater)
		{
			if (!mNearbyChatContainer->getVisible())
				onToggleNearbyChatPanel();
			pConvFloater->addFloater(this, TRUE, LLTabContainer::START);

			LLEventPump& pumpNearbyChat = LLEventPumps::instance().obtain("LLChat");
			pumpNearbyChat.listen("nearby_chat_bar", boost::bind(&LLNearbyChatBar::onNewNearbyChatMsg, this, _1));
		}

		onTearOff(LLSD(isTornOff()));
		setTearOffCallback(boost::bind(&LLNearbyChatBar::onTearOff, this, _2));
	}
// [/SL:KB]

	gSavedSettings.declareBOOL("nearbychat_history_visibility", mNearbyChat->getVisible(), "Visibility state of nearby chat history", TRUE);
	mNearbyChat->setVisible(gSavedSettings.getBOOL("nearbychat_history_visibility"));

	return TRUE;
}

// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-11-12 (Catznip-3.2.0a) | Added: Catznip-3.2.0a
bool LLNearbyChatBar::onNewNearbyChatMsg(const LLSD& sdEvent)
{
	if ( (!isTornOff()) && (!isInVisibleChain()) )
	{
		LLIMFloaterContainer* pConvFloater = LLIMFloaterContainer::getInstance();
		if (pConvFloater)
		{
			if (pConvFloater->isFloaterFlashing(this))
				pConvFloater->setFloaterFlashing(this, FALSE);
			pConvFloater->setFloaterFlashing(this, TRUE);
		}
	}
	return false;
}

void LLNearbyChatBar::onTearOff(const LLSD& sdData)
{
	LLUICtrl* pTogglePanel = findChild<LLUICtrl>("panel_nearby_chat_toggle");
	if (sdData.asBoolean())		// Tearing off
	{
		pTogglePanel->setVisible(TRUE);
	}
	else						// Attaching
	{
		showHistory();
		pTogglePanel->setVisible(FALSE);
	}

	// Don't allow closing the nearby chat floater while it's attached
	setCanClose(sdData.asBoolean());
}
// [/SL:KB]

bool LLNearbyChatBar::applyRectControl()
{
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-10-26 (Catznip-3.2.0a) | Added: Catznip-3.2.0a
	if (isTabbedNearbyChat())
	{
		return true;
	}
// [/SL:KB]

	bool rect_controlled = LLFloater::applyRectControl();

//	LLView* nearby_chat = getChildView("nearby_chat");	
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-11-12 (Catznip-3.2.0a) | Added: Catznip-3.2.0a
	LLView* nearby_chat = mNearbyChatContainer;
// [/SL:KB]
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
	mNearbyChatContainer->setVisible(mExpandedHeight > COLLAPSED_HEIGHT);
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

//	if (!getChildView("nearby_chat")->getVisible())
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-11-12 (Catznip-3.2.0a) | Added: Catznip-3.2.0a
	if (!mNearbyChatContainer->getVisible())
// [/SL:KB]
	{
		onToggleNearbyChatPanel();
	}
}

void LLNearbyChatBar::enableTranslationCheckbox(BOOL enable)
{
	getChild<LLUICtrl>("translate_chat_checkbox")->setEnabled(enable);
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

	if( KEY_RETURN == key )
	{
		if (mask == MASK_CONTROL)
		{
			// shout
//		sendChat(CHAT_TYPE_SHOUT);
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-10-26 (Catznip-3.2.0a) | Modified: Catznip-3.2.0a
			mChatBarImpl->sendChat(CHAT_TYPE_SHOUT);
// [/SL:KB]
			handled = TRUE;
		}
		else if (mask == MASK_SHIFT)
		{
			// whisper
			mChatBarImpl->sendChat(CHAT_TYPE_WHISPER);
			handled = TRUE;
		}
		else if (mask == MASK_ALT)
		{
			// OOC
			mChatBarImpl->sendChat(CHAT_TYPE_OOC);
			handled = TRUE;
		}
		else if (mask == MASK_NONE)
		{
			// say
			mChatBarImpl->sendChat( CHAT_TYPE_NORMAL );
			handled = TRUE;
		}
	}

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

	// Get the currently selected channel from the channel spinner in the nearby chat bar, if present and used.
	// NOTE: Parts of the gAgent.startTyping() code are duplicated in 3 places:
	// - llnearbychatbar.cpp
	// - llchatbar.cpp
	// - llnearbychat.cpp
	// So be sure to look in all three places if changes are needed. This needs to be addressed at some point.
	// -Zi
	S32 channel=0;
	if (gSavedSettings.getBOOL("FSNearbyChatbar") &&
		gSavedSettings.getBOOL("FSShowChatChannel"))
	{
		channel = (S32)(LLNearbyChat::getInstance()->getChild<LLSpinCtrl>("ChatChannel")->get());
	}
	// -Zi

//	if( (length > 0) && (raw_text[0] != '/') )  // forward slash is used for escape (eg. emote) sequences
// [RLVa:KB] - Checked: 2010-03-26 (RLVa-1.2.0b) | Modified: RLVa-1.0.0d
	if ( (length > 0) && (raw_text[0] != '/') && (!gRlvHandler.hasBehaviour(RLV_BHVR_REDIRCHAT)) )
// [/RLVa:KB]
	{
		// only start typing animation if we are chatting without / on channel 0 -Zi
		if(channel==0)
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

void LLNearbyChatBarBase::updateFSPlayChatAnimation(const LLSD &data)
{
	FSPlayChatAnimation = data.asBoolean();
}

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
			if(type == CHAT_TYPE_OOC)
			{
				std::string tempText = wstring_to_utf8str( text );
				tempText = gSavedSettings.getString("FSOOCPrefix") + " " + tempText + " " + gSavedSettings.getString("FSOOCPostfix");
				text = utf8str_to_wstring(tempText);
				setChatBoxText(LLStringExplicit(tempText));
			}

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
			// If "/<number>" is not specified, see if a channel has been set in
			//  the spinner.
			if (gSavedSettings.getBOOL("FSNearbyChatbar") &&
				gSavedSettings.getBOOL("FSShowChatChannel") &&
				(channel == 0))
			{
				channel = (S32)(LLNearbyChat::getInstance()->getChild<LLSpinCtrl>("ChatChannel")->get());
			}
			
			std::string utf8text = wstring_to_utf8str(text);
			// Try to trigger a gesture, if not chat to a script.
			std::string utf8_revised_text;
			if (0 == channel)
			{
//-TT Satomi Ahn - Patch MU_OOC	
				if (gSavedSettings.getBOOL("AutoCloseOOC"))
				{
					// Try to find any unclosed OOC chat (i.e. an opening
					// double parenthesis without a matching closing double
					// parenthesis.
					if (utf8text.find("(( ") != -1 && utf8text.find("))") == -1)
					{
						// add the missing closing double parenthesis.
						utf8text += " ))";
					}
					else if (utf8text.find("((") != -1 && utf8text.find("))") == -1)
					{
						if (utf8text.at(utf8text.length() - 1) == ')')
						{
							// cosmetic: add a space first to avoid a closing triple parenthesis
							utf8text += " ";
						}
						// add the missing closing double parenthesis.
						utf8text += "))";
					}
					else if (utf8text.find("[[ ") != -1 && utf8text.find("]]") == -1)
					{
						// add the missing closing double parenthesis.
						utf8text += " ]]";
					}
					else if (utf8text.find("[[") != -1 && utf8text.find("]]") == -1)
					{
						if (utf8text.at(utf8text.length() - 1) == ']')
						{
							// cosmetic: add a space first to avoid a closing triple parenthesis
							utf8text += " ";
						}
						// add the missing closing double parenthesis.
						utf8text += "]]";
					}
				}

				// Convert MU*s style poses into IRC emotes here.
				if (gSavedSettings.getBOOL("AllowMUpose") && utf8text.find(":") == 0 && utf8text.length() > 3)
				{
					if (utf8text.find(":'") == 0)
					{
						utf8text.replace(0, 1, "/me");
	 				}
					else if (isalpha(utf8text.at(1)))	// Do not prevent smileys and such.
					{
						utf8text.replace(0, 1, "/me ");
					}
				}				
//-TT Satomi Ahn - Patch MU_OOC	
				// discard returned "found" boolean
				LLGestureMgr::instance().triggerAndReviseString(utf8text, &utf8_revised_text);
			}
			else
			{
				utf8_revised_text = utf8text;
			}

			utf8_revised_text = utf8str_trim(utf8_revised_text);

			EChatType nType;
			if(type == CHAT_TYPE_OOC)
				nType = CHAT_TYPE_NORMAL;
			else
				nType = type;

			type = processChatTypeTriggers(nType, utf8_revised_text);

			if (!utf8_revised_text.empty() && cmd_line_chat(utf8_revised_text, type))
			{
				// Chat with animation
				sendChatFromViewer(utf8_revised_text, type, FSPlayChatAnimation);
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
	if (gSavedSettings.getBOOL("CloseChatOnReturn") || gSavedSettings.getBOOL("AutohideChatBar"))
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
	LLView* nearby_chat = mNearbyChatContainer;
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

	gSavedSettings.setBOOL("nearbychat_history_visibility", mNearbyChat->getVisible());
}

//void LLNearbyChatBar::setMinimized(BOOL b)
//{
//	LLNearbyChat* nearby_chat = getChild<LLNearbyChat>("nearby_chat");
//	// when unminimizing with nearby chat visible, go ahead and kill off screen chats
//	if (!b && nearby_chat->getVisible())
//	{
//		nearby_chat->removeScreenChat();
//	}
//		LLFloater::setMinimized(b);
//}
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-11-17 (Catznip-3.2.0a) | Added: Catznip-3.2.0a
BOOL LLNearbyChatBar::canClose()
{
	if (getHost())
		return false;
	return LLFloater::canClose();
}

void LLNearbyChatBar::onOpen(const LLSD& sdKey)
{
	// When open the floater with nearby chat visible, go ahead and kill off screen chats
	if (mNearbyChatContainer->getVisible())
	{
		mNearbyChat->removeScreenChat();
	}
	enableTranslationCheckbox(LLTranslate::isTranslationConfigured()); // <FS:ND> Translation stuffs
}
// [/SL:KB]

//void LLNearbyChatBar::onChatBoxCommit()
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-10-26 (Catznip-3.2.0a) | Added: Catznip-3.2.0a
void LLNearbyChatBarSingle::onChatBoxCommit()
// [/SL:KB]
{
	if (mChatBox->getText().length() > 0)
	{
		sendChat(CHAT_TYPE_NORMAL);
	}
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-12-02 (Catznip-3.2.0d) | Added: Catznip-3.2.0d
	else if (gSavedSettings.getBOOL("CloseChatOnEmptyReturn"))
	{
		// Close if we're the child of a floater
		LLFloater* pFloater = getParentByType<LLFloater>();
		if (pFloater)
			pFloater->closeFloater();
	}
// [/SL:KB]

	gAgent.stopTyping();
}

void LLNearbyChatBar::setText(const LLStringExplicit &new_text)
{
	mChatBarImpl->setChatBoxText(new_text);
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
	// If "/<number>" is not specified, see if a channel has been set in
	//  the spinner.
	if (gSavedSettings.getBOOL("FSNearbyChatbar") &&
		gSavedSettings.getBOOL("FSShowChatChannel") &&
		(channel == 0))
	{
		channel = (S32)(LLNearbyChat::getInstance()->getChild<LLSpinCtrl>("ChatChannel")->get());
	}
	std::string utf8_out_text = wstring_to_utf8str(out_text);
	std::string utf8_text = wstring_to_utf8str(wtext);

	utf8_text = utf8str_trim(utf8_text);
	if (!utf8_text.empty())
	{
		utf8_text = utf8str_truncate(utf8_text, MAX_STRING - 1);
	}

// [RLVa:KB] - Checked: 2010-03-27 (RLVa-1.2.0b) | Modified: RLVa-1.2.0b
	if ( (0 == channel) && (rlv_handler_t::isEnabled()) )
	{
		// Adjust the (public) chat "volume" on chat and gestures (also takes care of playing the proper animation)
		if ( ((CHAT_TYPE_SHOUT == type) || (CHAT_TYPE_NORMAL == type)) && (gRlvHandler.hasBehaviour(RLV_BHVR_CHATNORMAL)) )
			type = CHAT_TYPE_WHISPER;
		else if ( (CHAT_TYPE_SHOUT == type) && (gRlvHandler.hasBehaviour(RLV_BHVR_CHATSHOUT)) )
			type = CHAT_TYPE_NORMAL;
		else if ( (CHAT_TYPE_WHISPER == type) && (gRlvHandler.hasBehaviour(RLV_BHVR_CHATWHISPER)) )
			type = CHAT_TYPE_NORMAL;

		animate &= !gRlvHandler.hasBehaviour( (!RlvUtil::isEmote(utf8_text)) ? RLV_BHVR_REDIRCHAT : RLV_BHVR_REDIREMOTE );
	}
// [/RLVa:KB]

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

	pSelf->openFloater(LLSD());
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

	//	cb->setChatBarVisible(FALSE); ND_MERGE
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
			 && ( LLStringOps::isDigit(mesg[1])
				 || (mesg[1] == '-'
				 	&& LLStringOps::isDigit(mesg[2]))))
	{
		// This a special "/20" speak on a channel
		S32 pos = 0;
		if(mesg[1] == '-')
			pos++;
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
		if(mesg[1] == '-')
			sLastSpecialChatChannel = -sLastSpecialChatChannel;
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

//void send_chat_from_viewer(const std::string& utf8_out_text, EChatType type, S32 channel)
// [RLVa:KB] - Checked: 2010-02-27 (RLVa-1.2.0b) | Modified: RLVa-0.2.2a
void send_chat_from_viewer(std::string utf8_out_text, EChatType type, S32 channel)
// [/RLVa:KB]
{
// [RLVa:KB] - Checked: 2010-02-27 (RLVa-1.2.0b) | Modified: RLVa-1.2.0a
	// Only process chat messages (ie not CHAT_TYPE_START, CHAT_TYPE_STOP, etc)
	if ( (rlv_handler_t::isEnabled()) && ( (CHAT_TYPE_WHISPER == type) || (CHAT_TYPE_NORMAL == type) || (CHAT_TYPE_SHOUT == type) ) )
	{
		if (0 == channel)
		{
			// (We already did this before, but LLChatHandler::handle() calls this directly)
			if ( ((CHAT_TYPE_SHOUT == type) || (CHAT_TYPE_NORMAL == type)) && (gRlvHandler.hasBehaviour(RLV_BHVR_CHATNORMAL)) )
				type = CHAT_TYPE_WHISPER;
			else if ( (CHAT_TYPE_SHOUT == type) && (gRlvHandler.hasBehaviour(RLV_BHVR_CHATSHOUT)) )
				type = CHAT_TYPE_NORMAL;
			else if ( (CHAT_TYPE_WHISPER == type) && (gRlvHandler.hasBehaviour(RLV_BHVR_CHATWHISPER)) )
				type = CHAT_TYPE_NORMAL;

			// Redirect chat if needed
			if ( ( (gRlvHandler.hasBehaviour(RLV_BHVR_REDIRCHAT) || (gRlvHandler.hasBehaviour(RLV_BHVR_REDIREMOTE)) ) && 
				 (gRlvHandler.redirectChatOrEmote(utf8_out_text)) ) )
			{
				return;
			}

			// Filter public chat if sendchat restricted
			if (gRlvHandler.hasBehaviour(RLV_BHVR_SENDCHAT))
				gRlvHandler.filterChat(utf8_out_text, true);
		}
		else
		{
			// Don't allow chat on a non-public channel if sendchannel restricted (unless the channel is an exception)
			if ( (gRlvHandler.hasBehaviour(RLV_BHVR_SENDCHANNEL)) && (!gRlvHandler.isException(RLV_BHVR_SENDCHANNEL, channel)) )
				return;

			// Don't allow chat on debug channel if @sendchat, @redirchat or @rediremote restricted (shows as public chat on viewers)
			if (CHAT_CHANNEL_DEBUG == channel)
			{
				bool fIsEmote = RlvUtil::isEmote(utf8_out_text);
				if ( (gRlvHandler.hasBehaviour(RLV_BHVR_SENDCHAT)) || 
					 ((!fIsEmote) && (gRlvHandler.hasBehaviour(RLV_BHVR_REDIRCHAT))) || 
					 ((fIsEmote) && (gRlvHandler.hasBehaviour(RLV_BHVR_REDIREMOTE))) )
				{
					return;
				}
			}
		}
	}
// [/RLVa:KB]

	LLMessageSystem* msg = gMessageSystem;
	if(channel >= 0)
	{
		msg->newMessageFast(_PREHASH_ChatFromViewer);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_ChatData);
		msg->addStringFast(_PREHASH_Message, utf8_out_text);
		msg->addU8Fast(_PREHASH_Type, type);
		msg->addS32("Channel", channel);
	}
	else
	{
		msg->newMessage("ScriptDialogReply");
		msg->nextBlock("AgentData");
		msg->addUUID("AgentID", gAgent.getID());
		msg->addUUID("SessionID", gAgent.getSessionID());
		msg->nextBlock("Data");
		msg->addUUID("ObjectID", gAgent.getID());
		msg->addS32("ChatChannel", channel);
		msg->addS32("ButtonIndex", 0);
		msg->addString("ButtonLabel", utf8_out_text);
	}

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

std::string LLNearbyChatBar::getCurrentChat()
{
	LLLineEditor* pLE(0);
	if( mChatBarImpl )
		pLE = dynamic_cast< LLLineEditor* >( mChatBarImpl->getChatBoxCtrl() );

	if( !pLE )
		return LLStringUtil::null;

	return pLE->getText();
}
