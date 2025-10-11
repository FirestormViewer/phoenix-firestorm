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

FSPosingMotion::FSPosingMotion(const LLUUID& id) : LLKeyframeMotion(id)
{
    mName = "fs_poser_pose";
    mMotionID = id;
    mJointMotionList = &dummyMotionList;
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

        FSJointPose jointPose = FSJointPose(targetJoint, POSER_JOINT_STATE);
        mJointPoses.push_back(jointPose);

        addJointState(jointPose.getJointState());
    }

    for (S32 i = 0; (targetJoint = character->findCollisionVolume(i)); ++i)
    {
        if (!targetJoint)
            continue;

        FSJointPose jointPose = FSJointPose(targetJoint, POSER_JOINT_STATE, true);
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
        targetRotation  = jointPose.getTargetRotation();
        targetPosition  = jointPose.getTargetPosition();
        targetScale     = jointPose.getTargetScale();

        if (vectorsNotQuiteEqual(currentPosition, targetPosition))
        {
            currentPosition = lerp(currentPosition, targetPosition, mInterpolationTime);
            jointPose.getJointState()->setPosition(currentPosition);
        }

        if (quatsNotQuiteEqual(currentRotation, targetRotation))
        {
            currentRotation = slerp(mInterpolationTime, currentRotation, targetRotation);
            jointPose.getJointState()->setRotation(currentRotation);
        }

        if (vectorsNotQuiteEqual(currentScale, targetScale))
        {
            currentScale = lerp(currentScale, targetScale, mInterpolationTime);
            jointPose.getJointState()->setScale(currentScale);
        }
    }

    return true;
}

void FSPosingMotion::onDeactivate() { revertJointsAndCollisionVolumes(); }

