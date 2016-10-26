/** 
 * @file fskeywords.cpp
 *
 * $LicenseInfo:firstyear=2011&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2011, The Phoenix Firestorm Project, Inc.
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
#include "fskeywords.h"
#include "growlmanager.h"
#include "llagent.h"
#include "llchat.h"
#include "llinstantmessage.h"
#include "llmutelist.h"
#include "llui.h"
#include "llviewercontrol.h"

#include <boost/regex.hpp>

FSKeywords::FSKeywords()
{
	gSavedPerAccountSettings.getControl("FSKeywords")->getSignal()->connect(boost::bind(&FSKeywords::updateKeywords, this));
	gSavedPerAccountSettings.getControl("FSKeywordCaseSensitive")->getSignal()->connect(boost::bind(&FSKeywords::updateKeywords, this));
	gSavedPerAccountSettings.getControl("FSKeywordMatchWholeWords")->getSignal()->connect(boost::bind(&FSKeywords::updateKeywords, this));
	updateKeywords();
}

FSKeywords::~FSKeywords()
{
}

void FSKeywords::updateKeywords()
{
	BOOL match_whole_words = gSavedPerAccountSettings.getBOOL("FSKeywordMatchWholeWords");
	std::string s = gSavedPerAccountSettings.getString("FSKeywords");
	if (!gSavedPerAccountSettings.getBOOL("FSKeywordCaseSensitive"))
	{
		LLStringUtil::toLower(s);
	}
	boost::regex re(",");
	boost::sregex_token_iterator begin(s.begin(), s.end(), re, -1), end;
	mWordList.clear();
	while (begin != end)
	{
		if (match_whole_words)
		{
			mWordList.push_back(boost::regex_replace(std::string(*begin++), boost::regex("[.^$|()\\[\\]{}*+?\\\\]"), "\\\\&", boost::match_default|boost::format_sed));
		}
		else
		{
			mWordList.push_back(*begin++);
		}
	}
}

bool FSKeywords::chatContainsKeyword(const LLChat& chat, bool is_local)
{

	// Don't check if message is from us - unless it's a radar notification
	if (chat.mFromID == gAgentID && chat.mFromName != SYSTEM_FROM)
	{
		return false;
	}

	static LLCachedControl<bool> sFSKeywordOn(gSavedPerAccountSettings, "FSKeywordOn", false);
	static LLCachedControl<bool> sFSKeywordInChat(gSavedPerAccountSettings, "FSKeywordInChat", false);
	static LLCachedControl<bool> sFSKeywordInIM(gSavedPerAccountSettings, "FSKeywordInIM", false);

	if (!sFSKeywordOn ||
		(is_local && !sFSKeywordInChat) ||
		(!is_local && !sFSKeywordInIM))
	{
		return false;
	}

	static LLCachedControl<bool> sFSKeywordSpeakersName(gSavedPerAccountSettings, "FSKeywordSpeakersName", false);

	std::string source;
	if (sFSKeywordSpeakersName)
	{
		source = chat.mFromName + " " + chat.mText;
	}
	else
	{
		source = chat.mText;
	}

	static LLCachedControl<bool> sFSKeywordCaseSensitive(gSavedPerAccountSettings, "FSKeywordCaseSensitive", false);

	if (!sFSKeywordCaseSensitive)
	{
		LLStringUtil::toLower(source);
	}

	static LLCachedControl<bool> sFSKeywordMatchWholeWords(gSavedPerAccountSettings, "FSKeywordMatchWholeWords", false);

	if (sFSKeywordMatchWholeWords)
	{
		for (std::vector<std::string>::iterator it = mWordList.begin(); it != mWordList.end(); ++it)
		{
			if (boost::regex_search(source, boost::regex("\\b" + (*it) + "\\b")))
			{
				return true;
			}
		}
	}
	else
	{
		for (std::vector<std::string>::iterator it = mWordList.begin(); it != mWordList.end(); ++it)
		{
			if (source.find((*it)) != std::string::npos)
			{
				return true;
			}
		}
	}

	return false;
}

// <FS:PP> FIRE-10178: Keyword Alerts in group IM do not work unless the group is in the foreground
void FSKeywords::notify(const LLChat& chat)
{
	if (chat.mFromID != gAgentID || chat.mFromName == SYSTEM_FROM)
	{
		if (!chat.mMuted && !LLMuteList::getInstance()->isMuted(chat.mFromID))
		{
			static LLCachedControl<bool> PlayModeUISndFSKeywordSound(gSavedPerAccountSettings, "PlayModeUISndFSKeywordSound");
			if (PlayModeUISndFSKeywordSound)
			{
				LLUI::sAudioCallback(LLUUID(gSavedPerAccountSettings.getString("UISndFSKeywordSound")));
			}

			static LLCachedControl<bool> FSEnableGrowl(gSavedSettings, "FSEnableGrowl");
			if (FSEnableGrowl)
			{
				std::string msg = chat.mFromName;
				if (is_irc_me_prefix(chat.mText))
				{
					msg = msg + chat.mText.substr(3);
				}
				else
				{
					msg = msg + ": " + chat.mText;
				}
				GrowlManager::notify("Keyword Alert", msg, GROWL_KEYWORD_ALERT_TYPE);
			}
		}
	}
}
// </FS:PP>
