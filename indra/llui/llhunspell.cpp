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

#include "lldir.h"
#include "llsdserialize.h"

#include "llhunspell.h"

#if LL_WINDOWS
	#include <hunspell/hunspelldll.h>
#else
	#include <hunspell/hunspell.hxx>
#endif

// ============================================================================
// Static member variables
//

bool LLHunspellWrapper::s_fSpellCheck = false;

// ============================================================================

LLHunspellWrapper::LLHunspellWrapper()
	: m_pHunspell(NULL)
{
	m_strDictionaryPath = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "hunspell", "");

	// Load dictionary information (file name, friendly name, ...)
	llifstream fileDictMap(m_strDictionaryPath + "dictionaries.xml", std::ios::binary);
	if (fileDictMap.is_open())
		LLSDSerialize::fromXMLDocument(m_sdDictionaryMap, fileDictMap);

	// Look for installed dictionaries
	std::string strTempPath;
	for (LLSD::array_iterator itDictInfo = m_sdDictionaryMap.beginArray(), endDictInfo = m_sdDictionaryMap.endArray();
			itDictInfo != endDictInfo; ++itDictInfo)
	{
		LLSD& sdDict = *itDictInfo;
		strTempPath = (sdDict.has("name")) ? m_strDictionaryPath + sdDict["name"].asString() : LLStringUtil::null;
		sdDict["installed"] = 
			(!strTempPath.empty()) && (gDirUtilp->fileExists(strTempPath + ".aff")) && (gDirUtilp->fileExists(strTempPath + ".dic"));
	}
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
// Dictionary related functions
//

// Checked: 2010-12-23 (Catznip-2.5.0a) | Added: Catznip-2.5.0a
S32 LLHunspellWrapper::getDictionaries(std::vector<std::string>& strDictionaryList) const
{
	strDictionaryList.clear();
	for (LLSD::array_const_iterator itDictInfo = m_sdDictionaryMap.beginArray(), endDictInfo = m_sdDictionaryMap.endArray();
			itDictInfo != endDictInfo; ++itDictInfo)
	{
		const LLSD& sdDict = *itDictInfo;
		strDictionaryList.push_back(sdDict["language"].asString());
	}
	return strDictionaryList.size();
}

// Checked: 2010-12-23 (Catznip-2.5.0a) | Added: Catznip-2.5.0a
S32 LLHunspellWrapper::getInstalledDictionaries(std::vector<std::string>& strDictionaryList) const
{
	strDictionaryList.clear();
	for (LLSD::array_const_iterator itDictInfo = m_sdDictionaryMap.beginArray(), endDictInfo = m_sdDictionaryMap.endArray();
			itDictInfo != endDictInfo; ++itDictInfo)
	{
		const LLSD& sdDict = *itDictInfo;
		if (sdDict["installed"].asBoolean())
			strDictionaryList.push_back(sdDict["language"].asString());
	}
	return strDictionaryList.size();
}

// Checked: 2010-12-23 (Catznip-2.5.0a) | Added: Catznip-2.5.0a
bool LLHunspellWrapper::setCurrentDictionary(const std::string& strDictionary)
{
	if (m_pHunspell)
	{
		delete m_pHunspell;
		m_pHunspell = NULL;
		m_strDictionary = strDictionary;
	}

	if ( (!useSpellCheck()) || (strDictionary.empty()) )
		return false;

	std::string strDictFile;
	for (LLSD::array_const_iterator itDictInfo = m_sdDictionaryMap.beginArray(), endDictInfo = m_sdDictionaryMap.endArray();
			itDictInfo != endDictInfo; ++itDictInfo)
	{
		const LLSD& sdDict = *itDictInfo;
		if ( (sdDict["installed"].asBoolean()) && (strDictionary == sdDict["language"].asString()) )
			strDictFile = sdDict["name"].asString();
	}

	if (!strDictFile.empty())
	{
		std::string strPathAff = m_strDictionaryPath + strDictFile + ".aff";
		std::string strPathDic = m_strDictionaryPath + strDictFile + ".dic";

		m_pHunspell = new Hunspell(strPathAff.c_str(), strPathDic.c_str());
		m_strDictionary = strDictionary;
	}

	return (NULL != m_pHunspell);
}

// ============================================================================
// Static member functions
//

void LLHunspellWrapper::setUseSpellCheck(bool fSpellCheck)
{
	s_fSpellCheck = fSpellCheck;

	if ( (!s_fSpellCheck) && (LLHunspellWrapper::instanceExists()) )
		LLHunspellWrapper::instance().setCurrentDictionary(LLStringUtil::null);
}

// ============================================================================
