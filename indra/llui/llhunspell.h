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
#include <boost/signals2.hpp>

class Hunspell;

// ============================================================================

class LLHunspellWrapper : public LLSingleton<LLHunspellWrapper>
{
	friend class LLSingleton<LLHunspellWrapper>;
protected:
	LLHunspellWrapper();
	~LLHunspellWrapper();

public:
	bool	checkSpelling(const std::string& strWord) const;
	S32		getSuggestions(const std::string& strWord, std::vector<std::string>& strSuggestionList) const;

	/*
	 * Dictionary related functions
	 */
public:
	const std::string	getCurrentDictionary() const					{ return m_strDictionaryName; }
	S32					getDictionaries(std::vector<std::string>& strDictionaryList) const;
	S32					getInstalledDictionaries(std::vector<std::string>& strDictionaryList) const;

	void				addToCustomDictionary(const std::string& strWord);
	void				addToIgnoreList(const std::string& strWord);
protected:
	void				addToDictFile(const std::string& strDictPath, const std::string& strWord);
	bool				setCurrentDictionary(const std::string& strDictionary);

	/*
	 * Event callbacks
	 */
public:
	typedef boost::signals2::signal<void()> settings_change_signal_t;
	static boost::signals2::connection setSettingsChangeCallback(const settings_change_signal_t::slot_type& cb);

	/*
	 * Static member functions
	 */
public:
	static bool useSpellCheck();
	static void setUseSpellCheck(const std::string& strDictionary);

	/*
	 * Member variables
	 */
protected:
	Hunspell*					m_pHunspell;
	std::string					m_strDictionaryName;
	std::string					m_strDictionaryFile;
	std::string					m_strDictionaryPath;
	LLSD						m_sdDictionaryMap;
	std::vector<std::string>	m_IgnoreList;

	static settings_change_signal_t	s_SettingsChangeSignal;
};

// ============================================================================

#endif // LLHUNSPELL_H
