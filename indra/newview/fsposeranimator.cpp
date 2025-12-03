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
#include "llagentcamera.h"
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

bool FSPoserAnimator::hasJointBeenChanged(LLVOAvatar* avatar, const FSPoserJoint& joint)
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

    return jointPose->getJointModified();
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

    if (jointPose->undoLastChange())
        undoOrRedoWorldLockedDescendants(joint, posingMotion, false);

    if (style == NONE || style == DELTAMODE)
        return;

    FSJointPose* oppositeJointPose = posingMotion->getJointPoseByJointName(joint.mirrorJointName());
    if (!oppositeJointPose)
        return;

    if (!oppositeJointPose->undoLastChange())
        return;

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

void FSPoserAnimator::setJointPosition(LLVOAvatar* avatar, const FSPoserJoint* joint, const LLVector3& position, E_PoserReferenceFrame frame,
                                       E_BoneDeflectionStyles style)
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

    LLVector3 jointPosition = jointPose->getPublicPosition();
    LLVector3 positionDelta = jointPosition - position;

    switch (style)
    {
        case MIRROR:
        case MIRROR_DELTA:
        case SYMPATHETIC_DELTA:
        case SYMPATHETIC:
            jointPose->setPublicPosition(jointPosition - positionDelta);
            break;

        case DELTAMODE:
        case NONE:
        default:
            jointPose->setPublicPosition(jointPosition - positionDelta);
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

bool FSPoserAnimator::getRotationIsMirrored(LLVOAvatar* avatar, const FSPoserJoint& joint) const
{
    if (!isAvatarSafeToUse(avatar))
        return false;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return false;

    FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint.jointName());
    if (!jointPose)
        return false;

    return jointPose->getRotationMirrorState();
}

void FSPoserAnimator::setRotationIsMirrored(LLVOAvatar* avatar, const FSPoserJoint& joint, bool newState)
{
    if (!isAvatarSafeToUse(avatar))
        return;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return;

    FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint.jointName());
    if (!jointPose)
        return;

    jointPose->setRotationMirrorState(newState);
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
    mPosingState.purgeMotionStates(avatar);

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

void FSPoserAnimator::recaptureJoint(LLVOAvatar* avatar, const FSPoserJoint& joint)
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

void FSPoserAnimator::updateJointFromManip(LLVOAvatar* avatar, const FSPoserJoint* joint, bool resetBaseRotationToZero,
                                           E_BoneDeflectionStyles style, E_PoserReferenceFrame frame, const LLQuaternion& rotation,
                                           const LLVector3& position, const LLVector3& scale)
{
    if (!isAvatarSafeToUse(avatar))
        return;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return;

    FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint->jointName());
    if (!jointPose)
        return;

    LLQuaternion framedRotation = changeToRotationFrame(avatar, rotation, frame, jointPose);
    jointPose->setPublicRotation(resetBaseRotationToZero, framedRotation * jointPose->getPublicRotation());

    deRotateWorldLockedDescendants(joint, posingMotion, framedRotation);

    if (style == NONE || style == DELTAMODE)
        return;

    auto         oppositePoserJoint = getPoserJointByName(joint->mirrorJointName());
    FSJointPose* oppositeJointPose = posingMotion->getJointPoseByJointName(joint->mirrorJointName());
    if (!oppositeJointPose)
        return;

    LLQuaternion mirroredRotation = LLQuaternion(-framedRotation.mQ[VX], framedRotation.mQ[VY], -framedRotation.mQ[VZ], framedRotation.mQ[VW]);
    switch (style)
    {
        case SYMPATHETIC:
        case SYMPATHETIC_DELTA:
            oppositeJointPose->setPublicRotation(resetBaseRotationToZero, framedRotation * oppositeJointPose->getPublicRotation());
            if (oppositePoserJoint)
                deRotateWorldLockedDescendants(oppositePoserJoint, posingMotion, framedRotation);
            break;

        case MIRROR:
        case MIRROR_DELTA:
            oppositeJointPose->setPublicRotation(resetBaseRotationToZero, mirroredRotation * oppositeJointPose->getPublicRotation());
            if (oppositePoserJoint)
                deRotateWorldLockedDescendants(oppositePoserJoint, posingMotion, mirroredRotation);
            break;

        default:
            break;
    }
}

