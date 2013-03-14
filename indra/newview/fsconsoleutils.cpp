/**
 * @file fsconsoleutils.cpp
 * @brief Class for console-related functions in Firestorm
 *
 * $LicenseInfo:firstyear=2013&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2013 Ansariel Hiller @ Second Life
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
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "fsconsoleutils.h"

#include "fsfloaternearbychat.h"
#include "lggcontactsets.h"
#include "llagent.h"
#include "llconsole.h"
#include "llfloaterreg.h"
#include "llimview.h"
#include "llnotificationhandler.h"
#include "lltrans.h"
#include "llviewercontrol.h"
#include "rlvhandler.h"

// static
BOOL FSConsoleUtils::isNearbyChatVisible()
{
	FSFloaterNearbyChat* nearby_chat = LLFloaterReg::getTypedInstance<FSFloaterNearbyChat>("fs_nearby_chat", LLSD());
	return nearby_chat->getVisible();
}

// static
bool FSConsoleUtils::ProcessChatMessage(const LLChat& chat_msg, const LLSD &args)
{
	static LLCachedControl<bool> fsUseNearbyChatConsole(gSavedSettings, "FSUseNearbyChatConsole");
	static LLCachedControl<bool> useChatBubbles(gSavedSettings, "UseChatBubbles");

	if (!fsUseNearbyChatConsole)
	{
		return false;
	}

	// Don't write to console if avatar chat and user wants
	// bubble chat or if the user is busy.
	if ( (chat_msg.mSourceType == CHAT_SOURCE_AGENT && useChatBubbles)
		|| gAgent.getBusy() )
	{
		return true;
	}

	std::string consoleChat;
		
	if (chat_msg.mSourceType == CHAT_SOURCE_AGENT) 
	{
		LLAvatarNameCache::get(chat_msg.mFromID, boost::bind(&FSConsoleUtils::onProcessChatAvatarNameLookup, _1, _2, chat_msg));
	}
	else if (chat_msg.mSourceType == CHAT_SOURCE_OBJECT)
	{
		std::string senderName(chat_msg.mFromName);
		std::string prefix = chat_msg.mText.substr(0, 4);
		LLStringUtil::toLower(prefix);

		//IRC styled /me messages.
		bool irc_me = prefix == "/me " || prefix == "/me'";

		// Delimiter after a name in header copy/past and in plain text mode
		std::string delimiter = ": ";
		std::string shout = LLTrans::getString("shout");
		std::string whisper = LLTrans::getString("whisper");
		if (chat_msg.mChatType == CHAT_TYPE_SHOUT || 
			chat_msg.mChatType == CHAT_TYPE_WHISPER ||
			chat_msg.mText.compare(0, shout.length(), shout) == 0 ||
			chat_msg.mText.compare(0, whisper.length(), whisper) == 0)
		{
			delimiter = " ";
		}

		// Don't add any delimiter after name in irc styled messages
		if (irc_me || chat_msg.mChatStyle == CHAT_STYLE_IRC)
		{
			delimiter = LLStringUtil::null;
		}

		std::string message = irc_me ? chat_msg.mText.substr(3) : chat_msg.mText;
		consoleChat = senderName + delimiter + message;
		LLColor4 chatcolor;
		LLViewerChat::getChatColor(chat_msg, chatcolor);
		gConsole->addConsoleLine(consoleChat, chatcolor);
		gConsole->setVisible(!isNearbyChatVisible());
	}
	else
	{
		if (chat_msg.mSourceType == CHAT_SOURCE_SYSTEM &&
			args["type"].asInteger() == LLNotificationsUI::NT_MONEYCHAT)
		{
			consoleChat = args["console_message"].asString();
		}
		// FS:LO FIRE-1439 - Clickable avatar names on local chat radar crossing reports
		else if (chat_msg.mFromName.empty())
		{
			consoleChat = chat_msg.mText;
		}
		else
		{
			consoleChat = chat_msg.mFromName + " " + chat_msg.mText;
		}
		// FS:LO FIRE-1439 - Clickable avatar names on local chat radar crossing reports

		LLColor4 chatcolor;
		LLViewerChat::getChatColor(chat_msg, chatcolor);
		gConsole->addConsoleLine(consoleChat, chatcolor);
		gConsole->setVisible(!isNearbyChatVisible());
	}

	return true;
}

//static
void FSConsoleUtils::onProcessChatAvatarNameLookup(const LLUUID& agent_id, const LLAvatarName& av_name, const LLChat& chat_msg)
{
	std::string consoleChat;
	std::string senderName(chat_msg.mFromName);
	std::string prefix = chat_msg.mText.substr(0, 4);
	LLStringUtil::toLower(prefix);

	//IRC styled /me messages.
	bool irc_me = prefix == "/me " || prefix == "/me'";

	// Delimiter after a name in header copy/past and in plain text mode
	std::string delimiter = ": ";
	std::string shout = LLTrans::getString("shout");
	std::string whisper = LLTrans::getString("whisper");
	if (chat_msg.mChatType == CHAT_TYPE_SHOUT || 
		chat_msg.mChatType == CHAT_TYPE_WHISPER ||
		chat_msg.mText.compare(0, shout.length(), shout) == 0 ||
		chat_msg.mText.compare(0, whisper.length(), whisper) == 0)
	{
		delimiter = " ";
	}

	// Don't add any delimiter after name in irc styled messages
	if (irc_me || chat_msg.mChatStyle == CHAT_STYLE_IRC)
	{
		delimiter = LLStringUtil::null;
	}

	std::string message = irc_me ? chat_msg.mText.substr(3) : chat_msg.mText;

	// Get the display name of the sender if required
	static LLCachedControl<bool> nameTagShowUsernames(gSavedSettings, "NameTagShowUsernames");
	static LLCachedControl<bool> useDisplayNames(gSavedSettings, "UseDisplayNames");
	if (!chat_msg.mRlvNamesFiltered)
	{
		if (nameTagShowUsernames && useDisplayNames)
		{
			senderName = av_name.getCompleteName();
		}
		else if (useDisplayNames)
		{
			senderName = av_name.mDisplayName;
		}
	}

	consoleChat = senderName + delimiter + message;
	LLColor4 chatcolor;
	LLViewerChat::getChatColor(chat_msg, chatcolor);
	gConsole->addConsoleLine(consoleChat, chatcolor);
	gConsole->setVisible(!isNearbyChatVisible());
}

//static
bool FSConsoleUtils::ProcessInstantMessage(const LLUUID& session_id, const LLUUID& from_id, const std::string& message)
{
	static LLCachedControl<bool> fsUseNearbyChatConsole(gSavedSettings, "FSUseNearbyChatConsole");
	static LLCachedControl<bool> fsLogGroupImToChatConsole(gSavedSettings, "FSLogGroupImToChatConsole");
	static LLCachedControl<bool> fsLogImToChatConsole(gSavedSettings, "FSLogImToChatConsole");

	if (!fsUseNearbyChatConsole)
	{
		return false;
	}

	LLIMModel::LLIMSession* session = LLIMModel::instance().findIMSession(session_id);
	if (!fsLogGroupImToChatConsole && session->isGroupSessionType())
	{
		return false;
	}
	if (!fsLogImToChatConsole && !session->isGroupSessionType())
	{
		return false;
	}

	if (from_id.isNull() || message.empty())
	{
		return true;
	}

	// Replacing the "IM" in front of group chat messages with the actual group name
	std::string group;
	if (session)
	{
		S32 groupNameLength = gSavedSettings.getS32("FSShowGroupNameLength");
		if (groupNameLength != 0 && session->isGroupSessionType())
		{
			group = session->mName.substr(0, groupNameLength);
		}
	}

	LLAvatarNameCache::get(from_id, boost::bind(&FSConsoleUtils::onProccessInstantMessageNameLookup, _1, _2, message, group));

	return true;
}

//static
void FSConsoleUtils::onProccessInstantMessageNameLookup(const LLUUID& agent_id, const LLAvatarName& av_name, const std::string& message_str, const std::string& group)
{
	std::string senderName;
	std::string message(message_str);
	std::string delimiter = ": ";
	std::string prefix = message.substr(0, 4);
	LLStringUtil::toLower(prefix);

	// irc styled messages
	if (prefix == "/me " || prefix == "/me'")
	{
		delimiter = LLStringUtil::null;
		message = message.substr(3);
	}

	static LLCachedControl<bool> nameTagShowUsernames(gSavedSettings, "NameTagShowUsernames");
	static LLCachedControl<bool> useDisplayNames(gSavedSettings, "UseDisplayNames");
	if (nameTagShowUsernames && useDisplayNames)
	{
		senderName = av_name.getCompleteName();
	}
	else if (useDisplayNames)
	{
		senderName = av_name.mDisplayName;
	}
	else
	{
		senderName = av_name.getLegacyName();
	}

	if (rlv_handler_t::isEnabled() && gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
	{
		senderName = RlvStrings::getAnonym(senderName);
	}

	// Replacing the "IM" in front of group chat messages with the actual group name
	if (!group.empty())
	{
		senderName = "[" + group + "] " + senderName;
	}

	LLColor4 textColor = LLUIColorTable::instance().getColor("AgentChatColor");

	//color based on contact sets prefs
	if (LGGContactSets::getInstance()->hasFriendColorThatShouldShow(agent_id,TRUE))
	{
		textColor = LGGContactSets::getInstance()->getFriendColor(agent_id);
	}

	gConsole->addConsoleLine("IM: " + senderName + delimiter + message, textColor);
	gConsole->setVisible(!isNearbyChatVisible());
}
