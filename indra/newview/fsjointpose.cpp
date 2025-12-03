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
    mJointNumber       = joint->getJointNum();

    mCurrentState = FSJointState(joint);
}

void FSJointPose::setPublicPosition(const LLVector3& pos)
{
    addStateToUndo(mCurrentState);
    mCurrentState.mPosition.set(pos);
    mCurrentState.mLastChangeWasRotational = false;
}

void FSJointPose::setPublicRotation(bool zeroBase, const LLQuaternion& rot)
{
    addStateToUndo(mCurrentState);

    if (zeroBase)
        zeroBaseRotation(true);
    else
        mCurrentState.mUserSpecifiedBaseZero = false;

    mCurrentState.mRotation.set(rot);
    mCurrentState.mLastChangeWasRotational = true;
}

void FSJointPose::setPublicScale(const LLVector3& scale)
{
    addStateToUndo(mCurrentState);
    mCurrentState.mScale.set(scale);
    mCurrentState.mLastChangeWasRotational = false;
}

bool FSJointPose::undoLastChange()
{
    bool changeType = mCurrentState.mLastChangeWasRotational;
    mCurrentState   = undoLastStateChange(mCurrentState);

    return changeType;
}

void FSJointPose::redoLastChange()
{
    mCurrentState = redoLastStateChange(mCurrentState);
}

void FSJointPose::resetJoint()
{
    addStateToUndo(mCurrentState);
    mCurrentState.resetJoint();
    mCurrentState.mLastChangeWasRotational = true;
}

void FSJointPose::addStateToUndo(const FSJointState& stateToAddToUndo)
{
    mModifiedThisSession = true;

    auto now = std::chrono::system_clock::now();
    auto timeIntervalSinceLastChange = now - mTimeLastUpdatedCurrentState;
    mTimeLastUpdatedCurrentState     = now;

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

FSJointPose::FSJointState FSJointPose::undoLastStateChange(const FSJointState& thingToSet)
{
    if (mLastSetJointStates.empty())
        return thingToSet;

    if (mUndoneJointStatesIndex == 0)
        mLastSetJointStates.push_front(thingToSet);

    mUndoneJointStatesIndex += 1;
    mUndoneJointStatesIndex = llclamp(mUndoneJointStatesIndex, 0, mLastSetJointStates.size() - 1);

    return mLastSetJointStates.at(mUndoneJointStatesIndex);
}

FSJointPose::FSJointState FSJointPose::redoLastStateChange(const FSJointState& thingToSet)
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
    LLJoint* joint = mJointState->getJoint();
    if (!joint)
        return;

    addStateToUndo(mCurrentState);

    if (mIsCollisionVolume)
    {
        mCurrentState.mPosition.clear();
        mCurrentState.mScale.clear();
    }

    mCurrentState = FSJointState(joint);
    mCurrentState.mLastChangeWasRotational = true;
}

LLQuaternion FSJointPose::updateJointAsDelta(bool zeroBase, const LLQuaternion& rotation, const LLVector3& position, const LLVector3& scale)
{
    addStateToUndo(mCurrentState);
    mCurrentState.mLastChangeWasRotational = true;

    return mCurrentState.updateFromJointProperties(zeroBase, rotation, position, scale);
}

void FSJointPose::setBaseRotation(const LLQuaternion& rotation, LLJoint::JointPriority priority)
{
    mCurrentState.resetBaseRotation(rotation, priority);
}

void FSJointPose::setBasePosition(const LLVector3& position, LLJoint::JointPriority priority)
{
    mCurrentState.resetBasePosition(position, priority);
}

void FSJointPose::setBaseScale(const LLVector3& scale, LLJoint::JointPriority priority)
{
    mCurrentState.resetBaseScale(scale, priority);
}

void FSJointPose::setJointPriority(LLJoint::JointPriority priority)
{
    mCurrentState.setPriority(priority);
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

void FSJointPose::swapBaseRotationWith(FSJointPose* oppositeJoint)
{
    if (!oppositeJoint)
        return;
    if (mIsCollisionVolume)
        return;

    auto tempState = FSJointState(mCurrentState);
    mCurrentState.cloneBaseRotationFrom(oppositeJoint->mCurrentState);
    oppositeJoint->mCurrentState.cloneBaseRotationFrom(tempState);
}

void FSJointPose::cloneRotationFrom(FSJointPose* fromJoint)
{
    if (!fromJoint)
        return;

    addStateToUndo(FSJointState(mCurrentState));
    mCurrentState.cloneRotationFrom(fromJoint->mCurrentState);
    mCurrentState.mLastChangeWasRotational = true;
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

    mModifiedThisSession = true;
    mCurrentState.reflectRotation();
}

void FSJointPose::reflectBaseRotation()
{
    if (mIsCollisionVolume)
        return;

    mCurrentState.reflectBaseRotation();
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
    return mCurrentState.mRotationIsWorldLocked;
}

void FSJointPose::setWorldRotationLockState(bool newState)
{
    mCurrentState.mRotationIsWorldLocked = newState;
}

bool FSJointPose::getRotationMirrorState() const
{
    return mCurrentState.mJointRotationIsMirrored;
}

void FSJointPose::setRotationMirrorState(bool newState)
{
    mCurrentState.mJointRotationIsMirrored = newState;
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