LLQuaternion FSPoserAnimator::getManipGimbalRotation(LLVOAvatar* avatar, const FSPoserJoint* joint, E_PoserReferenceFrame frame)
{
    LLQuaternion globalRot(-1.f, 0.f, 0.f, 0.f);
    if (frame == POSER_FRAME_WORLD)
        return globalRot;

    if (!joint)
        return globalRot;

    if (!isAvatarSafeToUse(avatar))
        return globalRot;

    if (frame == POSER_FRAME_AVATAR)
    {
        LLJoint* pelvis = avatar->getJoint("mPelvis");
        if (pelvis)
            return pelvis->getWorldRotation();

        return globalRot;
    }

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return globalRot;

    FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint->jointName());
    if (!jointPose)
        return globalRot;

    LLJoint* llJoint = jointPose->getJointState()->getJoint();
    if (!llJoint)
        return globalRot;

    if (frame == POSER_FRAME_BONE)
        return llJoint->getWorldRotation();

    LLVector3 skyward(0.f, 0.f, 1.f);
    LLVector3 left(1.f, 0.f, 0.f);
    LLVector3 up, jointToCameraPosition, jointPosition;
    jointPosition         = llJoint->getWorldPosition();
    jointToCameraPosition = jointPosition - gAgentCamera.getCameraPositionAgent();
    jointToCameraPosition.normalize();
    left.setVec(skyward % jointToCameraPosition);
    up.setVec(jointToCameraPosition % left);

    return LLQuaternion(jointToCameraPosition, left, up);
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
 
    return translateRotationFromQuaternion(jointPose, translation, negation, jointPose->getPublicRotation());
}

void FSPoserAnimator::setJointRotation(LLVOAvatar* avatar, const FSPoserJoint* joint, const LLVector3& absRotation,
                                       const LLVector3& deltaRotation, E_BoneDeflectionStyles style, E_PoserReferenceFrame frame,
                                       E_BoneAxisTranslation translation, S32 negation, bool resetBaseRotationToZero,
                                       E_RotationStyle rotationStyle)
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

    bool translationRequiresDelta = frame != POSER_FRAME_BONE;

    LLQuaternion absRot   = translateRotationToQuaternion(avatar, jointPose, frame, translation, negation, absRotation);
    LLQuaternion deltaRot = translateRotationToQuaternion(avatar, jointPose, frame, translation, negation, deltaRotation);
    switch (style)
    {
        case SYMPATHETIC:
        case MIRROR:
            if (rotationStyle == DELTAIC_ROT || translationRequiresDelta)
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
            if (rotationStyle == DELTAIC_ROT || translationRequiresDelta)
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

    LLQuaternion mirroredRotation = LLQuaternion(-deltaRot.mQ[VX], deltaRot.mQ[VY], -deltaRot.mQ[VZ], deltaRot.mQ[VW]);
    switch (style)
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
                deRotateWorldLockedDescendants(oppositePoserJoint, posingMotion, mirroredRotation);
            break;

        case MIRROR_DELTA:
            oppositeJointPose->setPublicRotation(resetBaseRotationToZero, mirroredRotation * oppositeJointPose->getPublicRotation());
            if (oppositePoserJoint)
                deRotateWorldLockedDescendants(oppositePoserJoint, posingMotion, mirroredRotation);
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

// from the UI to the bone. Bone rotations we store are relative to the skeleton (or to T-Pose, if you want to visualize).
LLQuaternion FSPoserAnimator::translateRotationToQuaternion(LLVOAvatar* avatar, FSJointPose* joint, E_PoserReferenceFrame frame,
                                                            E_BoneAxisTranslation translation, S32 negation, LLVector3 rotation)
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

    LLMatrix3 rot_mat;
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

    rot_quat = changeToRotationFrame(avatar, rot_quat, frame, joint);

    return rot_quat;
}

