#ifndef FS_KEYWORDS_H
#define FS_KEYWORDS_H

#include "llsingleton.h"
#include "llstring.h"
#include "llchat.h"

class LLViewerRegion;


class FSKeywords : public LLSingleton<FSKeywords>
{
public:
	FSKeywords();
	virtual ~FSKeywords();

	void updateKeywords();
	bool chatContainsKeyword(const LLChat& chat, bool is_local);
	void static notify(const LLChat& chat); // <FS:PP> FIRE-10178: Keyword Alerts in group IM do not work unless the group is in the foreground

private:
	std::vector<std::string> mWordList;
	
	
};

#endif // FS_KEYWORDS_H
