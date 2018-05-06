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

#pragma once

#include "llsingleton.h"
#include "rlvhelper.h"

// ====================================================================================
// RlvBehaviourModifierAnimator - Helper types
//

enum class RlvBehaviourModifierAnimationType { Lerp };

struct RlvBehaviourModifierTween
{
	LLUUID idObject;
	ERlvBehaviourModifier eBhvrMod;
	RlvBehaviourModifierAnimationType eAnimType;
	double nStartTime;
	float nDuration;
	RlvBehaviourModifierValue startValue;
	RlvBehaviourModifierValue endValue;
};

// ====================================================================================
// RlvBehaviourModifierAnimator - A class to animate behaviour modifiers
//

class RlvBehaviourModifierAnimator : public LLSingleton<RlvBehaviourModifierAnimator>
{
	friend class AnimationTimer;
	LLSINGLETON_EMPTY_CTOR(RlvBehaviourModifierAnimator);
public:
	~RlvBehaviourModifierAnimator() override;

	/*
	 * Member functions
	 */
public:
	void addTween(const LLUUID& idObject, ERlvBehaviourModifier eBhvrMod, RlvBehaviourModifierAnimationType eAnimType, const RlvBehaviourModifierValue& endValue, float nDuration);
	void clearTweens(const LLUUID& idObject) { clearTweens(idObject, RLV_MODIFIER_UNKNOWN); }
	void clearTweens(const LLUUID& idObject, ERlvBehaviourModifier eBhvrMod);

	/*
	 * Animation timer
	 */
	class AnimationTimer : LLEventTimer
	{
	public:
		AnimationTimer();
		BOOL tick() override;
	};

	/*
	 * Member variables
	 */
	std::list<struct RlvBehaviourModifierTween> m_Tweens;
	AnimationTimer*                             m_pTimer = nullptr;
};

// ====================================================================================