LLQuaternion FSPoserAnimator::changeToRotationFrame(LLVOAvatar* avatar, const LLQuaternion& rotation, E_PoserReferenceFrame frame, FSJointPose* joint)
{
    if (!joint || !avatar)
        return rotation;

    LLJoint* pelvis = avatar->getJoint("mPelvis");
    if (!pelvis)
        return rotation;

    LLVector3 skyward(0.f, 0.f, 1.f);
    LLVector3 left(1.f, 0.f, 0.f);
    LLVector3 forwards(0.f, 1.f, 0.f);
    LLVector3 up, jointToCameraPosition, jointPosition;
    LLQuaternion worldRotOfWorld(forwards, left, skyward);
    LLQuaternion differenceInWorldRot, rotDiffInChildFrame, worldRotOfPelvis, worldRotOfCamera;
    LLQuaternion worldRotOfThisJoint = joint->getJointState()->getJoint()->getWorldRotation();

    switch (frame)
    {
        case POSER_FRAME_WORLD:
            differenceInWorldRot = worldRotOfThisJoint * ~worldRotOfWorld;
            break;

        case POSER_FRAME_AVATAR:
            worldRotOfPelvis     = pelvis->getWorldRotation();
            differenceInWorldRot = worldRotOfThisJoint * ~worldRotOfPelvis;
            break;

        case POSER_FRAME_CAMERA:
            jointPosition  = joint->getJointState()->getJoint()->getWorldPosition();
            jointToCameraPosition = jointPosition - gAgentCamera.getCameraPositionAgent();
            jointToCameraPosition.normalize();
            left.setVec(skyward % jointToCameraPosition);
            up.setVec(jointToCameraPosition % left);

            worldRotOfCamera = LLQuaternion(jointToCameraPosition, left, up);
            differenceInWorldRot = worldRotOfThisJoint * ~worldRotOfCamera;
            break;

        case POSER_FRAME_BONE:
        default:
            return rotation;
    }

    rotDiffInChildFrame = differenceInWorldRot * rotation * ~differenceInWorldRot;
    rotDiffInChildFrame.conjugate();

    return rotDiffInChildFrame;
}

// from the bone to the UI; this is the 'forwards' use of the enum
LLVector3 FSPoserAnimator::translateRotationFromQuaternion(FSJointPose* joint, E_BoneAxisTranslation translation, S32 negation, const LLQuaternion& rotation) const
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

void FSPoserAnimator::setJointScale(LLVOAvatar* avatar, const FSPoserJoint* joint, const LLVector3& scale, E_PoserReferenceFrame frame,
                                    E_BoneDeflectionStyles style)
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

    LLVector3 jointScale = jointPose->getPublicScale();
    LLVector3 scaleDelta = jointScale - scale;

    switch (style)
    {
        case MIRROR:
        case MIRROR_DELTA:
        case SYMPATHETIC_DELTA:
        case SYMPATHETIC:
            jointPose->setPublicScale(jointScale - scaleDelta);
            break;

        case DELTAMODE:
        case NONE:
        default:
            jointPose->setPublicScale(jointScale - scaleDelta);
            return;
    }

    FSJointPose* oppositeJointPose = posingMotion->getJointPoseByJointName(joint->mirrorJointName());
    if (!oppositeJointPose)
        return;

    LLVector3 oppositeJointScale = oppositeJointPose->getPublicScale();

    switch (style)
    {
        case MIRROR:
        case MIRROR_DELTA:
            oppositeJointPose->setPublicScale(oppositeJointScale + scaleDelta);
            break;

        case SYMPATHETIC_DELTA:
        case SYMPATHETIC:
            oppositeJointPose->setPublicScale(oppositeJointScale - scaleDelta);
            break;

        default:
            break;
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

    LLQuaternion rot = translateRotationToQuaternion(avatar, jointPose, POSER_FRAME_BONE, SWAP_NOTHING, NEGATE_NOTHING, rotation);
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
    {
        jointPose->setJointPriority(LLJoint::LOW_PRIORITY);
        jointPose->setBasePosition(position, LLJoint::LOW_PRIORITY);
    }
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
    {
        jointPose->setJointPriority(LLJoint::LOW_PRIORITY);
        jointPose->setBaseScale(scale, LLJoint::LOW_PRIORITY);
        jointPose->setPublicScale(LLVector3::zero);
    }
}

bool FSPoserAnimator::loadPosingState(LLVOAvatar* avatar, bool ignoreOwnership, LLSD pose)
{
    if (!isAvatarSafeToUse(avatar))
        return false;

    mPosingState.purgeMotionStates(avatar);
    mPosingState.restoreMotionStates(avatar, ignoreOwnership, pose);

    return applyStatesToPosingMotion(avatar);
}

bool FSPoserAnimator::applyStatesToPosingMotion(LLVOAvatar* avatar)
{
    if (!isAvatarSafeToUse(avatar))
        return false;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return false;

    bool loadSuccess = mPosingState.applyMotionStatesToPosingMotion(avatar, posingMotion);
    if (loadSuccess)
        applyJointMirrorToBaseRotations(posingMotion);

    return loadSuccess;
}

