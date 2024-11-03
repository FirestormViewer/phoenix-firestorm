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

FSPosingMotion::FSPosingMotion(const LLUUID &id) : LLMotion(id)
{
    mName = "fs_poser_pose";
    mMotionID = id;
}

LLMotion::LLMotionInitStatus FSPosingMotion::onInitialize(LLCharacter *character)
{
    if (!character)
        return STATUS_FAILURE;

    mJointPoses.clear();

    LLJoint* targetJoint;
    for (S32 i = 0; (targetJoint = character->getCharacterJoint(i)); ++i)
    {
        if (!targetJoint)
            continue;

        FSJointPose jointPose = FSJointPose(targetJoint);
        mJointPoses.push_back(jointPose);

        addJointState(jointPose.getJointState());
    }

    for (S32 i = 0; (targetJoint = character->findCollisionVolume(i)); ++i)
    {
        if (!targetJoint)
            continue;

        FSJointPose jointPose = FSJointPose(targetJoint, true);
        mJointPoses.push_back(jointPose);

        addJointState(jointPose.getJointState());
    }

    return STATUS_SUCCESS;
}

bool FSPosingMotion::onActivate()
{
    return true;
}

bool FSPosingMotion::onUpdate(F32 time, U8* joint_mask)
{
    LLQuaternion targetRotation;
    LLQuaternion currentRotation;
    LLVector3 currentPosition;
    LLVector3 targetPosition;
    LLVector3 currentScale;
    LLVector3 targetScale;

    for (FSJointPose jointPose : mJointPoses)
    {
        LLJoint* joint = jointPose.getJointState()->getJoint();
        if (!joint)
            continue;

        currentRotation = joint->getRotation();
        currentPosition = joint->getPosition();
        currentScale = joint->getScale();
        targetRotation = jointPose.getTargetRotation();
        targetPosition = jointPose.getTargetPosition();
        targetScale = jointPose.getTargetScale();

        if (currentPosition != targetPosition)
        {
            currentPosition = lerp(currentPosition, targetPosition, mInterpolationTime);
            jointPose.getJointState()->setPosition(currentPosition);
        }

        if (currentRotation != targetRotation)
        {
            currentRotation = slerp(mInterpolationTime, currentRotation, targetRotation);
            jointPose.getJointState()->setRotation(currentRotation);
        }

        if (currentScale != targetScale)
        {
            currentScale = lerp(currentScale, targetScale, mInterpolationTime);
            jointPose.getJointState()->setScale(currentScale);
        }
    }

    return true;
}

void FSPosingMotion::onDeactivate() { revertChangesToPositionsScalesAndCollisionVolumes(); }

void FSPosingMotion::revertChangesToPositionsScalesAndCollisionVolumes()
{
    for (FSJointPose jointPose : mJointPoses)
    {
        jointPose.revertJointScale();
        jointPose.revertJointPosition();

        if (jointPose.isCollisionVolume())
            jointPose.revertCollisionVolume();

        LLJoint* joint = jointPose.getJointState()->getJoint();
        if (!joint)
            continue;

        addJointToState(joint);
    }
}

bool FSPosingMotion::currentlyPosingJoint(FSJointPose* joint)
{
    if (!joint)
        return false;

    LLJoint* avJoint = joint->getJointState()->getJoint();
    if (!avJoint)
        return false;

    return currentlyPosingJoint(avJoint);
}

void FSPosingMotion::addJointToState(FSJointPose* joint)
{
    if (!joint)
        return;

    LLJoint* avJoint = joint->getJointState()->getJoint();
    if (!avJoint)
        return;

    setJointState(avJoint, POSER_JOINT_STATE);
}

void FSPosingMotion::removeJointFromState(FSJointPose* joint)
{
    if (!joint)
        return;

    LLJoint* avJoint = joint->getJointState()->getJoint();
    if (!avJoint)
        return;

    joint->revertJointScale();
    joint->revertJointPosition();

    if (joint->isCollisionVolume())
        joint->revertCollisionVolume();

    setJointState(avJoint, 0);
}

void FSPosingMotion::addJointToState(LLJoint* joint)
{
    setJointState(joint, POSER_JOINT_STATE);
}

void FSPosingMotion::removeJointFromState(LLJoint* joint)
{
    setJointState(joint, 0);
}

