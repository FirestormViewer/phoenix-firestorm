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

#include "linden_common.h"

#include "llhunspell.h"

#if LL_WINDOWS
	#include <hunspell/hunspelldll.h>
#else
	#include <hunspell/hunspell.hxx>
#endif

// ============================================================================

bool LLHunspellWrapper::s_fSpellCheck = false;
std::string	LLHunspellWrapper::s_strDictPath = "";

// ============================================================================

LLHunspellWrapper::LLHunspellWrapper()
	: m_pHunspell(NULL)
{
}

LLHunspellWrapper::~LLHunspellWrapper()
{
	delete m_pHunspell;
}

bool LLHunspellWrapper::checkSpelling(const std::string& strWord)
{
	if ( (!s_fSpellCheck) || (!m_pHunspell) || (strWord.length() < 3) )
		return true;
	return m_pHunspell->spell(strWord.c_str());
}

S32 LLHunspellWrapper::getSuggestions(const std::string& strWord, std::vector<std::string>& strSuggestionList)
{
	if ( (!s_fSpellCheck) || (!m_pHunspell) || (strWord.length() < 3) )
		return 0;

	strSuggestionList.clear();

	char** ppstrSuggestionList; int cntSuggestion = 0;
	if ( (cntSuggestion = m_pHunspell->suggest(&ppstrSuggestionList, strWord.c_str())) != 0 )
	{
		for (int idxSuggestion = 0; idxSuggestion < cntSuggestion; idxSuggestion++)
			strSuggestionList.push_back(ppstrSuggestionList[idxSuggestion]);
		m_pHunspell->free_list(&ppstrSuggestionList, cntSuggestion);	
	}
	return strSuggestionList.size();
}

// ============================================================================

void LLHunspellWrapper::setUseSpellCheck(bool fSpellCheck)
{
	s_fSpellCheck = fSpellCheck;

	std::string strPathAff = s_strDictPath + "en_us.aff";
	std::string strPathDic = s_strDictPath + "en_us.dic";

	LLHunspellWrapper::instance().m_pHunspell = new Hunspell(strPathAff.c_str(), strPathDic.c_str());
}

// ============================================================================
