/**
 * @file fsmoneytracker.cpp
 * @brief Tip Tracker Window
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Copyright (c) 2011 Arrehn Oberlander
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
#include "fsmoneytracker.h"
#include "llfloaterreg.h"
#include "llviewercontrol.h"
#include "lllineeditor.h"
#include "llnotificationmanager.h"
#include "lltrans.h"



FSMoneyTracker::FSMoneyTracker(const LLSD& seed)
: LLFloater(seed)
{
}

FSMoneyTracker::~FSMoneyTracker()
{
	if (mTransactionHistory)
		mTransactionHistory->clear();
}

BOOL FSMoneyTracker::postBuild()
{
	// <FS:Ansariel> [FS communication UI]
	//mTransactionHistory = getChild<LLChatHistory>("money_chat_history");
	mTransactionHistory = getChild<FSChatHistory>("money_chat_history");
	// </FS:Ansariel> [FS communication UI]
	mTransactionHistory->clear();
	
	// Button Actions
	childSetAction("Clear", boost::bind(&FSMoneyTracker::clear,this)); 

	return TRUE;
}

void FSMoneyTracker::addMessage(const LLChat& chat,bool archive,const LLSD &args)
{
	LLChat& tmp_chat = const_cast<LLChat&>(chat);
	tmp_chat.mFromName = chat.mFromName;
	LLSD chat_args = args;
	chat_args["use_plain_text_chat_history"] = true;
	if(tmp_chat.mTimeStr.empty())
		tmp_chat.mTimeStr = appendTime();
	
	mTransactionHistory->appendMessage(tmp_chat, chat_args);
}

std::string FSMoneyTracker::appendTime()
{
	time_t utc_time;
	utc_time = time_corrected();
	std::string timeStr ="["+ LLTrans::getString("TimeHour")+"]:[" + LLTrans::getString("TimeMin")+"]";
	
	// <FS:PP> Attempt to speed up things a little
	// if (gSavedSettings.getBOOL("FSSecondsinChatTimestamps"))
	static LLCachedControl<bool> FSSecondsinChatTimestamps(gSavedSettings, "FSSecondsinChatTimestamps");
	if (FSSecondsinChatTimestamps)
	// </FS:PP>
	{
		timeStr += ":[" + LLTrans::getString("TimeSec")+"]";
	}
	
	LLSD substitution;
	
	substitution["datetime"] = (S32) utc_time;
	LLStringUtil::format (timeStr, substitution);
	
	return timeStr;
}

void FSMoneyTracker::clear()
{
	llinfos << "Cleared." << llendl;
	mTransactionHistory->clear();
}
