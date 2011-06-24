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
	#pragma comment(lib, "libhunspell.lib")
#else
	#include <hunspell/hunspell.hxx>
#endif

// ============================================================================
// Static variables
//

bool LLHunspellWrapper::s_fSpellCheck = false;

static const std::string c_strDictCustomSuffix = "_custom";
static const std::string c_strDictIgnoreSuffix = "_ignore";

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
		sdDict["has_custom"] = (!strTempPath.empty()) && (gDirUtilp->fileExists(strTempPath + c_strDictCustomSuffix + ".dic"));
		sdDict["has_ignore"] = (!strTempPath.empty()) && (gDirUtilp->fileExists(strTempPath + c_strDictIgnoreSuffix + ".dic"));
	}
}

LLHunspellWrapper::~LLHunspellWrapper()
{
	delete m_pHunspell;
}

// Checked: 2010-12-23 (Catznip-2.5.0a) | Added: Catznip-2.5.0a
bool LLHunspellWrapper::checkSpelling(const std::string& strWord) const
{
	if ( (!s_fSpellCheck) || (!m_pHunspell) || (strWord.length() < 3) )
	{
		return true;
	}
	if (0 != m_pHunspell->spell(strWord.c_str()))
	{
		return true;
	}
	if (m_IgnoreList.size())
	{
		std::string strWordLower(strWord);
		LLStringUtil::toLower(strWordLower);
		return (std::find(m_IgnoreList.begin(), m_IgnoreList.end(), strWordLower) != m_IgnoreList.end());
	}
	return false;
}

S32 LLHunspellWrapper::getSuggestions(const std::string& strWord, std::vector<std::string>& strSuggestionList) const
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
		m_strDictionaryName = m_strDictionaryFile = "";
		m_IgnoreList.clear();
	}

	if ( (!useSpellCheck()) || (strDictionary.empty()) )
		return false;

	LLSD sdDictInfo;
	for (LLSD::array_const_iterator itDictInfo = m_sdDictionaryMap.beginArray(), endDictInfo = m_sdDictionaryMap.endArray();
			itDictInfo != endDictInfo; ++itDictInfo)
	{
		const LLSD& sdDict = *itDictInfo;
		if ( (sdDict["installed"].asBoolean()) && (strDictionary == sdDict["language"].asString()) )
			sdDictInfo = sdDict;
	}

	if (sdDictInfo.has("name"))
	{
		m_strDictionaryName = strDictionary;
		m_strDictionaryFile = sdDictInfo["name"].asString();

		std::string strPathAff = m_strDictionaryPath + m_strDictionaryFile + ".aff";
		std::string strPathDic = m_strDictionaryPath + m_strDictionaryFile + ".dic";
		m_pHunspell = new Hunspell(strPathAff.c_str(), strPathDic.c_str());

		// Add the custom dictionary (if there is one)
		if (sdDictInfo["has_custom"].asBoolean())
		{
			std::string strPathCustomDic = m_strDictionaryPath + m_strDictionaryFile + c_strDictCustomSuffix + ".dic";
			m_pHunspell->add_dic(strPathCustomDic.c_str());
		}

		// Load the ignore list (if there is one)
		if (sdDictInfo["has_ignore"].asBoolean())
		{
			llifstream fileDictIgnore(m_strDictionaryPath + m_strDictionaryFile + c_strDictIgnoreSuffix + ".dic", std::ios::in);
			if (fileDictIgnore.is_open())
			{
				std::string strWord; int idxLine = 0;
				while (getline(fileDictIgnore, strWord))
				{
					// Skip over the first line since that's just a line count
					if (0 != idxLine)
					{
						LLStringUtil::toLower(strWord);
						m_IgnoreList.push_back(strWord);
					}
					idxLine++;
				}
			}
		}
	}

	return (NULL != m_pHunspell);
}

// Checked: 2010-12-23 (Catznip-2.5.0a) | Added: Catznip-2.5.0a
void LLHunspellWrapper::addToCustomDictionary(const std::string& strWord)
{
	if (m_pHunspell)
		m_pHunspell->add(strWord.c_str());
	addToDictFile(m_strDictionaryPath + m_strDictionaryFile + c_strDictCustomSuffix + ".dic", strWord);
}

// Checked: 2010-12-23 (Catznip-2.5.0a) | Added: Catznip-2.5.0a
void LLHunspellWrapper::addToIgnoreList(const std::string& strWord)
{
	std::string strWordLower(strWord);
	LLStringUtil::toLower(strWordLower);
	if (std::find(m_IgnoreList.begin(), m_IgnoreList.end(), strWordLower) == m_IgnoreList.end())
	{
		m_IgnoreList.push_back(strWordLower);
		addToDictFile(m_strDictionaryPath + m_strDictionaryFile + c_strDictIgnoreSuffix + ".dic", strWordLower);
	}
}

// Checked: 2010-12-23 (Catznip-2.5.0a) | Added: Catznip-2.5.0a
void LLHunspellWrapper::addToDictFile(const std::string& strDictPath, const std::string& strWord)
{
	// TODO-Catznip: has to be a better way to add one word to the end and increment the line count?
	std::vector<std::string> wordList;

	// Read any existing words
	if (gDirUtilp->fileExists(strDictPath))
	{
		llifstream fileDictIn(strDictPath, std::ios::in);
		if (fileDictIn.is_open())
		{
			std::string strWord; int idxLine = 0;
			while (getline(fileDictIn, strWord))
			{
				// Skip over the first line since that's just a line count
				if (0 != idxLine)
					wordList.push_back(strWord);
				idxLine++;
			}
		}
		else
		{
			// TODO-Catznip: show error message?
			return;
		}
	}

	// Add the new word to the end
	wordList.push_back(strWord);

	// Recreate the file with the new line count and new word added
	llofstream fileDictOut(strDictPath, std::ios::out | std::ios::trunc);	
	if (fileDictOut.is_open())
	{
		fileDictOut << wordList.size() << std::endl;
		for (std::vector<std::string>::const_iterator itWord = wordList.begin(); itWord != wordList.end(); ++itWord)
			fileDictOut << *itWord << std::endl;
		fileDictOut.close();
	}
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
