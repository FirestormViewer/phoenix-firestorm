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

private:
	std::vector<std::string> mWordList;
	
	
};

#endif // FS_KEYWORDS_H