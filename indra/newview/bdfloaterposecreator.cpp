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

#include "bdfloaterposecreator.h"
#include "lluictrlfactory.h"
#include "llagent.h"
#include "llavatarname.h"
#include "llavatarnamecache.h"
#include "llclipboard.h"
#include "lldatapacker.h"
#include "lldiriterator.h"
#include "llfilepicker.h"
#include "llfilesystem.h"
#include "llkeyframemotion.h"
#include "llnotificationsutil.h"
#include "llmenugl.h"
#include "llmenubutton.h"
#include "lltoggleablemenu.h"
#include "llviewermenu.h"
#include "llsdserialize.h"
#include "llsdutil.h"
#include "llviewerjointattachment.h"
#include "llviewerjoint.h"
#include "llvoavatarself.h"
#include "pipeline.h"

#include "llviewerobjectlist.h"
#include "lldrawpoolavatar.h"

//BD - Animesh Support
#include "llcontrolavatar.h"

//BD - Black Dragon specifics
#include "bdanimator.h"
#include "bdposingmotion.h"
#include "bdfloaterposer.h"

//BD - 1 frame is always 1/60 of a second, we assume animations run at 60 FPS by default.
//     I might add an option to change it but right now its all manual work.
const F32 FRAMETIME = 1.f / 60.f;


BDFloaterPoseCreator::BDFloaterPoseCreator(const LLSD& key)
    :   LLFloater(key),
    mJointScrolls()
{
    //BD - Save our current pose as XML or ANIM file to be used or uploaded later.
    mCommitCallbackRegistrar.add("Pose.Save", boost::bind(&BDFloaterPoseCreator::onClickPoseSave, this, _2));
    //BD - Start our custom pose.
    mCommitCallbackRegistrar.add("Pose.Start", boost::bind(&BDFloaterPoseCreator::onPoseStart, this));
    //BD - Import ANIM file to the poser.
    mCommitCallbackRegistrar.add("Pose.Import", boost::bind(&BDFloaterPoseCreator::onPoseImport, this));
    //BD - Start/Stop the pose to preview it.
    mCommitCallbackRegistrar.add("Pose.StartStop", boost::bind(&BDFloaterPoseCreator::onPoseStartStop, this));
    //BD - Multipurpose function to edit several basic parameters in the animation.
    mCommitCallbackRegistrar.add("Pose.EditInfo", boost::bind(&BDFloaterPoseCreator::onEditAnimationInfo, this, _2));
    //BD - Multipurpose function to edit several basic parameters in the animation.
    mCommitCallbackRegistrar.add("Pose.Interpolation", boost::bind(&BDFloaterPoseCreator::onInterpolationChange, this, _1));

    //BD - Add a keyframe to the motion curve.
    mCommitCallbackRegistrar.add("Keyframe.Add", boost::bind(&BDFloaterPoseCreator::onKeyframeAdd, this));
    //BD - Remove a keyframe from the motion curve.
    mCommitCallbackRegistrar.add("Keyframe.Remove", boost::bind(&BDFloaterPoseCreator::onKeyframeRemove, this));
    //BD - Change a keyframe's time value.
    mCommitCallbackRegistrar.add("Keyframe.Time", boost::bind(&BDFloaterPoseCreator::onKeyframeTime, this));
    //BD - Refresh the keyframe scroll list and fill it with all relevant keys.
    mCommitCallbackRegistrar.add("Keyframe.Refresh", boost::bind(&BDFloaterPoseCreator::onKeyframeRefresh, this));
	//BD - Refresh the keyframe scroll list and fill it with all relevant keys.
	mCommitCallbackRegistrar.add("Keyframe.ResetAll", boost::bind(&BDFloaterPoseCreator::onKeyframeResetAll, this));

    //BD - Change a bone's rotation.
    mCommitCallbackRegistrar.add("Joint.Set", boost::bind(&BDFloaterPoseCreator::onJointSet, this, _1, _2));
    //BD - Change a bone's position.
    mCommitCallbackRegistrar.add("Joint.PosSet", boost::bind(&BDFloaterPoseCreator::onJointPosSet, this, _1, _2));
    //BD - Change a bone's scale.
    mCommitCallbackRegistrar.add("Joint.SetScale", boost::bind(&BDFloaterPoseCreator::onJointScaleSet, this, _1, _2));
    //BD - Add or remove a joint state to or from the pose (enable/disable our overrides).
    mCommitCallbackRegistrar.add("Joint.ChangeState", boost::bind(&BDFloaterPoseCreator::onJointChangeState, this));
    //BD - Reset all selected bone rotations and positions.
    mCommitCallbackRegistrar.add("Joint.ResetJointFull", boost::bind(&BDFloaterPoseCreator::onJointRotPosScaleReset, this));
    //BD - Reset all selected bone rotations back to 0,0,0.
    mCommitCallbackRegistrar.add("Joint.ResetJointRotation", boost::bind(&BDFloaterPoseCreator::onJointRotationReset, this));
    //BD - Reset all selected bones positions back to their default.
    mCommitCallbackRegistrar.add("Joint.ResetJointPosition", boost::bind(&BDFloaterPoseCreator::onJointPositionReset, this));
    //BD - Reset all selected bones scales back to their default.
    mCommitCallbackRegistrar.add("Joint.ResetJointScale", boost::bind(&BDFloaterPoseCreator::onJointScaleReset, this));
    //BD - Reset all selected bone rotations back to the initial rotation.
    mCommitCallbackRegistrar.add("Joint.RevertJointRotation", boost::bind(&BDFloaterPoseCreator::onJointRotationRevert, this));
    //BD - Mirror the current bone's rotation to match what the other body side's rotation should be.
    mCommitCallbackRegistrar.add("Joint.Mirror", boost::bind(&BDFloaterPoseCreator::onJointMirror, this));
    //BD - Copy and mirror the other body side's bone rotation.
    mCommitCallbackRegistrar.add("Joint.Symmetrize", boost::bind(&BDFloaterPoseCreator::onJointSymmetrize, this));

    //BD - Toggle Mirror Mode on/off.
    mCommitCallbackRegistrar.add("Joint.ToggleMirror", boost::bind(&BDFloaterPoseCreator::toggleMirrorMode, this, _1));
    //BD - Toggle Easy Rotation on/off.
    mCommitCallbackRegistrar.add("Joint.EasyRotations", boost::bind(&BDFloaterPoseCreator::toggleEasyRotations, this, _1));
    //BD - Flip pose (mirror).
    mCommitCallbackRegistrar.add("Joint.FlipPose", boost::bind(&BDFloaterPoseCreator::onFlipPose, this));
}

BDFloaterPoseCreator::~BDFloaterPoseCreator()
{
}

bool BDFloaterPoseCreator::postBuild()
{
    std::string pathname = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "animations");
    if (!gDirUtilp->fileExists(pathname))
    {
        LLFile::mkdir(pathname);
    }

    //BD - Posing
    mJointScrolls = { { this->getChild<FSScrollListCtrl>("joints_scroll", true),
                        this->getChild<FSScrollListCtrl>("cv_scroll", true),
                        this->getChild<FSScrollListCtrl>("attach_scroll", true) } };

    mJointScrolls[BD_JOINTS]->setCommitOnSelectionChange(TRUE);
    mJointScrolls[BD_JOINTS]->setCommitCallback(boost::bind(&BDFloaterPoseCreator::onKeyframeRefresh, this));
    mJointScrolls[BD_JOINTS]->setDoubleClickCallback(boost::bind(&BDFloaterPoseCreator::onJointChangeState, this));

    //BD - Collision Volumes
    mJointScrolls[BD_COLLISION_VOLUMES]->setCommitOnSelectionChange(TRUE);
    mJointScrolls[BD_COLLISION_VOLUMES]->setCommitCallback(boost::bind(&BDFloaterPoseCreator::onKeyframeRefresh, this));

    //BD - Attachment Bones
    mJointScrolls[BD_ATTACHMENT_BONES]->setCommitOnSelectionChange(TRUE);
    mJointScrolls[BD_ATTACHMENT_BONES]->setCommitCallback(boost::bind(&BDFloaterPoseCreator::onKeyframeRefresh, this));

    mKeyframeScroll = this->getChild<FSScrollListCtrl>("keyframe_scroll", true);
    mKeyframeScroll->setCommitOnSelectionChange(TRUE);
    mKeyframeScroll->setCommitCallback(boost::bind(&BDFloaterPoseCreator::onKeyframeSelect, this));

    mTimelineScroll = this->getChild<FSScrollListCtrl>("timeframe_scroll", true);

    mRotationSliders = { { getChild<LLUICtrl>("Rotation_X"), getChild<LLUICtrl>("Rotation_Y"), getChild<LLUICtrl>("Rotation_Z") } };
    mPositionSliders = { { getChild<LLSliderCtrl>("Position_X"), getChild<LLSliderCtrl>("Position_Y"), getChild<LLSliderCtrl>("Position_Z") } };
    mScaleSliders = { { getChild<LLSliderCtrl>("Scale_X"), getChild<LLSliderCtrl>("Scale_Y"), getChild<LLSliderCtrl>("Scale_Z") } };

    mJointTabs = getChild<LLTabContainer>("joints_tabs");
    mJointTabs->setCommitCallback(boost::bind(&BDFloaterPoseCreator::onJointControlsRefresh, this));

    mModifierTabs = getChild<LLTabContainer>("modifier_tabs");
    mModifierTabs->setCommitCallback(boost::bind(&BDFloaterPoseCreator::onKeyframeRefresh, this));

    //BD - Misc
    mDelayRefresh = false;

    mMirrorMode = false;
    mEasyRotations = true;

    mAutoDuration = true;

    mStartPosingBtn = getChild<LLButton>("activate");

    //BD - Poser Menu
    LLUICtrl::CommitCallbackRegistry::ScopedRegistrar pose_reg;
    LLUICtrl::EnableCallbackRegistry::ScopedRegistrar enable_registrar;

    //BD - Poser Right Click Menu
    pose_reg.add("Joints.Menu", boost::bind(&BDFloaterPoseCreator::onJointContextMenuAction, this, _2));
    enable_registrar.add("Joints.OnEnable", boost::bind(&BDFloaterPoseCreator::onJointContextMenuEnable, this, _2));
    LLContextMenu* joint_menu = LLUICtrlFactory::getInstance()->createFromFile<LLContextMenu>("menu_bd_poser_joints.xml",
        gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance());
    mJointScrolls[BD_JOINTS]->setContextMenu(FSScrollListCtrl::MENU_EXTERNAL, joint_menu);

    //mJointScrolls[BD_JOINTS]->refreshLineHeight();
    //mJointScrolls[BD_COLLISION_VOLUMES]->refreshLineHeight();
    //mJointScrolls[BD_ATTACHMENT_BONES]->refreshLineHeight();
    //mKeyframeScroll->refreshLineHeight();
    //mTimelineScroll->refreshLineHeight();

    return TRUE;
}

void BDFloaterPoseCreator::draw()
{
    LLFloater::draw();
}

void BDFloaterPoseCreator::onOpen(const LLSD& key)
{
    //BD - Check whether we should delay the default value collection or fire it immediately.
    mDelayRefresh = !gAgentAvatarp->isFullyLoaded();
    if (!mDelayRefresh)
    {
        onCollectDefaults();
    }

    onJointRotPosScaleReset();
    onJointRefresh();
}

void BDFloaterPoseCreator::onClose(bool app_quitting)
{
    //BD - Doesn't matter because we destroy the window and rebuild it every time we open it anyway.
    mJointScrolls[BD_JOINTS]->clearRows();
    mJointScrolls[BD_COLLISION_VOLUMES]->clearRows();
    mJointScrolls[BD_ATTACHMENT_BONES]->clearRows();
}

////////////////////////////////
//BD - Keyframes
////////////////////////////////
void BDFloaterPoseCreator::onKeyframesRebuild()
{
    if (!gDragonAnimator.mPoseCreatorMotion)
        return;

    //BD - Now write an entry with all given information into our list so we can use it.
    LLKeyframeMotion::JointMotionList* joint_list = gDragonAnimator.mPoseCreatorMotion->getJointMotionList();
    if (!joint_list)
        return;

    mKeyframeScroll->clearRows();
    mTimelineScroll->clearRows();

    for(U32 joint_idx = 0; joint_idx < joint_list->getNumJointMotions(); joint_idx++)
    {
        LLKeyframeMotion::JointMotion* joint_motion = joint_list->getJointMotion(joint_idx);
        if (!joint_motion)
            return;

        LLKeyframeMotion::RotationCurve rot_curve = joint_motion->mRotationCurve;
        LLKeyframeMotion::PositionCurve pos_curve = joint_motion->mPositionCurve;
        LLKeyframeMotion::ScaleCurve scale_curve = joint_motion->mScaleCurve;

        LLScrollListMultiSlider* col_slider = nullptr;
        if (!rot_curve.mKeys.empty()
            || !pos_curve.mKeys.empty()
            || !scale_curve.mKeys.empty())
        {
            LLSD slider;
            const std::string joint_name = joint_motion->mJointName;
            slider["columns"][0]["column"] = "joint";
            slider["columns"][0]["value"] = joint_name;
            slider["columns"][1]["column"] = "multislider";
            slider["columns"][1]["type"] = "multislider";
            slider["columns"][1]["min_value"] = 0;
            slider["columns"][1]["max_value"] = 3600;
            slider["columns"][1]["increment"] = 1.f;
            LLScrollListItem* element = mTimelineScroll->addElement(slider);
            col_slider = (LLScrollListMultiSlider*)element->getColumn(1);
        }

        for (auto& rot_key : rot_curve.mKeys)
        {
            F32 roll, pitch, yaw;
            LLQuaternion rot_quat = rot_key.second.mRotation;
            rot_quat.getEulerAngles(&roll, &pitch, &yaw);

            S32 time_frames = (S32)ll_round(rot_key.second.mTime / FRAMETIME, 1.f);

            if (col_slider)
            {
                //F32 time_key = time_frames / col_slider->getMaxValue();
                col_slider->addKeyframe((F32)time_frames, (LLSD)rot_key.first);
            }
        }

        for (auto& pos_key : pos_curve.mKeys)
        {
            LLVector3 pos = pos_key.second.mPosition;
            S32 time_frames = (S32)ll_round(pos_key.second.mTime / FRAMETIME, 1.f);

            if (col_slider)
            {
                //F32 time_key = time_frames / col_slider->getMaxValue();
                col_slider->addKeyframe((F32)time_frames, (LLSD)pos_key.first);
            }
        }

        for (auto& scale_key : scale_curve.mKeys)
        {
            LLVector3 scale = scale_key.second.mScale;
            S32 time_frames = (S32)ll_round(scale_key.second.mTime / FRAMETIME, 1.f);

            if (col_slider)
            {
                //F32 time_key = time_frames / col_slider->getMaxValue();
                col_slider->addKeyframe((F32)time_frames, (LLSD)scale_key.first);
            }
        }
    }
}

