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

#include "fskeywords.h"
#include "llui.h"
#include "llviewercontrol.h"

#include <boost/regex.hpp>
//#include <boost/algorithm/string/find.hpp> //for boost::ifind_first


FSKeywords::FSKeywords()
{
	gSavedPerAccountSettings.getControl("FSKeywords")->getSignal()->connect(boost::bind(&FSKeywords::updateKeywords, this));
	updateKeywords();
}

FSKeywords::~FSKeywords()
{
}

void FSKeywords::updateKeywords()
{
	std::string s = gSavedPerAccountSettings.getString("FSKeywords");
	LLStringUtil::toLower(s);
	boost::regex re(",");
	boost::sregex_token_iterator begin(s.begin(), s.end(), re, -1), end;
	mWordList.clear();
	while(begin != end)
	{
		mWordList.push_back(*begin++);
	}
}

bool FSKeywords::chatContainsKeyword(const LLChat& chat, bool is_local)
{
	static LLCachedControl<bool> sFSKeywordOn(gSavedPerAccountSettings, "FSKeywordOn", false);
	static LLCachedControl<bool> sFSKeywordInChat(gSavedPerAccountSettings, "FSKeywordInChat", false);
	static LLCachedControl<bool> sFSKeywordInIM(gSavedPerAccountSettings, "FSKeywordInIM", false);
	if (!sFSKeywordOn ||
	(is_local && !sFSKeywordInChat) ||
	(!is_local && !sFSKeywordInIM))
		return FALSE;

	std::string source(chat.mText);
	LLStringUtil::toLower(source);
	
	for(U32 i=0; i < mWordList.size(); i++)
	{
		if(source.find(mWordList[i]) != std::string::npos)
		{
			return true;
		}
	}
	return false;
}

// <FS:PP> FIRE-10178: Keyword Alerts in group IM do not work unless the group is in the foreground
void FSKeywords::notify(const LLChat& chat)
{
	static LLCachedControl<bool> PlayModeUISndFSKeywordSound(gSavedSettings, "PlayModeUISndFSKeywordSound");
	if(PlayModeUISndFSKeywordSound)
		LLUI::sAudioCallback(LLUUID(gSavedSettings.getString("UISndFSKeywordSound")));
}
// </FS:PP>
