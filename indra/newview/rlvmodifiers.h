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

#include "llhandle.h"
#include "llsingleton.h"
#include "rlvhelper.h"

// ====================================================================================
// RlvBehaviourModifierHandler - Behaviour modifiers change handler class (use this if changes require code to be processed)
//

template<ERlvBehaviourModifier eBhvrMod>
class RlvBehaviourModifierHandler : public RlvBehaviourModifier
{
public:
    //using RlvBehaviourModifier::RlvBehaviourModifier; // Needs VS2015 and up
    RlvBehaviourModifierHandler(const std::string& strName, const RlvBehaviourModifierValue& defaultValue, bool fAddDefaultOnEmpty, RlvBehaviourModifierComp* pValueComparator)
        : RlvBehaviourModifier(strName, defaultValue, fAddDefaultOnEmpty, pValueComparator) {}
protected:
    void onValueChange() const override;
};

// ====================================================================================
// RlvBehaviourModifierComp - Behaviour modifiers comparers (used for sorting)
//

struct RlvBehaviourModifierComp
{
    virtual ~RlvBehaviourModifierComp() {}
    virtual bool operator()(const RlvBehaviourModifierValueTuple& lhs, const RlvBehaviourModifierValueTuple& rhs)
    {
        // Values that match the primary object take precedence (otherwise maintain relative ordering)
        if ( (std::get<1>(rhs) == m_idPrimaryObject) && (std::get<1>(lhs) != m_idPrimaryObject) )
            return false;
        return true;
    }

    LLUUID m_idPrimaryObject;
};

struct RlvBehaviourModifierCompMin : public RlvBehaviourModifierComp
{
    bool operator()(const RlvBehaviourModifierValueTuple& lhs, const RlvBehaviourModifierValueTuple& rhs) override
    {
        if ( (m_idPrimaryObject.isNull()) || ((std::get<1>(lhs) == m_idPrimaryObject) && (std::get<1>(rhs) == m_idPrimaryObject)) )
            return std::get<0>(lhs) < std::get<0>(rhs);
        return RlvBehaviourModifierComp::operator()(lhs, rhs);
    }
};

struct RlvBehaviourModifierCompMax : public RlvBehaviourModifierComp
{
    bool operator()(const RlvBehaviourModifierValueTuple& lhs, const RlvBehaviourModifierValueTuple& rhs) override
    {
        if ( (m_idPrimaryObject.isNull()) || ((std::get<1>(lhs) == m_idPrimaryObject) && (std::get<1>(rhs) == m_idPrimaryObject)) )
            return std::get<0>(rhs) < std::get<0>(lhs);
        return RlvBehaviourModifierComp::operator()(lhs, rhs);
    }
};

// ====================================================================================
// RlvCachedBehaviourModifier - Provides an optimized way to access a modifier that's frequently accessed and rarely updated
//

// Inspired by LLControlCache<T>
template<typename T>
class RlvBehaviourModifierCache : public LLRefCount, public LLInstanceTracker<RlvBehaviourModifierCache<T>, ERlvBehaviourModifier>
{
public:
    RlvBehaviourModifierCache(ERlvBehaviourModifier eModifier)
        : LLInstanceTracker<RlvBehaviourModifierCache<T>, ERlvBehaviourModifier>(eModifier)
    {
        if (RlvBehaviourModifier* pBhvrModifier = RlvBehaviourDictionary::instance().getModifier(eModifier))
        {
            mConnection = pBhvrModifier->getSignal().connect(boost::bind(&RlvBehaviourModifierCache<T>::handleValueChange, this, _1));
            mCachedValue = pBhvrModifier->getValue<T>();
        }
        else
        {
            mCachedValue = {};
            RLV_ASSERT(false);
        }
    }
    ~RlvBehaviourModifierCache() {}

    /*
     * Member functions
     */
public:
    const T& getValue() const { return mCachedValue; }
protected:
    void handleValueChange(const RlvBehaviourModifierValue& newValue) { mCachedValue = boost::get<T>(newValue); }

    /*
     * Member variables
     */
protected:
    T mCachedValue;
    boost::signals2::scoped_connection mConnection;
};

// Inspired by LLCachedControl<T>
template <typename T>
class RlvCachedBehaviourModifier
{
public:
    RlvCachedBehaviourModifier(ERlvBehaviourModifier eModifier)
    {
        if ((mCachedModifierPtr = RlvBehaviourModifierCache<T>::getInstance(eModifier).get()) == nullptr)
            mCachedModifierPtr = new RlvBehaviourModifierCache<T>(eModifier);
    }

    /*
     * Operators
     */
public:
    operator const T&() const { return mCachedModifierPtr->getValue(); }
    const T& operator()()     { return mCachedModifierPtr->getValue(); }

    /*
     * Member variables
     */
protected:
    LLPointer<RlvBehaviourModifierCache<T>> mCachedModifierPtr;
};

// ====================================================================================
