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

// ============================================================================

void LLHunspellWrapper::setUseSpellCheck(bool fSpellCheck)
{
	s_fSpellCheck = fSpellCheck;

	std::string strPathAff = s_strDictPath + "en_GB.aff";
	std::string strPathDic = s_strDictPath + "en_GB.dic";

	LLHunspellWrapper::instance().m_pHunspell = new Hunspell(strPathAff.c_str(), strPathDic.c_str());
}

// ============================================================================