void BDFloaterPoseCreator::onKeyframeResetAll()
{
    if (!gDragonAnimator.mPoseCreatorMotion)
        return;

    S32 usage = 0;
    LLKeyframeMotion::JointMotionList* joint_list = gDragonAnimator.mPoseCreatorMotion->getJointMotionList();
    if (!joint_list)
        return;

    for (U32 joint_idx = 0; joint_idx < joint_list->getNumJointMotions(); joint_idx++)
    {
        LLKeyframeMotion::JointMotion* joint_motion = joint_list->getJointMotion(joint_idx);
        if (!joint_motion)
            continue;

        //LLJoint* joint = gAgentAvatarp->getJoint(JointKey::construct(joint_motion->mJointName));
        LLJoint* joint = gAgentAvatarp->getJoint(joint_motion->mJointName);
        if (!joint)
            continue;

        //BD - Kill all rotation, position and scale keyframes.
        LLKeyframeMotion::RotationCurve rot_curve;
        joint_motion->mRotationCurve = rot_curve;

        LLKeyframeMotion::PositionCurve pos_curve;
        joint_motion->mPositionCurve = pos_curve;

        LLKeyframeMotion::ScaleCurve scale_curve;
        joint_motion->mScaleCurve = scale_curve;

        //BD - Reset the usage.
        LLJointState* joint_state = gDragonAnimator.mPoseCreatorMotion->findJointState(joint);
        joint_state->setUsage(usage);
    }

    //BD - Reset the pose.
    onJointRotPosScaleReset();

    //BD - Refresh the keyframes.
    onKeyframeRefresh();
    onKeyframesRebuild();

    //BD - Check if our animation duration needs changing.
    onAnimationDurationCheck();
}
void BDFloaterPoseCreator::onKeyframeRefresh()
{
    S32 idx = mJointTabs->getCurrentPanelIndex();
    LLScrollListItem* item = mJointScrolls[idx]->getFirstSelected();
    if (!item)
        return;

    if (!item->getEnabled())
        return;

    LLJoint* joint = (LLJoint*)item->getUserdata();
    if (!joint)
        return;

    if (!gDragonAnimator.mPoseCreatorMotion)
        return;

    //BD - Now write an entry with all given information into our list so we can use it.
    LLKeyframeMotion::JointMotionList* joint_list = gDragonAnimator.mPoseCreatorMotion->getJointMotionList();
    if (!joint_list)
        return;

    LLKeyframeMotion::JointMotion* joint_motion = joint_list->getJointMotion(joint->getJointNum());
    if (!joint_motion)
        return;

    mKeyframeScroll->clearRows();

    LLKeyframeMotion::RotationCurve rot_curve = joint_motion->mRotationCurve;
    LLKeyframeMotion::PositionCurve pos_curve = joint_motion->mPositionCurve;
    LLKeyframeMotion::ScaleCurve scale_curve = joint_motion->mScaleCurve;

    LLScrollListMultiSlider* col_slider = nullptr;
    if (!rot_curve.mKeys.empty()
        || !pos_curve.mKeys.empty()
        || !scale_curve.mKeys.empty())
    {
        //BD - Find and delete the current entry.
        for (auto timeline_item : mTimelineScroll->getAllData())
        {
            std::string name = timeline_item->getColumn(0)->getValue();
            if (name == joint->getName())
            {
                timeline_item->setFlagged(true);
                break;
            }
        }
        mTimelineScroll->deleteFlaggedItems();

        //BD - Now rewrite the entry.
        LLSD slider;
        const std::string joint_name = joint->getName();
        slider["columns"][0]["column"] = "joint";
        slider["columns"][0]["value"] = joint_name;
        slider["columns"][1]["column"] = "multislider";
        slider["columns"][1]["type"] = "multislider";
        slider["columns"][1]["min_value"] = 0;
        slider["columns"][1]["max_value"] = 3600;
        slider["columns"][1]["increment"] = 1.f;
        LLScrollListItem* element = mTimelineScroll->addElement(slider);
        col_slider = (LLScrollListMultiSlider*)element->getColumn(1);
    }

    S32 modidx = mModifierTabs->getCurrentPanelIndex();
    if (modidx == 0)
    {
        for (auto& rot_key : rot_curve.mKeys)
        {
            F32 roll, pitch, yaw;
            LLQuaternion rot_quat = rot_key.second.mRotation;
            rot_quat.getEulerAngles(&roll, &pitch, &yaw);

            S32 time_frames = (S32)ll_round(rot_key.second.mTime / FRAMETIME, 1.f);

            LLSD row;
            row["columns"][0]["column"] = "time";
            row["columns"][0]["value"] = ll_round((F32)time_frames, 1.f);
            row["columns"][1]["column"] = "value";
            row["columns"][1]["value"] = rot_key.first;
            row["columns"][2]["column"] = "x";
            row["columns"][2]["value"] = ll_round(roll, 0.001f);
            row["columns"][3]["column"] = "y";
            row["columns"][3]["value"] = ll_round(pitch, 0.001f);
            row["columns"][4]["column"] = "z";
            row["columns"][4]["value"] = ll_round(yaw, 0.001f);
            mKeyframeScroll->addElement(row);

            if (col_slider)
            {
                //F32 time_key = time_frames / col_slider->getMaxValue();
                col_slider->addKeyframe((F32)time_frames, (LLSD)rot_key.first);
            }
        }
    }
    else if (modidx == 1)
    {
        for (auto& pos_key : pos_curve.mKeys)
        {
            LLVector3 pos = pos_key.second.mPosition;
            S32 time_frames = (S32)ll_round(pos_key.second.mTime / FRAMETIME, 1.f);
            LLSD row;
            row["columns"][0]["column"] = "time";
            row["columns"][0]["value"] = ll_round((F32)time_frames, 1.f);
            row["columns"][1]["column"] = "value";
            row["columns"][1]["value"] = pos_key.first;
            row["columns"][2]["column"] = "x";
            row["columns"][2]["value"] = ll_round(pos.mV[VX], 0.001f);
            row["columns"][3]["column"] = "y";
            row["columns"][3]["value"] = ll_round(pos.mV[VY], 0.001f);
            row["columns"][4]["column"] = "z";
            row["columns"][4]["value"] = ll_round(pos.mV[VZ], 0.001f);
            mKeyframeScroll->addElement(row);

            if (col_slider)
            {
                //F32 time_key = time_frames / col_slider->getMaxValue();
                col_slider->addKeyframe((F32)time_frames, (LLSD)pos_key.first);
            }
        }
    }
    else if (modidx == 2)
    {
        for (auto& scale_key : scale_curve.mKeys)
        {
            LLVector3 scale = scale_key.second.mScale;
            S32 time_frames = (S32)ll_round(scale_key.second.mTime / FRAMETIME, 1.f);
            LLSD row;
            row["columns"][0]["column"] = "time";
            row["columns"][0]["value"] = ll_round((F32)time_frames, 1.f);
            row["columns"][1]["column"] = "value";
            row["columns"][1]["value"] = scale_key.first;
            row["columns"][2]["column"] = "x";
            row["columns"][2]["value"] = ll_round(scale.mV[VX], 0.001f);
            row["columns"][3]["column"] = "y";
            row["columns"][3]["value"] = ll_round(scale.mV[VY], 0.001f);
            row["columns"][4]["column"] = "z";
            row["columns"][4]["value"] = ll_round(scale.mV[VZ], 0.001f);
            mKeyframeScroll->addElement(row);

            if (col_slider)
            {
                //F32 time_key = time_frames / col_slider->getMaxValue();
                col_slider->addKeyframe((F32)time_frames, (LLSD)scale_key.first);
            }
        }
    }

    //BD - Always select the last entry whenever we switch bones to allow quickly making
    //     changes or adding new keyframes.
    mKeyframeScroll->selectNthItem(mKeyframeScroll->getItemCount() - 1);

    onJointControlsRefresh();
    onKeyframeSelect();
}

void BDFloaterPoseCreator::onKeyframeSelect()
{
    LLScrollListItem* item = mKeyframeScroll->getFirstSelected();
    if (!item)
        return;

    LLScrollListItem* joint_item = mJointScrolls[BD_JOINTS]->getFirstSelected();
    if (!joint_item)
        return;

    LLJoint* joint = (LLJoint*)joint_item->getUserdata();
    if (!joint)
        return;

    if (!gDragonAnimator.mPoseCreatorMotion)
        return;

    LLKeyframeMotion::JointMotionList* joint_list = gDragonAnimator.mPoseCreatorMotion->getJointMotionList();
    //if (!joint_list)
    //    return;

    LLKeyframeMotion::JointMotion* joint_motion = joint_list->getJointMotion(joint->getJointNum());
    S32 si = item->getColumn(1)->getValue().asInteger();
    S32 idx = mModifierTabs->getCurrentPanelIndex();

    if (idx == 0)
    {
        //BD - Find the key we are looking for and remove it.
        for (auto& it : joint_motion->mRotationCurve.mKeys)
        {
            if ((S32)it.first == si)
            {
                LLQuaternion rot_quat = it.second.mRotation;
                LLVector3 rot_vec;
                rot_quat.getEulerAngles(&rot_vec.mV[VX], &rot_vec.mV[VY], &rot_vec.mV[VZ]);
                mRotationSliders[VX]->setValue(rot_vec.mV[VX]);
                mRotationSliders[VY]->setValue(rot_vec.mV[VY]);
                mRotationSliders[VZ]->setValue(rot_vec.mV[VZ]);
                joint->setTargetRotation(rot_quat);
            }
        }
    }
    else if (idx == 1)
    {
        for (auto& it : joint_motion->mPositionCurve.mKeys)
        {
            if ((S32)it.first == si)
            {
                LLVector3 pos = it.second.mPosition;
                mPositionSliders[VX]->setValue(pos.mV[VX]);
                mPositionSliders[VY]->setValue(pos.mV[VY]);
                mPositionSliders[VZ]->setValue(pos.mV[VZ]);
                joint->setTargetPosition(pos);
            }
        }
    }
    else if (idx == 2)
    {
        for (auto& it : joint_motion->mScaleCurve.mKeys)
        {
            if ((S32)it.first == si)
            {
                LLVector3 scale = it.second.mScale;
                mScaleSliders[VX]->setValue(scale.mV[VX]);
                mScaleSliders[VY]->setValue(scale.mV[VY]);
                mScaleSliders[VZ]->setValue(scale.mV[VZ]);
                joint->setScale(scale);
            }
        }
    }
}

void BDFloaterPoseCreator::onKeyframeAdd(F32 time, LLJoint* joint)
{
    //if (!gDragonAnimator.mPoseCreatorMotion)
    //    return;

    LLKeyframeMotion::JointMotionList* joint_list = gDragonAnimator.mPoseCreatorMotion->getJointMotionList();
    //if (!joint_list)
    //    return;

    S32 modifier_idx = mModifierTabs->getCurrentPanelIndex();

    if (joint)
    {
        LLKeyframeMotion::JointMotion* joint_motion = joint_list->getJointMotion(joint->getJointNum());

        if (modifier_idx == 0)
        {
            //BD - Create a rotation key and put it into the rotation curve.
            LLKeyframeMotion::RotationKey rotation_key = LLKeyframeMotion::RotationKey(ll_round(time, 0.001f), joint->getTargetRotation());

            //BD - Increase the key number since we are adding another key.
            joint_motion->mRotationCurve.mNumKeys++;

            joint_motion->mRotationCurve.mKeys[(float)(joint_motion->mRotationCurve.mNumKeys)] = rotation_key;

            LLJointState* joint_state = gDragonAnimator.mPoseCreatorMotion->findJointState(joint);
            if (joint_state->getUsage() ^ LLJointState::ROT)
            {
                joint_state->setUsage(joint_state->getUsage() | LLJointState::ROT);
            }
        }
        else if (modifier_idx == 1)
        {
            //BD - Create a position key and put it into the position curve.
            LLKeyframeMotion::PositionKey position_key = LLKeyframeMotion::PositionKey(ll_round(time, 0.001f), joint->getTargetPosition());

            //BD - Increase the key number since we are adding another key.
            joint_motion->mPositionCurve.mNumKeys++;

            joint_motion->mPositionCurve.mKeys[(float)(joint_motion->mPositionCurve.mNumKeys)] = position_key;

            LLJointState* joint_state = gDragonAnimator.mPoseCreatorMotion->findJointState(joint);
            if (joint_state->getUsage() ^ LLJointState::POS)
            {
                joint_state->setUsage(joint_state->getUsage() | LLJointState::POS);
            }
        }
        else if (modifier_idx == 2)
        {
            //BD - Create a scale key and put it into the scale curve.
            LLKeyframeMotion::ScaleKey scale_key = LLKeyframeMotion::ScaleKey(ll_round(time, 0.001f), joint->getScale());

            //BD - Increase the key number since we are adding another key.
            joint_motion->mScaleCurve.mNumKeys++;

            joint_motion->mScaleCurve.mKeys[(float)(joint_motion->mScaleCurve.mNumKeys)] = scale_key;

            LLJointState* joint_state = gDragonAnimator.mPoseCreatorMotion->findJointState(joint);
            if (joint_state->getUsage() ^ LLJointState::SCALE)
            {
                joint_state->setUsage(joint_state->getUsage() | LLJointState::SCALE);
            }
        }
    }

    //BD - Check if our animation duration needs changing.
    onAnimationDurationCheck();
}

