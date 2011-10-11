
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
			if(gSavedPerAccountSettings.getBOOL("FSKeywordPlaySound"))
				LLUI::sAudioCallback(LLUUID(gSavedPerAccountSettings.getString("FSKeywordSound")));

			return true;
		}
	}
	return false;
}
