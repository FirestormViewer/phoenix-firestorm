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

#include "linden_common.h"
#include "llviewerprecompiledheaders.h"
#include "bdposingmotion.h"
#include "llcharacter.h"
#include "llviewercontrol.h"

BDPosingMotion::BDPosingMotion(const LLUUID &id) :
LLMotion(id),
    mCharacter(NULL),
    mTargetJoint(NULL)
{
    mName = "custom_pose";
    //BD - Use slight spherical linear interpolation by default.
    mInterpolationTime = 0.25f;
    mInterpolationType = 2;

    for (auto& entry : mJointState)
        entry = new LLJointState;
}

BDPosingMotion::~BDPosingMotion() {}

LLMotion::LLMotionInitStatus BDPosingMotion::onInitialize(LLCharacter *character)
{
    // save character for future use
    mCharacter = character;

    for (S32 i = 0; (mTargetJoint = mCharacter->getCharacterJoint(i)); ++i) // this seems poorly constrained
    {
        mJointState[i]->setJoint(mTargetJoint);
        mJointState[i]->setUsage(LLJointState::POS | LLJointState::ROT /* | LLJointState::SCALE*/);

        addJointState(mJointState[i]);
    }

    return STATUS_SUCCESS;
}

bool BDPosingMotion::onActivate()
{
    for (auto joint_state : mJointState)
    {
        LLJoint* joint = joint_state->getJoint();
        if (!joint)
            continue;

        joint->setTargetRotation(joint->getRotation());
        joint->setTargetPosition(joint->getPosition());
    }

    return true;
}

bool BDPosingMotion::onUpdate(F32 time, U8* joint_mask)
{
    LLQuaternion target_quat;
    LLQuaternion joint_quat;
    LLQuaternion last_quat;
    LLQuaternion next_quat;
    LLVector3 joint_pos;
    LLVector3 last_pos;
    LLVector3 target_pos;
    LLVector3 next_pos;
    F32 perc = 0.0f;

    for (auto joint_state : mJointState)
    {
        LLJoint* joint = joint_state->getJoint();
        if (!joint)
            continue;

        target_quat = joint->getTargetRotation();
        joint_quat = joint->getRotation();
        last_quat = joint->getLastRotation();

        //BD - Merge these two together?
        perc = llclamp(mInterpolationTimer.getElapsedTimeF32() / mInterpolationTime, 0.0f, 1.0f);
        //BD - All bones support positions now.
        joint_pos = joint->getPosition();
        target_pos = joint->getTargetPosition();
        last_pos = joint->getLastPosition();
        if (target_pos != joint_pos)
        {
            if (mInterpolationType == 2)
            {
                //BD - Do spherical linear interpolation.
                //     We emulate the spherical linear interpolation here because
                //     slerp() does not support LLVector3. mInterpolationTime is always
                //     in a range between 0.00 and 1.00 which makes it perfect to use
                //     as percentage directly.
                //     We use the current joint position rather than the original like
                //     in linear interpolation to take a fraction of the fraction, this
                //     re-creates spherical linear interpolation's behavior.
                joint_pos = lerp(joint_pos, target_pos, mInterpolationTime);
            }
            else if (mInterpolationType == 3)
            {
                next_pos = joint->getNextPosition();
                //BD - Do curve interpolation.
                //     This is a special kind of interpolation where we interpolate towards
                //     a "middle" pose to a given degree while on our way to the actual final
                //     pose.
                joint_pos = lerp(joint_pos, next_pos, perc);
                joint_pos = lerp(joint_pos, target_pos, 0.5f - abs(0.5f - perc));
            }
            else
            {
                if (perc >= 1.0f)
                {
                    //BD - Can be used to do no interpolation too.
                    joint_pos = target_pos;
                    last_pos = joint_pos;
                }
                else
                {
                    //BD - Do linear interpolation.
                    joint_pos = lerp(last_pos, target_pos, perc);
                }
            }
            joint_state->setPosition(joint_pos);
        }

        if (target_quat != joint_quat)
        {
            if (mInterpolationType == 2)
            {
                //BD - Do spherical linear interpolation.
                joint_quat = slerp(mInterpolationTime, joint_quat, target_quat);
            }
            else if (mInterpolationType == 3)
            {
                next_pos = joint->getNextPosition();
                //BD - Do curve interpolation.
                //     This is a special kind of interpolation where we interpolate towards
                //     a "middle" pose to a given degree while on our way to the actual final
                //     pose.
                joint_quat = lerp(perc, joint_quat, next_quat);
                joint_quat = lerp(0.5f - abs(0.5f - perc), joint_quat, target_quat);
            }
            else
            {
                if (perc >= 1.0f)
                {
                    //BD - Can be used to do no interpolation too.
                    joint_quat = target_quat;
                    last_quat = joint_quat;
                }
                else
                {
                    //BD - Do linear interpolation.
                    joint_quat = lerp(perc, last_quat, target_quat);
                }
            }
            joint_state->setRotation(joint_quat);
        }
    }

    if (perc >= 1.0f && mInterpolationTimer.getStarted()
        && (last_quat == joint_quat
        && (last_pos == joint_pos)))
    {
        mInterpolationTimer.stop();
    }

    return true;
}

void BDPosingMotion::onDeactivate() {}

/// <summary>
/// Adds the supplied joint to the current animation.
/// </summary>
void BDPosingMotion::addJointToState(LLJoint *joint) { setJointState(joint, LLJointState::POS | LLJointState::ROT); }

void BDPosingMotion::removeJointFromState(LLJoint *joint) { setJointState(joint, 0); }

void BDPosingMotion::setJointState(LLJoint *joint, U32 state)
{
    if (!joint)
        return;

    S32 jointNumber = joint->getJointNum();
    if (jointNumber < 0 || jointNumber >= _numberOfBonesApropoOfNothing)
        return;

    mJointState[jointNumber]->setJoint(joint);
    mJointState[jointNumber]->setUsage(state);

    addJointState(mJointState[jointNumber]);
}

bool BDPosingMotion::currentlyPosingJoint(LLJoint* joint)
{
    if (!joint)
        return false;

    S32 jointNumber = joint->getJointNum();
    if (jointNumber < 0 || jointNumber >= _numberOfBonesApropoOfNothing)
        return false;

    U32 state = mJointState[jointNumber]->getUsage();

    return (state & LLJointState::POS) && (state & LLJointState::ROT);
}
