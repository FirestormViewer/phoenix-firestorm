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

void FSPoserAnimator::resetAvatarJoint(LLVOAvatar* avatar, const FSPoserJoint& joint)
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

    jointPose->setPositionDelta(LLVector3());
    jointPose->setRotationDelta(LLQuaternion());
}

void FSPoserAnimator::undoLastJointRotation(LLVOAvatar* avatar, const FSPoserJoint& joint, E_BoneDeflectionStyles style)
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

    jointPose->undoLastRotationChange();

    if (style == NONE || style == DELTAMODE)
        return;

    FSJointPose* oppositeJointPose = posingMotion->getJointPoseByJointName(joint.mirrorJointName());
    if (!oppositeJointPose)
        return;

    oppositeJointPose->undoLastRotationChange();
}

void FSPoserAnimator::undoLastJointPosition(LLVOAvatar* avatar, const FSPoserJoint& joint, E_BoneDeflectionStyles style)
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

    jointPose->undoLastPositionChange();

    if (style == NONE || style == DELTAMODE)
        return;

    FSJointPose* oppositeJointPose = posingMotion->getJointPoseByJointName(joint.mirrorJointName());
    if (!oppositeJointPose)
        return;

    oppositeJointPose->undoLastPositionChange();
}

void FSPoserAnimator::undoLastJointScale(LLVOAvatar* avatar, const FSPoserJoint& joint, E_BoneDeflectionStyles style)
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

    jointPose->undoLastScaleChange();

    if (style == NONE || style == DELTAMODE)
        return;

    FSJointPose* oppositeJointPose = posingMotion->getJointPoseByJointName(joint.mirrorJointName());
    if (!oppositeJointPose)
        return;

    oppositeJointPose->undoLastScaleChange();
}

void FSPoserAnimator::resetJointPosition(LLVOAvatar* avatar, const FSPoserJoint& joint, E_BoneDeflectionStyles style)
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

    jointPose->setPositionDelta(LLVector3());

    if (style == NONE || style == DELTAMODE)
        return;

    FSJointPose* oppositeJointPose = posingMotion->getJointPoseByJointName(joint.mirrorJointName());
    if (!oppositeJointPose)
        return;

    oppositeJointPose->setPositionDelta(LLVector3());
}

void FSPoserAnimator::resetJointScale(LLVOAvatar* avatar, const FSPoserJoint& joint, E_BoneDeflectionStyles style)
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

    jointPose->setScaleDelta(LLVector3());

    if (style == NONE || style == DELTAMODE)
        return;

    FSJointPose* oppositeJointPose = posingMotion->getJointPoseByJointName(joint.mirrorJointName());
    if (!oppositeJointPose)
        return;

    oppositeJointPose->setScaleDelta(LLVector3());
}

bool FSPoserAnimator::canRedoJointRotation(LLVOAvatar* avatar, const FSPoserJoint& joint)
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

    return jointPose->canRedoRotation();
}

void FSPoserAnimator::redoLastJointRotation(LLVOAvatar* avatar, const FSPoserJoint& joint, E_BoneDeflectionStyles style)
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

    jointPose->redoLastRotationChange();

    if (style == NONE || style == DELTAMODE)
        return;

    FSJointPose* oppositeJointPose = posingMotion->getJointPoseByJointName(joint.mirrorJointName());
    if (!oppositeJointPose)
        return;

    oppositeJointPose->redoLastRotationChange();
}

void FSPoserAnimator::redoLastJointPosition(LLVOAvatar* avatar, const FSPoserJoint& joint, E_BoneDeflectionStyles style)
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

    jointPose->redoLastPositionChange();

    if (style == NONE || style == DELTAMODE)
        return;

    FSJointPose* oppositeJointPose = posingMotion->getJointPoseByJointName(joint.mirrorJointName());
    if (!oppositeJointPose)
        return;

    oppositeJointPose->redoLastPositionChange();
}

