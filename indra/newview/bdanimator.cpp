/**
*
* Copyright (C) 2018, NiranV Dean
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
*/

#include "llviewerprecompiledheaders.h"

#include "lluictrlfactory.h"
#include "llagent.h"
#include "lldiriterator.h"
#include "llkeyframemotion.h"
#include "llsdserialize.h"
#include "llsdutil.h"
#include "llviewerjointattachment.h"
#include "llviewerjoint.h"
#include "llvoavatarself.h"
#include "llviewercontrol.h"

#include "bdfloaterposer.h"
#include "bdanimator.h"
#include "bdposingmotion.h"

#include "llagentcamera.h"

BDAnimator gDragonAnimator;


BDAnimator::BDAnimator() :
            mPlaying(false)
{
}

BDAnimator::~BDAnimator()
{
}

void BDAnimator::update()
{
    //BD - Don't scale our head when we are posing. We should probably implement a more
    //     precise solution such as saving the vectors and reapplying them on leave.
    if (gAgentAvatarp && !gAgentAvatarp->mIsPosing)
    {
        static LLCachedControl<bool> exp_scaling(gSavedSettings, "MouselookExperimentalHeadScaling");
        if (exp_scaling && gAgentCamera.cameraMouselook())
        {
            LLJoint* joint;
            for (S32 i = 0; (joint = gAgentAvatarp->getCharacterJoint(i)); ++i)
            {
                if (!joint)	continue;

                if (joint->mJointNum > 7 &&	//mHead
                    joint->mJointNum < 58)	//mCollarLeft
                {
                    joint->setScale(LLVector3::zero);
                }
            }

            //joint = gAgentAvatarp->getJoint(JointKey::construct("HEAD"));
            joint = gAgentAvatarp->getJoint("HEAD");
            if (joint)
                joint->setScale(LLVector3::zero);
        }
    }
    //BD - Don't do anything if the animator is not activated.
    if (!mPlaying) return;

    //BD - Don't even bother when our list is empty.
    if (mAvatarsList.empty()) return;

    for (LLVOAvatar* avatar : mAvatarsList)
    {
        if (!avatar || avatar->isDead()) continue;

        //BD - This should never happen since avatars are only in this list if they have at
        //     least one action saved, better be safe than sorry though.
        if (avatar->mAnimatorActions.empty())
        {
            avatar->mAnimPlayTimer.stop();
            continue;
        }

        if (avatar->mAnimPlayTimer.getStarted() &&
            avatar->mAnimPlayTimer.getElapsedTimeF32() > avatar->mExpiryTime)
        {
            Action action = avatar->mAnimatorActions[avatar->mCurrentAction];
            //BD - Stop the timer, we're going to reconfigure and restart it when we're done.
            avatar->mAnimPlayTimer.stop();

            BD_EActionType type = action.mType;
            std::string name = action.mPoseName;

            //BD - We can't use Wait or Restart as label, need to fix this.
            if (type == WAIT)
            {
                //BD - Do nothing?
                avatar->mExpiryTime = action.mTime;
                ++avatar->mCurrentAction;
            }
            else if (type == REPEAT)
            {
                avatar->mCurrentAction = 0;
                avatar->mExpiryTime = 0.0f;
            }
            else
            {
                avatar->mExpiryTime = 0.0f;
                mTargetAvatar = avatar;
                loadPose(name);
                ++avatar->mCurrentAction;
            }

            //BD - As long as we are not at the end, start the timer again, automatically
            //     resetting the counter in the process.
            if (avatar->mAnimatorActions.size() != avatar->mCurrentAction)
            {
                avatar->mAnimPlayTimer.start();
            }
        }
    }
}

void BDAnimator::onAddAction(LLVOAvatar* avatar, LLScrollListItem* item, S32 location)
{
    if (!avatar || avatar->isDead()) return;

    if (item)
    {
        Action action;
        S32 type = item->getColumn(2)->getValue().asInteger();
        if (type == 0)
        {
            action.mType = WAIT;
        }
        else if (type == 1)
        {
            action.mType = REPEAT;
        }
        else
        {
            action.mType = POSE;
        }
        action.mPoseName = item->getColumn(0)->getValue().asString();
        action.mTime = (F32)(item->getColumn(1)->getValue().asReal());
        avatar->mAnimatorActions.push_back(action);

        //BD - Lookup whether we already added this avatar, do it if we didn't this is
        //     important.
        for (LLVOAvatar* list_avatar : mAvatarsList)
        {
            if (list_avatar == avatar) return;
        }
        mAvatarsList.push_back(avatar);
    }
}

void BDAnimator::onAddAction(LLVOAvatar* avatar, std::string name, BD_EActionType type, F32 time, S32 location)
{
    if (!avatar || avatar->isDead()) return;

    Action action;
    action.mType = type;
    action.mPoseName = name;
    action.mTime = time;
    if (avatar->mAnimatorActions.size() == 0)
    {
        avatar->mAnimatorActions.push_back(action);
    }
    else
    {
        if (location <= avatar->mAnimatorActions.size())
        {
            avatar->mAnimatorActions.insert(avatar->mAnimatorActions.begin() + location, action);
        }
    }

    //BD - Lookup whether we already added this avatar, do it if we didn't this is
    //     important.
    for (LLVOAvatar* list_avatar : mAvatarsList)
    {
        if (list_avatar == avatar) return;
    }
    mAvatarsList.push_back(avatar);
}

