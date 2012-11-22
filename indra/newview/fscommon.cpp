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