void FSPosingMotion::revertJointsAndCollisionVolumes()
{
    for (FSJointPose jointPose : mJointPoses)
    {
        jointPose.revertJoint();

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

    joint->revertJoint();

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

FSJointPose* FSPosingMotion::getJointPoseByJointName(const std::string& name)
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
    for (auto poserJoint_iter = mJointPoses.begin(); poserJoint_iter != mJointPoses.end(); ++poserJoint_iter)
    {
        if (poserJoint_iter->isCollisionVolume())
            continue;

        if (!poserJoint_iter->isBaseRotationZero())
            return false;
    }

    return true;
}

void FSPosingMotion::setAllRotationsToZeroAndClearUndo()
{
    for (auto poserJoint_iter = mJointPoses.begin(); poserJoint_iter != mJointPoses.end(); ++poserJoint_iter)
    {
        poserJoint_iter->purgeUndoQueue();
        poserJoint_iter->setPublicRotation(true, LLQuaternion::DEFAULT);
    }
}

void FSPosingMotion::setJointBvhLock(FSJointPose* joint, bool lockInBvh)
{
    joint->zeroBaseRotation(lockInBvh);
}

bool FSPosingMotion::loadOtherMotionToBaseOfThisMotion(LLKeyframeMotion* motionToLoad, F32 timeToLoadAt, std::string selectedJointNames)
{
    FSPosingMotion* motionToLoadAsFsPosingMotion = static_cast<FSPosingMotion*>(motionToLoad);
    if (!motionToLoadAsFsPosingMotion)
        return false;

    LLJoint::JointPriority priority = motionToLoad->getPriority();
    bool                   motionIsForAllJoints = selectedJointNames.empty();

    LLQuaternion rot;
    LLVector3    position, scale;
    bool         hasRotation = false, hasPosition = false, hasScale = false;

    for (auto poserJoint_iter = mJointPoses.begin(); poserJoint_iter != mJointPoses.end(); ++poserJoint_iter)
    {
        std::string jointName = poserJoint_iter->jointName();

        bool motionIsForThisJoint = selectedJointNames.find(jointName) != std::string::npos;
        if (!motionIsForAllJoints && !motionIsForThisJoint)
            continue;

        hasRotation = hasPosition = hasScale = false;
        motionToLoadAsFsPosingMotion->getJointStateAtTime(jointName, timeToLoadAt, &hasRotation, &rot, &hasPosition, &position, &hasScale, &scale);

        if (hasRotation)
            poserJoint_iter->setBaseRotation(rot, priority);

        if (hasPosition)
            poserJoint_iter->setBasePosition(position, priority);

        if (hasScale)
            poserJoint_iter->setBaseScale(scale, priority);
    }

    return true;
}

void FSPosingMotion::getJointStateAtTime(std::string jointPoseName, F32 timeToLoadAt,
                                            bool* hasRotation, LLQuaternion* jointRotation,
                                            bool* hasPosition, LLVector3* jointPosition,
                                            bool* hasScale,    LLVector3* jointScale)
{
    if ( mJointMotionList == nullptr)
        return;

    for (U32 i = 0; i < mJointMotionList->getNumJointMotions(); i++)
    {
        JointMotion* jm = mJointMotionList->getJointMotion(i);
        if (!boost::iequals(jointPoseName, jm->mJointName))
            continue;

        *hasRotation = (jm->mRotationCurve.mNumKeys > 0);
        if (hasRotation)
            jointRotation->set(jm->mRotationCurve.getValue(timeToLoadAt, mJointMotionList->mDuration));

        *hasPosition = (jm->mPositionCurve.mNumKeys > 0);
        if (hasPosition)
            jointPosition->set(jm->mPositionCurve.getValue(timeToLoadAt, mJointMotionList->mDuration));

        *hasScale = (jm->mScaleCurve.mNumKeys > 0);
        if (hasScale)
            jointScale->set(jm->mScaleCurve.getValue(timeToLoadAt, mJointMotionList->mDuration));

        return;
    }
}

bool FSPosingMotion::otherMotionAnimatesJoints(LLKeyframeMotion* motionToQuery, std::string recapturedJointNames)
{
    FSPosingMotion* motionToLoadAsFsPosingMotion = static_cast<FSPosingMotion*>(motionToQuery);
    if (!motionToLoadAsFsPosingMotion)
        return false;

    return motionToLoadAsFsPosingMotion->motionAnimatesJoints(recapturedJointNames);
}

bool FSPosingMotion::motionAnimatesJoints(std::string recapturedJointNames)
{
    if (mJointMotionList == nullptr)
        return false;

    for (U32 i = 0; i < mJointMotionList->getNumJointMotions(); i++)
    {
        JointMotion* jm = mJointMotionList->getJointMotion(i);
        if (recapturedJointNames.find(jm->mJointName) == std::string::npos)
            continue;

        if (jm->mRotationCurve.mNumKeys > 0)
            return true;
    }

    return false;
}

void FSPosingMotion::resetBonePriority(std::string boneNamesToReset)
{
    if (boneNamesToReset.empty())
        return;

    for (auto poserJoint_iter = mJointPoses.begin(); poserJoint_iter != mJointPoses.end(); ++poserJoint_iter)
    {
        std::string jointName = poserJoint_iter->jointName();
        if (boneNamesToReset.find(jointName) != std::string::npos)
            poserJoint_iter->setJointPriority(LLJoint::LOW_PRIORITY);
    }
}

bool FSPosingMotion::vectorsNotQuiteEqual(LLVector3 v1, LLVector3 v2) const
{
    if (vectorAxesAlmostEqual(v1.mV[VX], v2.mV[VX]) &&
        vectorAxesAlmostEqual(v1.mV[VY], v2.mV[VY]) &&
        vectorAxesAlmostEqual(v1.mV[VZ], v2.mV[VZ]))
        return false;

    return true;
}

bool FSPosingMotion::quatsNotQuiteEqual(const LLQuaternion& q1, const LLQuaternion& q2) const
{
    if (vectorAxesAlmostEqual(q1.mQ[VW], q2.mQ[VW]) &&
        vectorAxesAlmostEqual(q1.mQ[VX], q2.mQ[VX]) &&
        vectorAxesAlmostEqual(q1.mQ[VY], q2.mQ[VY]) &&
        vectorAxesAlmostEqual(q1.mQ[VZ], q2.mQ[VZ]))
        return false;

    if (vectorAxesAlmostEqual(q1.mQ[VW], -q2.mQ[VW]) &&
        vectorAxesAlmostEqual(q1.mQ[VX], -q2.mQ[VX]) &&
        vectorAxesAlmostEqual(q1.mQ[VY], -q2.mQ[VY]) &&
        vectorAxesAlmostEqual(q1.mQ[VZ], -q2.mQ[VZ]))
        return false;

    return true;
}
