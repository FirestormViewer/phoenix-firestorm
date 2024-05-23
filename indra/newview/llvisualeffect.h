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
#include <boost/optional.hpp>
#include <boost/none.hpp>

// ============================================================================
//
//

class LLRenderTarget;

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

struct LLVisualEffectParams
{
    virtual void step(bool isLast) = 0;
};

struct LLShaderEffectParams : LLVisualEffectParams
{
    explicit LLShaderEffectParams(LLRenderTarget* pSrcBuffer, LLRenderTarget* pScratchBuffer, bool fBindLast) : m_pSrcBuffer(pScratchBuffer), m_pDstBuffer(pSrcBuffer), m_fBindLast(fBindLast) {}

    void step(bool isLast) override
    {
        LLRenderTarget* pPrevSrc = m_pSrcBuffer, *pPrevDst = m_pDstBuffer;
        m_pSrcBuffer = pPrevDst;
        m_pDstBuffer = (!isLast || !m_fBindLast) ? pPrevSrc : nullptr;
    }

    LLRenderTarget* m_pSrcBuffer = nullptr;
    LLRenderTarget* m_pDstBuffer = nullptr;
    bool            m_fBindLast = false;
};

// ============================================================================
//
//

class LLVisualEffect
{
    friend class LLVfxManager;
public:
    LLVisualEffect(LLUUID id, EVisualEffect eCode, EVisualEffectType eType)
        : m_id(id), m_eCode(eCode), m_eType(eType)
    {}
    virtual ~LLVisualEffect() {}

    EVisualEffect     getCode() const     { return m_eCode;}
    bool              getEnabled() const  { return m_fEnabled; }
    const LLUUID&     getId() const       { return m_id;}
    U32               getPriority() const { return m_nPriority; }
    EVisualEffectType getType() const     { return m_eType;}
    void              setEnabled(bool enable) { m_fEnabled = enable; }

    virtual void      run(const LLVisualEffectParams* pParams) = 0;

protected:
    void              setPriority(U32 priority) { m_nPriority = priority; }

    /*
     * Member variables
     */
protected:
    LLUUID            m_id;
    EVisualEffect     m_eCode;
    EVisualEffectType m_eType;
    bool              m_fEnabled = true;
    U32               m_nPriority = 0;
};

// ============================================================================
//
//

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
    LLTweenableValueLerp(const T& defaultValue) : LLTweenableValue<T>(defaultValue) {}
    T    get() override;
    void start(const T& endValue, double duration) override
    {
        m_StartValue = get();
        this->m_CurValue = boost::none;
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
    LLVisualEffect* getEffect(EVisualEffect eCode, const LLUUID& idEffect) const;
    template<typename T> T* getEffect(const LLUUID& idEffect) const { return dynamic_cast<T*>(getEffect(T::EffectCode, idEffect)); }
    bool            getEffects(std::list<LLVisualEffect*>& effectList, std::function<bool(const LLVisualEffect*)> fnFilter);
    template<typename T> bool getEffects(std::list<T*>& effectList);
    bool            hasEffect(EVisualEffect eCode) const;
    bool            removeEffect(EVisualEffect eCode, const LLUUID& idEffect);
    template<typename T> bool removeEffect(const LLUUID& idEffect) { return removeEffect(T::EffectCode, idEffect); }
    void            runEffect(EVisualEffect eCode, LLVisualEffectParams* pParams = nullptr);
    void            runEffect(EVisualEffectType eType, LLVisualEffectParams* pParams = nullptr);
    void            updateEffect(LLVisualEffect* pEffect, bool fEnabled, U32 nPriority);
protected:
    void            runEffect(std::function<bool(const LLVisualEffect*)> fnFilter, LLVisualEffectParams* pParams);
    static bool     cmpEffect(const LLVisualEffect* pLHS, const LLVisualEffect* pRHS);

    /*
     * Member variables
     */
protected:
    std::vector<LLVisualEffect*> m_Effects;
};

// ============================================================================
// Inlined member functions
//

inline bool LLVfxManager::hasEffect(EVisualEffect eCode) const
{
    return m_Effects.end() != std::find_if(m_Effects.begin(), m_Effects.end(), [eCode](const LLVisualEffect* pEffect) { return pEffect->getCode() == eCode; });
}

template<typename T>
inline bool LLVfxManager::getEffects(std::list<T*>& effectList)
{
    effectList.clear();

    std::function<bool(const LLVisualEffect*)> fnFilter = [](const LLVisualEffect* pEffect) { return pEffect->getCode() == T::EffectCode; };
    auto itEffect = boost::make_filter_iterator(fnFilter, m_Effects.begin(), m_Effects.end()),
         endEffect = boost::make_filter_iterator(fnFilter, m_Effects.end(), m_Effects.end());
    while (itEffect != endEffect)
    {
        if (T* pEffect = dynamic_cast<T*>(*itEffect++))
        {
            effectList.push_back(pEffect);
        }
    }

    return effectList.size();
}

// ============================================================================