void FSPosingMotion::setJointState(LLJoint* joint, U32 state)
{
    if (mJointPoses.size() < 1)
        return;
    if (!joint)
        return;

    LLPose* pose = this->getPose();
    if (!pose)
        return;

    LLPointer<LLJointState> jointState = pose->findJointState(joint);
    if (jointState.isNull())
        return;

    pose->removeJointState(jointState);
    FSJointPose *jointPose = getJointPoseByJointName(joint->getName());
    if (!jointPose)
        return;

    jointPose->getJointState()->setUsage(state);
    addJointState(jointPose->getJointState());
}

FSPosingMotion::FSJointPose* FSPosingMotion::getJointPoseByJointName(const std::string& name)
{
    if (mJointPoses.size() < 1)
        return nullptr;

    for (auto poserJoint_iter = mJointPoses.begin(); poserJoint_iter != mJointPoses.end(); ++poserJoint_iter)
    {
        if (!boost::iequals(poserJoint_iter->jointName(), name))
            continue;

        return &*poserJoint_iter;
    }

    return nullptr;
}

bool FSPosingMotion::currentlyPosingJoint(LLJoint* joint)
{
    if (mJointPoses.size() < 1)
        return false;

    if (!joint)
        return false;

    LLPose* pose = this->getPose();
    if (!pose)
        return false;

    LLPointer<LLJointState> jointState = pose->findJointState(joint);
    if (jointState.isNull())
        return false;

    U32 state = jointState->getUsage();
    return (state & POSER_JOINT_STATE);
}

bool FSPosingMotion::allStartingRotationsAreZero() const
{
    LLQuaternion zeroQuat;
    for (auto poserJoint_iter = mJointPoses.begin(); poserJoint_iter != mJointPoses.end(); ++poserJoint_iter)
    {
        if (poserJoint_iter->jointName() == "mPelvis")
            continue;
        if (poserJoint_iter->isCollisionVolume())
            continue;

        LLQuaternion quat = poserJoint_iter->getBeginningRotation();
        if (quat != zeroQuat)
            return false;
    }

    return true;
}

void FSPosingMotion::setAllRotationsToZero()
{
    for (auto poserJoint_iter = mJointPoses.begin(); poserJoint_iter != mJointPoses.end(); ++poserJoint_iter)
    {
        if (poserJoint_iter->isCollisionVolume())
            continue;

        poserJoint_iter->setRotationsToZero();
    }
}

constexpr size_t MaximumUndoQueueLength = 20;

/// <summary>
/// The constant time interval, in seconds, 
/// </summary>
constexpr std::chrono::duration<double> UndoUpdateInterval = std::chrono::duration<double>(0.3);


void FSPosingMotion::FSJointPose::addLastPositionToUndo()
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

void FSPosingMotion::FSJointPose::addLastRotationToUndo()
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

void FSPosingMotion::FSJointPose::addLastScaleToUndo()
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

LLVector3 FSPosingMotion::FSJointPose::getCurrentPosition()
{
    LLVector3 vec3;
    LLJoint* joint = mJointState->getJoint();
    if (!joint)
        return vec3;

    vec3 = joint->getPosition();
    return vec3;
}

void FSPosingMotion::FSJointPose::setTargetPosition(const LLVector3& pos)
{
    auto timeIntervalSinceLastRotationChange = std::chrono::system_clock::now() - mTimeLastUpdatedPosition;
    if (timeIntervalSinceLastRotationChange > UndoUpdateInterval)
        addLastPositionToUndo();

    mTimeLastUpdatedPosition = std::chrono::system_clock::now();
    mTargetPosition.set(pos);
}

LLQuaternion FSPosingMotion::FSJointPose::getCurrentRotation()
{
    LLQuaternion quat;
    LLJoint* joint = mJointState->getJoint();
    if (!joint)
        return quat;

    quat = joint->getRotation();
    return quat;
}

void FSPosingMotion::FSJointPose::setTargetRotation(const LLQuaternion& rot)
{
    auto timeIntervalSinceLastRotationChange = std::chrono::system_clock::now() - mTimeLastUpdatedRotation;
    if (timeIntervalSinceLastRotationChange > UndoUpdateInterval)
        addLastRotationToUndo();

    mTimeLastUpdatedRotation = std::chrono::system_clock::now();
    mTargetRotation.set(rot);
}

void FSPosingMotion::FSJointPose::applyDeltaRotation(const LLQuaternion& rot)
{
    auto timeIntervalSinceLastRotationChange = std::chrono::system_clock::now() - mTimeLastUpdatedRotation;
    if (timeIntervalSinceLastRotationChange > UndoUpdateInterval)
        addLastRotationToUndo();

    mTimeLastUpdatedRotation = std::chrono::system_clock::now();
    mTargetRotation = mTargetRotation * rot;
}

