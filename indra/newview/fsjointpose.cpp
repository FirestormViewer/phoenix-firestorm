/**
 * @file fsposingmotion.cpp
 * @brief Model for posing your (and other) avatar(s).
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

constexpr size_t MaximumUndoQueueLength = 20;

/// <summary>
/// The constant time interval, in seconds, specifying whether an 'undo' value should be added.
/// </summary>
constexpr std::chrono::duration<double> UndoUpdateInterval = std::chrono::duration<double>(0.3);

FSJointPose::FSJointPose(LLJoint* joint, U32 usage, bool isCollisionVolume)
{
    mJointState = new LLJointState;
    mJointState->setJoint(joint);
    mJointState->setUsage(usage);

    mJointName         = joint->getName();
    mIsCollisionVolume = isCollisionVolume;

    mBeginningRotation = mTargetRotation = joint->getRotation();
    mBeginningPosition = mTargetPosition = joint->getPosition();
    mBeginningScale = mTargetScale = joint->getScale();
}

void FSJointPose::addLastPositionToUndo()
{
    if (mUndonePositionIndex > 0)
    {
        for (int i = 0; i < mUndonePositionIndex; i++)
            mLastSetPositions.pop_front();

        mUndonePositionIndex = 0;
    }

    mLastSetPositions.push_front(mTargetPosition);

    while (mLastSetPositions.size() > MaximumUndoQueueLength)
        mLastSetPositions.pop_back();
}

void FSJointPose::addLastRotationToUndo()
{
    if (mUndoneRotationIndex > 0)
    {
        for (int i = 0; i < mUndoneRotationIndex; i++)
            mLastSetRotations.pop_front();

        mUndoneRotationIndex = 0;
    }

    mLastSetRotations.push_front(mTargetRotation);

    while (mLastSetRotations.size() > MaximumUndoQueueLength)
        mLastSetRotations.pop_back();
}

void FSJointPose::addLastScaleToUndo()
{
    if (mUndoneScaleIndex > 0)
    {
        for (int i = 0; i < mUndoneScaleIndex; i++)
            mLastSetScales.pop_front();

        mUndoneScaleIndex = 0;
    }

    mLastSetScales.push_front(mTargetScale);

    while (mLastSetScales.size() > MaximumUndoQueueLength)
        mLastSetScales.pop_back();
}

LLVector3 FSJointPose::getCurrentPosition()
{
    LLVector3 vec3;
    LLJoint* joint = mJointState->getJoint();
    if (!joint)
        return vec3;

    vec3 = joint->getPosition();
    return vec3;
}

void FSJointPose::setTargetPosition(const LLVector3& pos)
{
    auto timeIntervalSinceLastRotationChange = std::chrono::system_clock::now() - mTimeLastUpdatedPosition;
    if (timeIntervalSinceLastRotationChange > UndoUpdateInterval)
        addLastPositionToUndo();

    mTimeLastUpdatedPosition = std::chrono::system_clock::now();
    mTargetPosition.set(pos);
}

LLQuaternion FSJointPose::getCurrentRotation()
{
    LLQuaternion quat;
    LLJoint* joint = mJointState->getJoint();
    if (!joint)
        return quat;

    quat = joint->getRotation();
    return quat;
}

void FSJointPose::setTargetRotation(const LLQuaternion& rot)
{
    auto timeIntervalSinceLastRotationChange = std::chrono::system_clock::now() - mTimeLastUpdatedRotation;
    if (timeIntervalSinceLastRotationChange > UndoUpdateInterval)
        addLastRotationToUndo();

    mTimeLastUpdatedRotation = std::chrono::system_clock::now();
    mTargetRotation.set(rot);
}

void FSJointPose::applyDeltaRotation(const LLQuaternion& rot)
{
    auto timeIntervalSinceLastRotationChange = std::chrono::system_clock::now() - mTimeLastUpdatedRotation;
    if (timeIntervalSinceLastRotationChange > UndoUpdateInterval)
        addLastRotationToUndo();

    mTimeLastUpdatedRotation = std::chrono::system_clock::now();
    mTargetRotation = mTargetRotation * rot;
}

LLVector3 FSJointPose::getCurrentScale()
{
    LLVector3 vec3;
    LLJoint* joint = mJointState->getJoint();
    if (!joint)
        return vec3;

    vec3 = joint->getScale();
    return vec3;
}

void FSJointPose::setTargetScale(LLVector3 scale)
{
    auto timeIntervalSinceLastScaleChange = std::chrono::system_clock::now() - mTimeLastUpdatedScale;
    if (timeIntervalSinceLastScaleChange > UndoUpdateInterval)
        addLastScaleToUndo();

    mTimeLastUpdatedScale = std::chrono::system_clock::now();
    mTargetScale.set(scale);
}

void FSJointPose::undoLastPositionSet()
{
    if (mLastSetPositions.empty())
        return;

    if (mUndonePositionIndex == 0)  // at the top of the queue add the current
        addLastPositionToUndo();

    mUndonePositionIndex++;
    mUndonePositionIndex = llclamp(mUndonePositionIndex, 0, mLastSetPositions.size() - 1);
    mTargetPosition.set(mLastSetPositions[mUndonePositionIndex]);
}

void FSJointPose::redoLastPositionSet()
{
    if (mLastSetPositions.empty())
        return;

    mUndonePositionIndex--;
    mUndonePositionIndex = llclamp(mUndonePositionIndex, 0, mLastSetPositions.size() - 1);

    mTargetPosition.set(mLastSetPositions[mUndonePositionIndex]);
    if (mUndonePositionIndex == 0)
        mLastSetPositions.pop_front();
}

void FSJointPose::undoLastRotationSet()
{
    if (mLastSetRotations.empty())
        return;

    if (mUndoneRotationIndex == 0) // at the top of the queue add the current
        addLastRotationToUndo();

    mUndoneRotationIndex++;
    mUndoneRotationIndex = llclamp(mUndoneRotationIndex, 0, mLastSetRotations.size() - 1);
    mTargetRotation.set(mLastSetRotations[mUndoneRotationIndex]);
}

void FSJointPose::redoLastRotationSet()
{
    if (mLastSetRotations.empty())
        return;

    mUndoneRotationIndex--;
    mUndoneRotationIndex = llclamp(mUndoneRotationIndex, 0, mLastSetRotations.size() - 1);

    mTargetRotation.set(mLastSetRotations[mUndoneRotationIndex]);
    if (mUndoneRotationIndex == 0)
        mLastSetRotations.pop_front();
}

void FSJointPose::undoLastScaleSet()
{
    if (mLastSetScales.empty())
        return;

    if (mUndoneScaleIndex == 0)
        addLastScaleToUndo();

    mUndoneScaleIndex++;
    mUndoneScaleIndex = llclamp(mUndoneScaleIndex, 0, mLastSetScales.size() - 1);
    mTargetScale.set(mLastSetScales[mUndoneScaleIndex]);
}

void FSJointPose::redoLastScaleSet()
{
    if (mLastSetScales.empty())
        return;

    mUndoneScaleIndex--;
    mUndoneScaleIndex = llclamp(mUndoneScaleIndex, 0, mLastSetScales.size() - 1);

    mTargetScale.set(mLastSetScales[mUndoneScaleIndex]);
    if (mUndoneScaleIndex == 0)
        mLastSetScales.pop_front();
}

void FSJointPose::revertJointScale()
{
    LLJoint* joint = mJointState->getJoint();
    if (!joint)
        return;

    joint->setScale(mBeginningScale);
}

void FSJointPose::revertJointPosition()
{
    LLJoint* joint = mJointState->getJoint();
    if (!joint)
        return;

    joint->setPosition(mBeginningPosition);
}

void FSJointPose::revertCollisionVolume()
{
    if (!mIsCollisionVolume)
        return;

    LLJoint* joint = mJointState->getJoint();
    if (!joint)
        return;

    joint->setRotation(mBeginningRotation);
    joint->setPosition(mBeginningPosition);
    joint->setScale(mBeginningScale);
}

void FSJointPose::setJointStartRotations(LLQuaternion quat) { mBeginningRotation = mTargetRotation = quat; }