void BDFloaterPoseCreator::onKeyframeAdd()
{
    //if (!gDragonAnimator.mPoseCreatorMotion)
    //    return;

    //LLKeyframeMotion::JointMotionList* joint_list = gDragonAnimator.mPoseCreatorMotion->getJointMotionList();
    //if (!joint_list)
    //    return;

    std::vector<LLScrollListItem*> items = mJointScrolls[BD_JOINTS]->getAllSelected();
	LLKeyframeMotion::JointMotionList* joint_list = gDragonAnimator.mPoseCreatorMotion->getJointMotionList();
    LLScrollListItem* item = mKeyframeScroll->getFirstSelected();
    MASK mask = gKeyboard->currentMask(TRUE);
    bool multiple = (items.size() > 1);
    S32 new_selected_idx = (mask == MASK_SHIFT || multiple) ? mKeyframeScroll->getChildCount() - 1 : mKeyframeScroll->getFirstSelectedIndex() + 1;
    S32 modifier_idx = mModifierTabs->getCurrentPanelIndex();


    for (auto joint_item : items)
    {
        LLJoint* joint = (LLJoint*)joint_item->getUserdata();
        if (!joint)
            continue;

        LLKeyframeMotion::JointMotion* joint_motion = joint_list->getJointMotion(joint->getJointNum());
        S32 selected_idx = 0;
        S32 count = 1;
        S32 keys = 1;
        F32 time = FRAMETIME;
        if (item)
            selected_idx = item->getColumn(1)->getValue().asInteger();

        if (modifier_idx == 0)
        {
            //BD - Put the key at the end of the list if SHIFT is held while pressing the add key button.
            if (multiple || joint_motion->mRotationCurve.mNumKeys == 0 || gKeyboard->currentMask(TRUE) == MASK_SHIFT)
            {
                //BD - Add a bit of time to differentiate the keys.
                //     This also kinda determines if this is the first keyframe or not. If we can't find a previous
                //     entry we assume its the first keyframe, so simply use 1.f.
                if (joint_motion->mRotationCurve.mNumKeys > 0)
                    time = joint_motion->mRotationCurve.mKeys[(float)(joint_motion->mRotationCurve.mNumKeys)].mTime + FRAMETIME;

                //BD - Create a rotation key and put it into the rotation curve.
                LLKeyframeMotion::RotationKey rotation_key = LLKeyframeMotion::RotationKey(ll_round(time, 0.001f), joint->getTargetRotation());

                //BD - Increase the key number since we are adding another key.
                joint_motion->mRotationCurve.mNumKeys++;

                joint_motion->mRotationCurve.mKeys[(float)(joint_motion->mRotationCurve.mNumKeys)] = rotation_key;
            }
            //BD - Conversely put the key at the start of the list if CTRL is held while adding a key.
            else if (gKeyboard->currentMask(TRUE) == MASK_CONTROL)
            {
                LLKeyframeMotion::RotationCurve rot_curve;

                //BD - Create a rotation key and put it into the rotation curve.
                LLKeyframeMotion::RotationKey rotation_key = LLKeyframeMotion::RotationKey(ll_round(time, 0.001f), joint->getTargetRotation());

                rot_curve.mNumKeys++;
                rot_curve.mKeys[(float)(count)] = rotation_key;
                count++;

                //BD - We have to rewrite the entire rotation curve in order to push a key to the beginning.
                for (auto& it : joint_motion->mRotationCurve.mKeys)
                {
                    rot_curve.mNumKeys++;
                    rot_curve.mKeys[(float)(count)] = it.second;
                    count++;
                }

                joint_motion->mRotationCurve = rot_curve;
            }
            //BD - Put the key after whatever is currently selected.
            else
            {
                LLKeyframeMotion::RotationCurve rot_curve;

                //BD - We have to rewrite the entire rotation curve in order to push a key inbetween others.
                for (auto& it : joint_motion->mRotationCurve.mKeys)
                {
                    rot_curve.mNumKeys++;
                    rot_curve.mKeys[(float)(count)] = it.second;
                    count++;

                    if ((S32)it.first == selected_idx)
                    {
                        //BD - Add a bit of time to differentiate the keys.
                        //     This also kinda determines if this is the first keyframe or not. If we can't find a previous
                        //     entry we assume its the first keyframe, so simply use 1.f.
                        time = it.second.mTime + FRAMETIME;

                        //BD - Create a rotation key and put it into the rotation curve.
                        LLKeyframeMotion::RotationKey rotation_key = LLKeyframeMotion::RotationKey(ll_round(time, 0.001f), joint->getTargetRotation());

                        rot_curve.mNumKeys++;
                        rot_curve.mKeys[(float)(count)] = rotation_key;
                        count++;
                    }
                }

                joint_motion->mRotationCurve = rot_curve;
            }

            keys = joint_motion->mRotationCurve.mNumKeys;

            LLJointState* joint_state = gDragonAnimator.mPoseCreatorMotion->findJointState(joint);
            if (joint_state->getUsage() ^ LLJointState::ROT)
            {
                joint_state->setUsage(joint_state->getUsage() | LLJointState::ROT);
            }
        }
        else if (modifier_idx == 1)
        {
            //BD - Put the key at the end of the list if SHIFT is held while pressing the add key button.
            if (multiple || joint_motion->mPositionCurve.mNumKeys == 0 || gKeyboard->currentMask(TRUE) == MASK_SHIFT)
            {
                //BD - Add a bit of time to differentiate the keys.
                //     This also kinda determines if this is the first keyframe or not. If we can't find a previous
                //     entry we assume its the first keyframe, so simply use 1.f.
                if (joint_motion->mPositionCurve.mNumKeys > 0)
                    time = joint_motion->mPositionCurve.mKeys[(float)(joint_motion->mPositionCurve.mNumKeys)].mTime + FRAMETIME;

                //BD - Create a position key and put it into the position curve.
                LLKeyframeMotion::PositionKey position_key = LLKeyframeMotion::PositionKey(ll_round(time, 0.001f), joint->getTargetPosition());

                //BD - Increase the key number since we are adding another key.
                joint_motion->mPositionCurve.mNumKeys++;

                joint_motion->mPositionCurve.mKeys[(float)(joint_motion->mPositionCurve.mNumKeys)] = position_key;
            }
            //BD - Conversely put the key at the start of the list if CTRL is held while adding a key.
            else if (gKeyboard->currentMask(TRUE) == MASK_CONTROL)
            {
                LLKeyframeMotion::PositionCurve pos_curve;

                //BD - Create a position key and put it into the position curve.
                LLKeyframeMotion::PositionKey position_key = LLKeyframeMotion::PositionKey(ll_round(time, 0.001f), joint->getTargetPosition());

                pos_curve.mNumKeys++;
                pos_curve.mKeys[(float)(count)] = position_key;
                count++;

                //BD - We have to rewrite the entire rotation curve in order to push a key to the beginning.
                for (auto& it : joint_motion->mPositionCurve.mKeys)
                {
                    pos_curve.mNumKeys++;
                    pos_curve.mKeys[(float)(count)] = it.second;
                    count++;
                }

                joint_motion->mPositionCurve = pos_curve;
            }
            //BD - Put the key after whatever is currently selected.
            else
            {
                LLKeyframeMotion::PositionCurve pos_curve;

                //BD - We have to rewrite the entire rotation curve in order to push a key inbetween others.
                for (auto& it : joint_motion->mPositionCurve.mKeys)
                {
                    pos_curve.mNumKeys++;
                    pos_curve.mKeys[(float)(count)] = it.second;
                    count++;

                    if ((S32)it.first == selected_idx)
                    {
                        //BD - Add a bit of time to differentiate the keys.
                        //     This also kinda determines if this is the first keyframe or not. If we can't find a previous
                        //     entry we assume its the first keyframe, so simply use 1.f.
                        time = it.second.mTime + FRAMETIME;

                        //BD - Create a position key and put it into the position curve.
                        LLKeyframeMotion::PositionKey position_key = LLKeyframeMotion::PositionKey(ll_round(time, 0.001f), joint->getTargetPosition());

                        pos_curve.mNumKeys++;
                        pos_curve.mKeys[(float)(count)] = position_key;
                        count++;
                    }
                }

                joint_motion->mPositionCurve = pos_curve;
            }

            keys = joint_motion->mPositionCurve.mNumKeys;

            LLJointState* joint_state = gDragonAnimator.mPoseCreatorMotion->findJointState(joint);
            if (joint_state->getUsage() ^ LLJointState::POS)
            {
                joint_state->setUsage(joint_state->getUsage() | LLJointState::POS);
            }
        }
        else if (modifier_idx == 2)
        {
            //BD - Create a scale key and put it into the scale curve.
            //LLKeyframeMotion::ScaleKey scale_key = LLKeyframeMotion::ScaleKey(ll_round(time, 0.001f), joint->getScale());

            //BD - Put the key at the end of the list if SHIFT is held while pressing the add key button.
            if (multiple || joint_motion->mScaleCurve.mNumKeys == 0 || gKeyboard->currentMask(TRUE) == MASK_SHIFT)
            {
                //BD - Add a bit of time to differentiate the keys.
                //     This also kinda determines if this is the first keyframe or not. If we can't find a previous
                //     entry we assume its the first keyframe, so simply use 1.f.
                if (joint_motion->mScaleCurve.mNumKeys > 0)
                    time = joint_motion->mScaleCurve.mKeys[(float)(joint_motion->mScaleCurve.mNumKeys)].mTime + FRAMETIME;

                //BD - Create a scale key and put it into the scale curve.
                LLKeyframeMotion::ScaleKey scale_key = LLKeyframeMotion::ScaleKey(ll_round(time, 0.001f), joint->getScale());

                //BD - Increase the key number since we are adding another key.
                joint_motion->mScaleCurve.mNumKeys++;

                joint_motion->mScaleCurve.mKeys[(float)(joint_motion->mScaleCurve.mNumKeys)] = scale_key;
            }
            //BD - Conversely put the key at the start of the list if CTRL is held while adding a key.
            else if (gKeyboard->currentMask(TRUE) == MASK_CONTROL)
            {
                LLKeyframeMotion::ScaleCurve scale_curve;

                //BD - Create a scale key and put it into the scale curve.
                LLKeyframeMotion::ScaleKey scale_key = LLKeyframeMotion::ScaleKey(ll_round(time, 0.001f), joint->getScale());

                scale_curve.mNumKeys++;
                scale_curve.mKeys[(float)(count)] = scale_key;
                count++;

                //BD - We have to rewrite the entire rotation curve in order to push a key to the beginning.
                for (auto& it : joint_motion->mScaleCurve.mKeys)
                {
                    scale_curve.mNumKeys++;
                    scale_curve.mKeys[(float)(count)] = it.second;
                    count++;
                }

                joint_motion->mScaleCurve = scale_curve;
            }
            //BD - Put the key after whatever is currently selected.
            else
            {
                LLKeyframeMotion::ScaleCurve scale_curve;

                //BD - We have to rewrite the entire rotation curve in order to push a key inbetween others.
                for (auto& it : joint_motion->mScaleCurve.mKeys)
                {
                    scale_curve.mNumKeys++;
                    scale_curve.mKeys[(float)(count)] = it.second;
                    count++;

                    if ((S32)it.first == selected_idx)
                    {
                        //BD - Add a bit of time to differentiate the keys.
                        //     This also kinda determines if this is the first keyframe or not. If we can't find a previous
                        //     entry we assume its the first keyframe, so simply use 1.f.
                        time = it.second.mTime + FRAMETIME;

                        //BD - Create a scale key and put it into the scale curve.
                        LLKeyframeMotion::ScaleKey scale_key = LLKeyframeMotion::ScaleKey(ll_round(time, 0.001f), joint->getScale());

                        scale_curve.mNumKeys++;
                        scale_curve.mKeys[(float)(count)] = scale_key;
                        count++;
                    }
                }

                joint_motion->mScaleCurve = scale_curve;
            }

            keys = joint_motion->mScaleCurve.mNumKeys;

            LLJointState* joint_state = gDragonAnimator.mPoseCreatorMotion->findJointState(joint);
            if (joint_state->getUsage() ^ LLJointState::SCALE)
            {
                joint_state->setUsage(joint_state->getUsage() | LLJointState::SCALE);
            }
        }
    }

    //BD - Refresh the keyframes.
    onKeyframeRefresh();

    //BD - Check if our animation duration needs changing.
    onAnimationDurationCheck();

    //BD - Always select the last entry whenever we switch bones to allow quickly making
    //     changes or adding new keyframes.
    mKeyframeScroll->selectNthItem(new_selected_idx);
}

void BDFloaterPoseCreator::onKeyframeRemove()
{
    LLScrollListItem* item = mKeyframeScroll->getFirstSelected();
    if (!item)
        return;

    LLScrollListItem* joint_item = mJointScrolls[BD_JOINTS]->getFirstSelected();
    if (!joint_item)
        return;

    LLJoint* joint = (LLJoint*)joint_item->getUserdata();
    if (!joint)
        return;

    if (!gDragonAnimator.mPoseCreatorMotion)
        return;

    LLKeyframeMotion::JointMotionList* joint_list = gDragonAnimator.mPoseCreatorMotion->getJointMotionList();
    //if (!joint_list)
    //    return;

    LLKeyframeMotion::JointMotion* joint_motion = joint_list->getJointMotion(joint->getJointNum());
    LLJointState* joint_state = gDragonAnimator.mPoseCreatorMotion->findJointState(joint);
    S32 idx = mModifierTabs->getCurrentPanelIndex();
    S32 si = mKeyframeScroll->getFirstSelected()->getColumn(1)->getValue().asInteger();
    S32 usage = joint_state->getUsage();
    S32 count = 1;
    S32 last_selected = mKeyframeScroll->getFirstSelectedIndex();

    if (idx == 0)
    {
        LLKeyframeMotion::RotationCurve rot_curve;
        //BD - Rather than removing the key, rewriting all index numbers or shoving keys around
        //     we simply opt to create a new curve, this automatically reorganizes and counts all
        //     keys putting them in their proper place making this a simple solution. Not exactly
        //     efficient but it hardly matters here.
        for (auto& it : joint_motion->mRotationCurve.mKeys)
        {
            if((S32)it.first != si)
            {
                rot_curve.mKeys[(float)(count)] = it.second;
                rot_curve.mNumKeys = count;
                count++;
            }
        }

        joint_motion->mRotationCurve = rot_curve;

        if (joint_motion->mRotationCurve.mNumKeys == 0)
        {
            if (usage & LLJointState::ROT)
            {
                usage &= !((S32)(LLJointState::ROT));
                joint_state->setUsage(usage);
            }
        }
    }
    else if (idx == 1)
    {
        LLKeyframeMotion::PositionCurve pos_curve;
        for (auto& it : joint_motion->mPositionCurve.mKeys)
        {
            if ((S32)it.first != si)
            {
                pos_curve.mKeys[(float)(count)] = it.second;
                pos_curve.mNumKeys = count;
                count++;
            }
        }

        joint_motion->mPositionCurve = pos_curve;

        if (joint_motion->mPositionCurve.mNumKeys == 0)
        {
            if (usage & LLJointState::POS)
            {
                usage &= !((S32)(LLJointState::POS));
                joint_state->setUsage(usage);
            }
        }
    }
    else if (idx == 2)
    {
        LLKeyframeMotion::ScaleCurve scale_curve;
        for (auto& it : joint_motion->mScaleCurve.mKeys)
        {
            if ((S32)it.first != si)
            {
                scale_curve.mKeys[(float)(count)] = it.second;
                scale_curve.mNumKeys = count;
                count++;
            }
        }

        joint_motion->mScaleCurve = scale_curve;

        if (joint_motion->mScaleCurve.mNumKeys == 0)
        {
            if (usage & LLJointState::SCALE)
            {
                usage &= !((S32)(LLJointState::SCALE));
                joint_state->setUsage(usage);
            }
        }
    }

    //BD - Refresh the keyframes.
    onKeyframeRefresh();

    //BD - Check if our animation duration needs changing.
    onAnimationDurationCheck();

    //BD - Select the next entry in line after deleting one.
    mKeyframeScroll->selectNthItem(last_selected);
}

void BDFloaterPoseCreator::onKeyframeTime()
{
    LLScrollListItem* item = mKeyframeScroll->getFirstSelected();
    if (!item)
        return;

    LLScrollListItem* joint_item = mJointScrolls[BD_JOINTS]->getFirstSelected();
    if (!joint_item)
        return;

    LLJoint* joint = (LLJoint*)joint_item->getUserdata();
    if (!joint)
        return;

    if (!gDragonAnimator.mPoseCreatorMotion)
        return;

    LLKeyframeMotion::JointMotionList* joint_list = gDragonAnimator.mPoseCreatorMotion->getJointMotionList();
    //if (!joint_list)
    //    return;

    LLKeyframeMotion::JointMotion* joint_motion = joint_list->getJointMotion(joint->getJointNum());
    S32 si = mKeyframeScroll->getFirstSelectedIndex() + 1;
    S32 idx = mModifierTabs->getCurrentPanelIndex();
    //BD - Set times in frames rather than direct time, it's easier for the user.
    //     We assume 60 FPS for an animation by default.
    S32 time_frames = getChild<LLUICtrl>("key_time")->getValue().asInteger();
    F32 time_float = FRAMETIME * time_frames;

    if (idx == 0)
    {
        //BD - Find the key we are looking for and remove it.
        for (auto& it : joint_motion->mRotationCurve.mKeys)
        {
            if ((S32)it.first == si)
            {
                it.second.mTime = ll_round(time_float, 0.001f);
            }
        }
    }
    else if (idx == 1)
    {
        for (auto& it : joint_motion->mPositionCurve.mKeys)
        {
            if((S32)it.first == si)
            {
                it.second.mTime = ll_round(time_float, 0.001f);
            }
        }
    }
    else if (idx == 2)
    {
        for (auto& it : joint_motion->mScaleCurve.mKeys)
        {
            if ((S32)it.first == si)
            {
                it.second.mTime = ll_round(time_float, 0.001f);
            }
        }
    }

    onKeyframeRefresh();
    mKeyframeScroll->selectNthItem(si - 1);
    //item->getColumn(0)->setValue(time_frames);
    onAnimationDurationCheck();
}

void BDFloaterPoseCreator::onEditAnimationInfo(const LLSD& param)
{
    if (!gDragonAnimator.mPoseCreatorMotion)
        return;

    LLKeyframeMotion::JointMotionList* joint_list = gDragonAnimator.mPoseCreatorMotion->getJointMotionList();
    //if (!joint_list)
    //    return;

    std::string msg = param.asString();
    if (msg == "ease_in")
    {
        joint_list->mEaseInDuration = (F32)(getChild<LLUICtrl>("ease_in")->getValue().asReal());
    }
    else if (msg == "ease_out")
    {
        joint_list->mEaseOutDuration = (F32)(getChild<LLUICtrl>("ease_out")->getValue().asReal());
    }
    else if (msg == "duration_auto")
    {
        mAutoDuration = getChild<LLUICtrl>("AutoDuration")->getValue().asBoolean();
        onAnimationDurationCheck();
    }
    else if (msg == "duration")
    {
        joint_list->mDuration = (F32)(getChild<LLUICtrl>("keyframe_duration")->getValue().asReal());
    }
    else if (msg == "loop")
    {
        joint_list->mLoop = getChild<LLUICtrl>("LoopAnimation")->getValue().asBoolean();
    }
    else if (msg == "loop_in")
    {
        joint_list->mLoopInPoint = (F32)(getChild<LLUICtrl>("loop_in")->getValue().asReal());
    }
    else if (msg == "loop_out")
    {
        joint_list->mLoopOutPoint = (F32)(getChild<LLUICtrl>("loop_out")->getValue().asReal());
    }
    else if (msg == "priority")
    {
        joint_list->mBasePriority = (LLJoint::JointPriority)getChild<LLUICtrl>("base_priority")->getValue().asInteger();
    }
    else if (msg == "hand")
    {
        joint_list->mHandPose = (LLHandMotion::eHandPose)getChild<LLUICtrl>("hand_poses")->getValue().asInteger();
    }
}

void BDFloaterPoseCreator::onInterpolationChange(LLUICtrl* ctrl)
{
    LLScrollListItem* joint_item = mJointScrolls[BD_JOINTS]->getFirstSelected();
    if (!joint_item)
        return;

    LLJoint* joint = (LLJoint*)joint_item->getUserdata();
    if (!joint)
        return;

    if (!gDragonAnimator.mPoseCreatorMotion)
        return;

    LLKeyframeMotion::JointMotionList* joint_list = gDragonAnimator.mPoseCreatorMotion->getJointMotionList();
    //if (!joint_list)
    //    return;

    LLKeyframeMotion::JointMotion* joint_motion = joint_list->getJointMotion(joint->getJointNum());
    S32 interp_idx = ctrl->getValue().asInteger();
    S32 idx = mModifierTabs->getCurrentPanelIndex();

    if (idx == 0)
    {
        joint_motion->mRotationCurve.mInterpolationType = (LLKeyframeMotion::InterpolationType)interp_idx;
    }
    else if (idx == 1)
    {
        joint_motion->mPositionCurve.mInterpolationType = (LLKeyframeMotion::InterpolationType)interp_idx;
    }
    else if (idx == 2)
    {
        joint_motion->mScaleCurve.mInterpolationType = (LLKeyframeMotion::InterpolationType)interp_idx;
    }
}

