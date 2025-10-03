/**
 * @file fsposeranimator.cpp
 * @brief business-layer for posing your (and other) avatar(s) and animeshes.
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
#include "fsposeranimator.h"
#include "llcharacter.h"
#include "llagent.h"
#include "fsposingmotion.h"

std::map<LLUUID, LLAssetID> FSPoserAnimator::sAvatarIdToRegisteredAnimationId;

bool FSPoserAnimator::isPosingAvatarJoint(LLVOAvatar* avatar, const FSPoserJoint& joint)
{
    if (!isAvatarSafeToUse(avatar))
        return false;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return false;

    if (posingMotion->isStopped())
        return false;

    FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint.jointName());
    if (!jointPose)
        return false;

    return posingMotion->currentlyPosingJoint(jointPose);
}

void FSPoserAnimator::setPosingAvatarJoint(LLVOAvatar* avatar, const FSPoserJoint& joint, bool shouldPose)
{
    if (!isAvatarSafeToUse(avatar))
        return;

    bool arePosing = isPosingAvatarJoint(avatar, joint);
    if ( (arePosing && shouldPose) || (!arePosing && !shouldPose) ) // could !XOR, but this is readable
        return;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return;

    if (posingMotion->isStopped())
        return;

    FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint.jointName());
    if (!jointPose)
        return;

    if (shouldPose)
        posingMotion->addJointToState(jointPose);
    else
        posingMotion->removeJointFromState(jointPose);
}

void FSPoserAnimator::undoLastJointChange(LLVOAvatar* avatar, const FSPoserJoint& joint, E_BoneDeflectionStyles style)
{
    if (!isAvatarSafeToUse(avatar))
        return;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return;

    if (posingMotion->isStopped())
        return;

    FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint.jointName());
    if (!jointPose)
        return;

    jointPose->undoLastChange();
    undoOrRedoWorldLockedDescendants(joint, posingMotion, false);

    if (style == NONE || style == DELTAMODE)
        return;

    FSJointPose* oppositeJointPose = posingMotion->getJointPoseByJointName(joint.mirrorJointName());
    if (!oppositeJointPose)
        return;

    oppositeJointPose->undoLastChange();

    auto oppositePoserJoint = getPoserJointByName(joint.mirrorJointName());
    if (oppositePoserJoint)
        undoOrRedoWorldLockedDescendants(*oppositePoserJoint, posingMotion, false);
}

void FSPoserAnimator::resetJoint(LLVOAvatar* avatar, const FSPoserJoint& joint, E_BoneDeflectionStyles style)
{
    if (!isAvatarSafeToUse(avatar))
        return;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return;

    if (posingMotion->isStopped())
        return;

    FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint.jointName());
    if (!jointPose)
        return;

    jointPose->resetJoint();

    if (style == NONE || style == DELTAMODE)
        return;

    FSJointPose* oppositeJointPose = posingMotion->getJointPoseByJointName(joint.mirrorJointName());
    if (!oppositeJointPose)
        return;

    oppositeJointPose->resetJoint();
}

bool FSPoserAnimator::canRedoOrUndoJointChange(LLVOAvatar* avatar, const FSPoserJoint& joint, bool canUndo)
{
    if (!isAvatarSafeToUse(avatar))
        return false;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return false;

    if (posingMotion->isStopped())
        return false;

    FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint.jointName());
    if (!jointPose)
        return false;

    if (canUndo)
        return jointPose->canPerformUndo();

    return jointPose->canPerformRedo();
}

void FSPoserAnimator::redoLastJointChange(LLVOAvatar* avatar, const FSPoserJoint& joint, E_BoneDeflectionStyles style)
{
    if (!isAvatarSafeToUse(avatar))
        return;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return;

    if (posingMotion->isStopped())
        return;

    FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint.jointName());
    if (!jointPose)
        return;

    jointPose->redoLastChange();
    undoOrRedoWorldLockedDescendants(joint, posingMotion, true);

    if (style == NONE || style == DELTAMODE)
        return;

    FSJointPose* oppositeJointPose = posingMotion->getJointPoseByJointName(joint.mirrorJointName());
    if (!oppositeJointPose)
        return;

    oppositeJointPose->redoLastChange();

    auto oppositePoserJoint = getPoserJointByName(joint.mirrorJointName());
    if (oppositePoserJoint)
        undoOrRedoWorldLockedDescendants(*oppositePoserJoint, posingMotion, true);
}

LLVector3 FSPoserAnimator::getJointPosition(LLVOAvatar* avatar, const FSPoserJoint& joint) const
{
    LLVector3 pos;
    if (!isAvatarSafeToUse(avatar))
        return pos;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return pos;

    FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint.jointName());
    if (!jointPose)
        return pos;

    return jointPose->getPublicPosition();
}

void FSPoserAnimator::setJointPosition(LLVOAvatar* avatar, const FSPoserJoint* joint, const LLVector3& position, E_BoneDeflectionStyles style)
{
    if (!isAvatarSafeToUse(avatar))
        return;
    if (!joint)
        return;

    std::string jn = joint->jointName();
    if (jn.empty())
        return;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return;

    FSJointPose* jointPose = posingMotion->getJointPoseByJointName(jn);
    if (!jointPose)
        return;

    LLVector3 positionDelta = jointPose->getPublicPosition() - position;

    switch (style)
    {
        case MIRROR:
        case MIRROR_DELTA:
        case SYMPATHETIC_DELTA:
        case SYMPATHETIC:
            jointPose->setPublicPosition(position);
            break;

        case DELTAMODE:
        case NONE:
        default:
            jointPose->setPublicPosition(position);
            return;
    }

    FSJointPose* oppositeJointPose = posingMotion->getJointPoseByJointName(joint->mirrorJointName());
    if (!oppositeJointPose)
        return;

    LLVector3 oppositeJointPosition = oppositeJointPose->getPublicPosition();

    switch (style)
    {
        case MIRROR:
        case MIRROR_DELTA:
            oppositeJointPose->setPublicPosition(oppositeJointPosition + positionDelta);
            break;

        case SYMPATHETIC_DELTA:
        case SYMPATHETIC:
            oppositeJointPose->setPublicPosition(oppositeJointPosition - positionDelta);
            break;

        default:
            break;
    }
}

bool FSPoserAnimator::getRotationIsWorldLocked(LLVOAvatar* avatar, const FSPoserJoint& joint) const
{
    if (!isAvatarSafeToUse(avatar))
        return false;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return false;

    FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint.jointName());
    if (!jointPose)
        return false;

    return jointPose->getWorldRotationLockState();
}

void FSPoserAnimator::setRotationIsWorldLocked(LLVOAvatar* avatar, const FSPoserJoint& joint, bool newState)
{
    if (!isAvatarSafeToUse(avatar))
        return;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return;

    FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint.jointName());
    if (!jointPose)
        return;

    jointPose->setWorldRotationLockState(newState);
}

bool FSPoserAnimator::exportRotationWillLockJoint(LLVOAvatar* avatar, const FSPoserJoint& joint) const
{
    const F32 ROTATION_KEYFRAME_THRESHOLD = 0.01f; // this is a guestimate: see BVH loader

    if (!isAvatarSafeToUse(avatar))
        return false;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return false;

    FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint.jointName());
    if (!jointPose)
        return false;

    if (!jointPose->userHasSetBaseRotationToZero())
        return false;

    F32          rot_threshold = ROTATION_KEYFRAME_THRESHOLD / llmax((F32)getChildJointDepth(&joint, 0) * 0.33f, 1.f);
    LLQuaternion rotToExport = jointPose->getPublicRotation();

    F32 x_delta  = dist_vec(LLVector3::x_axis * LLQuaternion::DEFAULT, LLVector3::x_axis * rotToExport); // when exporting multiple frames this will need to compare frames.
    F32 y_delta  = dist_vec(LLVector3::y_axis * LLQuaternion::DEFAULT, LLVector3::y_axis * rotToExport);
    F32 rot_test = x_delta + y_delta;

    return rot_test > rot_threshold;
}

bool FSPoserAnimator::userSetBaseRotationToZero(LLVOAvatar* avatar, const FSPoserJoint& joint) const
{
    if (!isAvatarSafeToUse(avatar))
        return false;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return false;

    FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint.jointName());
    if (!jointPose)
        return false;

    return jointPose->userHasSetBaseRotationToZero();
}

bool FSPoserAnimator::allBaseRotationsAreZero(LLVOAvatar* avatar) const
{
    if (!isAvatarSafeToUse(avatar))
        return false;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return false;

    bool allStartingRotationsAreZero = posingMotion->allStartingRotationsAreZero();
    if (allStartingRotationsAreZero)
        return true;

    return false;
}

void FSPoserAnimator::setAllAvatarStartingRotationsToZero(LLVOAvatar* avatar)
{
    if (!isAvatarSafeToUse(avatar))
        return;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return;

    posingMotion->setAllRotationsToZeroAndClearUndo();

    for (size_t index = 0; index != PoserJoints.size(); ++index)
    {
        auto boneType     = PoserJoints[index].boneType();
        bool setBvhToLock = boneType == BODY || boneType == WHOLEAVATAR;
        if (setBvhToLock)
            continue; // setAllRotationsToZeroAndClearUndo specified this is the default behaviour

        FSJointPose* jointPose = posingMotion->getJointPoseByJointName(PoserJoints[index].jointName());
        if (!jointPose)
            continue;

        posingMotion->setJointBvhLock(jointPose, false);
    }

}

void FSPoserAnimator::recaptureJoint(LLVOAvatar* avatar, const FSPoserJoint& joint, E_BoneAxisTranslation translation, S32 negation)
{
    if (!isAvatarSafeToUse(avatar))
        return;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return;

    FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint.jointName());
    if (!jointPose)
        return;

    jointPose->recaptureJoint();
    setPosingAvatarJoint(avatar, joint, true);
}

void FSPoserAnimator::recaptureJointAsDelta(LLVOAvatar* avatar, const FSPoserJoint* joint, bool resetBaseRotationToZero,
                                            E_BoneDeflectionStyles style)
{
    if (!isAvatarSafeToUse(avatar))
        return;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return;

    FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint->jointName());
    if (!jointPose)
        return;

    LLQuaternion deltaRot = jointPose->recaptureJointAsDelta(resetBaseRotationToZero);

    deRotateWorldLockedDescendants(joint, posingMotion, deltaRot);

    if (style == NONE || style == DELTAMODE)
        return;

    FSJointPose* oppositeJointPose = posingMotion->getJointPoseByJointName(joint->mirrorJointName());
    if (!oppositeJointPose)
        return;

    switch (style)
    {
        case SYMPATHETIC:
        case SYMPATHETIC_DELTA:
            oppositeJointPose->cloneRotationFrom(jointPose);
            break;

        case MIRROR:
        case MIRROR_DELTA:
            oppositeJointPose->mirrorRotationFrom(jointPose);
            break;

        default:
            break;
    }
}

LLVector3 FSPoserAnimator::getJointExportRotation(LLVOAvatar* avatar, const FSPoserJoint& joint, bool lockWholeAvatar) const
{
    auto rotation = getJointRotation(avatar, joint, SWAP_NOTHING, NEGATE_NOTHING);
    if (exportRotationWillLockJoint(avatar, joint))
        return rotation;

    LLVector3 vec3;
    if (!isAvatarSafeToUse(avatar))
        return vec3;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return vec3;

    FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint.jointName());
    if (!jointPose)
        return vec3;

    if (!jointPose->userHasSetBaseRotationToZero())
        return vec3;

    if (lockWholeAvatar && joint.boneType() == WHOLEAVATAR)
        return LLVector3(DEG_TO_RAD * 0.295f, 0.f, 0.f);

    F32 minimumRotation = DEG_TO_RAD * 0.65f / llmax((F32)getChildJointDepth(&joint, 0) * 0.33f, 1.f);

    return LLVector3(minimumRotation, 0.f, 0.f);
}

LLVector3 FSPoserAnimator::getJointRotation(LLVOAvatar* avatar, const FSPoserJoint& joint, E_BoneAxisTranslation translation, S32 negation) const
{
    LLVector3 vec3;
    if (!isAvatarSafeToUse(avatar))
        return vec3;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return vec3;

    FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint.jointName());
    if (!jointPose)
        return vec3;
 
    return translateRotationFromQuaternion(translation, negation, jointPose->getPublicRotation());
}

void FSPoserAnimator::setJointRotation(LLVOAvatar* avatar, const FSPoserJoint* joint, const LLVector3& absRotation,
                                       const LLVector3& deltaRotation, E_BoneDeflectionStyles deflectionStyle,
                                       E_BoneAxisTranslation translation, S32 negation, bool resetBaseRotationToZero, E_RotationStyle rotationStyle)
{
    if (!isAvatarSafeToUse(avatar))
        return;
    if (!joint)
        return;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return;

    FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint->jointName());
    if (!jointPose)
        return;

    LLQuaternion absRot = translateRotationToQuaternion(translation, negation, absRotation);
    LLQuaternion deltaRot = translateRotationToQuaternion(translation, negation, deltaRotation);
    switch (deflectionStyle)
    {
        case SYMPATHETIC:
        case MIRROR:
            if (rotationStyle == DELTAIC_ROT)
                jointPose->setPublicRotation(resetBaseRotationToZero, deltaRot * jointPose->getPublicRotation());
            else
                jointPose->setPublicRotation(resetBaseRotationToZero, absRot);

            break;

        case SYMPATHETIC_DELTA:
        case MIRROR_DELTA:
            jointPose->setPublicRotation(resetBaseRotationToZero, deltaRot * jointPose->getPublicRotation());
            break;

        case DELTAMODE:
            jointPose->setPublicRotation(resetBaseRotationToZero, deltaRot * jointPose->getPublicRotation());
            deRotateWorldLockedDescendants(joint, posingMotion, deltaRot);
            return;

        case NONE:
        default:
            if (rotationStyle == DELTAIC_ROT)
                jointPose->setPublicRotation(resetBaseRotationToZero, deltaRot * jointPose->getPublicRotation());
            else
                jointPose->setPublicRotation(resetBaseRotationToZero, absRot);

            deRotateWorldLockedDescendants(joint, posingMotion, deltaRot);
            return;
    }

    deRotateWorldLockedDescendants(joint, posingMotion, deltaRot);

    auto         oppositePoserJoint = getPoserJointByName(joint->mirrorJointName());
    FSJointPose* oppositeJointPose = posingMotion->getJointPoseByJointName(joint->mirrorJointName());
    if (!oppositeJointPose)
        return;

    LLQuaternion inv_quat = LLQuaternion(-deltaRot.mQ[VX], deltaRot.mQ[VY], -deltaRot.mQ[VZ], deltaRot.mQ[VW]);
    switch (deflectionStyle)
    {
        case SYMPATHETIC:
            oppositeJointPose->cloneRotationFrom(jointPose);
            if (oppositePoserJoint)
                deRotateWorldLockedDescendants(oppositePoserJoint, posingMotion, deltaRot);
            break;

        case SYMPATHETIC_DELTA:
            oppositeJointPose->setPublicRotation(resetBaseRotationToZero, deltaRot * oppositeJointPose->getPublicRotation());
            if (oppositePoserJoint)
                deRotateWorldLockedDescendants(oppositePoserJoint, posingMotion, deltaRot);
            break;

        case MIRROR:
            oppositeJointPose->mirrorRotationFrom(jointPose);
            if (oppositePoserJoint)
                deRotateWorldLockedDescendants(oppositePoserJoint, posingMotion, inv_quat);
            break;

        case MIRROR_DELTA:
            oppositeJointPose->setPublicRotation(resetBaseRotationToZero, inv_quat * oppositeJointPose->getPublicRotation());
            if (oppositePoserJoint)
                deRotateWorldLockedDescendants(oppositePoserJoint, posingMotion, inv_quat);
            break;

        default:
            break;
    }
}

void FSPoserAnimator::reflectJoint(LLVOAvatar* avatar, const FSPoserJoint* joint)
{
    if (!isAvatarSafeToUse(avatar))
        return;

    if (!joint)
        return;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return;

    FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint->jointName());
    if (!jointPose)
        return;

    jointPose->reflectRotation();
    FSJointPose* oppositeJointPose = posingMotion->getJointPoseByJointName(joint->mirrorJointName());

    if (oppositeJointPose)
    {
        oppositeJointPose->reflectRotation();
        jointPose->swapRotationWith(oppositeJointPose);
    }
}

void FSPoserAnimator::symmetrizeLeftToRightOrRightToLeft(LLVOAvatar* avatar, bool rightToLeft)
{
    if (!isAvatarSafeToUse(avatar))
        return;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return;

    for (size_t index = 0; index != PoserJoints.size(); ++index)
    {
        if (!PoserJoints[index].dontFlipOnMirror())
            continue;

        bool currentlyPosing = isPosingAvatarJoint(avatar, PoserJoints[index]);
        if (!currentlyPosing)
            continue;

        auto oppositeJoint = getPoserJointByName(PoserJoints[index].mirrorJointName());
        if (!oppositeJoint)
            continue;

        bool currentlyPosingOppositeJoint = isPosingAvatarJoint(avatar, *oppositeJoint);
        if (!currentlyPosingOppositeJoint)
            continue;

        FSJointPose* rightJointPose = posingMotion->getJointPoseByJointName(PoserJoints[index].jointName());
        FSJointPose* leftJointPose = posingMotion->getJointPoseByJointName(oppositeJoint->jointName());

        if (!leftJointPose || !rightJointPose)
            return;

        if (rightToLeft)
            leftJointPose->mirrorRotationFrom(rightJointPose);
        else
            rightJointPose->mirrorRotationFrom(leftJointPose);
    }
}

void FSPoserAnimator::flipEntirePose(LLVOAvatar* avatar)
{
    if (!isAvatarSafeToUse(avatar))
        return;

    for (size_t index = 0; index != PoserJoints.size(); ++index)
    {
        if (PoserJoints[index].dontFlipOnMirror()) // we only flip one side.
            continue;

        bool currentlyPosing = isPosingAvatarJoint(avatar, PoserJoints[index]);
        if (!currentlyPosing)
            continue;

        auto oppositeJoint = getPoserJointByName(PoserJoints[index].mirrorJointName());
        if (oppositeJoint)
        {
            bool currentlyPosingOppositeJoint = isPosingAvatarJoint(avatar, *oppositeJoint);
            if (!currentlyPosingOppositeJoint)
                continue;
        }

        reflectJoint(avatar, &PoserJoints[index]);
    }
}

// from the UI to the bone, the inverse translation, the un-swap, the backwards
LLQuaternion FSPoserAnimator::translateRotationToQuaternion(E_BoneAxisTranslation translation, S32 negation, LLVector3 rotation)
{
    if (negation & NEGATE_ALL)
    {
        rotation.mV[VX] *= -1;
        rotation.mV[VY] *= -1;
        rotation.mV[VZ] *= -1;
    }
    else
    {
        if (negation & NEGATE_YAW)
            rotation.mV[VX] *= -1;
        if (negation & NEGATE_PITCH)
            rotation.mV[VY] *= -1;
        if (negation & NEGATE_ROLL)
            rotation.mV[VZ] *= -1;
    }

    LLMatrix3    rot_mat;
    switch (translation)
    {
        case SWAP_YAW_AND_ROLL:
            rot_mat = LLMatrix3(rotation.mV[VZ], rotation.mV[VY], rotation.mV[VX]);
            break;

        case SWAP_YAW_AND_PITCH:
            rot_mat = LLMatrix3(rotation.mV[VY], rotation.mV[VX], rotation.mV[VZ]);
            break;

        case SWAP_ROLL_AND_PITCH:
            rot_mat = LLMatrix3(rotation.mV[VX], rotation.mV[VZ], rotation.mV[VY]);
            break;

        case SWAP_X2Y_Y2Z_Z2X:
            rot_mat = LLMatrix3(rotation.mV[VZ], rotation.mV[VX], rotation.mV[VY]);
            break;

        case SWAP_X2Z_Y2X_Z2Y:
            rot_mat = LLMatrix3(rotation.mV[VY], rotation.mV[VZ], rotation.mV[VX]);
            break;

        case SWAP_NOTHING:
        default:
            rot_mat = LLMatrix3(rotation.mV[VX], rotation.mV[VY], rotation.mV[VZ]);
            break;
    }

    LLQuaternion rot_quat;
    rot_quat = LLQuaternion(rot_mat) * rot_quat;
    rot_quat.normalize();

    return rot_quat;
}

// from the bone to the UI; this is the 'forwards' use of the enum
LLVector3 FSPoserAnimator::translateRotationFromQuaternion(E_BoneAxisTranslation translation, S32 negation, const LLQuaternion& rotation) const
{
    LLVector3 vec3;

    switch (translation)
    {
        case SWAP_YAW_AND_ROLL:
            rotation.getEulerAngles(&vec3.mV[VZ], &vec3.mV[VY], &vec3.mV[VX]);
            break;

        case SWAP_YAW_AND_PITCH:
            rotation.getEulerAngles(&vec3.mV[VY], &vec3.mV[VX], &vec3.mV[VZ]);
            break;

        case SWAP_ROLL_AND_PITCH:
            rotation.getEulerAngles(&vec3.mV[VX], &vec3.mV[VZ], &vec3.mV[VY]);
            break;

        case SWAP_X2Y_Y2Z_Z2X:
            rotation.getEulerAngles(&vec3.mV[VZ], &vec3.mV[VX], &vec3.mV[VY]);
            break;

        case SWAP_X2Z_Y2X_Z2Y:
            rotation.getEulerAngles(&vec3.mV[VY], &vec3.mV[VZ], &vec3.mV[VX]);
            break;

        case SWAP_NOTHING:
        default:
            rotation.getEulerAngles(&vec3.mV[VX], &vec3.mV[VY], &vec3.mV[VZ]);
            break;
    }

    if (negation & NEGATE_ALL)
    {
        vec3.mV[VX] *= -1;
        vec3.mV[VY] *= -1;
        vec3.mV[VZ] *= -1;
    }
    else
    {
        if (negation & NEGATE_YAW)
            vec3.mV[VX] *= -1;
        if (negation & NEGATE_PITCH)
            vec3.mV[VY] *= -1;
        if (negation & NEGATE_ROLL)
            vec3.mV[VZ] *= -1;
    }

    return vec3;
}

LLVector3 FSPoserAnimator::getJointScale(LLVOAvatar* avatar, const FSPoserJoint& joint) const
{
    LLVector3 scale;
    if (!isAvatarSafeToUse(avatar))
        return scale;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return scale;

    FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint.jointName());
    if (!jointPose)
        return scale;

    return jointPose->getPublicScale();
}

void FSPoserAnimator::setJointScale(LLVOAvatar* avatar, const FSPoserJoint* joint, const LLVector3& scale, E_BoneDeflectionStyles style)
{
    if (!isAvatarSafeToUse(avatar))
        return;
    if (!joint)
        return;

    std::string jn = joint->jointName();
    if (jn.empty())
        return;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return;

    FSJointPose* jointPose = posingMotion->getJointPoseByJointName(jn);
    if (!jointPose)
        return;

    jointPose->setPublicScale(scale);
    FSJointPose* oppositeJointPose = posingMotion->getJointPoseByJointName(joint->mirrorJointName());
    if (!oppositeJointPose)
        return;

    switch (style)
    {
        case SYMPATHETIC:
        case MIRROR:
        case SYMPATHETIC_DELTA:
        case MIRROR_DELTA:
            oppositeJointPose->setPublicScale(scale);
            break;

        case DELTAMODE:
        case NONE:
        default:
            return;
    }
}

bool FSPoserAnimator::tryGetJointSaveVectors(LLVOAvatar* avatar, const FSPoserJoint& joint, LLVector3* rot, LLVector3* pos,
                                             LLVector3* scale, bool* baseRotationIsZero)
{
    if (!rot || !pos || !scale)
        return false;

    if (!isAvatarSafeToUse(avatar))
        return false;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return false;

    FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint.jointName());
    if (!jointPose)
        return false;

    LLQuaternion rotationDelta = jointPose->getPublicRotation();
    rotationDelta.getEulerAngles(&rot->mV[VX], &rot->mV[VY], &rot->mV[VZ]);
    pos->set(jointPose->getPublicPosition());
    scale->set(jointPose->getPublicScale());
    *baseRotationIsZero = jointPose->isBaseRotationZero();

    return true;
}

void FSPoserAnimator::loadJointRotation(LLVOAvatar* avatar, const FSPoserJoint* joint, bool setBaseToZero, LLVector3 rotation)
{
    if (!isAvatarSafeToUse(avatar) || !joint)
        return;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return;

    FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint->jointName());
    if (!jointPose)
        return;

    jointPose->purgeUndoQueue();

    LLQuaternion rot = translateRotationToQuaternion(SWAP_NOTHING, NEGATE_NOTHING, rotation);
    jointPose->setPublicRotation(setBaseToZero, rot);
}

void FSPoserAnimator::loadJointPosition(LLVOAvatar* avatar, const FSPoserJoint* joint, bool loadPositionAsDelta, LLVector3 position)
{
    if (!isAvatarSafeToUse(avatar) || !joint)
        return;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return;

    FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint->jointName());
    if (!jointPose)
        return;

    if (loadPositionAsDelta)
        jointPose->setPublicPosition(position);
    else
        jointPose->setPublicPosition(position);
}

void FSPoserAnimator::loadJointScale(LLVOAvatar* avatar, const FSPoserJoint* joint, bool loadScaleAsDelta, LLVector3 scale)
{
    if (!isAvatarSafeToUse(avatar) || !joint)
        return;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return;

    FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint->jointName());
    if (!jointPose)
        return;

    if (loadScaleAsDelta)
        jointPose->setPublicScale(scale);
    else
        jointPose->setPublicScale(scale);
}

const FSPoserAnimator::FSPoserJoint* FSPoserAnimator::getPoserJointByName(const std::string& jointName) const
{
    for (size_t index = 0; index != PoserJoints.size(); ++index)
    {
        if (boost::iequals(PoserJoints[index].jointName(), jointName))
            return &PoserJoints[index];
    }

    return nullptr;
}

bool FSPoserAnimator::tryPosingAvatar(LLVOAvatar* avatar)
{
    if (!isAvatarSafeToUse(avatar))
        return false;

    FSPosingMotion* posingMotion = findOrCreatePosingMotion(avatar);
    if (!posingMotion)
        return false;

    if (posingMotion->isStopped())
    {
        if (avatar->isSelf())
            gAgent.stopFidget();

        avatar->startDefaultMotions();
        avatar->startMotion(posingMotion->motionId());

        return true;
    }

    return false;
}

void FSPoserAnimator::stopPosingAvatar(LLVOAvatar *avatar)
{
    if (!avatar || avatar->isDead())
        return;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return;

    avatar->stopMotion(posingMotion->motionId());
}

bool FSPoserAnimator::isPosingAvatar(LLVOAvatar* avatar) const
{
    if (!isAvatarSafeToUse(avatar))
        return false;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return false;

    return !posingMotion->isStopped();
}

FSPosingMotion* FSPoserAnimator::getPosingMotion(LLVOAvatar* avatar) const
{
    if (!isAvatarSafeToUse(avatar))
        return nullptr;

    if (sAvatarIdToRegisteredAnimationId.find(avatar->getID()) == sAvatarIdToRegisteredAnimationId.end())
        return nullptr;

    return dynamic_cast<FSPosingMotion*>(avatar->findMotion(sAvatarIdToRegisteredAnimationId[avatar->getID()]));
}

FSPosingMotion* FSPoserAnimator::findOrCreatePosingMotion(LLVOAvatar* avatar)
{
    FSPosingMotion* motion = getPosingMotion(avatar);
    if (motion)
        return motion;

    LLTransactionID mTransactionID;
    mTransactionID.generate();
    LLAssetID animationAssetId = mTransactionID.makeAssetID(gAgent.getSecureSessionID());

    if (avatar->registerMotion(animationAssetId, FSPosingMotion::create))
        sAvatarIdToRegisteredAnimationId[avatar->getID()] = animationAssetId;

    return dynamic_cast<FSPosingMotion*>(avatar->createMotion(animationAssetId));

}

bool FSPoserAnimator::isAvatarSafeToUse(LLVOAvatar* avatar) const
{
    if (!avatar)
        return false;
    if (avatar->isDead())
        return false;
    if (avatar->getRegion() != gAgent.getRegion())
        return false;

    return true;
}

int FSPoserAnimator::getChildJointDepth(const FSPoserJoint* joint, int depth) const
{
    size_t numberOfBvhChildNodes = joint->bvhChildren().size();
    if (numberOfBvhChildNodes < 1)
        return depth;

    depth++;

    for (size_t index = 0; index != numberOfBvhChildNodes; ++index)
    {
        auto nextJoint = getPoserJointByName(joint->bvhChildren()[index]);
        if (!nextJoint)
            continue;

        depth = llmax(depth, getChildJointDepth(nextJoint, depth));
    }

    return depth;
}

void FSPoserAnimator::deRotateWorldLockedDescendants(const FSPoserJoint* joint, FSPosingMotion* posingMotion, LLQuaternion rotationChange)
{
    size_t numberOfBvhChildNodes = joint->bvhChildren().size();
    if (numberOfBvhChildNodes < 1)
        return;

    FSJointPose* parentJoint = posingMotion->getJointPoseByJointName(joint->jointName());
    if (!parentJoint)
        return;

    LLJoint* pJoint = parentJoint->getJointState()->getJoint();

    for (size_t index = 0; index != numberOfBvhChildNodes; ++index)
    {
        auto nextJoint = getPoserJointByName(joint->bvhChildren()[index]);
        if (!nextJoint)
            continue;

        deRotateJointOrFirstLockedChild(nextJoint, posingMotion, pJoint->getWorldRotation(), rotationChange);
    }
}

void FSPoserAnimator::deRotateJointOrFirstLockedChild(const FSPoserJoint* joint, FSPosingMotion* posingMotion, LLQuaternion rotatedParentWorldRot, LLQuaternion rotationChange)
{
    FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint->jointName());
    if (!jointPose)
        return;

    if (jointPose->getWorldRotationLockState())
    {
        LLQuaternion worldRotOfThisJoint  = jointPose->getJointState()->getJoint()->getWorldRotation();
        LLQuaternion differenceInWorldRot = worldRotOfThisJoint * ~rotatedParentWorldRot;
        LLQuaternion rotDiffInChildFrame  = differenceInWorldRot * rotationChange * ~differenceInWorldRot;
        rotDiffInChildFrame.conjugate();

        jointPose->setPublicRotation(false, rotDiffInChildFrame * jointPose->getPublicRotation());
        return;
    }

    size_t numberOfBvhChildNodes = joint->bvhChildren().size();
    if (numberOfBvhChildNodes < 1)
        return;

    for (size_t index = 0; index != numberOfBvhChildNodes; ++index)
    {
        auto nextJoint = getPoserJointByName(joint->bvhChildren()[index]);
        if (!nextJoint)
            continue;

        deRotateJointOrFirstLockedChild(nextJoint, posingMotion, rotatedParentWorldRot, rotationChange);
    }
}

void FSPoserAnimator::undoOrRedoWorldLockedDescendants(const FSPoserJoint& joint, FSPosingMotion* posingMotion, bool redo)
{
    size_t numberOfBvhChildNodes = joint.bvhChildren().size();
    if (numberOfBvhChildNodes < 1)
        return;

    for (size_t index = 0; index != numberOfBvhChildNodes; ++index)
    {
        auto nextJoint = getPoserJointByName(joint.bvhChildren()[index]);
        if (!nextJoint)
            continue;

        undoOrRedoJointOrFirstLockedChild(*nextJoint, posingMotion, redo);
    }
}

void FSPoserAnimator::undoOrRedoJointOrFirstLockedChild(const FSPoserJoint& joint, FSPosingMotion* posingMotion, bool redo)
{
    FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint.jointName());
    if (!jointPose)
        return;

    if (jointPose->getWorldRotationLockState())
    {
        redo ? jointPose->redoLastChange() : jointPose->undoLastChange();
        return;
    }

    size_t numberOfBvhChildNodes = joint.bvhChildren().size();
    if (numberOfBvhChildNodes < 1)
        return;

    for (size_t index = 0; index != numberOfBvhChildNodes; ++index)
    {
        auto nextJoint = getPoserJointByName(joint.bvhChildren()[index]);
        if (!nextJoint)
            continue;

        undoOrRedoJointOrFirstLockedChild(*nextJoint, posingMotion, redo);
    }
}