void BDAnimator::onAddAction(LLVOAvatar* avatar, Action action, S32 location)
{
    if (!avatar || avatar->isDead()) return;

    avatar->mAnimatorActions.insert(avatar->mAnimatorActions.begin() + location, action);

    //BD - Lookup whether we already added this avatar, do it if we didn't this is
    //     important.
    for (LLVOAvatar* list_avatar : mAvatarsList)
    {
        if (list_avatar == avatar) return;
    }
    mAvatarsList.push_back(avatar);
}


void BDAnimator::onDeleteAction(LLVOAvatar* avatar, S32 location)
{
    if (!mPlaying)
    {
        avatar->mAnimatorActions.erase(avatar->mAnimatorActions.begin() + location);

        //BD - If this avatar has no more actions, delete it from our list.
        if (avatar->mAnimatorActions.empty())
        {
            S32 i = 0;
            for (LLVOAvatar* list_avatar : mAvatarsList)
            {
                if (list_avatar == avatar)
                {
                    mAvatarsList.erase(mAvatarsList.begin() + i);
                }
                ++i;
            }
        }
    }
}

void BDAnimator::startPlayback()
{
    //BD - Don't even bother when our list is empty.
    if (mAvatarsList.empty()) return;

    for (LLVOAvatar* avatar : mAvatarsList)
    {
        if (!avatar || avatar->isDead()) continue;

        avatar->mAnimPlayTimer.start();
        avatar->mExpiryTime = 0.0f;
        avatar->mCurrentAction = 0;
    }
    mPlaying = true;
}

void BDAnimator::stopPlayback()
{
    //BD - Don't even bother when our list is empty.
    if (mAvatarsList.empty()) return;

    for (LLVOAvatar* avatar : mAvatarsList)
    {
        if (!avatar || avatar->isDead()) continue;

        avatar->mAnimPlayTimer.stop();
    }
    mPlaying = false;
}