void BDFloaterPoseCreator::onAnimationDurationCheck()
{
    if (!mAutoDuration)
        return;

    if (!gDragonAnimator.mPoseCreatorMotion)
        return;

    //LLKeyframeMotion::JointMotionList* joint_list = gDragonAnimator.mPoseCreatorMotion->getJointMotionList();
    //if (!joint_list)
    //    return;

    LLScrollListItem* joint_item = mJointScrolls[BD_JOINTS]->getFirstSelected();
    if (!joint_item)
        return;

    LLJoint* joint = (LLJoint*)joint_item->getUserdata();
    if (!joint)
        return;

    LLKeyframeMotion::JointMotionList* joint_list = gDragonAnimator.mPoseCreatorMotion->getJointMotionList();
    F32 new_time = 0.0f;

    //BD - Go through all motions and all keys and determine the largest keyframe time.
    for (auto& joint_motion : joint_list->mJointMotionArray)
    {
        for (auto& it : joint_motion->mRotationCurve.mKeys)
        {
            new_time = ll_round(llmax(new_time, it.second.mTime), 0.001f);
        }

        for (auto& it : joint_motion->mPositionCurve.mKeys)
        {
            new_time = ll_round(llmax(new_time, it.second.mTime), 0.001f);
        }

        for (auto& it : joint_motion->mScaleCurve.mKeys)
        {
            new_time = ll_round(llmax(new_time, it.second.mTime), 0.001f);
        }
    }

    joint_list->mDuration = new_time;
    joint_list->mLoopOutPoint = new_time;
    getChild<LLUICtrl>("keyframe_duration")->setValue(new_time);
    getChild<LLSpinCtrl>("loop_out")->setMaxValue(new_time);
    getChild<LLUICtrl>("loop_out")->setValue(new_time);
}

////////////////////////////////
//BD - Poses
////////////////////////////////

void BDFloaterPoseCreator::onPoseStartStop()
{
    BDPosingMotion* pmotion = (BDPosingMotion*)gAgentAvatarp->findMotion(ANIM_BD_POSING_MOTION);
    LLKeyframeMotion* tmotion = (LLKeyframeMotion*)gAgentAvatarp->findMotion(mTempMotionID);
    if(gDragonAnimator.mPoseCreatorMotion)
    {
        //BD - The reason we create a new preview motion here is because LLMotionController can deprecate and
        //     destroy our preview animation if we start and stop it and leave it stopped for a while. This
        //     would destroy all our work. Leaving the actual internal temp motion alive without ever running
        //     it prevents it from ever being put into the active list, thus never dropping it into the deactivated
        //     list when we stop it preventing it from being destroyed at some point.
        if (!tmotion)
        {
            std::string full_path = gDirUtilp->getExpandedFilename(LL_PATH_ANIMATIONS, "_poser_temp.anim");
            gDragonAnimator.mPoseCreatorMotion->dumpToFile(full_path);

            LLKeyframeMotion* motion = onReadyTempMotion();
            if (motion)
            {
                //BD - Start the newly created preview animation and set it to be eternal as
                //     we are going to continue editing it unless we start the preview again.
                gAgentAvatarp->startMotion(motion->getID());
                mTempMotionID = motion->getID();
            }
        }
        else
        {
            if (tmotion->isStopped())
            {
                //BD - Save the animation temporarily.
                std::string full_path = gDirUtilp->getExpandedFilename(LL_PATH_ANIMATIONS, "_poser_temp.anim");
                gDragonAnimator.mPoseCreatorMotion->dumpToFile(full_path);
                //BD - Reload the animation data back into our already existing animation.
                //     To refresh it and make sure its always up to date with all our changes.
                LLKeyframeMotion* tmotion = onReadyTempMotion();
                if (tmotion)
                {
                    //BD - Start the newly created preview animation and set it to be eternal as
                    //     we are going to continue editing it unless we start the preview again.
                    gAgentAvatarp->startMotion(tmotion->getID());
                    mTempMotionID = tmotion->getID();
                }
            }
            else
            {
                gAgentAvatarp->stopMotion(tmotion->getID());
            }
        }

        //BD - Stop our Poser motion if its active, start it again if we are still posing after disabling
        //     the preview.
        if (gAgentAvatarp->mIsPosing)
        {
            if (!pmotion || pmotion->isStopped())
            {
                gAgentAvatarp->startMotion(ANIM_BD_POSING_MOTION);
                //BD - We need to reset the skeleton here and reapply all the latest keyframes.
                onJointRotPosScaleReset();
                onPoseReapply();
            }
            else
            {
                gAgentAvatarp->stopMotion(ANIM_BD_POSING_MOTION);
            }
        }
    }
}

bool BDFloaterPoseCreator::onClickPoseSave(const LLSD& param)
{
    bool ret = onPoseExport();
    if (ret)
        LLNotificationsUtil::add("PoserExportANIMSuccess");
    return ret;
}

void BDFloaterPoseCreator::onPoseReapply()
{
    if(!gDragonAnimator.mPoseCreatorMotion)
        return;

    LLKeyframeMotion::JointMotionList* joint_list = gDragonAnimator.mPoseCreatorMotion->getJointMotionList();
    if (!joint_list)
        return;

    for (auto& joint_motion : joint_list->mJointMotionArray)
    {
        //LLJoint* joint = gAgentAvatarp->getJoint(JointKey::construct(joint_motion->mJointName));
        LLJoint* joint = gAgentAvatarp->getJoint(joint_motion->mJointName);
        if (joint)
        {
            LLKeyframeMotion::RotationCurve rot_curve = joint_motion->mRotationCurve;
            LLKeyframeMotion::PositionCurve pos_curve = joint_motion->mPositionCurve;
            LLKeyframeMotion::ScaleCurve scale_curve = joint_motion->mScaleCurve;

            //BD - Load in the last keyframe of every bone and apply it to give us a
            //     start.
            for (auto& rot_key : rot_curve.mKeys)
            {
                LLQuaternion rot = rot_key.second.mRotation;
                joint->setTargetRotation(rot);
            }

            for (auto& pos_key : pos_curve.mKeys)
            {
                LLVector3 pos = pos_key.second.mPosition;
                joint->setTargetPosition(pos);
            }

            for (auto& scale_key : scale_curve.mKeys)
            {
                LLVector3 scale = scale_key.second.mScale;
                joint->setScale(scale);
            }

            //BD - We support per bone priority. Question is does SL?
            //LLJoint::JointPriority priority = joint_motion->mPriority;
        }
    }
}

void BDFloaterPoseCreator::onPoseStart()
{
    BDPosingMotion* motion = (BDPosingMotion*)gAgentAvatarp->findMotion(ANIM_BD_POSING_MOTION);
    if (!motion || motion->isStopped())
    {
        gAgentAvatarp->setPosing();

        //BD - Grab our current defaults to revert to them when stopping the Poser.
        if(gAgentAvatarp->isFullyLoaded())
            onCollectDefaults();

        gAgent.stopFidget();

        gAgentAvatarp->startMotion(ANIM_BD_POSING_MOTION);
        onJointRotPosScaleReset();

        //BD - Do not recreate the pose, keep the animation so we can return to it.
        if(!gDragonAnimator.mPoseCreatorMotion)
        {
            //BD - Create the temp posing motion that we will use for the animation.
            onCreateTempMotion();
            gDragonAnimator.mPoseCreatorMotion = onReadyTempMotion("_poser_temp.anim" ,true);
        }
    }
    else
    {
        //BD - Reset everything, all rotations, positions and scales of all bones.
        onJointRotPosScaleReset();

        //BD - Clear posing when we're done now that we've safely endangered getting spaghetified.
        gAgentAvatarp->clearPosing();
        gAgentAvatarp->stopMotion(ANIM_BD_POSING_MOTION);
    }

    //BD - Wipe the joint list.
    onJointRefresh();
}

bool BDFloaterPoseCreator::onPoseExport()
{
    BDPosingMotion* motion = (BDPosingMotion*)gAgentAvatarp->findMotion(ANIM_BD_POSING_MOTION);
    if (!motion)
        return false;

    if (!gDragonAnimator.mPoseCreatorMotion)
        return false;

    LLPose* pose = motion->getPose();
    if (!pose)
        return false;

    std::string motion_name = getChild<LLUICtrl>("export_name")->getValue().asString();
    if (motion_name.empty())
        return false;

    std::string full_path = gDirUtilp->getExpandedFilename(LL_PATH_ANIMATIONS, motion_name + ".anim");
    return gDragonAnimator.mPoseCreatorMotion->dumpToFile(full_path);
}

void BDFloaterPoseCreator::onPoseImport()
{
    BDPosingMotion* motion = (BDPosingMotion*)gAgentAvatarp->findMotion(ANIM_BD_POSING_MOTION);
    if (!motion)
        return;

    //BD - Put us back in T pose before importing a new animation. We want to apply
    //     the changes to a clean pose.
    onJointRotPosScaleReset();

    //BD - Create a new motion from an anim file from disk.
    //     Let us pick the file we want to preview with the filepicker.
    LLFilePicker& picker = LLFilePicker::instance();
    if (picker.getOpenFile(LLFilePicker::FFLOAD_ANIM))
    {
        std::string outfilename = picker.getFirstFile().c_str();
        gDragonAnimator.mPoseCreatorMotion = onReadyTempMotion(gDirUtilp->getBaseFileName(outfilename), true);

        onJointRefresh();
        onPoseReapply();
        onKeyframesRebuild();
        //onKeyframeRefresh();

        //BD - Always select the last entry whenever we switch bones to allow quickly making
        //     changes or adding new keyframes.
        //mKeyframeScroll->selectNthItem(mKeyframeScroll->getItemCount() - 1);
    }
}

////////////////////////////////
//BD - Joints
////////////////////////////////
void BDFloaterPoseCreator::onJointRefresh()
{
    //BD - Getting collision volumes and attachment points.
    std::vector<std::string> cv_names, attach_names;
    gAgentAvatarp->getSortedJointNames(1, cv_names);
    gAgentAvatarp->getSortedJointNames(2, attach_names);

    bool is_posing = gAgentAvatarp->getPosing();
    mJointScrolls[BD_JOINTS]->clearRows();
    mJointScrolls[BD_COLLISION_VOLUMES]->clearRows();
    mJointScrolls[BD_ATTACHMENT_BONES]->clearRows();

    LLVector3 rot;
    LLVector3 pos;
    LLVector3 scale;
    LLJoint* joint;
    for (S32 i = 0; (joint = gAgentAvatarp->getCharacterJoint(i)); ++i)
    {
        //BD - Nothing? Invalid? Skip, when we hit the end we'll break out anyway.
        if (!joint) continue;

        LLSD row;
        const std::string joint_name = joint->getName();
        //BD - Show some categories to make it a bit easier finding out which
        //     bone belongs where and what they might be for those who can't use
        //     bone names.
        if (joint->mJointNum == 0 ||    //mPelvis
            joint->mJointNum == 8 ||    //mHead
            joint->mJointNum == 58 ||   //mCollarLeft
            joint->mJointNum == 77 ||   //mCollarRight
            joint->mJointNum == 96 ||   //mWingsRoot
            joint->mJointNum == 107 ||  //mHipRight
            joint->mJointNum == 112 ||  //mHipLeft
            joint->mJointNum == 117 ||  //mTail1
            joint->mJointNum == 123)    //mGroin
        {
            row["columns"][BD_COL_ICON]["column"] = "icon";
            row["columns"][BD_COL_ICON]["type"] = "icon";
            row["columns"][BD_COL_ICON]["value"] = getString("icon_category");
            row["columns"][BD_COL_NAME]["column"] = "joint";
            row["columns"][BD_COL_NAME]["value"] = getString("title_" + joint_name);
            LLScrollListItem* element = mJointScrolls[BD_JOINTS]->addElement(row);
            element->setEnabled(FALSE);
        }

        row["columns"][BD_COL_ICON]["column"] = "icon";
        row["columns"][BD_COL_ICON]["type"] = "icon";
        row["columns"][BD_COL_ICON]["value"] = getString("icon_bone");
        row["columns"][BD_COL_NAME]["column"] = "joint";
        row["columns"][BD_COL_NAME]["value"] = joint_name;

        if (is_posing)
        {
            //BD - Bone Rotations
            joint->getTargetRotation().getEulerAngles(&rot.mV[VX], &rot.mV[VY], &rot.mV[VZ]);
            row["columns"][BD_COL_ROT_X]["column"] = "x";
            row["columns"][BD_COL_ROT_X]["value"] = ll_round(rot.mV[VX], 0.001f);
            row["columns"][BD_COL_ROT_Y]["column"] = "y";
            row["columns"][BD_COL_ROT_Y]["value"] = ll_round(rot.mV[VY], 0.001f);
            row["columns"][BD_COL_ROT_Z]["column"] = "z";
            row["columns"][BD_COL_ROT_Z]["value"] = ll_round(rot.mV[VZ], 0.001f);

            //BD - Bone Positions
            pos = joint->getTargetPosition();
            row["columns"][BD_COL_POS_X]["column"] = "pos_x";
            row["columns"][BD_COL_POS_X]["value"] = ll_round(pos.mV[VX], 0.001f);
            row["columns"][BD_COL_POS_Y]["column"] = "pos_y";
            row["columns"][BD_COL_POS_Y]["value"] = ll_round(pos.mV[VY], 0.001f);
            row["columns"][BD_COL_POS_Z]["column"] = "pos_z";
            row["columns"][BD_COL_POS_Z]["value"] = ll_round(pos.mV[VZ], 0.001f);

            //BD - Bone Scales
            scale = joint->getScale();
            row["columns"][BD_COL_SCALE_X]["column"] = "scale_x";
            row["columns"][BD_COL_SCALE_X]["value"] = ll_round(scale.mV[VX], 0.001f);
            row["columns"][BD_COL_SCALE_Y]["column"] = "scale_y";
            row["columns"][BD_COL_SCALE_Y]["value"] = ll_round(scale.mV[VY], 0.001f);
            row["columns"][BD_COL_SCALE_Z]["column"] = "scale_z";
            row["columns"][BD_COL_SCALE_Z]["value"] = ll_round(scale.mV[VZ], 0.001f);
        }

        LLScrollListItem* item = mJointScrolls[BD_JOINTS]->addElement(row);
        item->setUserdata(joint);

        //BD - We need to check if we are posing or not, simply set all bones to deactivated
        //     when we are not posed otherwise they will remain on "enabled" state. This behavior
        //     could be confusing to the user, this is due to how animations work.
        if (is_posing)
        {
            BDPosingMotion* motion = (BDPosingMotion*)gAgentAvatarp->findMotion(ANIM_BD_POSING_MOTION);
            if (motion)
            {
                LLPose* pose = motion->getPose();
                if (pose)
                {
                    // BD - We do check here for the joint_state because otherwise we end up with the toggle
                    //      state not appearing properly toggled/untoggled when we first refresh after firing
                    //      up the poser. At the same time this is used to refresh all bone states when we
                    //      load a pose.
                    LLPointer<LLJointState> joint_state = pose->findJointState(joint);
                    if (joint_state)
                    {
                        ((LLScrollListText*)item->getColumn(BD_COL_NAME))->setFontStyle(LLFontGL::BOLD);
                    }
                }
            }
        }
        else
        {
            ((LLScrollListText*)item->getColumn(BD_COL_NAME))->setFontStyle(LLFontGL::NORMAL);
        }
    }

    //BD - We add an empty line into both lists and include an icon to automatically resize
    //     the list row heights to sync up with the height in our joint list. We remove it
    //     immediately after anyway.
    LLSD test_row;
    test_row["columns"][BD_COL_ICON]["column"] = "icon";
    test_row["columns"][BD_COL_ICON]["type"] = "icon";
    test_row["columns"][BD_COL_ICON]["value"] = getString("icon_category");
    mJointScrolls[BD_COLLISION_VOLUMES]->addElement(test_row);
    mJointScrolls[BD_COLLISION_VOLUMES]->deleteSingleItem(0);
    mJointScrolls[BD_ATTACHMENT_BONES]->addElement(test_row);
    mJointScrolls[BD_ATTACHMENT_BONES]->deleteSingleItem(0);

    //BD - Collision Volumes
    for (auto name : cv_names)
    {
        //joint = gAgentAvatarp->getJoint(JointKey::construct(name));
        joint = gAgentAvatarp->getJoint(name);
        //BD - Nothing? Invalid? Skip, when we hit the end we'll break out anyway.
        if (!joint) continue;

        LLSD row;
        row["columns"][BD_COL_ICON]["column"] = "icon";
        row["columns"][BD_COL_ICON]["type"] = "icon";
        row["columns"][BD_COL_ICON]["value"] = getString("icon_bone");
        row["columns"][BD_COL_NAME]["column"] = "joint";
        row["columns"][BD_COL_NAME]["value"] = name;

        if (is_posing)
        {
            //BD - Bone Rotations
            //     It's stupid but we have to define the empty columns here and fill them with
            //     nothing otherwise the addRow() function is assuming that the sometimes missing
            //     rotation columns have an "empty" name and thus creates faulty extra columns.
            row["columns"][BD_COL_ROT_X]["column"] = "x";
            row["columns"][BD_COL_ROT_X]["value"] = "";
            row["columns"][BD_COL_ROT_Y]["column"] = "y";
            row["columns"][BD_COL_ROT_Y]["value"] = "";
            row["columns"][BD_COL_ROT_Z]["column"] = "z";
            row["columns"][BD_COL_ROT_Z]["value"] = "";

            //BD - Bone Positions
            pos = joint->getPosition();
            row["columns"][BD_COL_POS_X]["column"] = "pos_x";
            row["columns"][BD_COL_POS_X]["value"] = ll_round(pos.mV[VX], 0.001f);
            row["columns"][BD_COL_POS_Y]["column"] = "pos_y";
            row["columns"][BD_COL_POS_Y]["value"] = ll_round(pos.mV[VY], 0.001f);
            row["columns"][BD_COL_POS_Z]["column"] = "pos_z";
            row["columns"][BD_COL_POS_Z]["value"] = ll_round(pos.mV[VZ], 0.001f);

            //BD - Bone Scales
            scale = joint->getScale();
            row["columns"][BD_COL_SCALE_X]["column"] = "scale_x";
            row["columns"][BD_COL_SCALE_X]["value"] = ll_round(scale.mV[VX], 0.001f);
            row["columns"][BD_COL_SCALE_Y]["column"] = "scale_y";
            row["columns"][BD_COL_SCALE_Y]["value"] = ll_round(scale.mV[VY], 0.001f);
            row["columns"][BD_COL_SCALE_Z]["column"] = "scale_z";
            row["columns"][BD_COL_SCALE_Z]["value"] = ll_round(scale.mV[VZ], 0.001f);
        }

        LLScrollListItem* item = mJointScrolls[BD_COLLISION_VOLUMES]->addElement(row);
        item->setUserdata(joint);
    }

    //BD - Attachment Bones
    for (auto name : attach_names)
    {
        //joint = gAgentAvatarp->getJoint(JointKey::construct(name));
        joint = gAgentAvatarp->getJoint(name);
        //BD - Nothing? Invalid? Skip, when we hit the end we'll break out anyway.
        if (!joint) continue;

        LLSD row;
        row["columns"][BD_COL_ICON]["column"] = "icon";
        row["columns"][BD_COL_ICON]["type"] = "icon";
        row["columns"][BD_COL_ICON]["value"] = getString("icon_bone");
        row["columns"][BD_COL_NAME]["column"] = "joint";
        row["columns"][BD_COL_NAME]["value"] = name;

        if (is_posing)
        {
            //BD - Bone Rotations
            //     It's stupid but we have to define the empty columns here and fill them with
            //     nothing otherwise the addRow() function is assuming that the sometimes missing
            //     rotation columns have an "empty" name and thus creates faulty extra columns.
            row["columns"][BD_COL_ROT_X]["column"] = "x";
            row["columns"][BD_COL_ROT_X]["value"] = "";
            row["columns"][BD_COL_ROT_Y]["column"] = "y";
            row["columns"][BD_COL_ROT_Y]["value"] = "";
            row["columns"][BD_COL_ROT_Z]["column"] = "z";
            row["columns"][BD_COL_ROT_Z]["value"] = "";

            //BD - Bone Positions
            pos = joint->getPosition();
            row["columns"][BD_COL_POS_X]["column"] = "pos_x";
            row["columns"][BD_COL_POS_X]["value"] = ll_round(pos.mV[VX], 0.001f);
            row["columns"][BD_COL_POS_Y]["column"] = "pos_y";
            row["columns"][BD_COL_POS_Y]["value"] = ll_round(pos.mV[VY], 0.001f);
            row["columns"][BD_COL_POS_Z]["column"] = "pos_z";
            row["columns"][BD_COL_POS_Z]["value"] = ll_round(pos.mV[VZ], 0.001f);

            //BD - Bone Scales
            scale = joint->getScale();
            row["columns"][BD_COL_SCALE_X]["column"] = "scale_x";
            row["columns"][BD_COL_SCALE_X]["value"] = ll_round(scale.mV[VX], 0.001f);
            row["columns"][BD_COL_SCALE_Y]["column"] = "scale_y";
            row["columns"][BD_COL_SCALE_Y]["value"] = ll_round(scale.mV[VY], 0.001f);
            row["columns"][BD_COL_SCALE_Z]["column"] = "scale_z";
            row["columns"][BD_COL_SCALE_Z]["value"] = ll_round(scale.mV[VZ], 0.001f);
        }

        LLScrollListItem* item = mJointScrolls[BD_ATTACHMENT_BONES]->addElement(row);
        item->setUserdata(joint);
    }

    onJointControlsRefresh();
}