void FSPoserAnimator::redoLastJointScale(LLVOAvatar* avatar, const FSPoserJoint& joint, E_BoneDeflectionStyles style)
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

    jointPose->redoLastScaleChange();

    if (style == NONE || style == DELTAMODE)
        return;

    FSJointPose* oppositeJointPose = posingMotion->getJointPoseByJointName(joint.mirrorJointName());
    if (!oppositeJointPose)
        return;

    oppositeJointPose->redoLastScaleChange();
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

    return jointPose->getPositionDelta();
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

    LLVector3 positionDelta = jointPose->getPositionDelta() - position;

    switch (style)
    {
        case MIRROR:
        case MIRROR_DELTA:
        case SYMPATHETIC_DELTA:
        case SYMPATHETIC:
            jointPose->setPositionDelta(position);
            break;

        case DELTAMODE:
        case NONE:
        default:
            jointPose->setPositionDelta(position);
            return;
    }

    FSJointPose* oppositeJointPose = posingMotion->getJointPoseByJointName(joint->mirrorJointName());
    if (!oppositeJointPose)
        return;

    LLVector3 oppositeJointPosition = oppositeJointPose->getPositionDelta();

    switch (style)
    {
        case MIRROR:
        case MIRROR_DELTA:
            oppositeJointPose->setPositionDelta(oppositeJointPosition + positionDelta);
            break;

        case SYMPATHETIC_DELTA:
        case SYMPATHETIC:
            oppositeJointPose->setPositionDelta(oppositeJointPosition - positionDelta);
            break;

        default:
            break;
    }
}

bool FSPoserAnimator::baseRotationIsZero(LLVOAvatar* avatar, const FSPoserJoint& joint) const
{
    if (!isAvatarSafeToUse(avatar))
        return false;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return false;

    FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint.jointName());
    if (!jointPose)
        return false;

    return jointPose->isBaseRotationZero();
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

    posingMotion->setAllRotationsToZero();
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
 
    return translateRotationFromQuaternion(translation, negation, jointPose->getRotationDelta());
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

    if (resetBaseRotationToZero)
        jointPose->zeroBaseRotation();

    LLQuaternion absRot = translateRotationToQuaternion(translation, negation, absRotation);
    LLQuaternion deltaRot = translateRotationToQuaternion(translation, negation, deltaRotation);
    switch (deflectionStyle)
    {
        case SYMPATHETIC:
        case MIRROR:
            if (rotationStyle == DELTAIC_ROT)
                jointPose->setRotationDelta(deltaRot * jointPose->getRotationDelta());
            else
                jointPose->setRotationDelta(absRot);

            break;

        case SYMPATHETIC_DELTA:
        case MIRROR_DELTA:
            jointPose->setRotationDelta(deltaRot * jointPose->getRotationDelta());
            break;

        case DELTAMODE:
            jointPose->setRotationDelta(deltaRot * jointPose->getRotationDelta());
            return;

        case NONE:
        default:
            if (rotationStyle == DELTAIC_ROT)
                jointPose->setRotationDelta(deltaRot * jointPose->getRotationDelta());
            else
                jointPose->setRotationDelta(absRot);

            return;
    }

    FSJointPose* oppositeJointPose = posingMotion->getJointPoseByJointName(joint->mirrorJointName());
    if (!oppositeJointPose)
        return;

    LLQuaternion inv_quat;
    switch (deflectionStyle)
    {
        case SYMPATHETIC:
            oppositeJointPose->cloneRotationFrom(jointPose);
            break;

        case SYMPATHETIC_DELTA:
            oppositeJointPose->setRotationDelta(deltaRot * oppositeJointPose->getRotationDelta());
            break;

        case MIRROR:
            oppositeJointPose->mirrorRotationFrom(jointPose);
            break;

        case MIRROR_DELTA:
            inv_quat = LLQuaternion(-deltaRot.mQ[VX], deltaRot.mQ[VY], -deltaRot.mQ[VZ], deltaRot.mQ[VW]);
            oppositeJointPose->setRotationDelta(inv_quat * oppositeJointPose->getRotationDelta());
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

    return jointPose->getScaleDelta();
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

    jointPose->setScaleDelta(scale);
    FSJointPose* oppositeJointPose = posingMotion->getJointPoseByJointName(joint->mirrorJointName());
    if (!oppositeJointPose)
        return;

    switch (style)
    {
        case SYMPATHETIC:
        case MIRROR:
        case SYMPATHETIC_DELTA:
        case MIRROR_DELTA:
            oppositeJointPose->setScaleDelta(scale);
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

    LLQuaternion rotationDelta = jointPose->getRotationDelta();
    rotationDelta.getEulerAngles(&rot->mV[VX], &rot->mV[VY], &rot->mV[VZ]);
    pos->set(jointPose->getPositionDelta());
    scale->set(jointPose->getScaleDelta());
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

    if (setBaseToZero)
        jointPose->zeroBaseRotation();

    LLQuaternion rot = translateRotationToQuaternion(SWAP_NOTHING, NEGATE_NOTHING, rotation);
    jointPose->setRotationDelta(rot);
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
        jointPose->setPositionDelta(position);
    else
        jointPose->setPositionDelta(position);
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
        jointPose->setScaleDelta(scale);
    else
        jointPose->setScaleDelta(scale);
}

const FSPoserAnimator::FSPoserJoint* FSPoserAnimator::getPoserJointByName(const std::string& jointName)
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
