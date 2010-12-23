/** 
 *
 * Copyright (c) 2010, Kitty Barnett
 * 
 * The source code in this file is provided to you under the terms of the 
 * GNU Lesser General Public License, version 2.1, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A 
 * PARTICULAR PURPOSE. Terms of the LGPL can be found in doc/LGPL-licence.txt 
 * in this distribution, or online at http://www.gnu.org/licenses/lgpl-2.1.txt
 * 
 * By copying, modifying or distributing this software, you acknowledge that
 * you have read and understood your obligations described above, and agree to 
 * abide by those obligations.
 * 
 */

#ifndef LLHUNSPELL_H
#define LLHUNSËLL_H

#include "llsingleton.h"

class Hunspell;

// ============================================================================

class LLHunspellWrapper : public LLSingleton<LLHunspellWrapper>
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
	S32		getSuggestions(const std::string& strWord, std::vector<std::string>& strSuggestionList);

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
