/** 
 * @file fsnearbychathub.cpp (was llnearbychat.cpp)
 * @brief @brief Nearby chat central class for handling multiple chat input controls
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

#include "fsnearbychathub.h"

#include "chatbar_as_cmdline.h"
#include "fscommon.h"
#include "fsfloaternearbychat.h"
#include "fsnearbychatcontrol.h"
#include "llagent.h" 			// gAgent
#include "llanimationstates.h"	// ANIM_AGENT_WHISPER, ANIM_AGENT_TALK, ANIM_AGENT_SHOUT
#include "llchatentry.h"
#include "llcommandhandler.h"
#include "llgesturemgr.h"
#include "lllineeditor.h"
#include "llspinctrl.h"
#include "llviewercontrol.h"
#include "llviewerkeyboard.h"
#include "llviewerstats.h"
#include "llworld.h"
#include "rlvactions.h"
#include "rlvhandler.h"

static const U32 NAME_PREDICTION_MINIMUM_LENGTH = 3;

// *HACK* chat bar cannot return its correct height for some reason
static const S32 MAGIC_CHAT_BAR_PAD = 5;

struct LLChatTypeTrigger {
	std::string name;
	EChatType type;
};

static LLChatTypeTrigger sChatTypeTriggers[] = {
	{ "/whisper"	, CHAT_TYPE_WHISPER},
	{ "/shout"		, CHAT_TYPE_SHOUT}
};


// This function just sends the message, with no other processing. Moved out
// of send_chat_from_viewer.
void really_send_chat_from_viewer(const std::string& utf8_out_text, EChatType type, S32 channel)
{
	LLMessageSystem* msg = gMessageSystem;

	if (!msg)
	{
		return;
	}

	if (channel >= 0)
	{
		msg->newMessageFast(_PREHASH_ChatFromViewer);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->nextBlockFast(_PREHASH_ChatData);
		msg->addStringFast(_PREHASH_Message, utf8_out_text);
		msg->addU8Fast(_PREHASH_Type, type);
		msg->addS32("Channel", channel);
	}
	else
	{
		msg->newMessage("ScriptDialogReply");
		msg->nextBlock("AgentData");
		msg->addUUID("AgentID", gAgentID);
		msg->addUUID("SessionID", gAgentSessionID);
		msg->nextBlock("Data");
		msg->addUUID("ObjectID", gAgentID);
		msg->addS32("ChatChannel", channel);
		msg->addS32("ButtonIndex", 0);
		msg->addString("ButtonLabel", utf8_out_text);
	}

	gAgent.sendReliableMessage();
}

void send_chat_from_viewer(std::string utf8_out_text, EChatType type, S32 channel)
{
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
			if (!RlvActions::canSendChannel(channel))
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

	size_t split = MAX_MSG_BUF_SIZE - 1;
	size_t pos = 0;
	size_t total = utf8_out_text.length();

	// Don't break null messages
	if (total == 0)
	{
		really_send_chat_from_viewer(utf8_out_text, type, channel);
	}

	while (pos < total)
	{
		size_t next_split = split;

		if (pos + next_split > total)
		{
			// just send the rest of the message
			next_split = total - pos;
		}
		else
		{
			// first, try to split at a space
			while((U8(utf8_out_text[pos + next_split]) != ' ')
				&& (next_split > 0))
			{
				--next_split;
			}
			
			if (next_split == 0)
			{
				next_split = split;
				// no space found, split somewhere not in the middle of UTF-8
				while((U8(utf8_out_text[pos + next_split]) >= 0x80)
					&& (U8(utf8_out_text[pos + next_split]) < 0xC0)
					&& (next_split > 0))
				{
					--next_split;
				}
			}

			if (next_split == 0)
			{
				next_split = split;
				LL_WARNS("Splitting") << "utf-8 couldn't be split correctly" << LL_ENDL;
			}
		}

		std::string send = utf8_out_text.substr(pos, next_split);
		pos += next_split;

		// *FIXME: Queue messages and wait for server
		really_send_chat_from_viewer(send, type, channel);
	}

	// moved here so we don't bump the count for every message segment
	add(LLStatViewer::CHAT_COUNT, 1);
}

bool matchChatTypeTrigger(const std::string& in_str, std::string* out_str)
{
	size_t in_len = in_str.length();
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
			return true;
		}
	}
	
	return false;
}


//////////////////////////////////////////////////////////////////////////////
// FSNearbyChat
//////////////////////////////////////////////////////////////////////////////

S32 FSNearbyChat::sLastSpecialChatChannel = 0;

FSNearbyChat::FSNearbyChat() :
	mDefaultChatBar(NULL),
	mFocusedInputEditor(NULL)
{
	gSavedSettings.getControl("MainChatbarVisible")->getSignal()->connect(boost::bind(&FSNearbyChat::onDefaultChatBarButtonClicked, this));
}

FSNearbyChat::~FSNearbyChat()
{
}

void FSNearbyChat::sendChat(LLWString text, EChatType type)
{
	LLWStringUtil::trim(text);

	if (!text.empty())
	{
		if (type == CHAT_TYPE_OOC)
		{
			text = utf8string_to_wstring(gSavedSettings.getString("FSOOCPrefix") + " ") + text + utf8string_to_wstring(" " + gSavedSettings.getString("FSOOCPostfix"));
		}

		// Check if this is destined for another channel
		S32 channel = 0;
		bool is_set = false;
		stripChannelNumber(text, &channel, &sLastSpecialChatChannel, &is_set);
		
		std::string utf8text = wstring_to_utf8str(text);
		// Try to trigger a gesture, if not chat to a script.
		std::string utf8_revised_text;
		if (0 == channel)
		{
			// Convert OOC and MU* style poses
			utf8text = FSCommon::applyAutoCloseOoc(utf8text);
			utf8text = FSCommon::applyMuPose(utf8text);

			// discard returned "found" boolean
			if (!LLGestureMgr::instance().triggerAndReviseString(utf8text, &utf8_revised_text))
			{
				utf8_revised_text = utf8text;
			}
		}
		else
		{
			utf8_revised_text = utf8text;
		}

		utf8_revised_text = utf8str_trim(utf8_revised_text);

		EChatType nType = (type == CHAT_TYPE_OOC ? CHAT_TYPE_NORMAL : type);
		type = processChatTypeTriggers(nType, utf8_revised_text);

		if (!utf8_revised_text.empty() && cmd_line_chat(utf8_revised_text, type))
		{
			// Chat with animation
			sendChatFromViewer(utf8_revised_text, type, gSavedSettings.getBOOL("PlayChatAnim"));
		}
	}

	gAgent.stopTyping();
}

void FSNearbyChat::sendChatFromViewer(const std::string& utf8text, EChatType type, BOOL animate)
{
	sendChatFromViewer(utf8str_to_wstring(utf8text), type, animate);
}

void FSNearbyChat::sendChatFromViewer(const LLWString& wtext, EChatType type, BOOL animate)
{
	// Look for "/20 foo" channel chats.
	S32 channel = 0;
	bool is_set = false;
	LLWString out_text = stripChannelNumber(wtext, &channel, &sLastSpecialChatChannel, &is_set);
	sendChatFromViewer(wtext, out_text, type, animate, channel);
}

// all chat bars call this function and we keep the first or one that's seen as the default
void FSNearbyChat::registerChatBar(FSNearbyChatControl* chatBar)
{
	if (!mDefaultChatBar || chatBar->isDefault())
	{
		mDefaultChatBar = chatBar;
	}
}

// unhide the default nearby chat bar on request (pressing Enter or a letter key)
void FSNearbyChat::showDefaultChatBar(BOOL visible, const char* text) const
{
	if (!mDefaultChatBar)
	{
		return;
	}

	// change settings control to signal button state
	gSavedSettings.setBOOL("MainChatbarVisible", visible);

	mDefaultChatBar->getParent()->setVisible(visible);
	mDefaultChatBar->setVisible(visible);
	mDefaultChatBar->setFocus(visible);

	// <FS:KC> Fix for bad edge snapping
	if (visible)
	{
		gFloaterView->setSnapOffsetChatBar(mDefaultChatBar->getRect().getHeight() + MAGIC_CHAT_BAR_PAD);
	}
	else
	{
		gFloaterView->setSnapOffsetChatBar(0);
	}

	if (!text)
	{
		return;
	}

	if (mDefaultChatBar->getText().empty())
	{
		mDefaultChatBar->setText(LLStringExplicit(text));
		mDefaultChatBar->setCursorToEnd();
	}
	// </FS:KC> Fix for bad edge snapping
}

// We want to know which nearby chat editor (if any) currently has focus
void FSNearbyChat::setFocusedInputEditor(FSNearbyChatControl* inputEditor, BOOL focus)
{
	if (focus)
	{
		mFocusedInputEditor = inputEditor;
	}
	else if (mFocusedInputEditor == inputEditor)
	{
		// only remove focus if the request came from the previously active input editor
		// to avoid races
		mFocusedInputEditor = NULL;
	}
}

// for the "arrow key moves avatar when chat is empty" hack in llviewerwindow.cpp
// and the hide chat bar feature in mouselook in llagent.cpp
BOOL FSNearbyChat::defaultChatBarIsIdle() const
{
	if (mFocusedInputEditor && mFocusedInputEditor->isDefault())
	{
		return mFocusedInputEditor->getText().empty();
	}

	// if any other chat bar has focus, report "idle", because they're not the default
	return TRUE;
}

// for the "arrow key moves avatar when chat is empty" hack in llviewerwindow.cpp
BOOL FSNearbyChat::defaultChatBarHasFocus() const
{
	if (mFocusedInputEditor && mFocusedInputEditor->isDefault())
	{
		return TRUE;
	}

	return FALSE;
}

void FSNearbyChat::onDefaultChatBarButtonClicked()
{
	showDefaultChatBar(gSavedSettings.getBOOL("MainChatbarVisible"));
}


//////////////////////////////////////////////////////////////////////////////
// General chat handling methods

void FSNearbyChat::sendChatFromViewer(const LLWString& wtext, const LLWString& out_text, EChatType type, BOOL animate, S32 channel)
{
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
			LL_DEBUGS("FSNearbyChatHub") << "You whisper " << utf8_text << LL_ENDL;
			gAgent.sendAnimationRequest(ANIM_AGENT_WHISPER, ANIM_REQUEST_START);
		}
		else if (type == CHAT_TYPE_NORMAL)
		{
			LL_DEBUGS("FSNearbyChatHub") << "You say " << utf8_text << LL_ENDL;
			gAgent.sendAnimationRequest(ANIM_AGENT_TALK, ANIM_REQUEST_START);
		}
		else if (type == CHAT_TYPE_SHOUT)
		{
			LL_DEBUGS("FSNearbyChatHub") << "You shout " << utf8_text << LL_ENDL;
			gAgent.sendAnimationRequest(ANIM_AGENT_SHOUT, ANIM_REQUEST_START);
		}
		else
		{
			LL_INFOS("FSNearbyChatHub") << "send_chat_from_viewer() - invalid volume" << LL_ENDL;
			return;
		}
	}
	else
	{
		if (type != CHAT_TYPE_START && type != CHAT_TYPE_STOP)
		{
			LL_DEBUGS("FSNearbyChatHub") << "Channel chat: " << utf8_text << LL_ENDL;
		}
	}

	send_chat_from_viewer(utf8_out_text, type, channel);
}

// static
EChatType FSNearbyChat::processChatTypeTriggers(EChatType type, std::string &str)
{
	size_t length = str.length();
	S32 cnt = sizeof(sChatTypeTriggers) / sizeof(*sChatTypeTriggers);
	
	for (S32 n = 0; n < cnt; n++)
	{
		if (length >= sChatTypeTriggers[n].name.length())
		{
			std::string trigger = str.substr(0, sChatTypeTriggers[n].name.length());

			if (!LLStringUtil::compareInsensitive(trigger, sChatTypeTriggers[n].name))
			{
				size_t trigger_length = sChatTypeTriggers[n].name.length();

				// It's to remove space after trigger name
				if (length > trigger_length && str[trigger_length] == ' ')
				{
					trigger_length++;
				}

				str = str.substr(trigger_length, length);

				if (CHAT_TYPE_NORMAL == type)
				{
					return sChatTypeTriggers[n].type;
				}
				else
				{
					break;
				}
			}
		}
	}

	return type;
}

// If input of the form "/20foo" or "/20 foo", returns "foo" and channel 20.
// Otherwise returns input and channel 0.
LLWString FSNearbyChat::stripChannelNumber(const LLWString &mesg, S32* channel, S32* last_channel, bool* is_set)
{
	*is_set = false;

	if (mesg[0] == '/'
		&& mesg[1] == '/')
	{
		// This is a "repeat channel send"
		*is_set = true;
		*channel = *last_channel;
		return mesg.substr(2, mesg.length() - 2);
	}
	else if (mesg[0] == '/'
			 && mesg[1]
	//<FS:TS> FIRE-11412: Allow saying /-channel for negative numbers
	//        (this code was here; documenting for the future)
			 //&& LLStringOps::isDigit(mesg[1]))
			 && (LLStringOps::isDigit(mesg[1])
				 || (mesg[1] == '-'
				 	&& mesg[2]
				 	&& LLStringOps::isDigit(mesg[2]))))
	//</FS:TS> FIRE-11412
	{
		// This a special "/20" speak on a channel
		*is_set = true;
		S32 pos = 0;
		//<FS:TS> FIRE-11412: Allow saying /-channel for negative numbers
		//        (this code was here; documenting for the future)
		if (mesg[1] == '-')
		{
			pos++;
		}
		//</FS:TS> FIRE-11412
		
		// Copy the channel number into a string
		LLWString channel_string;
		llwchar c;
		do
		{
			c = mesg[pos + 1];
			channel_string.push_back(c);
			pos++;
		}
		while (c && pos < 64 && LLStringOps::isDigit(c));
		
		// Move the pointer forward to the first non-whitespace char
		// Check isspace before looping, so we can handle "/33foo"
		// as well as "/33 foo"
		while (c && iswspace(c))
		{
			c = mesg[pos+1];
			pos++;
		}
		
		*last_channel = strtol(wstring_to_utf8str(channel_string).c_str(), NULL, 10);
		//<FS:TS> FIRE-11412: Allow saying /-channel for negative numbers
		//        (this code was here; documenting for the future)
		if (mesg[1] == '-')
		{
			*last_channel = -(*last_channel);
		}
		//</FS:TS> FIRE-11412
		*channel = *last_channel;
		return mesg.substr(pos, mesg.length() - pos);
	}
	else
	{
		// This is normal chat.
		*channel = 0;
		return mesg;
	}
}

//static
void FSNearbyChat::handleChatBarKeystroke(LLUICtrl* source, S32 channel /* = 0 */)
{
	LLChatEntry* chat_entry = dynamic_cast<LLChatEntry*>(source);
	LLLineEditor* line_editor = dynamic_cast<LLLineEditor*>(source);

	if (!chat_entry && !line_editor)
	{
		return;
	}

	LLWString raw_text;
	if (chat_entry)
	{
		raw_text = chat_entry->getWText();
	}
	else
	{
		raw_text = line_editor->getWText();
	}

	// Can't trim the end, because that will cause autocompletion
	// to eat trailing spaces that might be part of a gesture.
	LLWStringUtil::trimHead(raw_text);
	size_t length = raw_text.length();

	LLWString prefix;
	if (length > 3)
	{
		prefix = raw_text.substr(0, 3);
		LLWStringUtil::toLower(prefix);
	}

	static LLCachedControl<bool> type_during_emote(gSavedSettings, "FSTypeDuringEmote");
	static LLCachedControl<bool> allow_mu_pose(gSavedSettings, "AllowMUpose");
	if (length > 0 &&
		((raw_text[0] != '/' || (type_during_emote && length > 3 && prefix == utf8string_to_wstring("/me") && (raw_text[3] == ' ' || raw_text[3] == '\'')))
		&& (raw_text[0] != ':' || !allow_mu_pose || type_during_emote)) &&
		!gRlvHandler.hasBehaviour(RLV_BHVR_REDIRCHAT))
	{
		// only start typing animation if we are chatting without / on channel 0 -Zi
		if (channel == 0)
		{
			gAgent.startTyping();
		}
	}
	else
	{
		gAgent.stopTyping();
	}

	KEY key = gKeyboard->currentKey();
	MASK mask = gKeyboard->currentMask(FALSE);

	// Ignore "special" keys, like backspace, arrows, etc.
	if (length > 1
		&& raw_text[0] == '/'
		&& key < KEY_SPECIAL
		&& gSavedSettings.getBOOL("FSChatbarGestureAutoCompleteEnable"))
	{
		// we're starting a gesture, attempt to autocomplete

		std::string utf8_trigger = wstring_to_utf8str(raw_text);
		std::string utf8_out_str(utf8_trigger);

		if (LLGestureMgr::instance().matchPrefix(utf8_trigger, &utf8_out_str))
		{
			std::string rest_of_match = utf8_out_str.substr(utf8_trigger.size());
			if (!rest_of_match.empty())
			{
				if (chat_entry)
				{
					chat_entry->setText(utf8_trigger + rest_of_match); // keep original capitalization for user-entered part

					// Select to end of line, starting from the character
					// after the last one the user typed.
					chat_entry->selectByCursorPosition(utf8_out_str.size() - rest_of_match.size(), utf8_out_str.size());
				}
				else
				{
					line_editor->setText(utf8_trigger + rest_of_match); // keep original capitalization for user-entered part

					// Select to end of line, starting from the character
					// after the last one the user typed.
					S32 outlength = line_editor->getLength(); // in characters
					line_editor->setSelection(length, outlength);
					line_editor->setCursor(outlength);
				}
			}
		}
		else if (matchChatTypeTrigger(utf8_trigger, &utf8_out_str))
		{
			std::string rest_of_match = utf8_out_str.substr(utf8_trigger.size());
			if (chat_entry)
			{
				chat_entry->setText(utf8_trigger + rest_of_match + " "); // keep original capitalization for user-entered part
				chat_entry->endOfDoc();
			}
			else
			{
				line_editor->setText(utf8_trigger + rest_of_match + " "); // keep original capitalization for user-entered part
				line_editor->setCursorToEnd();
			}
		}
	}

	// <FS:CR> FIRE-3192 - Predictive name completion, based on code by Satomi Ahn
	static LLCachedControl<bool> sNameAutocomplete(gSavedSettings, "FSChatbarNamePrediction");
	if (length > NAME_PREDICTION_MINIMUM_LENGTH && sNameAutocomplete && key < KEY_SPECIAL && mask != MASK_CONTROL)
	{
		S32 cur_pos;
		if (chat_entry)
		{
			cur_pos = chat_entry->getCursorPos();
		}
		else
		{
			cur_pos = line_editor->getCursor();
		}
		if (cur_pos && (raw_text[cur_pos - 1] != ' '))
		{
			// Get a list of avatars within range
			uuid_vec_t avatar_ids;
			LLWorld::getInstance()->getAvatars(&avatar_ids, NULL, gAgent.getPositionGlobal(), gSavedSettings.getF32("NearMeRange"));

			if (avatar_ids.empty())
			{
				return; // Nobody's in range!
			}

			// Parse text for a pattern to search
			std::string prefix = wstring_to_utf8str(raw_text.substr(0, cur_pos)); // Text before search string
			std::string suffix = "";
			if (cur_pos <= raw_text.length()) // Is there anything after the cursor?
			{
				suffix = wstring_to_utf8str(raw_text.substr(cur_pos)); // Text after search string
			}
			size_t last_space = prefix.rfind(" ");
			std::string pattern = prefix.substr(last_space + 1, prefix.length() - last_space - 1); // Search pattern

			prefix = prefix.substr(0, last_space + 1);
			std::string match_pattern = "";

			if (pattern.size() < NAME_PREDICTION_MINIMUM_LENGTH)
			{
				return;
			}

			match_pattern = prefix.substr(last_space + 1, prefix.length() - last_space - 1);
			prefix = prefix.substr(0, last_space + 1);
			std::string match = pattern;
			LLStringUtil::toLower(pattern);

			std::string name;
			bool found = false;
			bool full_name = false;
			uuid_vec_t::iterator iter = avatar_ids.begin();

			if (last_space != std::string::npos && !prefix.empty())
			{
				last_space = prefix.substr(0, prefix.length() - 2).rfind(" ");
				match_pattern = prefix.substr(last_space + 1, prefix.length() - last_space - 1);
				prefix = prefix.substr(0, last_space + 1);

				// prepare search pattern
				std::string full_pattern(match_pattern + pattern);
				LLStringUtil::toLower(full_pattern);

				// Look for a match
				while (iter != avatar_ids.end() && !found)
				{
					if (gCacheName->getFullName(*iter++, name))
					{
						if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
						{
							name = RlvStrings::getAnonym(name);
						}
						LLStringUtil::toLower(name);
						found = (name.find(full_pattern) == 0);
					}
				}
			}

			if (found)
			{
				full_name = true; // ignore OnlyFirstName in case we want to disambiguate
				prefix += match_pattern;
			}
			else if (!pattern.empty()) // if first search did not work, try matching with last word before cursor only
			{
				prefix += match_pattern; // first part of the pattern wasn't a pattern, so keep it in prefix
				LLStringUtil::toLower(pattern);
				iter = avatar_ids.begin();

				// Look for a match
				while (iter != avatar_ids.end() && !found)
				{
					if (gCacheName->getFullName(*iter++, name))
					{
						if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
						{
							name = RlvStrings::getAnonym(name);
						}
						LLStringUtil::toLower(name);
						found = (name.find(pattern) == 0);
					}
				}
			}

			// if we found something by either method, replace the pattern by the avatar name
			if (found)
			{
				std::string first_name, last_name;
				gCacheName->getFirstLastName(*(iter - 1), first_name, last_name);
				std::string rest_of_match;
				std::string replaced_text;
				if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
				{
					replaced_text += RlvStrings::getAnonym(first_name + " " + last_name) + " ";
				}
				else
				{
					if (full_name)
					{
						rest_of_match = /*first_name + " " +*/ last_name.substr(pattern.size());
					}
					else
					{
						rest_of_match = first_name.substr(pattern.size());
					}
					replaced_text += match + rest_of_match + " ";
				}
				if (!rest_of_match.empty())
				{
					if (chat_entry)
					{
						chat_entry->setText(prefix + replaced_text + suffix);
						chat_entry->selectByCursorPosition(utf8string_to_wstring(prefix).size() + utf8string_to_wstring(match).size(), utf8string_to_wstring(prefix).size() + utf8string_to_wstring(replaced_text).size());
					}
					else
					{
						line_editor->setText(prefix + replaced_text + suffix);
						line_editor->setSelection(utf8str_to_wstring(prefix + replaced_text).length(), cur_pos);
					}
				}
			}
		}
	}
	// </FS:CR>
}


class LLChatCommandHandler : public LLCommandHandler
{
public:
	// not allowed from outside the app
	LLChatCommandHandler() : LLCommandHandler("chat", UNTRUSTED_THROTTLE) { }

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
