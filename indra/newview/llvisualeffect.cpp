/**
 *
 * Copyright (c) 2021, Kitty Barnett
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

#include "llviewerprecompiledheaders.h"

#include "llvisualeffect.h"

#include <boost/iterator/filter_iterator.hpp>
#include <boost/range/iterator_range.hpp>

// ============================================================================
// LLTweenableValue class
//

template<>
float LLTweenableValueLerp<float>::get()
{
	if (!m_CurValue)
	{
		float curFactor = (LLTimer::getElapsedSeconds() - m_StartTime) / m_Duration;
		if (curFactor < 1.0)
			return lerp(m_StartValue, m_EndValue, curFactor);
		m_CurValue = m_EndValue;
	}
	return m_CurValue.get();
}

template<>
LLColor3 LLTweenableValueLerp<LLColor3>::get()
{
	if (!m_CurValue)
	{
		float curFactor = (LLTimer::getElapsedSeconds() - m_StartTime) / m_Duration;
		if (curFactor < 1.0)
			return lerp(m_StartValue, m_EndValue, curFactor);
		m_CurValue = m_EndValue;
	}
	return m_CurValue.get();
}

template<>
LLVector4 LLTweenableValueLerp<LLVector4>::get()
{
	if (!m_CurValue)
	{
		float curFactor = (LLTimer::getElapsedSeconds() - m_StartTime) / m_Duration;
		if (curFactor < 1.0)
			return lerp(m_StartValue, m_EndValue, curFactor);
		m_CurValue = m_EndValue;
	}
	return m_CurValue.get();
}

// ============================================================================
// LLVfxManager class
//

LLVfxManager::LLVfxManager()
{

}

bool LLVfxManager::addEffect(LLVisualEffect* pEffectInst)
{
	if (m_Effects.end() != m_Effects.find(pEffectInst))
		return false;

	m_Effects.insert(pEffectInst);

	return true;
}

LLVisualEffect* LLVfxManager::getEffect(const LLUUID& idEffect) const
{
	auto itEffect = std::find_if(m_Effects.begin(), m_Effects.end(), [&idEffect](const LLVisualEffect* pEffect) { return pEffect->getId() == idEffect; });
	return (m_Effects.end() != itEffect) ? *itEffect : nullptr;
}

LLVisualEffect* LLVfxManager::getEffect(EVisualEffect eCode) const
{
	// NOTE: returns the first found but there could be more
	auto itEffect = std::find_if(m_Effects.begin(), m_Effects.end(), [eCode](const LLVisualEffect* pEffect) { return pEffect->getCode() == eCode; });
	return (m_Effects.end() != itEffect) ? *itEffect : nullptr;
}

bool LLVfxManager::removeEffect(const LLUUID& idEffect)
{
	auto itEffect = std::find_if(m_Effects.begin(), m_Effects.end(), [&idEffect](const LLVisualEffect* pEffect) { return pEffect->getId() == idEffect; });
	if (m_Effects.end() == itEffect)
		return false;

	delete *itEffect;
	m_Effects.erase(itEffect);
	return true;
}

void LLVfxManager::runEffect(EVisualEffect eCode, const LLVisualEffectParams* pParams)
{
	// *TODO-Catz: once we're done, check whether iterating over the entire list still has negliable impact
	auto pred = [eCode](const LLVisualEffect* pEffect) { return pEffect->getCode() == eCode; };

	auto beginEffect = boost::make_filter_iterator(pred, m_Effects.begin(), m_Effects.end()),
	     endEffect = boost::make_filter_iterator(pred, m_Effects.end(), m_Effects.end());

	auto effectRange = boost::make_iterator_range(beginEffect, endEffect);
	for (LLVisualEffect* pEffect : effectRange)
	{
		pEffect->run(pParams);
	}
}

void LLVfxManager::runEffect(EVisualEffectType eType, const LLVisualEffectParams* pParams)
{
	// *TODO-Catz: once we're done, check whether iterating over the entire list still has negliable impact
	auto pred = [eType](const LLVisualEffect* pEffect) { return pEffect->getType() == eType;  };

	auto beginEffect = boost::make_filter_iterator(pred, m_Effects.begin(), m_Effects.end()),
		endEffect = boost::make_filter_iterator(pred, m_Effects.end(), m_Effects.end());

	auto effectRange = boost::make_iterator_range(beginEffect, endEffect);
	for (LLVisualEffect* pEffect : effectRange)
	{
		pEffect->run(pParams);
	}
}

// ============================================================================
