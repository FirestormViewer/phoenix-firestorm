/**
 * @file fsjointpose.cpp
 * @brief Container for the pose of a joint.
 *
 * $LicenseInfo:firstyear=2024&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2024 Angeldark Raymaker @ Second Life
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include <deque>
#include <boost/algorithm/string.hpp>
#include "fsposingmotion.h"
#include "llcharacter.h"

/// <summary>
/// The maximum length of the undo queue; adding new members preens older ones.
/// </summary>
constexpr size_t MaximumUndoQueueLength = 20;

/// <summary>
/// The constant time interval, in seconds, specifying whether an 'undo' value should be added.
/// </summary>
constexpr std::chrono::duration<double> UndoUpdateInterval = std::chrono::duration<double>(0.8);

FSJointPose::FSJointPose(LLJoint* joint, U32 usage, bool isCollisionVolume)
{
    mJointState = new LLJointState;
    mJointState->setJoint(joint);
    mJointState->setUsage(usage);

    mJointName         = joint->getName();
    mIsCollisionVolume = isCollisionVolume;

    mCurrentState   = FSJointState(joint);
}

void FSJointPose::setPublicPosition(const LLVector3& pos)
{
    addStateToUndo(FSJointState(mCurrentState));
    mCurrentState.mPosition.set(pos);
}

void FSJointPose::setPublicRotation(bool zeroBase, const LLQuaternion& rot)
{
    addStateToUndo(FSJointState(mCurrentState));

    if (zeroBase)
        zeroBaseRotation(true);

    mCurrentState.mRotation.set(rot);
}

void FSJointPose::setPublicScale(const LLVector3& scale)
{
    addStateToUndo(FSJointState(mCurrentState));
    mCurrentState.mScale.set(scale);
}

void FSJointPose::undoLastChange()
{
    mCurrentState = undoLastStateChange(FSJointState(mCurrentState));
}

void FSJointPose::redoLastChange()
{
    mCurrentState = redoLastStateChange(FSJointState(mCurrentState));
}

void FSJointPose::resetJoint()
{
    addStateToUndo(FSJointState(mCurrentState));
    mCurrentState.resetJoint();
}

void FSJointPose::addStateToUndo(FSJointState stateToAddToUndo)
{
    auto timeIntervalSinceLastChange = std::chrono::system_clock::now() - mTimeLastUpdatedCurrentState;
    mTimeLastUpdatedCurrentState     = std::chrono::system_clock::now();

    if (timeIntervalSinceLastChange < UndoUpdateInterval)
        return;

    if (mUndoneJointStatesIndex > 0)
    {
        for (size_t i = 0; i <= mUndoneJointStatesIndex; i++)
            if (!mLastSetJointStates.empty())
                mLastSetJointStates.pop_front();

        mUndoneJointStatesIndex = 0;
    }

    mLastSetJointStates.push_front(stateToAddToUndo);

    while (mLastSetJointStates.size() > MaximumUndoQueueLength)
        mLastSetJointStates.pop_back();
}

FSJointPose::FSJointState FSJointPose::undoLastStateChange(FSJointState thingToSet)
{
    if (mLastSetJointStates.empty())
        return thingToSet;

    if (mUndoneJointStatesIndex == 0)
        mLastSetJointStates.push_front(thingToSet);

    mUndoneJointStatesIndex += 1;
    mUndoneJointStatesIndex = llclamp(mUndoneJointStatesIndex, 0, mLastSetJointStates.size() - 1);

    return mLastSetJointStates.at(mUndoneJointStatesIndex);
}

FSJointPose::FSJointState FSJointPose::redoLastStateChange(FSJointState thingToSet)
{
    if (mLastSetJointStates.empty())
        return thingToSet;
    if (mUndoneJointStatesIndex == 0)
        return thingToSet;

    mUndoneJointStatesIndex -= 1;
    mUndoneJointStatesIndex = llclamp(mUndoneJointStatesIndex, 0, mLastSetJointStates.size() - 1);
    FSJointState result     = mLastSetJointStates.at(mUndoneJointStatesIndex);
    if (mUndoneJointStatesIndex == 0)
        mLastSetJointStates.pop_front();

    return result;
}

void FSJointPose::recaptureJoint()
{
    if (mIsCollisionVolume)
        return;

    LLJoint* joint = mJointState->getJoint();
    if (!joint)
        return;

    addStateToUndo(FSJointState(mCurrentState));
    mCurrentState = FSJointState(joint);
}

LLQuaternion FSJointPose::recaptureJointAsDelta(bool zeroBase)
{
    LLJoint* joint = mJointState->getJoint();
    if (!joint)
        return LLQuaternion::DEFAULT;

    addStateToUndo(FSJointState(mCurrentState));
    return mCurrentState.updateFromJoint(joint, zeroBase);
}

void FSJointPose::swapRotationWith(FSJointPose* oppositeJoint)
{
    if (!oppositeJoint)
        return;
    if (mIsCollisionVolume)
        return;

    auto tempState = FSJointState(mCurrentState);
    mCurrentState.cloneRotationFrom(oppositeJoint->mCurrentState);
    oppositeJoint->mCurrentState.cloneRotationFrom(tempState);
}

void FSJointPose::cloneRotationFrom(FSJointPose* fromJoint)
{
    if (!fromJoint)
        return;

    addStateToUndo(FSJointState(mCurrentState));
    mCurrentState.cloneRotationFrom(fromJoint->mCurrentState);
}

void FSJointPose::mirrorRotationFrom(FSJointPose* fromJoint)
{
    if (!fromJoint)
        return;

    cloneRotationFrom(fromJoint);
    mCurrentState.reflectRotation();
}

void FSJointPose::revertJoint()
{
    mCurrentState.revertJointToBase(mJointState->getJoint());
}

void FSJointPose::reflectRotation()
{
    if (mIsCollisionVolume)
        return;

    mCurrentState.reflectRotation();
}

void FSJointPose::zeroBaseRotation(bool lockInBvh)
{
    if (mIsCollisionVolume)
        return;

    mCurrentState.zeroBaseRotation();
    mCurrentState.mUserSpecifiedBaseZero = lockInBvh;
}

bool FSJointPose::isBaseRotationZero() const
{
    if (mIsCollisionVolume)
        return true;

    return mCurrentState.baseRotationIsZero();
}

void FSJointPose::purgeUndoQueue()
{
    if (mIsCollisionVolume)
        return;

    mUndoneJointStatesIndex = 0;
    mLastSetJointStates.clear();
}

bool FSJointPose::userHasSetBaseRotationToZero() const
{
    if (mIsCollisionVolume)
        return false;

    return mCurrentState.mUserSpecifiedBaseZero;
}

bool FSJointPose::getWorldRotationLockState() const
{
    if (mIsCollisionVolume)
        return false;

    return mCurrentState.mRotationIsWorldLocked;
}

void FSJointPose::setWorldRotationLockState(bool newState)
{
    if (mIsCollisionVolume)
        return;

    mCurrentState.mRotationIsWorldLocked = newState;
}

bool FSJointPose::canPerformUndo() const
{
    switch (mLastSetJointStates.size())
    {
        case 0: // nothing to undo
            return false;
        case 1: // there is only one change
            return true;
        default: // current state is not the bottom of the deque
            return mUndoneJointStatesIndex != (mLastSetJointStates.size() - 1);
    }
}
