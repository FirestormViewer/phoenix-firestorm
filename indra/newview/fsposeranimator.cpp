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

void FSPoserAnimator::setJointPosition(LLVOAvatar *avatar, FSPoserJoint *joint, LLVector3 position, E_BoneDeflectionStyles style)
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
    rot.getEulerAngles(&vec3.mV[VX], &vec3.mV[VZ], &vec3.mV[VY]);

    return vec3;
}

void FSPoserAnimator::setJointRotation(LLVOAvatar *avatar, FSPoserJoint *joint, LLVector3 rotation, E_BoneDeflectionStyles style)
{
    if (!avatar || avatar->isDead())
        return;
    if (!joint)
        return;

    LLJoint *avJoint = avatar->getJoint(JointKey::construct(joint->jointName()));
    if (!avJoint)
        return;

    LLMatrix3    rot_mat = LLMatrix3(rotation.mV[VX], rotation.mV[VY], rotation.mV[VZ]);
    LLQuaternion rot_quat;
    rot_quat = LLQuaternion(rot_mat) * rot_quat;
    avJoint->setRotation(rot_quat);
}

LLVector3 FSPoserAnimator::getJointScale(LLVOAvatar *avatar, FSPoserJoint joint)
{
    LLVector3 vec3;
    return vec3;
}

void FSPoserAnimator::setJointScale(LLVOAvatar *avatar, FSPoserJoint *joint, LLVector3 scale, E_BoneDeflectionStyles style)
{
    if (!avatar || avatar->isDead())
        return;
    if (!joint)
        return;
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

