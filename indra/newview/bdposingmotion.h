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
    /// <summary>
    /// Unsure why it is special. Perhaps its a low-priority animation? Perhaps it just needs to be unique?
    /// </summary>
    const LLUUID ANIM_BD_POSING_MOTION = LLUUID("fd29b117-9429-09c4-10cb-933d0b2ab653");

    BDPosingMotion(const LLUUID &id);
    virtual ~BDPosingMotion();

public:
    //-------------------------------------------------------------------------
    // functions to support MotionController and MotionRegistry
    //-------------------------------------------------------------------------

    // static constructor
    // all subclasses must implement such a function and register it
    static LLMotion *create(const LLUUID &id) { return new BDPosingMotion(id); }

public:
    virtual bool getLoop() { return TRUE; }

    virtual F32 getDuration() { return 0.0; }

    virtual F32 getEaseInDuration() { return 0.0f; }

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
    virtual bool onUpdate(F32 time, U8 *joint_mask);

    // called when a motion is deactivated
    virtual void onDeactivate();

    /// <summary>
    /// Adds the supplied joint to the current animation-state.
    /// </summary>
    /// <param name="joint">The joint to animate.</param>
    void addJointToState(LLJoint *joint);

    /// <summary>
    /// Removes the supplied joint to the current animation-state.
    /// </summary>
    /// <param name="joint">The joint to stop animating.</param>
    void removeJointFromState(LLJoint *joint);

    void setJointState(LLJoint *joint, U32 state);

    /// <summary>
    /// Queries whether the supplied joint is being animated.
    /// </summary>
    /// <param name="joint">The joint to query.</param>
    bool currentlyPosingJoint(LLJoint *joint);

private:
    static const S32 _numberOfBonesApropoOfNothing = 134;

    // BD - functions to set/get our interpolation type
    //      0 = None,
    //      1 = Linear Interpolatioon,
    //      2 = Spherical Linear Interpolation,
    //      3 = Curve
    S32          mInterpolationType;
    F32          mInterpolationTime;
    LLFrameTimer mInterpolationTimer;

    LLCharacter *mCharacter;

    LLPointer<LLJointState> mJointState[_numberOfBonesApropoOfNothing];

    LLJoint *mTargetJoint;
};

#endif // LL_LLKEYFRAMEMOTION_H

