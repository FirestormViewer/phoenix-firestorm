/** 
 * @file llnearbychat.cpp
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

#include "llnearbychathub.h"
#include "llnearbychatcontrol.h"
#include "llfloaternearbychat.h"

#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "llrootview.h"
#include "llspinctrl.h"
#include "llfocusmgr.h"
#include "lllogchat.h"
#include "llmenugl.h"
#include "llviewermenu.h"//for gMenuHolder

#include "llnearbychathandler.h"
#include "llchannelmanager.h"

#include "llagent.h" 			// gAgent

//AO - includes for textentry
#include "rlvhandler.h"
#include "llcommandhandler.h"
#include "llkeyboard.h"
#include "llgesturemgr.h"
#include "llmultigesture.h"

#include "llconsole.h"

// <FS:Zi> Moved nearby chat functionality here
#include "chatbar_as_cmdline.h"
#include "llanimationstates.h"	// ANIM_AGENT_WHISPER, ANIM_AGENT_TALK, ANIM_AGENT_SHOUT
#include "llviewerstats.h"
// </FS:Zi>

//<FS:TS> FIRE-787: break up too long chat lines into multiple messages
void send_chat_from_viewer(std::string utf8_out_text, EChatType type, S32 channel);
void really_send_chat_from_viewer(std::string utf8_out_text, EChatType type, S32 channel);
//</FS:TS> FIRE-787

struct LLChatTypeTrigger {
	std::string name;
	EChatType type;
};

static LLChatTypeTrigger sChatTypeTriggers[] = {
	{ "/whisper"	, CHAT_TYPE_WHISPER},
	{ "/shout"	, CHAT_TYPE_SHOUT}
};

S32 LLNearbyChat::sLastSpecialChatChannel = 0;

LLNearbyChat::LLNearbyChat() :
	mDefaultChatBar(NULL),
	mFocusedInputEditor(NULL)
{
	gSavedSettings.getControl("MainChatbarVisible")->getSignal()->connect(boost::bind(&LLNearbyChat::onDefaultChatBarButtonClicked, this));
}

LLNearbyChat::~LLNearbyChat()
{
}

void LLNearbyChat::onDefaultChatBarButtonClicked()
{
	showDefaultChatBar(gSavedSettings.getBOOL("MainChatbarVisible"));
}

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

//<FS:TS> FIRE-787: break up too long chat lines into multiple messages
	U32 split = MAX_MSG_BUF_SIZE - 1;
	U32 pos = 0;
	U32 total = utf8_out_text.length();

	// Don't break null messages
	if(total == 0)
	{
		really_send_chat_from_viewer(utf8_out_text, type, channel);
	}

	while(pos < total)
	{
		U32 next_split = split;

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

			if(next_split == 0)
			{
				next_split = split;
				LL_WARNS("Splitting") <<
					"utf-8 couldn't be split correctly" << LL_ENDL;
			}
		}

		std::string send = utf8_out_text.substr(pos, next_split);
		pos += next_split;

		// *FIXME: Queue messages and wait for server
		really_send_chat_from_viewer(send, type, channel);
	}

	// moved here so we don't bump the count for every message segment
	LLViewerStats::getInstance()->incStat(LLViewerStats::ST_CHAT_COUNT);
//</FS:TS> FIRE-787
}

//<FS:TS> FIRE-787: break up too long chat lines into multiple messages
// This function just sends the message, with no other processing. Moved out
//	of send_chat_from_viewer.
void really_send_chat_from_viewer(std::string utf8_out_text, EChatType type, S32 channel)
{
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
}
//</FS:TS> FIRE-787

void LLNearbyChat::sendChatFromViewer(const std::string& utf8text, EChatType type, BOOL animate)
{
	sendChatFromViewer(utf8str_to_wstring(utf8text), type, animate);
}

void LLNearbyChat::sendChatFromViewer(const LLWString& wtext, EChatType type, BOOL animate)
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
		channel = (S32)(LLFloaterNearbyChat::getInstance()->getChild<LLSpinCtrl>("ChatChannel")->get());
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

EChatType LLNearbyChat::processChatTypeTriggers(EChatType type, std::string &str)
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

// If input of the form "/20foo" or "/20 foo", returns "foo" and channel 20.
// Otherwise returns input and channel 0.
LLWString LLNearbyChat::stripChannelNumber(const LLWString &mesg, S32* channel)
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

void LLNearbyChat::sendChat(LLWString text,EChatType type)
{
	LLWStringUtil::trim(text);

	if (!text.empty())
	{
		if(type == CHAT_TYPE_OOC)
		{
			std::string tempText = wstring_to_utf8str( text );
			tempText = gSavedSettings.getString("FSOOCPrefix") + " " + tempText + " " + gSavedSettings.getString("FSOOCPostfix");
			text = utf8str_to_wstring(tempText);
		}

		// Check if this is destined for another channel
		S32 channel = 0;
		stripChannelNumber(text, &channel);
		// If "/<number>" is not specified, see if a channel has been set in
		//  the spinner.
		if (gSavedSettings.getBOOL("FSNearbyChatbar") &&
			gSavedSettings.getBOOL("FSShowChatChannel") &&
			(channel == 0))
		{
			channel = (S32)(LLFloaterNearbyChat::getInstance()->getChild<LLSpinCtrl>("ChatChannel")->get());
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
			sendChatFromViewer(utf8_revised_text, type, gSavedSettings.getBOOL("FSPlayChatAnimation"));
		}
	}

	gAgent.stopTyping();
}

// all chat bars call this function and we keep the first or one that's seen as the default
void LLNearbyChat::registerChatBar(LLNearbyChatControl* chatBar)
{
	// TODO: make this a Param option "is_default"
	if(!mDefaultChatBar || chatBar->getName()=="default_chat_bar")
	{
		mDefaultChatBar=chatBar;
	}
}

// unhide the default nearby chat bar on request (pressing Enter or a letter key)
void LLNearbyChat::showDefaultChatBar(BOOL visible,const char* text) const
{
	if(!mDefaultChatBar)
		return;

	// change settings control to signal button state
	gSavedSettings.setBOOL("MainChatbarVisible",visible);

	mDefaultChatBar->getParent()->setVisible(visible);
	mDefaultChatBar->setVisible(visible);
	mDefaultChatBar->setFocus(visible);

	if(!text)
		return;

	if(mDefaultChatBar->getText().empty())
	{
		mDefaultChatBar->setText(LLStringExplicit(text));
		mDefaultChatBar->setCursorToEnd();
	}
}

// We want to know which nearby chat editor (if any) currently has focus
void LLNearbyChat::setFocusedInputEditor(LLNearbyChatControl* inputEditor,BOOL focus)
{
	if(focus)
		mFocusedInputEditor=inputEditor;

	// only remove focus if the request came from the previously active input editor
	// to avoid races
	else if(mFocusedInputEditor==inputEditor)
		mFocusedInputEditor=NULL;
}

// for the "arrow key moves avatar when chat is empty" hack in llviewerwindow.cpp
// and the hide chat bar feature in mouselook in llagent.cpp
BOOL LLNearbyChat::defaultChatBarIsIdle() const
{
	if(mFocusedInputEditor && mFocusedInputEditor->getName()=="default_chat_bar")
		return mFocusedInputEditor->getText().empty();

	// if any other chat bar has focus, report "idle", because they're not the default
	return TRUE;
}

// for the "arrow key moves avatar when chat is empty" hack in llviewerwindow.cpp
BOOL LLNearbyChat::defaultChatBarHasFocus() const
{
	if(mFocusedInputEditor && mFocusedInputEditor->getName()=="default_chat_bar")
		return TRUE;

	return FALSE;
}