LLVector3 FSPosingMotion::FSJointPose::getCurrentScale()
{
    LLVector3 vec3;
    LLJoint* joint = mJointState->getJoint();
    if (!joint)
        return vec3;

    vec3 = joint->getScale();
    return vec3;
}

void FSPosingMotion::FSJointPose::setTargetScale(LLVector3 scale)
{
    auto timeIntervalSinceLastScaleChange = std::chrono::system_clock::now() - mTimeLastUpdatedScale;
    if (timeIntervalSinceLastScaleChange > UndoUpdateInterval)
        addLastScaleToUndo();

    mTimeLastUpdatedScale = std::chrono::system_clock::now();
    mTargetScale.set(scale);
}

void FSPosingMotion::FSJointPose::undoLastPositionSet()
{
    if (mLastSetPositions.empty())
        return;

    if (mUndonePositionIndex == 0)  // at the top of the queue add the current
        addLastPositionToUndo();

    mUndonePositionIndex++;
    mUndonePositionIndex = llclamp(mUndonePositionIndex, 0, mLastSetPositions.size() - 1);
    mTargetPosition.set(mLastSetPositions[mUndonePositionIndex]);
}

void FSPosingMotion::FSJointPose::redoLastPositionSet()
{
    if (mLastSetPositions.empty())
        return;

    mUndonePositionIndex--;
    mUndonePositionIndex = llclamp(mUndonePositionIndex, 0, mLastSetPositions.size() - 1);

    mTargetPosition.set(mLastSetPositions[mUndonePositionIndex]);
    if (mUndonePositionIndex == 0)
        mLastSetPositions.pop_front();
}

void FSPosingMotion::FSJointPose::undoLastRotationSet()
{
    if (mLastSetRotations.empty())
        return;

    if (mUndoneRotationIndex == 0) // at the top of the queue add the current
        addLastRotationToUndo();

    mUndoneRotationIndex++;
    mUndoneRotationIndex = llclamp(mUndoneRotationIndex, 0, mLastSetRotations.size() - 1);
    mTargetRotation.set(mLastSetRotations[mUndoneRotationIndex]);
}

void FSPosingMotion::FSJointPose::redoLastRotationSet()
{
    if (mLastSetRotations.empty())
        return;

    mUndoneRotationIndex--;
    mUndoneRotationIndex = llclamp(mUndoneRotationIndex, 0, mLastSetRotations.size() - 1);

    mTargetRotation.set(mLastSetRotations[mUndoneRotationIndex]);
    if (mUndoneRotationIndex == 0)
        mLastSetRotations.pop_front();
}

void FSPosingMotion::FSJointPose::undoLastScaleSet()
{
    if (mLastSetScales.empty())
        return;

    if (mUndoneScaleIndex == 0)
        addLastScaleToUndo();

    mUndoneScaleIndex++;
    mUndoneScaleIndex = llclamp(mUndoneScaleIndex, 0, mLastSetScales.size() - 1);
    mTargetScale.set(mLastSetScales[mUndoneScaleIndex]);
}

void FSPosingMotion::FSJointPose::redoLastScaleSet()
{
    if (mLastSetScales.empty())
        return;

    mUndoneScaleIndex--;
    mUndoneScaleIndex = llclamp(mUndoneScaleIndex, 0, mLastSetScales.size() - 1);

    mTargetScale.set(mLastSetScales[mUndoneScaleIndex]);
    if (mUndoneScaleIndex == 0)
        mLastSetScales.pop_front();
}

void FSPosingMotion::FSJointPose::revertJointScale()
{
    LLJoint* joint = mJointState->getJoint();
    if (!joint)
        return;

    joint->setScale(mBeginningScale);
}

void FSPosingMotion::FSJointPose::revertJointPosition()
{
    LLJoint* joint = mJointState->getJoint();
    if (!joint)
        return;

    joint->setPosition(mBeginningPosition);
}

void FSPosingMotion::FSJointPose::revertCollisionVolume()
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

void FSPosingMotion::FSJointPose::setRotationsToZero() { mBeginningRotation = mTargetRotation = LLQuaternion(); }

FSPosingMotion::FSJointPose::FSJointPose(LLJoint* joint, bool isCollisionVolume)
{
    mJointState = new LLJointState;
    mJointState->setJoint(joint);
    mJointState->setUsage(POSER_JOINT_STATE);

    mJointName = joint->getName();
    mIsCollisionVolume = isCollisionVolume;

    mBeginningRotation = mTargetRotation = joint->getRotation();
    mBeginningPosition = mTargetPosition = joint->getPosition();
    mBeginningScale = mTargetScale = joint->getScale();
}