//BD - We allow loading rotations, positions and scales seperately
//     by giving the load an integer which determines what to load.
//     1 is default and loads rotations only, 2 = positions only,
//     4 = scales only, thus 3 = rotations and positions and so on.
bool BDAnimator::loadPose(const LLSD& name, S32 load_type)
{
    if (!mTargetAvatar || mTargetAvatar->isDead())
    {
        LL_WARNS("Posing") << "Couldn't find avatar, dead?" << LL_ENDL;
        return FALSE;
    }

    std::string filename;
    if (!name.asString().empty())
    {
        filename = gDirUtilp->getExpandedFilename(LL_PATH_POSES, LLDir::escapePathString(name.asString()) + ".xml");
    }

    LLSD pose;
    llifstream infile;
    infile.open(filename);
    if (!infile.is_open())
    {
        LL_WARNS("Posing") << "Cannot find file in: " << filename << LL_ENDL;
        return FALSE;
    }

    while (!infile.eof())
    {
        S32 count = LLSDSerialize::fromXML(pose, infile);
        if (count == LLSDParser::PARSE_FAILURE)
        {
            LL_WARNS("Posing") << "Failed to parse file: " << filename << LL_ENDL;
            return FALSE;
        }

        if (pose.has("version"))
        {
            for (LLSD::map_const_iterator itr = pose.beginMap(); itr != pose.endMap(); ++itr)
            {
                std::string const & name = itr->first;
                LLSD const & control_map = itr->second;

                //BD - Not sure how to read the exact line out of a XML file, so we're just going
                //     by the amount of tags here, since the header has only 3 it's a good indicator
                //     if it's the correct line we're in.
                BDPosingMotion* motion = (BDPosingMotion*)mTargetAvatar->findMotion(ANIM_BD_POSING_MOTION);
                if (motion)
                {
                    F32 time = 0.f;
                    S32 type = 0;
                    if (control_map.has("time"))
                        time = (F32)(control_map["time"].asReal());
                    if (control_map.has("type"))
                        type = control_map["type"].asInteger();
                    motion->setInterpolationType(type);
                    motion->setInterpolationTime(time);
                    motion->startInterpolationTimer();
                }

                LLJoint* joint = mTargetAvatar->getJoint(name);
                if (joint)
                {
                    //BD - Don't try to add/remove joint states for anything but our default bones.
                    if (motion && joint->getJointNum() < 134)
                    {
                        LLPose* mpose = motion->getPose();
                        if (mpose)
                        {
                            //BD - Fail safe, assume that a bone is always enabled in case we
                            //     load a pose that was created prior to including the enabled
                            //     state or for whatever reason end up not having an enabled state
                            //     written into the file.
                            bool state_enabled = true;

                            //BD - Check whether the joint state of the current joint has any enabled
                            //     status saved into the pose file or not.
                            if (control_map.has("enabled"))
                            {
                                state_enabled = control_map["enabled"].asBoolean();
                            }

                            //BD - Add the joint state but only if it's not active yet.
                            //     Same goes for removing it, don't remove it if it doesn't exist.
                            LLPointer<LLJointState> joint_state = mpose->findJointState(joint);
                            if (!joint_state && state_enabled)
                            {
                                motion->addJointToState(joint);
                            }
                            else if (joint_state && !state_enabled)
                            {
                                motion->removeJointState(joint_state);
                            }
                        }
                    }

                    LLVector3 vec3;
                    if (load_type & BD_ROTATIONS && control_map.has("rotation"))
                    {
                        LLQuaternion quat;
                        LLQuaternion new_quat = joint->getRotation();

                        joint->setLastRotation(new_quat);
                        vec3.setValue(control_map["rotation"]);
                        quat.setEulerAngles(vec3.mV[VX], vec3.mV[VZ], vec3.mV[VY]);
                        joint->setTargetRotation(quat);
                    }

                    //BD - Position information is only ever written when it is actually safe to do.
                    //     It's safe to assume that IF information is available it's safe to apply.
                    if (load_type & BD_POSITIONS && control_map.has("position"))
                    {
                        vec3.setValue(control_map["position"]);
                        joint->setLastPosition(joint->getPosition());
                        joint->setTargetPosition(vec3);
                    }

                    //BD - Bone Scales
                    if (load_type & BD_SCALES && control_map.has("scale"))
                    {
                        vec3.setValue(control_map["scale"]);
                        joint->setScale(vec3);
                    }
                }
            }
        }
        else
        {
            LLJoint* joint = mTargetAvatar->getJoint(pose["bone"].asString());
            if (joint)
            {
                BDPosingMotion* motion = (BDPosingMotion*)mTargetAvatar->findMotion(ANIM_BD_POSING_MOTION);
                //BD - Don't try to add/remove joint states for anything but our default bones.
                if (motion && joint->getJointNum() < 134)
                {
                    LLPose* mpose = motion->getPose();
                    if (mpose)
                    {
                        //BD - Fail safe, assume that a bone is always enabled in case we
                        //     load a pose that was created prior to including the enabled
                        //     state or for whatever reason end up not having an enabled state
                        //     written into the file.
                        bool state_enabled = true;

                        //BD - Check whether the joint state of the current joint has any enabled
                        //     status saved into the pose file or not.
                        if (pose["enabled"].isDefined())
                        {
                            state_enabled = pose["enabled"].asBoolean();
                        }

                        //BD - Add the joint state but only if it's not active yet.
                        //     Same goes for removing it, don't remove it if it doesn't exist.
                        LLPointer<LLJointState> joint_state = mpose->findJointState(joint);
                        if (!joint_state && state_enabled)
                        {
                            motion->addJointToState(joint);
                        }
                        else if (joint_state && !state_enabled)
                        {
                            motion->removeJointState(joint_state);
                        }
                    }
                }

                LLVector3 vec3;
                if (load_type & BD_ROTATIONS && pose["rotation"].isDefined())
                {
                    LLQuaternion quat;
                    LLQuaternion new_quat = joint->getRotation();

                    joint->setLastRotation(new_quat);
                    vec3.setValue(pose["rotation"]);
                    quat.setEulerAngles(vec3.mV[VX], vec3.mV[VZ], vec3.mV[VY]);
                    joint->setTargetRotation(quat);
                }

                //BD - Position information is only ever written when it is actually safe to do.
                //     It's safe to assume that IF information is available it's safe to apply.
                if (load_type & BD_POSITIONS && pose["position"].isDefined())
                {
                    vec3.setValue(pose["position"]);
                    joint->setLastPosition(joint->getPosition());
                    joint->setTargetPosition(vec3);
                }

                //BD - Bone Scales
                if (load_type & BD_SCALES && pose["scale"].isDefined())
                {
                    vec3.setValue(pose["scale"]);
                    joint->setScale(vec3);
                }
            }
        }
    }
    infile.close();
    return TRUE;
}

LLSD BDAnimator::returnPose(const LLSD& name)
{
    std::string filename;
    if (!name.asString().empty())
    {
        filename = gDirUtilp->getExpandedFilename(LL_PATH_POSES, LLDir::escapePathString(name.asString()) + ".xml");
    }

    LLSD pose;
    llifstream infile;
    infile.open(filename);
    if (!infile.is_open())
    {
        LL_WARNS("Posing") << "Cannot find file in: " << filename << LL_ENDL;
    }

    for (S32 i = 0; !infile.eof(); ++i)
    {
        S32 count = LLSDSerialize::fromXML(pose[i], infile);
        if (count == LLSDParser::PARSE_FAILURE)
        {
            LL_WARNS("Posing") << "Failed to parse file: " << filename << LL_ENDL;
        }
    }
    infile.close();
    return pose;
}
