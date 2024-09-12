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

#include "linden_common.h"
#include "llviewerprecompiledheaders.h"
#include "fsposeranimator.h"
#include "llcharacter.h"
#include "llviewercontrol.h"
#include "llagent.h"
#include "llvoavatarself.h"
#include <llanimationstates.h>

FSPoserAnimator::FSPoserAnimator() {}
FSPoserAnimator::~FSPoserAnimator() {}

bool FSPoserAnimator::isPosingAvatarJoint(LLVOAvatar *avatar, FSPoserJoint joint)
{
    if (!avatar || avatar->isDead())
        return false;

    return _currentlyPosingSelf;
}

void FSPoserAnimator::setPosingAvatarJoint(LLVOAvatar *avatar, FSPoserJoint joint, bool shouldPose)
{
    if (!avatar || avatar->isDead())
        return;

    // TODO: Bust a move. Or don't.
}

LLVector3 FSPoserAnimator::getJointPosition(LLVOAvatar *avatar, FSPoserJoint joint)
{
    LLVector3 pos;
    if (!avatar || avatar->isDead())
        return pos;

    LLJoint *avJoint = gAgentAvatarp->getJoint(JointKey::construct(joint.jointName()));
    if (!avJoint)
        return pos;

    pos = avJoint->getPosition();

    return pos;
}

void FSPoserAnimator::setJointPosition(LLVOAvatar *avatar, const FSPoserJoint *joint, LLVector3 position, E_BoneDeflectionStyles style)
{
    if (!avatar || avatar->isDead())
        return;
    if (!joint)
        return;

    std::string jn      = joint->jointName();
    if (jn.empty())
        return;

    JointKey key     = JointKey::construct(jn);
    LLJoint *avJoint = avatar->getJoint(key);
    if (!avJoint)
        return;
}

LLVector3 FSPoserAnimator::getJointRotation(LLVOAvatar *avatar, FSPoserJoint joint)
{
    // this needs to do this, to be compatible in some part with BD poses
    // LLQuaternion rot = _poserAnimator.getJointRotation(avatar, pj);
    // rot.getEulerAngles(&vec3.mV[VX], &vec3.mV[VZ], &vec3.mV[VY]);

    LLVector3 vec3;
    if (!avatar || avatar->isDead())
        return vec3;

    LLJoint  *avJoint = avatar->getJoint(JointKey::construct(joint.jointName()));
    if (!avJoint)
        return vec3;

    LLQuaternion rot = avJoint->getRotation();
    
    return translateRotationFromQuaternion(joint.boneTranslation(), rot);
}

// from the bone to the UI; this is the 'forwards' use of the enum
LLVector3 FSPoserAnimator::translateRotationFromQuaternion(E_BoneAxisTranslation translation, LLQuaternion rotation)
{
    LLVector3 vec3;

    switch (translation)
    {
        case SWAP_YAW_AND_ROLL:
            rotation.getEulerAngles(&vec3.mV[VY], &vec3.mV[VZ], &vec3.mV[VX]);
            break;

        case SWAP_YAW_AND_PITCH:
            rotation.getEulerAngles(&vec3.mV[VZ], &vec3.mV[VX], &vec3.mV[VY]);
            break;

        case SWAP_ROLL_AND_PITCH:
            rotation.getEulerAngles(&vec3.mV[VX], &vec3.mV[VY], &vec3.mV[VZ]);
            break;

        case SWAP_NOTHING:
        default:
            rotation.getEulerAngles(&vec3.mV[VX], &vec3.mV[VZ], &vec3.mV[VY]);
            break;
    }

    return vec3;
}

void FSPoserAnimator::setJointRotation(LLVOAvatar *avatar, const FSPoserJoint *joint, LLVector3 rotation, E_BoneDeflectionStyles style)
{
    if (!avatar || avatar->isDead())
        return;
    if (!joint)
        return;

    LLJoint *avJoint = avatar->getJoint(JointKey::construct(joint->jointName()));
    if (!avJoint)
        return;

    LLQuaternion rot_quat = translateRotationToQuaternion(joint->boneTranslation(), rotation);
    avJoint->setRotation(rot_quat);
}

// from the UI to the bone, the inverse translation, the un-swap, the backwards
LLQuaternion FSPoserAnimator::translateRotationToQuaternion(E_BoneAxisTranslation translation, LLVector3 rotation)
{
    LLMatrix3    rot_mat;
    switch (translation)
    {
        case SWAP_YAW_AND_ROLL:
            rot_mat = LLMatrix3(rotation.mV[VZ], rotation.mV[VY], -1 * rotation.mV[VX]);
            break;

        case SWAP_YAW_AND_PITCH:
            rot_mat = LLMatrix3(rotation.mV[VY], rotation.mV[VX], rotation.mV[VZ]);
            break;

        case SWAP_ROLL_AND_PITCH:
            rot_mat = LLMatrix3(rotation.mV[VX], rotation.mV[VZ], rotation.mV[VY]);
            break;

        case SWAP_NOTHING:
        default:
            rot_mat = LLMatrix3(rotation.mV[VX], rotation.mV[VY], rotation.mV[VZ]);
            break;
    }

    LLQuaternion rot_quat;
    rot_quat = LLQuaternion(rot_mat) * rot_quat;

    return rot_quat;
}

LLVector3 FSPoserAnimator::getJointScale(LLVOAvatar *avatar, FSPoserJoint joint)
{
    LLVector3 vec3;
    return vec3;
}

void FSPoserAnimator::setJointScale(LLVOAvatar *avatar, const FSPoserJoint *joint, LLVector3 scale, E_BoneDeflectionStyles style)
{
    if (!avatar || avatar->isDead())
        return;
    if (!joint)
        return;
}

const FSPoserAnimator::FSPoserJoint* FSPoserAnimator::getPoserJointByName(std::string jointName)
{
    for (size_t index = 0; index != PoserJoints.size(); ++index)
    {
        if (boost::iequals(PoserJoints[index].jointName(), jointName))
            return &PoserJoints[index];
    }

    return nullptr;
}

bool FSPoserAnimator::tryPosingAvatar(LLVOAvatar *avatar)
{
    if (!avatar || avatar->isDead())
        return false;

    LLMotion *motion = avatar->findMotion(ANIM_AGENT_TARGET);
    gAgent.stopFidget();
    avatar->startDefaultMotions();
    _currentlyPosingSelf = avatar->startMotion(ANIM_AGENT_TARGET);

    return _currentlyPosingSelf;
}

void FSPoserAnimator::stopPosingAvatar(LLVOAvatar *avatar)
{
    if (!_currentlyPosingSelf)
        return;
    if (!avatar || avatar->isDead())
        return;

    bool result = avatar->stopMotion(ANIM_AGENT_TARGET);

    _currentlyPosingSelf = false;
}

bool FSPoserAnimator::isPosingAvatar(LLVOAvatar* avatar)
{
    if (!avatar || avatar->isDead())
        return false;

    if (avatar->isSelf())
        return _currentlyPosingSelf;

    return false;
}

