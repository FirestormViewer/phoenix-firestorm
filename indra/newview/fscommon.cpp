/**
 * @file fscommon.cpp
 * @brief Central class for common used functions in Firestorm
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2012 Ansariel Hiller @ Second Life
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

#include "fscommon.h"
#include "llnotificationmanager.h"

#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
using namespace boost::posix_time;
using namespace boost::gregorian;

void reportToNearbyChat(const std::string& message)
{
	LLChat chat;
    chat.mText = message;
	chat.mSourceType = CHAT_SOURCE_SYSTEM;
	LLSD args;
	args["type"] = LLNotificationsUI::NT_NEARBYCHAT;
	LLNotificationsUI::LLNotificationManager::instance().onChat(chat, args);
}

std::string applyAutoCloseOoc(const std::string& message)
{
	if (!gSavedSettings.getBOOL("AutoCloseOOC"))
	{
		return message;
	}

	std::string utf8_text(message);

	// Try to find any unclosed OOC chat (i.e. an opening
	// double parenthesis without a matching closing double
	// parenthesis.
	if (utf8_text.find("(( ") != -1 && utf8_text.find("))") == -1)
	{
		// add the missing closing double parenthesis.
		utf8_text += " ))";
	}
	else if (utf8_text.find("((") != -1 && utf8_text.find("))") == -1)
	{
		if (utf8_text.at(utf8_text.length() - 1) == ')')
		{
			// cosmetic: add a space first to avoid a closing triple parenthesis
			utf8_text += " ";
		}
		// add the missing closing double parenthesis.
		utf8_text += "))";
	}
	else if (utf8_text.find("[[ ") != -1 && utf8_text.find("]]") == -1)
	{
		// add the missing closing double parenthesis.
		utf8_text += " ]]";
	}
	else if (utf8_text.find("[[") != -1 && utf8_text.find("]]") == -1)
	{
		if (utf8_text.at(utf8_text.length() - 1) == ']')
		{
			// cosmetic: add a space first to avoid a closing triple parenthesis
			utf8_text += " ";
		}
			// add the missing closing double parenthesis.
		utf8_text += "]]";
	}

	return utf8_text;
}

std::string applyMuPose(const std::string& message)
{
	std::string utf8_text(message);

	// Convert MU*s style poses into IRC emotes here.
	if (gSavedSettings.getBOOL("AllowMUpose") && utf8_text.find(":") == 0 && utf8_text.length() > 3)
	{
		if (utf8_text.find(":'") == 0)
		{
			utf8_text.replace(0, 1, "/me");
 		}
		else if (!isdigit(utf8_text.at(1)) && !ispunct(utf8_text.at(1)) && !isspace(utf8_text.at(1)))	// Do not prevent smileys and such.
		{
			utf8_text.replace(0, 1, "/me ");
		}
	}

	return utf8_text;
}


S32 FSCommon::secondsSinceEpochFromString(const std::string& format, const std::string& str)
{
	// LLDateUtil::secondsSinceEpochFromString does not handle time, only the date.
	// copied that function here and added the needed code to handle time fields.  -- TL
	time_input_facet *facet = new time_input_facet(format);
	std::stringstream ss;
	ss << str;
	ss.imbue(std::locale(ss.getloc(), facet));
	ptime time_t_date;
	ss >> time_t_date;
	ptime time_t_epoch(date(1970,1,1));
	time_duration diff = time_t_date - time_t_epoch;
	return diff.total_seconds();
}
