#ifndef LLHUNSPELL_H
#define LLHUNSËLL_H

#include "llsingleton.h"

class Hunspell;

// ============================================================================

class LL_COMMON_API LLHunspellWrapper : public LLSingleton<LLHunspellWrapper>
{
	friend class LLSingleton<LLHunspellWrapper>;
protected:
	LLHunspellWrapper();
	~LLHunspellWrapper();

	/*
	 *
	 */
public:
	bool	checkSpelling(const std::string& strWord);
	S32		getSuggestions(const std::string& strWord, std::vector<std::string>& strSuggestions);

	static const std::string&	getDictPath()							{ return s_strDictPath; }
	static void					setDictPath(const std::string& strPath)	{ s_strDictPath = strPath; }

	static bool					useSpellCheck()							{ return s_fSpellCheck; }
	static void					setUseSpellCheck(bool fSpellCheck);

	/*
	 * Member variables
	 */
protected:
	Hunspell*	m_pHunspell;

	static bool			s_fSpellCheck;
	static std::string	s_strDictPath;
};

// ============================================================================

#endif // LLHUNSPELL_H
