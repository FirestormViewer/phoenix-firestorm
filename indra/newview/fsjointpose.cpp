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
/// The maximum length of any undo queue; adding new members preens older ones.
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

    mRotation          = FSJointRotation(joint->getRotation());
    mBeginningPosition = joint->getPosition();
    mBeginningScale = joint->getScale();
}

void FSJointPose::setPositionDelta(const LLVector3& pos)
{
    addToUndo(mPositionDelta, &mUndonePositionIndex, &mLastSetPositionDeltas, &mTimeLastUpdatedPosition);
    mPositionDelta.set(pos);
}

void FSJointPose::setRotationDelta(const LLQuaternion& rot)
{
    addToUndo(mRotation, &mUndoneRotationIndex, &mLastSetRotationDeltas, &mTimeLastUpdatedRotation);
    mRotation = FSJointRotation(mRotation.baseRotation, rot);
}

void FSJointPose::setScaleDelta(const LLVector3& scale)
{
    addToUndo(mScaleDelta, &mUndoneScaleIndex, &mLastSetScaleDeltas, &mTimeLastUpdatedScale);
    mScaleDelta.set(scale);
}

void FSJointPose::undoLastPositionChange()
{
    mPositionDelta.set(undoLastChange(mPositionDelta, &mUndonePositionIndex, &mLastSetPositionDeltas));
}

void FSJointPose::undoLastRotationChange()
{
    mRotation.set(undoLastChange(mRotation, &mUndoneRotationIndex, &mLastSetRotationDeltas));
}

void FSJointPose::undoLastScaleChange() { mScaleDelta.set(undoLastChange(mScaleDelta, &mUndoneScaleIndex, &mLastSetScaleDeltas)); }

void FSJointPose::redoLastPositionChange()
{
    mPositionDelta.set(redoLastChange(mPositionDelta, &mUndonePositionIndex, &mLastSetPositionDeltas));
}

void FSJointPose::redoLastRotationChange()
{
    mRotation.set(redoLastChange(mRotation, &mUndoneRotationIndex, &mLastSetRotationDeltas));
}

void FSJointPose::redoLastScaleChange() { mScaleDelta.set(redoLastChange(mScaleDelta, &mUndoneScaleIndex, &mLastSetScaleDeltas)); }

template <typename T>
inline void FSJointPose::addToUndo(T delta, size_t* undoIndex, std::deque<T>* dequeue,
                                   std::chrono::system_clock::time_point* timeLastUpdated)
{
    auto timeIntervalSinceLastChange = std::chrono::system_clock::now() - *timeLastUpdated;
    *timeLastUpdated                 = std::chrono::system_clock::now();

    if (timeIntervalSinceLastChange < UndoUpdateInterval)
        return;

    if (*undoIndex > 0)
    {
        for (size_t i = 0; i < *undoIndex; i++)
            dequeue->pop_front();

        *undoIndex = 0;
    }

    dequeue->push_front(delta);

    while (dequeue->size() > MaximumUndoQueueLength)
        dequeue->pop_back();
}

template <typename T> T FSJointPose::undoLastChange(T thingToSet, size_t* undoIndex, std::deque<T>* dequeue)
{
    if (dequeue->empty())
        return thingToSet;

    if (*undoIndex == 0)
        dequeue->push_front(thingToSet);

    *undoIndex += 1;
    *undoIndex = llclamp(*undoIndex, 0, dequeue->size() - 1);

    return dequeue->at(*undoIndex);
}

template <typename T> T FSJointPose::redoLastChange(T thingToSet, size_t* undoIndex, std::deque<T>* dequeue)
{
    if (dequeue->empty())
        return thingToSet;
    if (*undoIndex == 0)
        return thingToSet;

    *undoIndex -= 1;
    *undoIndex = llclamp(*undoIndex, 0, dequeue->size() - 1);
    T result = dequeue->at(*undoIndex);
    if (*undoIndex == 0)
        dequeue->pop_front();

    return result;
}

void FSJointPose::recaptureJoint()
{
    if (mIsCollisionVolume)
        return;

    LLJoint* joint = mJointState->getJoint();
    if (!joint)
        return;

    addToUndo(mRotation, &mUndoneRotationIndex, &mLastSetRotationDeltas, &mTimeLastUpdatedRotation);
    mRotation = FSJointRotation(joint->getRotation());
}

void FSJointPose::swapRotationWith(FSJointPose* oppositeJoint)
{
    if (!oppositeJoint)
        return;
    if (mIsCollisionVolume)
        return;

    auto tempRot             = FSJointRotation(mRotation);
    mRotation                = FSJointRotation(oppositeJoint->mRotation);
    oppositeJoint->mRotation = tempRot;
}

void FSJointPose::cloneRotationFrom(FSJointPose* fromJoint)
{
    if (!fromJoint)
        return;

    addToUndo(mRotation, &mUndoneRotationIndex, &mLastSetRotationDeltas, &mTimeLastUpdatedRotation);
    mRotation = FSJointRotation(fromJoint->mRotation);
}

void FSJointPose::mirrorRotationFrom(FSJointPose* fromJoint)
{
    if (!fromJoint)
        return;

    cloneRotationFrom(fromJoint);

    mRotation.baseRotation = LLQuaternion(-mRotation.baseRotation.mQ[VX], mRotation.baseRotation.mQ[VY], -mRotation.baseRotation.mQ[VZ],
                                          mRotation.baseRotation.mQ[VW]);
    mRotation.deltaRotation = LLQuaternion(-mRotation.deltaRotation.mQ[VX], mRotation.deltaRotation.mQ[VY], -mRotation.deltaRotation.mQ[VZ],
                                           mRotation.deltaRotation.mQ[VW]);
}

void FSJointPose::revertJoint()
{
    LLJoint* joint = mJointState->getJoint();
    if (!joint)
        return;

    joint->setRotation(mRotation.baseRotation);
    joint->setPosition(mBeginningPosition);
    joint->setScale(mBeginningScale);
}

void FSJointPose::reflectRotation()
{
    if (mIsCollisionVolume)
        return;

    mRotation.reflectRotation();
}

void FSJointPose::zeroBaseRotation()
{
    if (mIsCollisionVolume)
        return;

    mRotation.baseRotation = LLQuaternion::DEFAULT;
}

bool FSJointPose::isBaseRotationZero() const
{
    if (mIsCollisionVolume)
        return true;

    return mRotation.baseRotation == LLQuaternion::DEFAULT;
}