void BDFloaterPoseCreator::onJointControlsRefresh()
{
    bool is_pelvis = false;
    bool is_posing = (gAgentAvatarp->isFullyLoaded() && gAgentAvatarp->getPosing());
    bool is_previewing = false;
    S32 index = mJointTabs->getCurrentPanelIndex();
    LLScrollListItem* item = mJointScrolls[index]->getFirstSelected();
    is_previewing = gDragonAnimator.mPoseCreatorMotion && !gDragonAnimator.mPoseCreatorMotion->isStopped();

    if (item)
    {
        LLJoint* joint = (LLJoint*)item->getUserdata();
        if (joint)
        {
            if (index == 0)
            {
                mRotationSliders[VX]->setValue(item->getColumn(BD_COL_ROT_X)->getValue());
                mRotationSliders[VY]->setValue(item->getColumn(BD_COL_ROT_Y)->getValue());
                mRotationSliders[VZ]->setValue(item->getColumn(BD_COL_ROT_Z)->getValue());
            }

            is_pelvis = (joint->mJointNum == 0);

            mPositionSliders[VX]->setValue(item->getColumn(BD_COL_POS_X)->getValue());
            mPositionSliders[VY]->setValue(item->getColumn(BD_COL_POS_Y)->getValue());
            mPositionSliders[VZ]->setValue(item->getColumn(BD_COL_POS_Z)->getValue());

            //BD - Bone Scales
            mScaleSliders[VX]->setValue(item->getColumn(BD_COL_SCALE_X)->getValue());
            mScaleSliders[VY]->setValue(item->getColumn(BD_COL_SCALE_Y)->getValue());
            mScaleSliders[VZ]->setValue(item->getColumn(BD_COL_SCALE_Z)->getValue());

            BDPosingMotion* motion = (BDPosingMotion*)gAgentAvatarp->findMotion(ANIM_BD_POSING_MOTION);
            if (motion)
            {
                //BD - If we don't use our default spherical interpolation, set it once.
                motion->setInterpolationTime(0.25f);
                motion->setInterpolationType(2);

                LLPose* pose = motion->getPose();
                if (pose)
                {
                    LLPointer<LLJointState> joint_state = pose->findJointState(joint);
                    getChild<LLButton>("toggle_bone")->setValue(joint_state.notNull());
                }
            }
        }
    }

    getChild<LLButton>("toggle_bone")->setEnabled(item && is_posing && index == BD_JOINTS);
    mStartPosingBtn->setValue(is_posing);
    getChild<LLUICtrl>("export_poses")->setEnabled(is_posing);
    getChild<LLUICtrl>("import_poses")->setEnabled(is_posing);
    getChild<LLUICtrl>("joints_tabs")->setEnabled(is_posing);

    //BD - Enable position tabs whenever positions are available, scales are always enabled
    //     unless we are editing attachment bones, rotations on the other hand are only
    //     enabled when editing joints.
    S32 curr_idx = mModifierTabs->getCurrentPanelIndex();
    mModifierTabs->setEnabled(item && is_posing && !is_previewing);
    mModifierTabs->enableTabButton(0, (item && is_posing && index == BD_JOINTS));
    mModifierTabs->enableTabButton(1, (item && is_posing));
    mModifierTabs->enableTabButton(2, (item && is_posing && index == BD_COLLISION_VOLUMES));
    //BD - Swap out of "Position" tab when it's not available.
    if (curr_idx == 1 && !mModifierTabs->getTabButtonEnabled(1))
    {
        mModifierTabs->selectTab(0);
    }
    //BD - Swap out of "Scale" and "Rotation" tabs when they are not available.
    if ((curr_idx == 2 && !mModifierTabs->getTabButtonEnabled(2))
        || (curr_idx == 0 && !mModifierTabs->getTabButtonEnabled(0)))
    {
        mModifierTabs->selectTab(1);
    }

    F32 max_val = is_pelvis ? 20.f : 1.0f;
    mPositionSliders[VX]->setMaxValue(max_val);
    mPositionSliders[VY]->setMaxValue(max_val);
    mPositionSliders[VZ]->setMaxValue(max_val);
    mPositionSliders[VX]->setMinValue(-max_val);
    mPositionSliders[VY]->setMinValue(-max_val);
    mPositionSliders[VZ]->setMinValue(-max_val);

    //BD - Change our animator's target, make sure it is always up-to-date.
    gDragonAnimator.mTargetAvatar = gAgentAvatarp;

    getChild<LLUICtrl>("add_keyframe")->setEnabled(is_posing && !is_previewing);
    getChild<LLUICtrl>("remove_keyframe")->setEnabled(is_posing && !is_previewing);
    getChild<LLUICtrl>("interpolation")->setEnabled(is_posing && !is_previewing);
    getChild<LLUICtrl>("key_time")->setEnabled(is_posing && !is_previewing);
    mStartPosingBtn->setEnabled(!is_previewing);

    //mJointScrolls[BD_JOINTS]->refreshLineHeight();
    //mJointScrolls[BD_COLLISION_VOLUMES]->refreshLineHeight();
    //mJointScrolls[BD_ATTACHMENT_BONES]->refreshLineHeight();
}

void BDFloaterPoseCreator::onJointSet(LLUICtrl* ctrl, const LLSD& param)
{
    //BD - Rotations are only supported by joints so far, everything
    //     else snaps back instantly.
    LLScrollListItem* item = mJointScrolls[BD_JOINTS]->getFirstSelected();
    if (!item)
        return;

    LLJoint* joint = (LLJoint*)item->getUserdata();
    if (!joint)
        return;

    if (!gDragonAnimator.mPoseCreatorMotion)
        return;

    //LLKeyframeMotion::JointMotionList* joint_list = gDragonAnimator.mPoseCreatorMotion->getJointMotionList();
    //if (!joint_list)
    //    return;

    LLScrollListItem* key_item = mKeyframeScroll->getFirstSelected();

    //BD - Neat yet quick and direct way of rotating our bones.
    //     No more need to include bone rotation orders.
    F32 val = (F32)(ctrl->getValue().asReal());
    S32 axis = param.asInteger();
    LLScrollListCell* cell[3] = { item->getColumn(BD_COL_ROT_X), item->getColumn(BD_COL_ROT_Y), item->getColumn(BD_COL_ROT_Z) };
    LLQuaternion rot_quat = joint->getTargetRotation();
    LLMatrix3 rot_mat;
    F32 old_value = (F32)(cell[axis]->getValue().asReal());
    F32 new_value = val - old_value;
    LLVector3 vec3;
    S32 time = key_item ? key_item->getColumn(0)->getValue().asInteger() : 1;

    cell[axis]->setValue(ll_round(val, 0.001f));
    vec3.mV[axis] = new_value;
    rot_mat = LLMatrix3(vec3.mV[VX], vec3.mV[VY], vec3.mV[VZ]);
    rot_quat = LLQuaternion(rot_mat)*rot_quat;
    joint->setTargetRotation(rot_quat);
    if (!mEasyRotations)
    {
        rot_quat.getEulerAngles(&vec3.mV[VX], &vec3.mV[VY], &vec3.mV[VZ]);
        S32 i = 0;
        while (i < 3)
        {
            if (i != axis)
            {
                cell[i]->setValue(ll_round(vec3.mV[i], 0.001f));
                mRotationSliders[i]->setValue(item->getColumn(i + 2)->getValue());
            }
            ++i;
        }
    }

    //BD - After a lot of different approaches this seemed to be the most feasible and functional.
	LLKeyframeMotion::JointMotionList* joint_list = gDragonAnimator.mPoseCreatorMotion->getJointMotionList();
    LLKeyframeMotion::JointMotion* joint_motion = joint_list->getJointMotion(joint->getJointNum());
    LLKeyframeMotion::RotationCurve rot_curve = joint_motion->mRotationCurve;

    //BD - Attempt to find the keyframe first.
    bool found = false;
    S32 si = mKeyframeScroll->getFirstSelectedIndex() + 1;
    for (auto it = joint_motion->mRotationCurve.mKeys.begin();
        it != joint_motion->mRotationCurve.mKeys.end(); )
    {
        auto curr_it = it;
        //BD - Previously we were rounding the keyframe time and comparing it to the time set
        //     in the keyframe list entry, this was ugly and had a big downside, having two
        //     or more entries with the same time resulting in all of them getting changed.
        //     Lucky for us, adding new keyframes into any transformation curve automatically
        //     counts a float value (used here as S32 since its whole numbers anyway) up by 1
        //     for every keyframe added, this allows us to easily "compare" the what seems to be
        //     the index number of the keyframe, against the index number in the list.
        //     This should work for now until we decide to allow reordering keyframes.
        //if (ll_round(curr_it->second.mTime, 0.1f) == ll_round(time, 0.1f))
        if ((S32)curr_it->first == si)
        {
            found = true;
            curr_it->second.mRotation = joint->getTargetRotation();

            F32 roll, pitch, yaw;
            LLQuaternion rot_quat = joint->getTargetRotation();
            rot_quat.getEulerAngles(&roll, &pitch, &yaw);
            //BD - Should we really be able to get here? The comparison above should already
            //     prevent ever getting here since a missing selection means we will never
            //     find a keyframe. I don't trust anything.
            if (key_item)
            {
                key_item->getColumn(2)->setValue(ll_round(roll, 0.001f));
                key_item->getColumn(3)->setValue(ll_round(pitch, 0.001f));
                key_item->getColumn(4)->setValue(ll_round(yaw, 0.001f));
            }
            break;
        }
        ++it;
    }

    //BD - We couldn't find a keyframe, create one automatically.
    if (!found)
    {
        onKeyframeAdd((time * FRAMETIME), joint);

        //BD - Refresh the keyframes.
        onKeyframeRefresh();

        //BD - Always select the last entry whenever we switch bones to allow quickly making
        //     changes or adding new keyframes.
        mKeyframeScroll->selectNthItem(mKeyframeScroll->getChildCount() - 1);
    }

    //BD - If we are in Mirror mode, try to find the opposite bone of our currently
    //     selected one, for now this simply means we take the name and replace "Left"
    //     with "Right" and vise versa since all bones are conveniently that way.
    //     TODO: Do this when creating the joint list so we don't try to find the joint
    //     over and over again.
    if (mMirrorMode)
    {
        LLJoint* mirror_joint = nullptr;
        std::string mirror_joint_name = joint->getName();
        size_t idx = joint->getName().find("Left");
        if (idx != -1)
            mirror_joint_name.replace(idx, mirror_joint_name.length(), "Right");

        idx = joint->getName().find("Right");
        if (idx != -1)
            mirror_joint_name.replace(idx, mirror_joint_name.length(), "Left");

        if (mirror_joint_name != joint->getName())
        {
            mirror_joint = gDragonAnimator.mTargetAvatar->mRoot->findJoint(mirror_joint_name);
        }

        if (mirror_joint)
        {
            //BD - For the opposite joint we invert X and Z axis, everything else is directly applied
            //     exactly like we do it in our currently selected joint.
            if (axis != 1)
                val = -val;

            LLQuaternion inv_quat = LLQuaternion(-rot_quat.mQ[VX], rot_quat.mQ[VY], -rot_quat.mQ[VZ], rot_quat.mQ[VW]);
            mirror_joint->setTargetRotation(inv_quat);

            LLKeyframeMotion::JointMotion* mirror_joint_motion = joint_list->getJointMotion(mirror_joint->getJointNum());
            LLKeyframeMotion::RotationCurve mirror_rot_curve = joint_motion->mRotationCurve;

            //BD - Attempt to find the keyframe first.
            bool found = false;

            LLScrollListItem* key_item = mKeyframeScroll->getItemByLabel(mirror_joint_name);
            if (key_item)
            {
                for (auto it = mirror_joint_motion->mRotationCurve.mKeys.begin();
                    it != mirror_joint_motion->mRotationCurve.mKeys.end(); )
                {
                    auto curr_it = it;
                    //BD - Previously we were rounding the keyframe time and comparing it to the time set
                    //     in the keyframe list entry, this was ugly and had a big downside, having two
                    //     or more entries with the same time resulting in all of them getting changed.
                    //     Lucky for us, adding new keyframes into any transformation curve automatically
                    //     counts a float value (used here as S32 since its whole numbers anyway) up by 1
                    //     for every keyframe added, this allows us to easily "compare" the what seems to be
                    //     the index number of the keyframe, against the index number in the list.
                    //     This should work for now until we decide to allow reordering keyframes.
                    //if (ll_round(curr_it->second.mTime, 0.1f) == ll_round(time, 0.1f))
                    if ((S32)curr_it->first == si)
                    {
                        found = true;
                        curr_it->second.mRotation = mirror_joint->getTargetRotation();

                        F32 roll, pitch, yaw;
                        LLQuaternion rot_quat = mirror_joint->getTargetRotation();
                        rot_quat.getEulerAngles(&roll, &pitch, &yaw);
                        //BD - Should we really be able to get here? The comparison above should already
                        //     prevent ever getting here since a missing selection means we will never
                        //     find a keyframe. I don't trust anything.
                        if (key_item)
                        {
                            key_item->getColumn(2)->setValue(ll_round(roll, 0.001f));
                            key_item->getColumn(3)->setValue(ll_round(pitch, 0.001f));
                            key_item->getColumn(4)->setValue(ll_round(yaw, 0.001f));
                        }
                        break;
                    }
                    ++it;
                }
            }

            //BD - We couldn't find a keyframe, create one automatically.
            if (!found)
            {
                onKeyframeAdd((time * FRAMETIME), mirror_joint);

                //BD - Refresh the keyframes.
                onKeyframeRefresh();

                //BD - Always select the last entry whenever we switch bones to allow quickly making
                //     changes or adding new keyframes.
                //mKeyframeScroll->selectNthItem(mKeyframeScroll->getChildCount() - 1);
            }
        }
    }
}

