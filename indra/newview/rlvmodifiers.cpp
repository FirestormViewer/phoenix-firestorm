/**
 *
 * Copyright (c) 2009-2018, Kitty Barnett
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

#include "rlvmodifiers.h"

// ====================================================================================
// RlvBehaviourModifierAnimator
//

RlvBehaviourModifierAnimator::~RlvBehaviourModifierAnimator()
{
	if (!m_TimerHandle.isDead())
		m_TimerHandle.markDead();
}

void RlvBehaviourModifierAnimator::addTween(const LLUUID& idObject, ERlvBehaviourModifier eBhvrMod, RlvBehaviourModifierAnimationType eAnimType, const RlvBehaviourModifierValue& endValue, float nDuration)
{
	// Make sure we don't run two animations on the same modifier for the same object
	const auto itTween = std::find_if(m_Tweens.begin(), m_Tweens.end(), [&idObject, eBhvrMod](const RlvBehaviourModifierTween& t) { return t.idObject == idObject && t.eBhvrMod == eBhvrMod; });
	if (m_Tweens.end() != itTween)
		m_Tweens.erase(itTween);

	if (const RlvBehaviourModifier* pBhvrModifier = RlvBehaviourDictionary::instance().getModifier(eBhvrMod))
	{
		RlvBehaviourModifierTween newTween;
		newTween.idObject = idObject;
		newTween.eBhvrMod = eBhvrMod;
		newTween.eAnimType = RlvBehaviourModifierAnimationType::Lerp;
		newTween.nStartTime = LLTimer::getElapsedSeconds();
		newTween.nDuration = nDuration;
		newTween.startValue = pBhvrModifier->getValue();
		newTween.endValue = endValue;
		if (newTween.startValue.which() == newTween.endValue.which())
		{
			if (m_TimerHandle.isDead())
				m_TimerHandle = (new AnimationTimer())->getHandle();
			m_Tweens.emplace_back(std::move(newTween));
		}
	}
}

void RlvBehaviourModifierAnimator::clearTweens(const LLUUID& idObject, ERlvBehaviourModifier eBhvrMod)
{
	m_Tweens.erase(std::remove_if(m_Tweens.begin(), m_Tweens.end(),
	                              [&idObject, eBhvrMod](const RlvBehaviourModifierTween& cmpTween)
	                              {
		                              return cmpTween.idObject == idObject && ((cmpTween.eBhvrMod == eBhvrMod) || (RLV_MODIFIER_UNKNOWN == eBhvrMod));
	                              }), m_Tweens.end());
}

// ====================================================================================
// RlvBehaviourModifierAnimator timer
//

RlvBehaviourModifierAnimator::AnimationTimer::AnimationTimer()
	: LLEventTimer(1.f / RLV_MODIFIER_ANIMATION_FREQUENCY)
{
}


BOOL RlvBehaviourModifierAnimator::AnimationTimer::tick()
{
	RlvBehaviourModifierAnimator& modAnimatior = RlvBehaviourModifierAnimator::instance();
	const double curTime = LLTimer::getElapsedSeconds();

	const auto activeTweens = modAnimatior.m_Tweens;
	for (const auto& curTween : activeTweens)
	{
		if (RlvBehaviourModifier* pBhvrModifier = RlvBehaviourDictionary::instance().getModifier(curTween.eBhvrMod))
		{
			// Update the modifier's value
			float curFactor = (curTime - curTween.nStartTime) / curTween.nDuration;
			if (curFactor < 1.0)
			{
				const auto& valueType = curTween.startValue.type();
				if (typeid(float) == valueType)
					pBhvrModifier->setValue(lerp(boost::get<float>(curTween.startValue), boost::get<float>(curTween.endValue), curFactor), curTween.idObject);
				else if (typeid(int) == valueType)
					pBhvrModifier->setValue(lerp(boost::get<int>(curTween.startValue), boost::get<int>(curTween.endValue), curFactor), curTween.idObject);
				else if (typeid(LLVector3) == valueType)
					pBhvrModifier->setValue(lerp(boost::get<LLVector3>(curTween.startValue), boost::get<LLVector3>(curTween.endValue), curFactor), curTween.idObject);
			}
			else
			{
				pBhvrModifier->setValue(curTween.endValue, curTween.idObject);
				auto itTween = std::find_if(modAnimatior.m_Tweens.begin(), modAnimatior.m_Tweens.end(),
											[&curTween](const RlvBehaviourModifierTween& t)
											{
												// NOTE: implementation leak - taking advantage of the fact that we know there can only be one active tween per object/modifier/type combination
												return t.idObject == curTween.idObject && t.eBhvrMod == curTween.eBhvrMod && t.eAnimType == curTween.eAnimType;
											});
				modAnimatior.m_Tweens.erase(itTween);
			}
		}
	}

	return modAnimatior.m_Tweens.empty();
}

 // ====================================================================================
