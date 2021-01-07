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

#pragma once

#include "llsingleton.h"

// ============================================================================
//
//

enum class EVisualEffect
{
	RlvOverlay,
	RlvSphere,
};

enum class EVisualEffectType
{
	PostProcessShader,
	Custom,
};

// ============================================================================
//
//

class LLVisualEffect
{
public:
	LLVisualEffect(LLUUID id, EVisualEffect eCode, EVisualEffectType eType)
		: m_id(id), m_eCode(eCode), m_eType(eType)
	{}
	virtual ~LLVisualEffect() {}

	EVisualEffect     getCode() const     { return m_eCode;}
	const LLUUID&     getId() const       { return m_id;}
	U32               getPriority() const { return m_nPriority; }
	EVisualEffectType getType() const     { return m_eType;}

	virtual void      run() = 0;

	/*
	 * Member variables
	 */
protected:
	LLUUID            m_id;
	EVisualEffect     m_eCode;
	EVisualEffectType m_eType;
	U32               m_nPriority;
};

// ============================================================================
//
//

enum class LLTweenType
{
	None,
	Lerp,
};

template<typename T>
class LLTweenableValue
{
public:
	LLTweenableValue(const T& defaultValue) : m_CurValue(defaultValue) {}
	virtual ~LLTweenableValue() {}

	virtual T    get() = 0;
	virtual void start(const T& endValue, double duration) = 0;

	T& operator =(const T& value) { m_CurValue = value; }

	/*
	 * Member variables
	 */
protected:
	boost::optional<T> m_CurValue;
};

template<typename T>
class LLTweenableValueLerp : public LLTweenableValue<T>
{
public:
	LLTweenableValueLerp(const T& defaultValue) : LLTweenableValue(defaultValue) {}

	T    get() override;
	void start(const T& endValue, double duration) override
	{
		m_StartValue = get();
		m_CurValue = boost::none;
		m_EndValue = endValue;

		m_StartTime = LLTimer::getElapsedSeconds();
		m_Duration = duration;
	}

	/*
	 * Member variables
	 */
protected:
	double m_StartTime;
	double m_Duration;
	T      m_StartValue;
	T      m_EndValue;
};

// ============================================================================
//
//

class LLVfxManager : public LLSingleton<LLVfxManager>
{
	LLSINGLETON(LLVfxManager);
protected:
	~LLVfxManager() {}

	/*
	 * Member functions
	 */
public:
	bool            addEffect(LLVisualEffect* pEffectInst);
	LLVisualEffect* getEffect(const LLUUID& idEffect) const;
	template<typename T> T* getEffect(const LLUUID& idEffect) const { return dynamic_cast<T*>(getEffect(idEffect)); }
	LLVisualEffect* getEffect(EVisualEffect eCode) const;
	template<typename T> T* getEffect(EVisualEffect eCode) const { return dynamic_cast<T*>(getEffect(eCode)); }
	bool            removeEffect(const LLUUID& idEffect);
	void            runEffect(EVisualEffect eCode);
	void            runEffect(EVisualEffectType eType);
protected:

	/*
	 * Member variables
	 */
protected:
	std::set<LLVisualEffect*> m_Effects;
};

// ============================================================================