void BDFloaterPoseCreator::onJointPosSet(LLUICtrl* ctrl, const LLSD& param)
{
    S32 index = mJointTabs->getCurrentPanelIndex();
    LLScrollListItem* item = mJointScrolls[index]->getFirstSelected();
    if (!item)
        return;

    LLJoint* joint = (LLJoint*)item->getUserdata();
    if (!joint)
        return;

    if (!gDragonAnimator.mPoseCreatorMotion)
        return;

    //LLKeyframeMotion::JointMotionList* joint_list = gDragonAnimator.mPoseCreatorMotion->getJointMotionList();
    //if (!joint_list)
    //    return;

    LLScrollListItem* key_item = mKeyframeScroll->getFirstSelected();

    //BD - We could just check whether position information is available since only joints
    //     which can have their position changed will have position information but we
    //     want this to be a minefield for crashes.
    //     Bones that can support position
    //     0, 9-37, 39-43, 45-59, 77, 97-107, 110, 112, 115, 117-121, 125, 128-129, 132
    //     as well as all attachment bones and collision volumes.
    F32 val = (F32)(ctrl->getValue().asReal());
    LLScrollListCell* cell[3] = { item->getColumn(BD_COL_POS_X), item->getColumn(BD_COL_POS_Y), item->getColumn(BD_COL_POS_Z) };
    LLVector3 vec3 = { F32(cell[VX]->getValue().asReal()),
                        F32(cell[VY]->getValue().asReal()),
                        F32(cell[VZ]->getValue().asReal()) };
    S32 time = key_item ? key_item->getColumn(0)->getValue().asInteger() : 1;

    S32 dir = param.asInteger();
    vec3.mV[dir] = val;
    cell[dir]->setValue(ll_round(vec3.mV[dir], 0.001f));
    joint->setTargetPosition(vec3);

	LLKeyframeMotion::JointMotionList* joint_list = gDragonAnimator.mPoseCreatorMotion->getJointMotionList();
    LLKeyframeMotion::JointMotion* joint_motion = joint_list->getJointMotion(joint->getJointNum());
    LLKeyframeMotion::RotationCurve rot_curve = joint_motion->mRotationCurve;

    bool found = false;
    S32 si = mKeyframeScroll->getFirstSelectedIndex() + 1;
    for (auto it = joint_motion->mPositionCurve.mKeys.begin();
        it != joint_motion->mPositionCurve.mKeys.end(); )
    {
        auto curr_it = it;
        //BD - Previously we were rounding the keyframe time and comparing it to the time set
        //     in the keyframe list entry, this was ugly and had a big downside, having two
        //     or more entries with the same time resulting in all of them getting changed.
        //     Lucky for us, adding new keyframes into any transformation curve automatically
        //     counts a float value (used here as S32 since its whole numbers anyway) up by 1
        //     for every keyframe added, this allows us to easily "compare" the what seems to be
        //     the index number of the keyframe, against the index number in the list.
        //     This should work for now until we decide to allow reordering keyframes.
        if ((S32)curr_it->first == si)
        {
            found = true;
            curr_it->second.mPosition = joint->getTargetPosition();

            LLVector3 pos = joint->getTargetPosition();
            //BD - Should we really be able to get here? The comparison above should already
            //     prevent ever getting here since a missing selection means we will never
            //     find a keyframe. I don't trust anything.
            if (key_item)
            {
                key_item->getColumn(2)->setValue(ll_round(pos.mV[VX], 0.001f));
                key_item->getColumn(3)->setValue(ll_round(pos.mV[VY], 0.001f));
                key_item->getColumn(4)->setValue(ll_round(pos.mV[VZ], 0.001f));
            }
        }
        ++it;
    }

    //BD - We couldn't find a keyframe, create one automatically.
    if (!found)
    {
        onKeyframeAdd((time * FRAMETIME), joint);

        //BD - Refresh the keyframes.
        onKeyframeRefresh();

        //BD - Always select the last entry whenever we switch bones to allow quickly making
        //     changes or adding new keyframes.
        mKeyframeScroll->selectNthItem(mKeyframeScroll->getChildCount() - 1);
    }
}

void BDFloaterPoseCreator::onJointScaleSet(LLUICtrl* ctrl, const LLSD& param)
{
    S32 index = mJointTabs->getCurrentPanelIndex();
    LLScrollListItem* item = mJointScrolls[index]->getFirstSelected();
    if (!item)
        return;

    LLJoint* joint = (LLJoint*)item->getUserdata();
    if (!joint)
        return;

    if (!gDragonAnimator.mPoseCreatorMotion)
        return;

    //LLKeyframeMotion::JointMotionList* joint_list = gDragonAnimator.mPoseCreatorMotion->getJointMotionList();
    //if (!joint_list)
    //    return;

    LLScrollListItem* key_item = mKeyframeScroll->getFirstSelected();
    if (!key_item)
        return;

    F32 val = (F32)(ctrl->getValue().asReal());
    LLScrollListCell* cell[3] = { item->getColumn(BD_COL_SCALE_X), item->getColumn(BD_COL_SCALE_Y), item->getColumn(BD_COL_SCALE_Z) };
    LLVector3 vec3 = { F32(cell[VX]->getValue().asReal()),
                        F32(cell[VY]->getValue().asReal()),
                        F32(cell[VZ]->getValue().asReal()) };

    S32 dir = param.asInteger();
    vec3.mV[dir] = val;
    cell[dir]->setValue(ll_round(vec3.mV[dir], 0.001f));
    joint->setScale(vec3);

	LLKeyframeMotion::JointMotionList* joint_list = gDragonAnimator.mPoseCreatorMotion->getJointMotionList();
    LLKeyframeMotion::JointMotion* joint_motion = joint_list->getJointMotion(joint->getJointNum());
    LLKeyframeMotion::RotationCurve rot_curve = joint_motion->mRotationCurve;

    S32 si = mKeyframeScroll->getFirstSelectedIndex() + 1;
    for (auto it = joint_motion->mScaleCurve.mKeys.begin();
        it != joint_motion->mScaleCurve.mKeys.end(); )
    {
        auto curr_it = it;
        //BD - Previously we were rounding the keyframe time and comparing it to the time set
        //     in the keyframe list entry, this was ugly and had a big downside, having two
        //     or more entries with the same time resulting in all of them getting changed.
        //     Lucky for us, adding new keyframes into any transformation curve automatically
        //     counts a float value (used here as S32 since its whole numbers anyway) up by 1
        //     for every keyframe added, this allows us to easily "compare" the what seems to be
        //     the index number of the keyframe, against the index number in the list.
        //     This should work for now until we decide to allow reordering keyframes.
        if ((S32)curr_it->first == si)
        {
            curr_it->second.mScale = joint->getScale();

            LLVector3 scale = joint->getScale();
            key_item->getColumn(2)->setValue(ll_round(scale.mV[VX], 0.001f));
            key_item->getColumn(3)->setValue(ll_round(scale.mV[VY], 0.001f));
            key_item->getColumn(4)->setValue(ll_round(scale.mV[VZ], 0.001f));
        }
        ++it;
    }
}

void BDFloaterPoseCreator::onJointChangeState()
{
    BDPosingMotion* motion = (BDPosingMotion*)gAgentAvatarp->findMotion(ANIM_BD_POSING_MOTION);
    if (motion)
    {
        for (auto item : mJointScrolls[BD_JOINTS]->getAllSelected())
        {
            LLJoint* joint = (LLJoint*)item->getUserdata();
            if (joint)
            {
                LLPose* pose = motion->getPose();
                if (pose)
                {
                    LLPointer<LLJointState> joint_state = pose->findJointState(joint);
                    if (joint_state)
                    {
                        motion->removeJointState(joint_state);
                        ((LLScrollListText*)item->getColumn(BD_COL_NAME))->setFontStyle(LLFontGL::NORMAL);
                    }
                    else
                    {
                        motion->addJointToState(joint);
                        ((LLScrollListText*)item->getColumn(BD_COL_NAME))->setFontStyle(LLFontGL::BOLD);
                    }
                }
            }
        }
    }
    onJointControlsRefresh();
}

//BD - We use this to reset everything at once.
void BDFloaterPoseCreator::onJointRotPosScaleReset()
{
    //BD - While editing rotations, make sure we use a bit of spherical linear interpolation
    //     to make movements smoother.
    BDPosingMotion* motion = (BDPosingMotion*)gAgentAvatarp->findMotion(ANIM_BD_POSING_MOTION);
    if (motion)
    {
        //BD - If we don't use our default spherical interpolation, set it once.
        motion->setInterpolationTime(0.25f);
        motion->setInterpolationType(2);
    }

    for (S32 it = 0; it < 3; ++it)
    {
        //BD - We use this bool to determine whether or not we'll be in need for a full skeleton
        //     reset and to prevent checking for it every single time.
        for (auto item : mJointScrolls[it]->getAllData())
        {
            if (item)
            {
                LLJoint* joint = (LLJoint*)item->getUserdata();
                if (joint)
                {
                    //BD - Resetting rotations first if there are any.
                    if (it == BD_JOINTS)
                    {
                        LLQuaternion quat;
                        LLScrollListCell* col_rot_x = item->getColumn(BD_COL_ROT_X);
                        LLScrollListCell* col_rot_y = item->getColumn(BD_COL_ROT_Y);
                        LLScrollListCell* col_rot_z = item->getColumn(BD_COL_ROT_Z);

                        col_rot_x->setValue(0.000f);
                        col_rot_y->setValue(0.000f);
                        col_rot_z->setValue(0.000f);

                        quat.setEulerAngles(0, 0, 0);
                        joint->setTargetRotation(quat);

                        //BD - If we are in Mirror mode, try to find the opposite bone of our currently
                        //     selected one, for now this simply means we take the name and replace "Left"
                        //     with "Right" and vise versa since all bones are conveniently that way.
                        //     TODO: Do this when creating the joint list so we don't try to find the joint
                        //     over and over again.
                        if (mMirrorMode)
                        {
                            LLJoint* mirror_joint = nullptr;
                            std::string mirror_joint_name = joint->getName();
                            size_t idx = joint->getName().find("Left");
                            if (idx != -1)
                                mirror_joint_name.replace(idx, mirror_joint_name.length(), "Right");

                            idx = joint->getName().find("Right");
                            if (idx != -1)
                                mirror_joint_name.replace(idx, mirror_joint_name.length(), "Left");

                            if (mirror_joint_name != joint->getName())
                            {
                                mirror_joint = gDragonAnimator.mTargetAvatar->mRoot->findJoint(mirror_joint_name);
                            }

                            if (mirror_joint)
                            {
                                //BD - We also need to find the opposite joint's list entry and change its values to reflect
                                //     the new ones, doing this here is still better than causing a complete refresh.
                                LLScrollListItem* item2 = mJointScrolls[BD_JOINTS]->getItemByLabel(mirror_joint_name, FALSE, BD_COL_NAME);
                                if (item2)
                                {
                                    col_rot_x = item2->getColumn(BD_COL_ROT_X);
                                    col_rot_y = item2->getColumn(BD_COL_ROT_Y);
                                    col_rot_z = item2->getColumn(BD_COL_ROT_Z);

                                    col_rot_x->setValue(0.000f);
                                    col_rot_y->setValue(0.000f);
                                    col_rot_z->setValue(0.000f);

                                    mirror_joint->setTargetRotation(quat);
                                }
                            }
                        }
                    }

                    //BD - Resetting positions next.
                    LLScrollListCell* col_pos_x = item->getColumn(BD_COL_POS_X);
                    LLScrollListCell* col_pos_y = item->getColumn(BD_COL_POS_Y);
                    LLScrollListCell* col_pos_z = item->getColumn(BD_COL_POS_Z);
                    LLVector3 pos = mDefaultPositions[joint->getName()];

                    col_pos_x->setValue(ll_round(pos.mV[VX], 0.001f));
                    col_pos_y->setValue(ll_round(pos.mV[VY], 0.001f));
                    col_pos_z->setValue(ll_round(pos.mV[VZ], 0.001f));
                    joint->setTargetPosition(pos);

                    //BD - Resetting scales last.
                    LLScrollListCell* col_scale_x = item->getColumn(BD_COL_SCALE_X);
                    LLScrollListCell* col_scale_y = item->getColumn(BD_COL_SCALE_Y);
                    LLScrollListCell* col_scale_z = item->getColumn(BD_COL_SCALE_Z);
                    LLVector3 scale = mDefaultScales[joint->getName()];

                    col_scale_x->setValue(ll_round(scale.mV[VX], 0.001f));
                    col_scale_y->setValue(ll_round(scale.mV[VY], 0.001f));
                    col_scale_z->setValue(ll_round(scale.mV[VZ], 0.001f));
                    joint->setScale(scale);
                }
            }
        }
    }

    onJointControlsRefresh();
}

