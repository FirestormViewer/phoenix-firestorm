/**
*
* Copyright (C) 2018, NiranV Dean
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation;
* version 2.1 of the License only.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
*/

#ifndef LL_BDPOSINGMOTION_H
#define LL_BDPOSINGMOTION_H

//-----------------------------------------------------------------------------
// Header files
//-----------------------------------------------------------------------------
#include "llmotion.h"
#include "lljointsolverrp3.h"
#include "v3dmath.h"

#define MIN_REQUIRED_PIXEL_AREA_POSING 500.f

//-----------------------------------------------------------------------------
// class BDPosingMotion
//-----------------------------------------------------------------------------
class BDPosingMotion :
    public LLMotion
{
public:
    // Constructor
    BDPosingMotion(const LLUUID &id);

    // Destructor
    virtual ~BDPosingMotion();

public:
    //-------------------------------------------------------------------------
    // functions to support MotionController and MotionRegistry
    //-------------------------------------------------------------------------

    // static constructor
    // all subclasses must implement such a function and register it
    static LLMotion *create(const LLUUID &id) { return new BDPosingMotion(id); }

public:
    //-------------------------------------------------------------------------
    // animation callbacks to be implemented by subclasses
    //-------------------------------------------------------------------------

    // motions must specify whether or not they loop
    virtual bool getLoop() { return TRUE; }

    // motions must report their total duration
    virtual F32 getDuration() { return 0.0; }

    // motions must report their "ease in" duration
    virtual F32 getEaseInDuration() { return 0.0f; }

    // motions must report their "ease out" duration.
    virtual F32 getEaseOutDuration() { return 0.5f; }

    // motions must report their priority
    virtual LLJoint::JointPriority getPriority() { return LLJoint::ADDITIVE_PRIORITY; }

    virtual LLMotionBlendType getBlendType() { return NORMAL_BLEND; }

    // called to determine when a motion should be activated/deactivated based on avatar pixel coverage
    virtual F32 getMinPixelArea() { return MIN_REQUIRED_PIXEL_AREA_POSING; }

    // run-time (post constructor) initialization,
    // called after parameters have been set
    // must return true to indicate success and be available for activation
    virtual LLMotionInitStatus onInitialize(LLCharacter *character);

    // called when a motion is activated
    // must return TRUE to indicate success, or else
    // it will be deactivated
    virtual bool onActivate();

    // called per time step
    // must return TRUE while it is active, and
    // must return FALSE when the motion is completed.
    virtual bool onUpdate(F32 time, U8* joint_mask);

    // called when a motion is deactivated
    virtual void onDeactivate();

    void addJointToState(LLJoint *joint);

public:
    //-------------------------------------------------------------------------
    // Joint States
    //-------------------------------------------------------------------------
    LLCharacter         *mCharacter;

    LLPointer<LLJointState> mJointState[134];

    //-------------------------------------------------------------------------
    // Joints
    //-------------------------------------------------------------------------
    LLJoint*            mTargetJoint;
};

#endif // LL_LLKEYFRAMEMOTION_H