void FSPoserAnimator::applyJointMirrorToBaseRotations(FSPosingMotion* posingMotion)
{
    for (size_t index = 0; index != PoserJoints.size(); ++index)
    {
        FSJointPose* jointPose = posingMotion->getJointPoseByJointName(PoserJoints[index].jointName());
        if (!jointPose)
            continue;

        if (!jointPose->getRotationMirrorState())
            continue;

        if (PoserJoints[index].dontFlipOnMirror()) // we only flip one side.
            continue;

        jointPose->reflectBaseRotation();
        FSJointPose* oppositeJointPose = posingMotion->getJointPoseByJointName(PoserJoints[index].mirrorJointName());

        if (!oppositeJointPose)
            continue;

        oppositeJointPose->reflectBaseRotation();
        jointPose->swapBaseRotationWith(oppositeJointPose);
    }
}

void FSPoserAnimator::savePosingState(LLVOAvatar* avatar, bool ignoreOwnership, LLSD* saveRecord)
{
    mPosingState.writeMotionStates(avatar, ignoreOwnership, saveRecord);
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

const FSPoserAnimator::FSPoserJoint* FSPoserAnimator::getPoserJointByNumber(LLVOAvatar* avatar, const S32 jointNumber) const
{
    if (!avatar)
        return nullptr;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return nullptr;

    FSJointPose* parentJoint = posingMotion->getJointPoseByJointNumber(jointNumber);
    if (!parentJoint)
        return nullptr;

    return getPoserJointByName(parentJoint->jointName());
}

bool FSPoserAnimator::tryGetJointNumber(LLVOAvatar* avatar, const FSPoserJoint &poserJoint, S32& jointNumber)
{
    if (!avatar)
        return false;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return false;

    FSJointPose* parentJoint = posingMotion->getJointPoseByJointName(poserJoint.jointName());
    if (!parentJoint)
        return false;

    jointNumber = parentJoint->getJointNumber();
    return jointNumber >= 0;
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

        mPosingState.captureMotionStates(avatar);
        avatar->startDefaultMotions();
        avatar->startMotion(posingMotion->motionId());

        return true;
    }

    return false;
}

void FSPoserAnimator::updatePosingState(LLVOAvatar* avatar, const std::vector<FSPoserAnimator::FSPoserJoint*>& jointsRecaptured)
{
    if (!avatar)
        return;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return;

    std::vector<S32> jointNumbersRecaptured;
    for (auto item : jointsRecaptured)
    {
        auto poserJoint = posingMotion->getJointPoseByJointName(item->jointName());
        if (!poserJoint)
            continue;

        jointNumbersRecaptured.push_back(poserJoint->getJointNumber());
    }

    mPosingState.updateMotionStates(avatar, posingMotion, jointNumbersRecaptured);
}

void FSPoserAnimator::stopPosingAvatar(LLVOAvatar* avatar)
{
    if (!avatar || avatar->isDead())
        return;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return;

    mPosingState.purgeMotionStates(avatar);
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

int FSPoserAnimator::getChildJointDepth(const FSPoserJoint* joint, S32 depth) const
{
    size_t numberOfBvhChildNodes = joint->bvhChildren().size();
    if (numberOfBvhChildNodes < 1)
        return depth;

    depth++;

    for (size_t index = 0; index != numberOfBvhChildNodes; ++index)
    {
        auto nextJoint = getPoserJointByName(joint->bvhChildren().at(index));
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
        auto nextJoint = getPoserJointByName(joint->bvhChildren().at(index));
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
        auto nextJoint = getPoserJointByName(joint->bvhChildren().at(index));
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
        auto nextJoint = getPoserJointByName(joint.bvhChildren().at(index));
        if (!nextJoint)
            continue;

        undoOrRedoJointOrFirstLockedChild(*nextJoint, posingMotion, redo);
    }
}

void FSPoserAnimator::undoOrRedoJointOrFirstLockedChild(const FSPoserJoint& joint, FSPosingMotion* posingMotion, bool redo)
{
    if (!posingMotion)
        return;

    FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint.jointName());
    if (!jointPose)
        return;

    if (jointPose->getWorldRotationLockState())
    {
        if (redo)
            jointPose->redoLastChange();
        else
            jointPose->undoLastChange();

        return;
    }

    size_t numberOfBvhChildNodes = joint.bvhChildren().size();
    if (numberOfBvhChildNodes < 1)
        return;

    for (size_t index = 0; index != numberOfBvhChildNodes; ++index)
    {
        auto nextJoint = getPoserJointByName(joint.bvhChildren().at(index));
        if (!nextJoint)
            continue;

        undoOrRedoJointOrFirstLockedChild(*nextJoint, posingMotion, redo);
    }
}