//BD - Used to reset rotations only.
void BDFloaterPoseCreator::onJointRotationReset()
{
    //BD - While editing rotations, make sure we use a bit of spherical linear interpolation
    //     to make movements smoother.
    BDPosingMotion* motion = (BDPosingMotion*)gAgentAvatarp->findMotion(ANIM_BD_POSING_MOTION);
    if (motion)
    {
        //BD - If we don't use our default spherical interpolation, set it once.
        motion->setInterpolationTime(0.25f);
        motion->setInterpolationType(2);
    }

    for (auto item : mJointScrolls[BD_JOINTS]->getAllSelected())
    {
        if (item)
        {
            LLJoint* joint = (LLJoint*)item->getUserdata();
            if (joint)
            {
                LLQuaternion quat;
                LLScrollListCell* col_x = item->getColumn(BD_COL_ROT_X);
                LLScrollListCell* col_y = item->getColumn(BD_COL_ROT_Y);
                LLScrollListCell* col_z = item->getColumn(BD_COL_ROT_Z);

                col_x->setValue(0.000f);
                col_y->setValue(0.000f);
                col_z->setValue(0.000f);

                quat.setEulerAngles(0, 0, 0);
                joint->setTargetRotation(quat);

                //BD - If we are in Mirror mode, try to find the opposite bone of our currently
                //     selected one, for now this simply means we take the name and replace "Left"
                //     with "Right" and vise versa since all bones are conveniently that way.
                //     TODO: Do this when creating the joint list so we don't try to find the joint
                //     over and over again.
                if (mMirrorMode)
                {
                    LLJoint* mirror_joint = nullptr;
                    std::string mirror_joint_name = joint->getName();
                    size_t idx = joint->getName().find("Left");
                    if (idx != -1)
                        mirror_joint_name.replace(idx, mirror_joint_name.length(), "Right");

                    idx = joint->getName().find("Right");
                    if (idx != -1)
                        mirror_joint_name.replace(idx, mirror_joint_name.length(), "Left");

                    if (mirror_joint_name != joint->getName())
                    {
                        mirror_joint = gDragonAnimator.mTargetAvatar->mRoot->findJoint(mirror_joint_name);
                    }

                    if (mirror_joint)
                    {
                        //BD - We also need to find the opposite joint's list entry and change its values to reflect
                        //     the new ones, doing this here is still better than causing a complete refresh.
                        LLScrollListItem* item2 = mJointScrolls[BD_JOINTS]->getItemByLabel(mirror_joint_name, FALSE, BD_COL_NAME);
                        if (item2)
                        {
                            col_x = item2->getColumn(BD_COL_ROT_X);
                            col_y = item2->getColumn(BD_COL_ROT_Y);
                            col_z = item2->getColumn(BD_COL_ROT_Z);

                            col_x->setValue(0.000f);
                            col_y->setValue(0.000f);
                            col_z->setValue(0.000f);

                            mirror_joint->setTargetRotation(quat);
                        }
                    }
                }
            }
        }
    }

    onJointControlsRefresh();
}

//BD - Used to reset positions, this is very tricky hence why it was separated.
//     It causes the avatar to flinch for a second which doesn't look as nice as resetting
//     rotations does.
void BDFloaterPoseCreator::onJointPositionReset()
{
    S32 index = mJointTabs->getCurrentPanelIndex();

    //BD - When resetting positions, we don't use interpolation for now, it looks stupid.
    BDPosingMotion* motion = (BDPosingMotion*)gAgentAvatarp->findMotion(ANIM_BD_POSING_MOTION);
    if (motion)
    {
        motion->setInterpolationTime(0.25f);
        motion->setInterpolationType(2);
    }

    //BD - We use this bool to prevent going through attachment override reset every single time.
    //bool has_reset = false;
    for (auto item : mJointScrolls[index]->getAllSelected())
    {
        if (item)
        {
            LLJoint* joint = (LLJoint*)item->getUserdata();
            if (joint)
            {
                LLScrollListCell* col_pos_x = item->getColumn(BD_COL_POS_X);
                LLScrollListCell* col_pos_y = item->getColumn(BD_COL_POS_Y);
                LLScrollListCell* col_pos_z = item->getColumn(BD_COL_POS_Z);
                LLVector3 pos = mDefaultPositions[joint->getName()];

                col_pos_x->setValue(ll_round(pos.mV[VX], 0.001f));
                col_pos_y->setValue(ll_round(pos.mV[VY], 0.001f));
                col_pos_z->setValue(ll_round(pos.mV[VZ], 0.001f));
                joint->setTargetPosition(pos);
            }
        }
    }

    onJointControlsRefresh();
}

//BD - Used to reset scales only.
void BDFloaterPoseCreator::onJointScaleReset()
{
    S32 index = mJointTabs->getCurrentPanelIndex();

    //BD - Clear all attachment bone scale changes we've done, they are not automatically
    //     reverted.
    for (auto item : mJointScrolls[index]->getAllSelected())
    {
        if (item)
        {
            LLJoint* joint = (LLJoint*)item->getUserdata();
            if (joint)
            {
                LLScrollListCell* col_scale_x = item->getColumn(BD_COL_SCALE_X);
                LLScrollListCell* col_scale_y = item->getColumn(BD_COL_SCALE_Y);
                LLScrollListCell* col_scale_z = item->getColumn(BD_COL_SCALE_Z);
                LLVector3 scale = mDefaultScales[joint->getName()];

                col_scale_x->setValue(ll_round(scale.mV[VX], 0.001f));
                col_scale_y->setValue(ll_round(scale.mV[VY], 0.001f));
                col_scale_z->setValue(ll_round(scale.mV[VZ], 0.001f));
                joint->setScale(scale);
            }
        }
    }
    onJointControlsRefresh();
}

//BD - Used to revert rotations only.
void BDFloaterPoseCreator::onJointRotationRevert()
{
    //BD - While editing rotations, make sure we use a bit of spherical linear interpolation
    //     to make movements smoother.
    BDPosingMotion* motion = (BDPosingMotion*)gAgentAvatarp->findMotion(ANIM_BD_POSING_MOTION);
    if (motion)
    {
        //BD - If we don't use our default spherical interpolation, set it once.
        motion->setInterpolationTime(0.25f);
        motion->setInterpolationType(2);
    }

    for (auto item : mJointScrolls[BD_JOINTS]->getAllSelected())
    {
        if (item)
        {
            LLJoint* joint = (LLJoint*)item->getUserdata();
            if (joint)
            {
                //BD - Reverting rotations first if there are any.
                LLQuaternion quat = mDefaultRotations[joint->getName()];
                LLVector3 rot;
                quat.getEulerAngles(&rot.mV[VX], &rot.mV[VY], &rot.mV[VZ]);
                LLScrollListCell* col_rot_x = item->getColumn(BD_COL_ROT_X);
                LLScrollListCell* col_rot_y = item->getColumn(BD_COL_ROT_Y);
                LLScrollListCell* col_rot_z = item->getColumn(BD_COL_ROT_Z);

                col_rot_x->setValue(rot.mV[VX]);
                col_rot_y->setValue(rot.mV[VY]);
                col_rot_z->setValue(rot.mV[VZ]);

                joint->setTargetRotation(quat);

                //BD - If we are in Mirror mode, try to find the opposite bone of our currently
                //     selected one, for now this simply means we take the name and replace "Left"
                //     with "Right" and vise versa since all bones are conveniently that way.
                //     TODO: Do this when creating the joint list so we don't try to find the joint
                //     over and over again.
                if (mMirrorMode)
                {
                    LLJoint* mirror_joint = nullptr;
                    std::string mirror_joint_name = joint->getName();
                    size_t idx = joint->getName().find("Left");
                    if (idx != -1)
                        mirror_joint_name.replace(idx, mirror_joint_name.length(), "Right");

                    idx = joint->getName().find("Right");
                    if (idx != -1)
                        mirror_joint_name.replace(idx, mirror_joint_name.length(), "Left");

                    if (mirror_joint_name != joint->getName())
                    {
                        mirror_joint = gDragonAnimator.mTargetAvatar->mRoot->findJoint(mirror_joint_name);
                    }

                    if (mirror_joint)
                    {
                        //BD - We also need to find the opposite joint's list entry and change its values to reflect
                        //     the new ones, doing this here is still better than causing a complete refresh.
                        LLScrollListItem* item2 = mJointScrolls[BD_JOINTS]->getItemByLabel(mirror_joint_name, FALSE, BD_COL_NAME);
                        if (item2)
                        {
                            col_rot_x = item2->getColumn(BD_COL_ROT_X);
                            col_rot_y = item2->getColumn(BD_COL_ROT_Y);
                            col_rot_z = item2->getColumn(BD_COL_ROT_Z);

                            col_rot_x->setValue(rot.mV[VX]);
                            col_rot_y->setValue(rot.mV[VY]);
                            col_rot_z->setValue(rot.mV[VZ]);

                            mirror_joint->setTargetRotation(quat);
                        }
                    }
                }
            }
        }
    }

    onJointControlsRefresh();
}

//BD - Flip our pose (mirror it)
void BDFloaterPoseCreator::onFlipPose()
{
    LLVOAvatar* avatar = gDragonAnimator.mTargetAvatar;
    if (!avatar || avatar->isDead()) return;

    if (!(avatar->getRegion() == gAgent.getRegion())) return;

    LLJoint* joint = nullptr;
    bool flipped[134] = { false };

    for (S32 i = 0; (joint = avatar->getCharacterJoint(i)); ++i)
    {
        //BD - Skip if we already flipped this bone.
        if (flipped[i]) continue;

        //BD - Nothing? Invalid? Skip, when we hit the end we'll break out anyway.
        if (!joint) continue;

        LLVector3 rot, mirror_rot;
        LLQuaternion rot_quat, mirror_rot_quat;
        std::string joint_name = joint->getName();
        std::string mirror_joint_name = joint->getName();
        //BD - Attempt to find the "right" version of this bone first, we assume we always
        //     end up with the "left" version of a bone first.
        size_t idx = joint->getName().find("Left");
        if (idx != -1)
            mirror_joint_name.replace(idx, mirror_joint_name.length(), "Right");
        //BD - Attempt to find the "right" version of this bone first, this is necessary
        //     because there are a couple bones starting with the "right" bone.
        idx = joint->getName().find("Right");
        if (idx != -1)
            mirror_joint_name.replace(idx, mirror_joint_name.length(), "Left");

        LLJoint* mirror_joint = nullptr;
        if (mirror_joint_name != joint->getName())
            mirror_joint = gDragonAnimator.mTargetAvatar->mRoot->findJoint(mirror_joint_name);

        //BD - Collect the joint and mirror joint entries and their cells, we need them later.
        LLScrollListItem* item1 = mJointScrolls[BD_JOINTS]->getItemByLabel(joint_name, FALSE, BD_COL_NAME);
        LLScrollListItem* item2 = nullptr;

        //BD - Get the rotation of our current bone and that of the mirror bone (if available).
        //     Flip our current bone's rotation and apply it to the mirror bone (if available).
        //     Flip the mirror bone's rotation (if available) and apply it to our current bone.
        //     If the mirror bone does not exist, flip the current bone rotation and use that.
        rot_quat = joint->getTargetRotation();
        LLQuaternion inv_rot_quat = LLQuaternion(-rot_quat.mQ[VX], rot_quat.mQ[VY], -rot_quat.mQ[VZ], rot_quat.mQ[VW]);
        inv_rot_quat.getEulerAngles(&rot[VX], &rot[VY], &rot[VZ]);

        if (mirror_joint)
        {
            mirror_rot_quat = mirror_joint->getTargetRotation();
            LLQuaternion inv_mirror_rot_quat = LLQuaternion(-mirror_rot_quat.mQ[VX], mirror_rot_quat.mQ[VY], -mirror_rot_quat.mQ[VZ], mirror_rot_quat.mQ[VW]);
            inv_mirror_rot_quat.getEulerAngles(&mirror_rot[VX], &mirror_rot[VY], &mirror_rot[VZ]);
            mirror_joint->setTargetRotation(inv_rot_quat);
            joint->setTargetRotation(inv_mirror_rot_quat);

            item2 = mJointScrolls[BD_JOINTS]->getItemByLabel(mirror_joint_name, FALSE, BD_COL_NAME);

            //BD - Make sure we flag this bone as flipped so we skip it next time we iterate over it.
            flipped[mirror_joint->getJointNum()] = true;
        }
        else
        {
            joint->setTargetRotation(inv_rot_quat);
        }

        S32 axis = 0;
        while (axis <= 2)
        {
            //BD - Now flip the list entry values.
            if (item1)
            {
                if (mirror_joint)
                    item1->getColumn(axis + 2)->setValue(ll_round(mirror_rot[axis], 0.001f));
                else
                    item1->getColumn(axis + 2)->setValue(ll_round(rot[axis], 0.001f));
            }

            //BD - Now flip the mirror joint list entry values.
            if (item2)
                item2->getColumn(axis + 2)->setValue(ll_round(rot[axis], 0.001f));

            ++axis;
        }
        flipped[i] = true;
    }
}

////////////////////////////////
//BD - Utility Functions
////////////////////////////////
//BD - Poser Utility Functions
void BDFloaterPoseCreator::onJointPasteRotation()
{
    for (auto item : mJointScrolls[BD_JOINTS]->getAllSelected())
    {
        LLJoint* joint = (LLJoint*)item->getUserdata();
        if (joint)
        {
            LLScrollListCell* col_rot_x = item->getColumn(BD_COL_ROT_X);
            LLScrollListCell* col_rot_y = item->getColumn(BD_COL_ROT_Y);
            LLScrollListCell* col_rot_z = item->getColumn(BD_COL_ROT_Z);

            LLVector3 euler_rot;
            LLQuaternion rot = (LLQuaternion)mClipboard["rot"];

            joint->setTargetRotation(rot);

            rot.getEulerAngles(&euler_rot.mV[VX], &euler_rot.mV[VY], &euler_rot.mV[VZ]);
            col_rot_x->setValue(ll_round(euler_rot.mV[VX], 0.001f));
            col_rot_y->setValue(ll_round(euler_rot.mV[VY], 0.001f));
            col_rot_z->setValue(ll_round(euler_rot.mV[VZ], 0.001f));
        }
    }
}

void BDFloaterPoseCreator::onJointPastePosition()
{
    for (auto item : mJointScrolls[BD_JOINTS]->getAllSelected())
    {
        LLJoint* joint = (LLJoint*)item->getUserdata();
        if (joint)
        {
            LLScrollListCell* col_pos_x = item->getColumn(BD_COL_POS_X);
            LLScrollListCell* col_pos_y = item->getColumn(BD_COL_POS_Y);
            LLScrollListCell* col_pos_z = item->getColumn(BD_COL_POS_Z);

            LLVector3 pos = (LLVector3)mClipboard["pos"];

            joint->setTargetPosition(pos);

            col_pos_x->setValue(ll_round(pos.mV[VX], 0.001f));
            col_pos_y->setValue(ll_round(pos.mV[VY], 0.001f));
            col_pos_z->setValue(ll_round(pos.mV[VZ], 0.001f));
        }
    }
}

void BDFloaterPoseCreator::onJointPasteScale()
{
    for (auto item : mJointScrolls[BD_JOINTS]->getAllSelected())
    {
        LLJoint* joint = (LLJoint*)item->getUserdata();
        if (joint)
        {
            LLScrollListCell* col_scale_x = item->getColumn(BD_COL_SCALE_X);
            LLScrollListCell* col_scale_y = item->getColumn(BD_COL_SCALE_Y);
            LLScrollListCell* col_scale_z = item->getColumn(BD_COL_SCALE_Z);
            LLVector3 scale = (LLVector3)mClipboard["scale"];

            joint->setScale(scale);

            col_scale_x->setValue(ll_round(scale.mV[VX], 0.001f));
            col_scale_y->setValue(ll_round(scale.mV[VY], 0.001f));
            col_scale_z->setValue(ll_round(scale.mV[VZ], 0.001f));
        }
    }
}

void BDFloaterPoseCreator::onJointMirror()
{
    for (auto item : mJointScrolls[BD_JOINTS]->getAllSelected())
    {
        LLJoint* joint = (LLJoint*)item->getUserdata();
        if (joint)
        {
            LLVector3 euler_rot;
            LLQuaternion rot_quat = joint->getTargetRotation();

            //BD - Simply mirror the current bone's rotation like we'd do if we pressed the mirror
            //     button without a mirror bone available.
            LLQuaternion inv_rot_quat = LLQuaternion(-rot_quat.mQ[VX], rot_quat.mQ[VY], -rot_quat.mQ[VZ], rot_quat.mQ[VW]);
            inv_rot_quat.getEulerAngles(&euler_rot[VX], &euler_rot[VY], &euler_rot[VZ]);
            joint->setTargetRotation(inv_rot_quat);

            LLScrollListCell* col_rot_x = item->getColumn(BD_COL_ROT_X);
            LLScrollListCell* col_rot_y = item->getColumn(BD_COL_ROT_Y);
            LLScrollListCell* col_rot_z = item->getColumn(BD_COL_ROT_Z);

            col_rot_x->setValue(ll_round(euler_rot.mV[VX], 0.001f));
            col_rot_y->setValue(ll_round(euler_rot.mV[VY], 0.001f));
            col_rot_z->setValue(ll_round(euler_rot.mV[VZ], 0.001f));
        }
    }
}

