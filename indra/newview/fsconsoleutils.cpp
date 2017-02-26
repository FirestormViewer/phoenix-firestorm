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

#include "fscommon.h"
#include "llagent.h"
#include "llavatarnamecache.h"
#include "llconsole.h"
#include "llimview.h"
#include "lltrans.h"
#include "llviewerchat.h"
#include "llviewercontrol.h"

// static
bool FSConsoleUtils::ProcessChatMessage(const LLChat& chat_msg, const LLSD &args)
{
	static LLCachedControl<bool> fsUseNearbyChatConsole(gSavedSettings, "FSUseNearbyChatConsole");
	static LLCachedControl<bool> useChatBubbles(gSavedSettings, "UseChatBubbles");
	static LLCachedControl<bool> fsBubblesHideConsoleAndToasts(gSavedSettings, "FSBubblesHideConsoleAndToasts");

	if (!fsUseNearbyChatConsole)
	{
		return false;
	}

	// Don't write to console if avatar chat and user wants
	// bubble chat or if the user is busy.
	if ( (chat_msg.mSourceType == CHAT_SOURCE_AGENT && useChatBubbles && fsBubblesHideConsoleAndToasts)
		|| gAgent.isDoNotDisturb() )
	{
		return true;
	}

	std::string console_chat;
		
	if (chat_msg.mSourceType == CHAT_SOURCE_AGENT) 
	{
		LLAvatarNameCache::get(chat_msg.mFromID, boost::bind(&FSConsoleUtils::onProcessChatAvatarNameLookup, _1, _2, chat_msg));
	}
	else if (chat_msg.mSourceType == CHAT_SOURCE_OBJECT)
	{
		std::string sender_name(chat_msg.mFromName);

		//IRC styled /me messages.
		bool irc_me = is_irc_me_prefix(chat_msg.mText);

		// Delimiter after a name in header copy/past and in plain text mode
		std::string delimiter = ": ";
		static const std::string shout = LLTrans::getString("shout");
		static const std::string whisper = LLTrans::getString("whisper");
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
		console_chat = sender_name + delimiter + message;
		LLColor4 chatcolor;
		LLViewerChat::getChatColor(chat_msg, chatcolor);
		gConsole->addConsoleLine(console_chat, chatcolor);
	}
	else
	{
		if (args.has("money_tracker") && args["money_tracker"].asBoolean() == true &&
			chat_msg.mSourceType == CHAT_SOURCE_SYSTEM)
		{
			console_chat = args["console_message"].asString();
		}
		// FS:LO FIRE-1439 - Clickable avatar names on local chat radar crossing reports
		else if (chat_msg.mFromName.empty())
		{
			console_chat = chat_msg.mText;
		}
		else
		{
			console_chat = chat_msg.mFromName + " " + chat_msg.mText;
		}
		// FS:LO FIRE-1439 - Clickable avatar names on local chat radar crossing reports

		LLColor4 chatcolor;
		LLViewerChat::getChatColor(chat_msg, chatcolor);
		gConsole->addConsoleLine(console_chat, chatcolor);
	}

	return true;
}

//static
void FSConsoleUtils::onProcessChatAvatarNameLookup(const LLUUID& agent_id, const LLAvatarName& av_name, const LLChat& chat_msg)
{
	std::string console_chat;
	std::string sender_name(chat_msg.mFromName);

	//IRC styled /me messages.
	bool irc_me = is_irc_me_prefix(chat_msg.mText);

	// Delimiter after a name in header copy/past and in plain text mode
	std::string delimiter = ": ";
	static const std::string shout = LLTrans::getString("shout");
	static const std::string whisper = LLTrans::getString("whisper");
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
	if (!chat_msg.mRlvNamesFiltered)
	{
		sender_name = FSCommon::getAvatarNameByDisplaySettings(av_name);
	}

	console_chat = sender_name + delimiter + message;
	LLColor4 chatcolor;
	LLViewerChat::getChatColor(chat_msg, chatcolor);
	gConsole->addConsoleLine(console_chat, chatcolor);
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
	if (!session)
	{
		return false;
	}

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
	S32 groupNameLength = gSavedSettings.getS32("FSShowGroupNameLength");
	if (groupNameLength != 0 && session->isGroupSessionType())
	{
		group = session->mName.substr(0, groupNameLength);
	}

	LLAvatarNameCache::get(from_id, boost::bind(&FSConsoleUtils::onProccessInstantMessageNameLookup, _1, _2, message, group, session_id));

	return true;
}

//static
void FSConsoleUtils::onProccessInstantMessageNameLookup(const LLUUID& agent_id, const LLAvatarName& av_name, const std::string& message_str, const std::string& group, const LLUUID& session_id)
{
	const bool is_group = !group.empty();

	std::string sender_name;
	std::string message(message_str);
	std::string delimiter = ": ";

	// irc styled messages
	if (is_irc_me_prefix(message))
	{
		delimiter = LLStringUtil::null;
		message = message.substr(3);
	}

	sender_name = FSCommon::getAvatarNameByDisplaySettings(av_name);

	// Replacing the "IM" in front of group chat messages with the actual group name
	if (is_group)
	{
		sender_name = "[" + group + "] " + sender_name;
	}

	LLChat chat;
	chat.mFromID = agent_id;
	chat.mFromName = sender_name;
	chat.mText = message_str;
	chat.mSourceType = CHAT_SOURCE_AGENT;
	chat.mChatType = is_group ? CHAT_TYPE_IM_GROUP : CHAT_TYPE_IM;
	LLColor4 textcolor;
	LLViewerChat::getChatColor(chat, textcolor, LLSD().with("is_local", false).with("for_console", true));

	gConsole->addConsoleLine("IM: " + sender_name + delimiter + message, textcolor, session_id);
}