void BDFloaterPoseCreator::onJointSymmetrize()
{
    for (auto item : mJointScrolls[BD_JOINTS]->getAllSelected())
    {
        LLJoint* joint = (LLJoint*)item->getUserdata();
        if (joint)
        {
            std::string joint_name = joint->getName();
            std::string mirror_joint_name = joint->getName();
            //BD - Attempt to find the "right" version of this bone, if we can't find it try
            //     the left version.
            size_t idx = joint->getName().find("Left");
            if (idx != -1)
                mirror_joint_name.replace(idx, mirror_joint_name.length(), "Right");
            idx = joint->getName().find("Right");
            if (idx != -1)
                mirror_joint_name.replace(idx, mirror_joint_name.length(), "Left");

            LLJoint* mirror_joint = nullptr;
            if (mirror_joint_name != joint->getName())
                mirror_joint = gDragonAnimator.mTargetAvatar->mRoot->findJoint(mirror_joint_name);

            //BD - Get the rotation of the mirror bone (if available).
            //     Flip the mirror bone's rotation (if available) and apply it to our current bone.
            if (mirror_joint)
            {
                LLVector3 mirror_rot;
                LLQuaternion mirror_rot_quat;
                mirror_rot_quat = mirror_joint->getTargetRotation();
                LLQuaternion inv_mirror_rot_quat = LLQuaternion(-mirror_rot_quat.mQ[VX], mirror_rot_quat.mQ[VY], -mirror_rot_quat.mQ[VZ], mirror_rot_quat.mQ[VW]);
                inv_mirror_rot_quat.getEulerAngles(&mirror_rot[VX], &mirror_rot[VY], &mirror_rot[VZ]);
                joint->setTargetRotation(inv_mirror_rot_quat);

                LLScrollListCell* col_rot_x = item->getColumn(BD_COL_ROT_X);
                LLScrollListCell* col_rot_y = item->getColumn(BD_COL_ROT_Y);
                LLScrollListCell* col_rot_z = item->getColumn(BD_COL_ROT_Z);

                col_rot_x->setValue(ll_round(mirror_rot.mV[VX], 0.001f));
                col_rot_y->setValue(ll_round(mirror_rot.mV[VY], 0.001f));
                col_rot_z->setValue(ll_round(mirror_rot.mV[VZ], 0.001f));
            }
        }
    }
}

void BDFloaterPoseCreator::onJointCopyTransforms()
{
    LLScrollListItem* item = mJointScrolls[BD_JOINTS]->getFirstSelected();
    LLJoint* joint = (LLJoint*)item->getUserdata();
    if (joint)
    {
        mClipboard["rot"] = joint->getTargetRotation().getValue();
        mClipboard["pos"] = joint->getTargetPosition().getValue();
        mClipboard["scale"] = joint->getScale().getValue();
        LL_INFOS("Posing") << "Copied all transforms " << LL_ENDL;
    }
}

bool BDFloaterPoseCreator::onJointContextMenuEnable(const LLSD& param)
{
    std::string action = param.asString();
    if (action == "clipboard")
    {
        return mClipboard.has("rot");
    }
    if (action == "enable_bone")
    {
        LLScrollListItem* item = mJointScrolls[BD_JOINTS]->getFirstSelected();
        if (item)
        {
            LLJoint* joint = (LLJoint*)item->getUserdata();
            if (joint)
            {
                BDPosingMotion* motion = (BDPosingMotion*)gAgentAvatarp->findMotion(ANIM_BD_POSING_MOTION);
                LLPose* pose = motion->getPose();
                if (pose)
                {
                    LLPointer<LLJointState> joint_state = pose->findJointState(joint);
                    return joint_state;
                }
            }
        }
    }
    return false;
}

void BDFloaterPoseCreator::onJointContextMenuAction(const LLSD& param)
{
    std::string action = param.asString();
    if (action == "copy_transforms")
    {
        onJointCopyTransforms();
    }
    else if (action == "paste_rot")
    {
        onJointPasteRotation();
    }
    else if (action == "paste_pos")
    {
        onJointPastePosition();
    }
    else if (action == "paste_scale")
    {
        onJointPasteScale();
    }
    else if (action == "paste_rot_pos")
    {
        onJointPasteRotation();
        onJointPastePosition();
    }
    else if (action == "paste_rot_scale")
    {
        onJointPasteRotation();
        onJointPasteScale();
    }
    else if (action == "paste_pos_scale")
    {
        onJointPastePosition();
        onJointPasteScale();
    }
    else if (action == "paste_all")
    {
        onJointPasteRotation();
        onJointPastePosition();
        onJointPasteScale();
    }
    else if (action == "symmetrize")
    {
        onJointSymmetrize();
    }
    else if (action == "mirror")
    {
        onJointMirror();
    }
    else if (action == "enable_bone")
    {
        onJointChangeState();
    }
    else if (action == "enable_override")
    {

    }
    else if (action == "enable_offset")
    {

    }
    else if (action == "reset_rot")
    {
        onJointRotationReset();
    }
    else if (action == "reset_pos")
    {
        onJointPositionReset();
    }
    else if (action == "reset_scale")
    {
        onJointScaleReset();
    }
    else if (action == "reset_all")
    {
        //BD - We do all 3 here because the combined function resets all bones regardless of
        //     our selection, these only reset the selected ones.
        onJointRotationReset();
        onJointPositionReset();
        onJointScaleReset();
    }
}

////////////////////////////////
//BD - Misc Functions
////////////////////////////////
LLKeyframeMotion* BDFloaterPoseCreator::onReadyTempMotion(std::string filename, bool eternal)
{
    std::string outfilename = gDirUtilp->getExpandedFilename(LL_PATH_ANIMATIONS, filename);
    S32 file_size;
    bool success = FALSE;
    LLAPRFile infile;
    LLKeyframeMotion* mTempMotion = NULL;
    LLAssetID mMotionID;
    LLTransactionID mTransactionID;

    //BD - To make this work we'll first need a unique UUID for this animation.
    mTransactionID.generate();
    mMotionID = mTransactionID.makeAssetID(gAgent.getSecureSessionID());
    mTempMotion = (LLKeyframeMotion*)gAgentAvatarp->createMotion(mMotionID);

    LL_INFOS("Posing") << mMotionID << LL_ENDL;

    //BD - Find and open the file, we'll need to write it temporarily into the VFS pool.
    infile.open(outfilename, LL_APR_RB, NULL, &file_size);
    if (infile.getFileHandle())
    {
        U8 *anim_data;
        S32 anim_file_size;

        LLFileSystem file(mMotionID, LLAssetType::AT_ANIMATION, LLFileSystem::WRITE);
        const S32 buf_size = 65536;
        U8 copy_buf[buf_size];
        while ((file_size = infile.read(copy_buf, buf_size)))
        {
            file.write(copy_buf, file_size);
        }

        //BD - Now that we wrote the temporary file, find it and use it to set the size
        //     and buffer into which we will unpack the .anim file into.
        LLFileSystem* anim_file = new LLFileSystem(mMotionID, LLAssetType::AT_ANIMATION);
        anim_file_size = anim_file->getSize();
        anim_data = new U8[anim_file_size];
        anim_file->read(anim_data, anim_file_size);

        //BD - Cleanup everything we don't need anymore.
        delete anim_file;
        anim_file = NULL;

        //BD - Use the datapacker now to actually deserialize and unpack the animation
        //     into our temporary motion so we can use it after we added it into the list.
        LLDataPackerBinaryBuffer dp(anim_data, anim_file_size);
        success = mTempMotion && mTempMotion->deserialize(dp, mMotionID);

        if (success && eternal)
        {
            mTempMotion->setEternal(true);
        }

        //BD - Cleanup the rest.
        delete[]anim_data;
    }
    infile.close();

    //return success ? mMotionID : LLUUID::null;
    return mTempMotion;
}

void BDFloaterPoseCreator::onCreateTempMotion()
{
    LLAssetID mMotionID;
    LLTransactionID mTransactionID;

    //BD - To make this work we'll first need a unique UUID for this animation.
    mTransactionID.generate();
    mMotionID = mTransactionID.makeAssetID(gAgent.getSecureSessionID());

    LLKeyframeMotion* temp_motion = (LLKeyframeMotion*)gAgentAvatarp->createMotion(mMotionID);

    if (!temp_motion)
        return;

    LLKeyframeMotion::JointMotionList* mTempMotionList = new LLKeyframeMotion::JointMotionList;
    std::vector<LLPointer<LLJointState>> joint_states;

    if (!mTempMotionList)
        return;

    if (!mTempMotionList->mJointMotionArray.empty())
        mTempMotionList->mJointMotionArray.clear();
    mTempMotionList->mJointMotionArray.reserve(134);
    joint_states.clear();
    joint_states.reserve(134);

    mTempMotionList->mBasePriority = (LLJoint::JointPriority)getChild<LLUICtrl>("base_priority")->getValue().asInteger();
    mTempMotionList->mDuration = (F32)(getChild<LLUICtrl>("keyframe_duration")->getValue().asReal());
    mTempMotionList->mEaseInDuration = (F32)(getChild<LLUICtrl>("ease_in")->getValue().asReal());
    mTempMotionList->mEaseOutDuration = (F32)(getChild<LLUICtrl>("ease_out")->getValue().asReal());
    mTempMotionList->mEmoteName = "";
    mTempMotionList->mHandPose = (LLHandMotion::eHandPose)getChild<LLUICtrl>("hand_poses")->getValue().asInteger();
    mTempMotionList->mLoop = true;
    mTempMotionList->mLoopInPoint = 0.0f;
    mTempMotionList->mLoopOutPoint = (F32)(getChild<LLUICtrl>("keyframe_duration")->getValue().asReal());
    mTempMotionList->mMaxPriority = LLJoint::HIGHEST_PRIORITY;

    for (S32 i = 0; i <= BD_ATTACHMENT_BONES; i++)
    {
        for (auto item : mJointScrolls[i]->getAllData())
        {
            LLJoint* joint = (LLJoint*)item->getUserdata();
            if (joint)
            {
                //BD - Create a new joint motion and add it to the pile.
                LLKeyframeMotion::JointMotion* joint_motion = new LLKeyframeMotion::JointMotion;
                mTempMotionList->mJointMotionArray.push_back(joint_motion);

                //BD - Fill out joint motion with relevant basic data.
                joint_motion->mJointName = joint->getName();
                joint_motion->mPriority = LLJoint::HIGHEST_PRIORITY;

                //BD - Create the basic joint state for this joint and add it to the joint states.
                LLPointer<LLJointState> joint_state = new LLJointState;
                joint_states.push_back(joint_state);
                joint_state->setJoint(joint); // note: can accept NULL
                joint_state->setUsage(0);

                //BD - Start with filling general rotation data in.
                joint_motion->mRotationCurve.mNumKeys = 0;
                joint_motion->mRotationCurve.mInterpolationType = LLKeyframeMotion::IT_LINEAR;

                //BD - Create a rotation key and put it into the rotation curve.
                /*LLKeyframeMotion::RotationKey rotation_key = LLKeyframeMotion::RotationKey(1.0f, joint->getTargetRotation());
                joint_motion->mRotationCurve.mKeys[0] = rotation_key;
                joint_state->setUsage(joint_state->getUsage() | LLJointState::ROT);*/

                //BD - Fill general position data in.
                joint_motion->mPositionCurve.mNumKeys = 0;
                joint_motion->mPositionCurve.mInterpolationType = LLKeyframeMotion::IT_LINEAR;

                /*LLKeyframeMotion::PositionKey position_key;
                if (joint->mHasPosition)
                {
                    //BD - Create a position key and put it into the position curve.
                    LLKeyframeMotion::PositionKey position_key = LLKeyframeMotion::PositionKey(1.f, joint->getTargetPosition());
                    joint_motion->mPositionCurve.mKeys[0] = position_key;
                    joint_state->setUsage(joint_state->getUsage() | LLJointState::POS);
                }*/

                temp_motion->addJointState(joint_state);

                //BD - Start with filling general scale data in.
                joint_motion->mScaleCurve.mNumKeys = 0;
                joint_motion->mScaleCurve.mInterpolationType = LLKeyframeMotion::IT_LINEAR;

                //BD - Create a scale key and put it into the scale curve.
                /*LLKeyframeMotion::ScaleKey scale_key = LLKeyframeMotion::ScaleKey(1.f, joint->getScale());
                joint_motion->mScaleCurve.mKeys[0] = scale_key;*/

                joint_motion->mUsage = joint_state->getUsage();

                //BD - We do not use constraints so we just leave them out here.
                //     Should we ever add them we'd do so here.
                //joint_motion_list->mConstraints.push_front(constraint);

                //BD - Get the pelvis's bounding box and add it.
                if (joint->getJointNum() == 0)
                {
                    //mTempMotionList->mPelvisBBox.addPoint(position_key.mPosition);
                    mTempMotionList->mPelvisBBox.addPoint(joint->getTargetPosition());
                }
            }
        }
    }

    temp_motion->setJointMotionList(mTempMotionList);
    //BD - We export this file immediately so we can import it later.
    //     This is going to be the animation we write everything we do into
    //     and it also neatly acts as an "autosave" thing that we can just
    //     load back in case we quit out of the pose creator.
    //     The main reason we do this however is because we are going to import
    //     this animation locally which will create a fully setup LLKeyframeAnimation
    //     that doesn't have some weird things going on causing crashes, it is also
    //     a lot easier to do.
    std::string full_path = gDirUtilp->getExpandedFilename(LL_PATH_ANIMATIONS, "_poser_temp.anim");
    temp_motion->dumpToFile(full_path);
    //return temp_motion;
}

//BD - This is used to collect all default values at the beginning to revert to later on.
void BDFloaterPoseCreator::onCollectDefaults()
{
    LLQuaternion rot;
    LLVector3 pos;
    LLVector3 scale;
    LLJoint* joint;

    //BD - Getting collision volumes and attachment points.
    std::vector<std::string> joint_names, cv_names, attach_names;
    gAgentAvatarp->getSortedJointNames(0, joint_names);
    gAgentAvatarp->getSortedJointNames(1, cv_names);
    gAgentAvatarp->getSortedJointNames(2, attach_names);

    mDefaultRotations.clear();
    mDefaultScales.clear();
    mDefaultPositions.clear();

    for (auto name : joint_names)
    {
        //joint = gAgentAvatarp->getJoint(JointKey::construct(name));
        joint = gAgentAvatarp->getJoint(name);
        //BD - Nothing? Invalid? Skip, when we hit the end we'll break out anyway.
        if (!joint) continue;

        LLSD row;

        rot = joint->getTargetRotation();
        mDefaultRotations.insert(std::pair<std::string, LLQuaternion>(name, rot));

        //BD - We always get the values but we don't write them out as they are not relevant for the
        //     user yet but we need them to establish default values we revert to later on.
        scale = joint->getScale();
        mDefaultScales.insert(std::pair<std::string, LLVector3>(name, scale));

        //BD - We always get the values but we don't write them out as they are not relevant for the
        //     user yet but we need them to establish default values we revert to later on.
        pos = joint->getPosition();
        mDefaultPositions.insert(std::pair<std::string, LLVector3>(name, pos));
    }

    //BD - Collision Volumes
    for (auto name : cv_names)
    {
        //LLJoint* joint = gAgentAvatarp->getJoint(JointKey::construct(name));
        LLJoint* joint = gAgentAvatarp->getJoint(name);
        //BD - Nothing? Invalid? Skip, when we hit the end we'll break out anyway.
        if (!joint) continue;

        //BD - We always get the values but we don't write them out as they are not relevant for the
        //     user yet but we need them to establish default values we revert to later on.
        pos = joint->getPosition();
        scale = joint->getScale();
        mDefaultPositions.insert(std::pair<std::string, LLVector3>(name, pos));
        mDefaultScales.insert(std::pair<std::string, LLVector3>(name, scale));
    }

    //BD - Attachment Bones
    for (auto name : attach_names)
    {
        //LLJoint* joint = gAgentAvatarp->getJoint(JointKey::construct(name));
        LLJoint* joint = gAgentAvatarp->getJoint(name);
        //BD - Nothing? Invalid? Skip, when we hit the end we'll break out anyway.
        if (!joint) continue;

        //BD - We always get the values but we don't write them out as they are not relevant for the
        //     user yet but we need them to establish default values we revert to later on.
        pos = joint->getPosition();
        scale = joint->getScale();

        mDefaultPositions.insert(std::pair<std::string, LLVector3>(name, pos));
        mDefaultScales.insert(std::pair<std::string, LLVector3>(name, scale));
    }
}
