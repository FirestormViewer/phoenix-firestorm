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

#include "bdfloaterposer.h"
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

BDFloaterPoser::BDFloaterPoser(const LLSD& key)
    :   LLFloater(key)
{
    //BD - Save our current pose as XML or ANIM file to be used or uploaded later.
    mCommitCallbackRegistrar.add("Pose.Save", boost::bind(&BDFloaterPoser::onClickPoseSave, this, _2));
    //BD - Start our custom pose.
    mCommitCallbackRegistrar.add("Pose.Start", boost::bind(&BDFloaterPoser::onPoseStart, this));
    //BD - Load the current pose and export all its values into the UI so we can alter them.
    mCommitCallbackRegistrar.add("Pose.Load", boost::bind(&BDFloaterPoser::onPoseLoad, this));
    //BD - Delete the currently selected Pose.
    mCommitCallbackRegistrar.add("Pose.Delete", boost::bind(&BDFloaterPoser::onPoseDelete, this));
    //BD - Extend or collapse the floater's pose list.
    mCommitCallbackRegistrar.add("Pose.Layout", boost::bind(&BDFloaterPoser::onUpdateLayout, this));
    //BD - Include some menu interactions. Sadly necessary.
    mCommitCallbackRegistrar.add("Pose.Menu", boost::bind(&BDFloaterPoser::onPoseMenuAction, this, _2));

    //BD - Change a bone's rotation.
    mCommitCallbackRegistrar.add("Joint.Set", boost::bind(&BDFloaterPoser::onJointSet, this, _1, _2));
    //BD - Change a bone's position.
    mCommitCallbackRegistrar.add("Joint.PosSet", boost::bind(&BDFloaterPoser::onJointPosSet, this, _1, _2));
    //BD - Change a bone's scale.
    mCommitCallbackRegistrar.add("Joint.SetScale", boost::bind(&BDFloaterPoser::onJointScaleSet, this, _1, _2));
    //BD - Add or remove a joint state to or from the pose (enable/disable our overrides).
    mCommitCallbackRegistrar.add("Joint.ChangeState", boost::bind(&BDFloaterPoser::onJointChangeState, this));
    //BD - Reset all selected bone rotations and positions.
    mCommitCallbackRegistrar.add("Joint.ResetJointFull", boost::bind(&BDFloaterPoser::onJointRotPosScaleReset, this));
    //BD - Reset all selected bone rotations back to 0,0,0.
    mCommitCallbackRegistrar.add("Joint.ResetJointRotation", boost::bind(&BDFloaterPoser::onJointRotationReset, this));
    //BD - Reset all selected bones positions back to their default.
    mCommitCallbackRegistrar.add("Joint.ResetJointPosition", boost::bind(&BDFloaterPoser::onJointPositionReset, this));
    //BD - Reset all selected bones scales back to their default.
    mCommitCallbackRegistrar.add("Joint.ResetJointScale", boost::bind(&BDFloaterPoser::onJointScaleReset, this));
    //BD - Reset all selected bone rotations back to the initial rotation.
    mCommitCallbackRegistrar.add("Joint.RevertJointRotation", boost::bind(&BDFloaterPoser::onJointRotationRevert, this));
    //BD - Recapture all bones either all or just disabled ones.
    mCommitCallbackRegistrar.add("Joint.Recapture", boost::bind(&BDFloaterPoser::onJointRecapture, this));
    //BD - Mirror the current bone's rotation to match what the other body side's rotation should be.
    mCommitCallbackRegistrar.add("Joint.Mirror", boost::bind(&BDFloaterPoser::onJointMirror, this));
    //BD - Copy and mirror the other body side's bone rotation.
    mCommitCallbackRegistrar.add("Joint.SymmetrizeFrom", boost::bind(&BDFloaterPoser::onJointSymmetrize, this, true));
    //BD - Copy and mirror the other body side's bone rotation.
    mCommitCallbackRegistrar.add("Joint.SymmetrizeTo", boost::bind(&BDFloaterPoser::onJointSymmetrize, this, false));

    //BD - Toggle Mirror Mode on/off.
    mCommitCallbackRegistrar.add("Joint.ToggleMirror", boost::bind(&BDFloaterPoser::toggleMirrorMode, this, _1));
    //BD - Toggle Easy Rotation on/off.
    mCommitCallbackRegistrar.add("Joint.EasyRotations", boost::bind(&BDFloaterPoser::toggleEasyRotations, this, _1));
    //BD - Flip pose (mirror).
    mCommitCallbackRegistrar.add("Joint.FlipPose", boost::bind(&BDFloaterPoser::onFlipPose, this));
    //BD - Symmetrize both sides of the opposite body.
    mCommitCallbackRegistrar.add("Joint.SymmetrizePose", boost::bind(&BDFloaterPoser::onPoseSymmetrize, this, _2));

    //BD - Refresh the avatar list.
    mCommitCallbackRegistrar.add("Poser.RefreshAvatars", boost::bind(&BDFloaterPoser::onAvatarsRefresh, this));
}

BDFloaterPoser::~BDFloaterPoser()
{
}

bool BDFloaterPoser::postBuild()
{
    //BD - Posing
    mJointScrolls = { { this->getChild<FSScrollListCtrl>("joints_scroll", true),
                        this->getChild<FSScrollListCtrl>("cv_scroll", true),
                        this->getChild<FSScrollListCtrl>("attach_scroll", true) } };

    mJointScrolls[BD_JOINTS]->setCommitOnSelectionChange(TRUE);
    mJointScrolls[BD_JOINTS]->setCommitCallback(boost::bind(&BDFloaterPoser::onJointControlsRefresh, this));
    mJointScrolls[BD_JOINTS]->setDoubleClickCallback(boost::bind(&BDFloaterPoser::onJointChangeState, this));
    //mJointScrolls[BD_JOINTS]->refreshLineHeight();

    //BD - Collision Volumes
    mJointScrolls[BD_COLLISION_VOLUMES]->setCommitOnSelectionChange(TRUE);
    mJointScrolls[BD_COLLISION_VOLUMES]->setCommitCallback(boost::bind(&BDFloaterPoser::onJointControlsRefresh, this));
    //mJointScrolls[BD_COLLISION_VOLUMES]->refreshLineHeight();

    //BD - Attachment Bones
    mJointScrolls[BD_ATTACHMENT_BONES]->setCommitOnSelectionChange(TRUE);
    mJointScrolls[BD_ATTACHMENT_BONES]->setCommitCallback(boost::bind(&BDFloaterPoser::onJointControlsRefresh, this));
    //mJointScrolls[BD_ATTACHMENT_BONES]->refreshLineHeight();

    mPoseScroll = this->getChild<FSScrollListCtrl>("poses_scroll", true);
    mPoseScroll->setCommitOnSelectionChange(TRUE);
    mPoseScroll->setCommitCallback(boost::bind(&BDFloaterPoser::onPoseControlsRefresh, this));
    mPoseScroll->setDoubleClickCallback(boost::bind(&BDFloaterPoser::onPoseLoad, this));
    //mPoseScroll->refreshLineHeight();

    mRotationSliders = { { getChild<LLUICtrl>("Rotation_X"), getChild<LLUICtrl>("Rotation_Y"), getChild<LLUICtrl>("Rotation_Z") } };
    mPositionSliders = { { getChild<LLSliderCtrl>("Position_X"), getChild<LLSliderCtrl>("Position_Y"), getChild<LLSliderCtrl>("Position_Z") } };
    mScaleSliders = { { getChild<LLSliderCtrl>("Scale_X"), getChild<LLSliderCtrl>("Scale_Y"), getChild<LLSliderCtrl>("Scale_Z") } };

    mJointTabs = getChild<LLTabContainer>("joints_tabs");
    mJointTabs->setCommitCallback(boost::bind(&BDFloaterPoser::onJointControlsRefresh, this));

    mModifierTabs = getChild<LLTabContainer>("modifier_tabs");

    //BD - Animesh
    mAvatarScroll = this->getChild<FSScrollListCtrl>("avatar_scroll", true);
    mAvatarScroll->setCommitCallback(boost::bind(&BDFloaterPoser::onAvatarsSelect, this));
    //mAvatarScroll->refreshLineHeight();

    //BD - Misc
    mDelayRefresh = false;

    mMirrorMode = false;
    mEasyRotations = true;

    mStartPosingBtn = getChild<LLButton>("activate");
    mLoadPosesBtn = getChild<LLMenuButton>("load_poses");
    mSavePosesBtn = getChild<LLButton>("save_poses");

    //BD - Poser Menu
    LLUICtrl::CommitCallbackRegistry::ScopedRegistrar pose_reg;
    LLUICtrl::EnableCallbackRegistry::ScopedRegistrar enable_registrar;
    pose_reg.add("Pose.Menu", boost::bind(&BDFloaterPoser::onPoseLoadSelective, this, _2));
    LLToggleableMenu* btn_menu = LLUICtrlFactory::getInstance()->createFromFile<LLToggleableMenu>("menu_bd_poser_poses_btn.xml",
        gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance());
    if(btn_menu)
        mLoadPosesBtn->setMenu(btn_menu, LLMenuButton::MP_BOTTOM_LEFT);

    LLContextMenu *context_menu = LLUICtrlFactory::getInstance()->createFromFile<LLContextMenu>("menu_bd_poser_poses.xml",
        gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance());
    if (context_menu)
    {
        mPoseScroll->setContextMenu(FSScrollListCtrl::MENU_EXTERNAL, context_menu);
    }

    //BD - Poser Right Click Menu
    pose_reg.add("Joints.Menu", boost::bind(&BDFloaterPoser::onJointContextMenuAction, this, _2));
    enable_registrar.add("Joints.OnEnable", boost::bind(&BDFloaterPoser::onJointContextMenuEnable, this, _2));
    LLContextMenu* joint_menu = LLUICtrlFactory::getInstance()->createFromFile<LLContextMenu>("menu_bd_poser_joints.xml",
        gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance());
    mJointScrolls[BD_JOINTS]->setContextMenu(FSScrollListCtrl::MENU_EXTERNAL, joint_menu);

    //BD - Experimental
    /*mTimeSlider = getChild<LLMultiSliderCtrl>("time_slider");
    mKeySlider = getChild<LLMultiSliderCtrl>("key_slider");

    mTimeSlider->addSlider();
    addSliderKey(0.f, BDPoseKey(std::string("Default")));
    mTimeSlider->setCommitCallback(boost::bind(&BDFloaterPoser::onTimeSliderMoved, this));
    mKeySlider->setCommitCallback(boost::bind(&BDFloaterPoser::onKeyTimeMoved, this));
    getChild<LLButton>("add_key")->setClickedCallback(boost::bind(&BDFloaterPoser::onAddKey, this));
    getChild<LLButton>("delete_key")->setClickedCallback(boost::bind(&BDFloaterPoser::onDeleteKey, this));*/

    //mJointScrolls[BD_JOINTS]->refreshLineHeight();
    //mJointScrolls[BD_COLLISION_VOLUMES]->refreshLineHeight();
    //mJointScrolls[BD_ATTACHMENT_BONES]->refreshLineHeight();
    //mPoseScroll->refreshLineHeight();
    //mAvatarScroll->refreshLineHeight();

    return TRUE;
}

void BDFloaterPoser::draw()
{
    LLFloater::draw();
}

void BDFloaterPoser::onOpen(const LLSD& key)
{
    //BD - Check whether we should delay the default value collection or fire it immediately.
    mDelayRefresh = !gAgentAvatarp->isFullyLoaded();
    if (!mDelayRefresh)
    {
        onCollectDefaults();
    }

    //BD - We first fill the avatar list because the creation controls require it.
    onAvatarsRefresh();
    onJointRefresh();
    onPoseRefresh();
    onUpdateLayout();
}

void BDFloaterPoser::onClose(bool app_quitting)
{
    //BD - Doesn't matter because we destroy the window and rebuild it every time we open it anyway.
    mJointScrolls[BD_JOINTS]->clearRows();
    mJointScrolls[BD_COLLISION_VOLUMES]->clearRows();
    mJointScrolls[BD_ATTACHMENT_BONES]->clearRows();
    mAvatarScroll->clearRows();
}

////////////////////////////////
//BD - Poses
////////////////////////////////
void BDFloaterPoser::onPoseRefresh()
{
    mPoseScroll->clearRows();
    std::string dir = gDirUtilp->getExpandedFilename(LL_PATH_POSES, "");
    std::string file;
    LLDirIterator dir_iter(dir, "*.xml");
    while (dir_iter.next(file))
    {
        std::string path = gDirUtilp->add(dir, file);
        std::string name = gDirUtilp->getBaseFileName(LLURI::unescape(path), true);

        LLSD row;
        row["columns"][0]["column"] = "name";
        row["columns"][0]["value"] = name;

        llifstream infile;
        infile.open(path);
        if (!infile.is_open())
        {
            LL_WARNS("Posing") << "Skipping: Cannot read file in: " << path << LL_ENDL;
            continue;
        }

        LLSD data;
        if (LLSDParser::PARSE_FAILURE == LLSDSerialize::fromXML(data, infile))
        {
            LL_WARNS("Posing") << "Skipping: Failed to parse pose file: " << path << LL_ENDL;
            continue;
        }
        mPoseScroll->addElement(row);
    }
    onJointControlsRefresh();
}

bool BDFloaterPoser::onClickPoseSave(const LLSD& param)
{
    //BD - Values don't matter when not editing.
    if (onPoseSave())
    {
        LLNotificationsUtil::add("PoserExportXMLSuccess");
        return true;
    }
    return false;
}

bool BDFloaterPoser::onPoseSave()
{
    LLScrollListItem* av_item = mAvatarScroll->getFirstSelected();
    if (!av_item)
    {
        LL_WARNS("Posing") << "No avatar selected." << LL_ENDL;
        return false;
    }

    LLVOAvatar* avatar = (LLVOAvatar*)av_item->getUserdata();
    if (!avatar || avatar->isDead())
    {
        LL_WARNS("Posing") << "Couldn't find avatar, dead?" << LL_ENDL;
        return false;
    }

    //BD - First and foremost before we do anything, check if the folder exists.
    std::string pathname = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "poses");
    if (!gDirUtilp->fileExists(pathname))
    {
        LL_WARNS("Posing") << "Couldn't find folder: " << pathname << " - creating one." << LL_ENDL;
        LLFile::mkdir(pathname);
    }

    std::string filename = getChild<LLUICtrl>("pose_name")->getValue().asString();
    if (filename.empty())
        return false;

    std::string full_path = gDirUtilp->getExpandedFilename(LL_PATH_POSES, LLDir::escapePathString(filename) + ".xml");
    LLSD record;

    //BD - Create the header first.
    S32 version = 3;
    record["version"]["value"] = version;

    //BD - Now create the rest.
    for (S32 it = 0; it < 3; ++it)
    {
        for (auto element : mJointScrolls[it]->getAllData())
        {
            LLVector3 vec3;
            LLJoint* joint = (LLJoint*)element->getUserdata();
            if (joint)
            {
                std::string bone_name = joint->getName();
                record[bone_name] = joint->getName();
                joint->getTargetRotation().getEulerAngles(&vec3.mV[VX], &vec3.mV[VZ], &vec3.mV[VY]);
                record[bone_name]["rotation"] = vec3.getValue();

                //BD - All bones support positions now.
                vec3 = it > BD_JOINTS ? joint->getPosition() : joint->getTargetPosition();
                record[bone_name]["position"] = vec3.getValue();

                vec3 = joint->getScale();
                record[bone_name]["scale"] = vec3.getValue();

                if (it == BD_JOINTS)
                {
                    //BD - Save the enabled state per preset so we can switch bones on and off
                    //     on demand inbetween poses additionally to globally.
                    BDPosingMotion* motion = (BDPosingMotion*)gAgentAvatarp->findMotion(ANIM_BD_POSING_MOTION);
                    if (motion)
                    {
                        LLPose* pose = motion->getPose();
                        if (pose)
                        {
                            if (pose->findJointState(joint))
                            {
                                record[bone_name]["enabled"] = true;
                            }
                            else
                            {
                                record[bone_name]["enabled"] = false;
                            }
                        }
                    }
                }
            }
        }
    }

    llofstream file;
    file.open(full_path.c_str());
    //BD - Now lets actually write the file, whether it is writing a new one
    //     or just rewriting the previous one with a new header.
    LLSDSerialize::toPrettyXML(record, file);
    file.close();

    onPoseRefresh();

    //BD - Flash the poses button to give the user a visual cue where it went.
    getChild<LLButton>("extend")->setFlashing(true, true);
    return true;
}

void BDFloaterPoser::onPoseLoad()
{
    LLScrollListItem* item = mPoseScroll->getFirstSelected();
    if (!item) return;

    std::string pose_name = item->getColumn(0)->getValue().asString();

    gDragonAnimator.loadPose(pose_name);
    onJointRefresh();
}

void BDFloaterPoser::onPoseLoadSelective(const LLSD& param)
{
    LLScrollListItem* item = mPoseScroll->getFirstSelected();
    if (!item) return;

    std::string pose_name = item->getColumn(0)->getValue().asString();

    S32 load_type = 0;
    if (param.asString() == "rotation")
        load_type |= BD_ROTATIONS;
    else if (param.asString() == "position")
        load_type |= BD_POSITIONS;
    else if (param.asString() == "scale")
        load_type |= BD_SCALES;
    else if (param.asString() == "rot_pos")
        load_type |= BD_ROTATIONS | BD_POSITIONS;
    else if (param.asString() == "rot_scale")
        load_type |= BD_ROTATIONS | BD_SCALES;
    else if (param.asString() == "pos_scale")
        load_type |= BD_POSITIONS | BD_SCALES;
    else if (param.asString() == "all")
        load_type |= BD_ROTATIONS | BD_POSITIONS | BD_SCALES;

    gDragonAnimator.loadPose(pose_name, load_type);
    onJointRefresh();
}

void BDFloaterPoser::onPoseStart()
{
    LLScrollListItem* item = mAvatarScroll->getFirstSelected();
    if (!item) return;

    LLVOAvatar* avatar = (LLVOAvatar*)item->getUserdata();
    if (!avatar || avatar->isDead()) return;

    BDPosingMotion* motion = (BDPosingMotion*)avatar->findMotion(ANIM_BD_POSING_MOTION);
    if (!motion || motion->isStopped())
    {
        avatar->setPosing();
        if (avatar->isSelf())
        {
            //BD - Grab our current defaults to revert to them when stopping the Poser.
            if(gAgentAvatarp->isFullyLoaded())
                onCollectDefaults();

            gAgent.stopFidget();
        }
        avatar->startDefaultMotions();
        avatar->startMotion(ANIM_BD_POSING_MOTION);
    }
    else
    {
        //BD - Reset everything, all rotations, positions and scales of all bones.
        onJointRotPosScaleReset();

        //BD - Clear posing when we're done now that we've safely endangered getting spaghetified.
        avatar->clearPosing();
        avatar->stopMotion(ANIM_BD_POSING_MOTION);

    }
    //BD - Wipe the joint list.
    onJointRefresh();

    onPoseControlsRefresh();
}

void BDFloaterPoser::onPoseDelete()
{
    for (auto item : mPoseScroll->getAllSelected())
    {
        std::string filename = item->getColumn(0)->getValue().asString();
        std::string dirname = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "poses");

        if (gDirUtilp->deleteFilesInDir(dirname, LLDir::escapePathString(filename) + ".xml") < 1)
        {
            LL_WARNS("Posing") << "Cannot remove file: " << filename << LL_ENDL;
        }
    }
    onPoseRefresh();
}

void BDFloaterPoser::onPoseControlsRefresh()
{
    //bool is_playing = gDragonAnimator.getIsPlaying();
    LLScrollListItem* item = mPoseScroll->getFirstSelected();
    getChild<LLUICtrl>("delete_poses")->setEnabled(bool(item));
    //getChild<LLUICtrl>("add_entry")->setEnabled(!is_playing && item);
    mLoadPosesBtn->setEnabled(bool(item));
}

void BDFloaterPoser::onPoseMenuAction(const LLSD& param)
{
    onPoseLoadSelective(param);
}

////////////////////////////////
//BD - Joints
////////////////////////////////
void BDFloaterPoser::onJointRefresh()
{
    LLScrollListItem* item = mAvatarScroll->getFirstSelected();
    if (!item) return;

    LLVOAvatar* avatar = (LLVOAvatar*)item->getUserdata();
    if (!avatar || avatar->isDead()) return;

    if (!(avatar->getRegion() == gAgent.getRegion())) return;

    //BD - Getting collision volumes and attachment points.
    std::vector<std::string> cv_names, attach_names;
    avatar->getSortedJointNames(1, cv_names);
    avatar->getSortedJointNames(2, attach_names);

    bool is_posing = avatar->getPosing();
    mJointScrolls[BD_JOINTS]->clearRows();
    mJointScrolls[BD_COLLISION_VOLUMES]->clearRows();
    mJointScrolls[BD_ATTACHMENT_BONES]->clearRows();

    LLVector3 rot;
    LLVector3 pos;
    LLVector3 scale;
    LLJoint* joint;
    for (S32 i = 0; (joint = avatar->getCharacterJoint(i)); ++i)
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

            //BD - All bones support positions now.
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
            BDPosingMotion* motion = (BDPosingMotion*)avatar->findMotion(ANIM_BD_POSING_MOTION);
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
        joint = avatar->getJoint(name);
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

        LLScrollListItem* new_item = mJointScrolls[BD_COLLISION_VOLUMES]->addElement(row);
        new_item->setUserdata(joint);
    }

    //BD - Attachment Bones
    for (auto name : attach_names)
    {
        joint = avatar->getJoint(name);
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

void BDFloaterPoser::onJointControlsRefresh()
{
    LLScrollListItem* av_item = mAvatarScroll->getFirstSelected();
    if (!av_item) return;

    LLVOAvatar* avatar = (LLVOAvatar*)av_item->getUserdata();
    if (!avatar || avatar->isDead()) return;

    bool is_pelvis = false;
    bool is_posing = (avatar->isFullyLoaded() && avatar->getPosing());
    S32 index = mJointTabs->getCurrentPanelIndex();
    LLScrollListItem* item = mJointScrolls[index]->getFirstSelected();

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

            //BD - All bones support positions now.
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
    getChild<LLUICtrl>("pose_name")->setEnabled(is_posing);
    getChild<LLUICtrl>("save_poses")->setEnabled(is_posing);
    getChild<LLUICtrl>("joints_tabs")->setEnabled(is_posing);

    //BD - Enable position tabs whenever positions are available, scales are always enabled
    //     unless we are editing attachment bones, rotations on the other hand are only
    //     enabled when editing joints.
    S32 curr_idx = mModifierTabs->getCurrentPanelIndex();
    mModifierTabs->setEnabled(item && is_posing);
    mModifierTabs->enableTabButton(0, (item && is_posing && index == BD_JOINTS));
    mModifierTabs->enableTabButton(1, (item && is_posing));
    mModifierTabs->enableTabButton(2, (item && is_posing && index != BD_ATTACHMENT_BONES));
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
    gDragonAnimator.mTargetAvatar = avatar;

    //mJointScrolls[BD_JOINTS]->refreshLineHeight();
    //mJointScrolls[BD_COLLISION_VOLUMES]->refreshLineHeight();
    //mJointScrolls[BD_ATTACHMENT_BONES]->refreshLineHeight();
    //mPoseScroll->refreshLineHeight();
    //mAvatarScroll->refreshLineHeight();
}

void BDFloaterPoser::onJointSet(LLUICtrl* ctrl, const LLSD& param)
{
    //BD - Rotations are only supported by joints so far, everything
    //     else snaps back instantly.
    LLScrollListItem* item = mJointScrolls[BD_JOINTS]->getFirstSelected();
    if (!item)
        return;

    LLJoint* joint = (LLJoint*)item->getUserdata();
    if (!joint)
        return;

    //BD - Neat yet quick and direct way of rotating our bones.
    //     No more need to include bone rotation orders.
    F32 val = (F32)(ctrl->getValue().asReal());
    S32 axis = param.asInteger();
    LLScrollListCell* cell[3] = { item->getColumn(BD_COL_ROT_X), item->getColumn(BD_COL_ROT_Y), item->getColumn(BD_COL_ROT_Z) };
    LLQuaternion rot_quat = joint->getTargetRotation();
    LLMatrix3 rot_mat;
    F32 old_value;
    F32 new_value;
    LLVector3 vec3;

    old_value = (F32)(cell[axis]->getValue().asReal());
    cell[axis]->setValue(ll_round(val, 0.001f));
    new_value = val - old_value;
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

            //BD - We also need to find the opposite joint's list entry and change its values to reflect
            //     the new ones, doing this here is still better than causing a complete refresh.
            LLScrollListItem* item2 = mJointScrolls[BD_JOINTS]->getItemByLabel(mirror_joint_name, FALSE, BD_COL_NAME);
            if (item2)
            {
                LLScrollListCell* cell2[3] = { item2->getColumn(BD_COL_ROT_X), item2->getColumn(BD_COL_ROT_Y), item2->getColumn(BD_COL_ROT_Z) };
                S32 i = 0;
                while (i < 3)
                {
                    cell2[i]->setValue(ll_round((F32)(item->getColumn(i + 2)->getValue().asReal()), 0.001f));
                    ++i;
                }
            }
        }
    }
}

void BDFloaterPoser::onJointPosSet(LLUICtrl* ctrl, const LLSD& param)
{
    S32 index = mJointTabs->getCurrentPanelIndex();
    LLScrollListItem* item = mJointScrolls[index]->getFirstSelected();

    if (item)
    {
        LLJoint* joint = (LLJoint*)item->getUserdata();
        if (joint)
        {
            //BD - All bones support positions now.
            F32 val = (F32)(ctrl->getValue().asReal());
            LLScrollListCell* cell[3] = { item->getColumn(BD_COL_POS_X), item->getColumn(BD_COL_POS_Y), item->getColumn(BD_COL_POS_Z) };
            LLVector3 vec3 = { F32(cell[VX]->getValue().asReal()),
                                F32(cell[VY]->getValue().asReal()),
                                F32(cell[VZ]->getValue().asReal()) };

            S32 dir = param.asInteger();
            vec3.mV[dir] = val;
            cell[dir]->setValue(ll_round(vec3.mV[dir], 0.001f));
            joint->setTargetPosition(vec3);
        }
    }
}

void BDFloaterPoser::onJointScaleSet(LLUICtrl* ctrl, const LLSD& param)
{
    S32 index = mJointTabs->getCurrentPanelIndex();
    LLScrollListItem* item = mJointScrolls[index]->getFirstSelected();

    if (item)
    {
        LLJoint* joint = (LLJoint*)item->getUserdata();
        if (joint)
        {
            F32 val = (F32)(ctrl->getValue().asReal());
            LLScrollListCell* cell[3] = { item->getColumn(BD_COL_SCALE_X), item->getColumn(BD_COL_SCALE_Y), item->getColumn(BD_COL_SCALE_Z) };
            LLVector3 vec3 = { F32(cell[VX]->getValue().asReal()),
                               F32(cell[VY]->getValue().asReal()),
                               F32(cell[VZ]->getValue().asReal()) };

            S32 dir = param.asInteger();
            vec3.mV[dir] = val;
            cell[dir]->setValue(ll_round(vec3.mV[dir], 0.001f));
            joint->setScale(vec3);
        }
    }
}

void BDFloaterPoser::onJointChangeState()
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
void BDFloaterPoser::onJointRotPosScaleReset()
{
    LLScrollListItem* av_item = mAvatarScroll->getFirstSelected();
    if (!av_item) return;

    //BD - We don't support resetting bones for anyone else yet.
    LLVOAvatar* avatar = (LLVOAvatar*)av_item->getUserdata();
    if (!avatar || avatar->isDead() || !avatar->isSelf()) return;

    //BD - While editing rotations, make sure we use a bit of spherical linear interpolation
    //     to make movements smoother.
    BDPosingMotion* motion = (BDPosingMotion*)avatar->findMotion(ANIM_BD_POSING_MOTION);
    if (motion)
    {
        //BD - If we don't use our default spherical interpolation, set it once.
        motion->setInterpolationTime(0.25f);
        motion->setInterpolationType(2);
    }

    for (S32 it = 0; it < 3; ++it)
    {
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
                    }

                    //BD - Resetting positions next.
                    //     All bones support positions now.
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
void BDFloaterPoser::onJointRotationReset()
{
    LLScrollListItem* item = mAvatarScroll->getFirstSelected();
    if (!item) return;

    //BD - We do support resetting bone rotations for everyone however.
    LLVOAvatar* avatar = (LLVOAvatar*)item->getUserdata();
    if (!avatar || avatar->isDead()) return;

    //BD - While editing rotations, make sure we use a bit of spherical linear interpolation
    //     to make movements smoother.
    BDPosingMotion* motion = (BDPosingMotion*)avatar->findMotion(ANIM_BD_POSING_MOTION);
    if (motion)
    {
        //BD - If we don't use our default spherical interpolation, set it once.
        motion->setInterpolationTime(0.25f);
        motion->setInterpolationType(2);
    }

    for (auto new_item : mJointScrolls[BD_JOINTS]->getAllSelected())
    {
        if (new_item)
        {
            LLJoint* joint = (LLJoint*)new_item->getUserdata();
            if (joint)
            {
                LLQuaternion quat;
                LLScrollListCell* col_x = new_item->getColumn(BD_COL_ROT_X);
                LLScrollListCell* col_y = new_item->getColumn(BD_COL_ROT_Y);
                LLScrollListCell* col_z = new_item->getColumn(BD_COL_ROT_Z);

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
void BDFloaterPoser::onJointPositionReset()
{
    LLScrollListItem* item = mAvatarScroll->getFirstSelected();
    if (!item) return;

    //BD - We don't support resetting bones positions for anyone else yet.
    LLVOAvatar* avatar = (LLVOAvatar*)item->getUserdata();
    if (!avatar || avatar->isDead() || !avatar->isSelf()) return;

    S32 index = mJointTabs->getCurrentPanelIndex();

    //BD - When resetting positions, we don't use interpolation for now, it looks stupid.
    BDPosingMotion* motion = (BDPosingMotion*)avatar->findMotion(ANIM_BD_POSING_MOTION);
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
                //BD - We could just check whether position information is available since only joints
                //     which can have their position changed will have position information but we
                //     want this to be a minefield for crashes.
                //     Bones that can support position
                //     0, 9-37, 39-43, 45-59, 77, 97-107, 110, 112, 115, 117-121, 125, 128-129, 132
                //     as well as all attachment bones and collision volumes.
                if (joint->mHasPosition || index > BD_JOINTS)
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
    }

    onJointControlsRefresh();
}

//BD - Used to reset scales only.
void BDFloaterPoser::onJointScaleReset()
{
    LLScrollListItem* item = mAvatarScroll->getFirstSelected();
    if (!item) return;

    //BD - We don't support resetting bones scales for anyone else yet.
    LLVOAvatar* avatar = (LLVOAvatar*)item->getUserdata();
    if (!avatar || avatar->isDead() || !avatar->isSelf()) return;

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
void BDFloaterPoser::onJointRotationRevert()
{
    LLScrollListItem* item = mAvatarScroll->getFirstSelected();
    if (!item) return;

    //BD - We do support reverting bone rotations for everyone however.
    LLVOAvatar* avatar = (LLVOAvatar*)item->getUserdata();
    if (!avatar || avatar->isDead()) return;

    //BD - While editing rotations, make sure we use a bit of spherical linear interpolation
    //     to make movements smoother.
    BDPosingMotion* motion = (BDPosingMotion*)avatar->findMotion(ANIM_BD_POSING_MOTION);
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
void BDFloaterPoser::onFlipPose()
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

//BD - Copy and mirror one side's joints to the other (symmetrize the pose).
void BDFloaterPoser::onPoseSymmetrize(const LLSD& param)
{
    LLVOAvatar* avatar = gDragonAnimator.mTargetAvatar;
    if (!avatar || avatar->isDead()) return;

    if (!(avatar->getRegion() == gAgent.getRegion())) return;

    LLJoint* joint = nullptr;
    bool flipped[134] = { false };
    bool flipLeft = false;
    if (param.asInteger() == 0)
    {
        flipLeft = true;
    }

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
        if (!flipLeft)
        {
            //BD - Attempt to find the "right" version of this bone first, we assume we always
            //     end up with the "left" version of a bone first.
            size_t idx = joint->getName().find("Left");
            if (idx != -1)
                mirror_joint_name.replace(idx, mirror_joint_name.length(), "Right");
            else
                continue;
        }
        else
        {
            //BD - Attempt to find the "right" version of this bone first, this is necessary
            //     because there are a couple bones starting with the "right" bone.
            size_t idx = joint->getName().find("Right");
            if (idx != -1)
                mirror_joint_name.replace(idx, mirror_joint_name.length(), "Left");
            else
                continue;
        }

        LLJoint* mirror_joint = nullptr;
        if (mirror_joint_name != joint->getName())
            mirror_joint = gDragonAnimator.mTargetAvatar->mRoot->findJoint(mirror_joint_name);

        //BD - Collect the joint and mirror joint entries and their cells, we need them later.
        //LLScrollListItem* item1 = mJointScrolls[BD_JOINTS]->getItemByLabel(joint_name, FALSE, BD_COL_NAME);
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
            //mirror_rot_quat = mirror_joint->getTargetRotation();
            //LLQuaternion inv_mirror_rot_quat = LLQuaternion(-mirror_rot_quat.mQ[VX], mirror_rot_quat.mQ[VY], -mirror_rot_quat.mQ[VZ], mirror_rot_quat.mQ[VW]);
            //inv_mirror_rot_quat.getEulerAngles(&mirror_rot[VX], &mirror_rot[VY], &mirror_rot[VZ]);
            mirror_joint->setTargetRotation(inv_rot_quat);
            //joint->setTargetRotation(inv_mirror_rot_quat);

            item2 = mJointScrolls[BD_JOINTS]->getItemByLabel(mirror_joint_name, FALSE, BD_COL_NAME);

            //BD - Make sure we flag this bone as flipped so we skip it next time we iterate over it.
            flipped[mirror_joint->getJointNum()] = true;
        }
        /*else
        {
            joint->setTargetRotation(inv_rot_quat);
        }*/

        S32 axis = 0;
        while (axis <= 2)
        {
            //BD - Now flip the list entry values.
            /*if (item1)
            {
                if (mirror_joint)
                    item1->getColumn(axis + 2)->setValue(ll_round(mirror_rot[axis], 0.001f));
                else
                    item1->getColumn(axis + 2)->setValue(ll_round(rot[axis], 0.001f));
            }*/

            //BD - Now flip the mirror joint list entry values.
            if (item2)
                item2->getColumn(axis + 2)->setValue(ll_round(rot[axis], 0.001f));

            ++axis;
        }
        flipped[i] = true;
    }
}

//BD - Recapture the current joint's values.
void BDFloaterPoser::onJointRecapture()
{
    LLScrollListItem* item = mAvatarScroll->getFirstSelected();
    if (!item) return;

    LLVOAvatar* avatar = (LLVOAvatar*)item->getUserdata();
    if (!avatar || avatar->isDead()) return;

    if (!(avatar->getRegion() == gAgent.getRegion())) return;

    LLQuaternion rot;
    LLVector3 pos;

    BDPosingMotion* motion = (BDPosingMotion*)avatar->findMotion(ANIM_BD_POSING_MOTION);
    if (motion)
    {
        LLPose* pose = motion->getPose();
        if (pose)
        {
            for (auto item : mJointScrolls[BD_JOINTS]->getAllData())
            {
                if (item)
                {
                    LLJoint* joint = (LLJoint*)item->getUserdata();
                    if (joint)
                    {
                        // BD - Check for the joint state (whether a bone is enabled or not)
                        //      If not proceed.
                        LLPointer<LLJointState> joint_state = pose->findJointState(joint);
                        if (!joint_state)
                        {
                            //BD - First gather the current rotation and position.
                            rot = joint->getRotation();
                            pos = joint->getPosition();

                            //BD - Now, re-add the joint state and enable changing the pose.
                            motion->addJointToState(joint);
                            ((LLScrollListText*)item->getColumn(BD_COL_NAME))->setFontStyle(LLFontGL::BOLD);

                            //BD - Apply the newly collected rotation and position to the pose.
                            joint->setTargetRotation(rot);
                            joint->setTargetPosition(pos);

                            //BD - Get all columns and fill in the new values.
                            LLScrollListCell* col_rot_x = item->getColumn(BD_COL_ROT_X);
                            LLScrollListCell* col_rot_y = item->getColumn(BD_COL_ROT_Y);
                            LLScrollListCell* col_rot_z = item->getColumn(BD_COL_ROT_Z);

                            LLScrollListCell* col_pos_x = item->getColumn(BD_COL_POS_X);
                            LLScrollListCell* col_pos_y = item->getColumn(BD_COL_POS_Y);
                            LLScrollListCell* col_pos_z = item->getColumn(BD_COL_POS_Z);

                            LLVector3 euler_rot;
                            rot.getEulerAngles(&euler_rot.mV[VX], &euler_rot.mV[VY], &euler_rot.mV[VZ]);
                            col_rot_x->setValue(ll_round(euler_rot.mV[VX], 0.001f));
                            col_rot_y->setValue(ll_round(euler_rot.mV[VY], 0.001f));
                            col_rot_z->setValue(ll_round(euler_rot.mV[VZ], 0.001f));

                            col_pos_x->setValue(ll_round(pos.mV[VX], 0.001f));
                            col_pos_y->setValue(ll_round(pos.mV[VY], 0.001f));
                            col_pos_z->setValue(ll_round(pos.mV[VZ], 0.001f));
                        }
                    }
                }
            }
        }
    }
}

//BD - Poser Utility Functions
void BDFloaterPoser::onJointPasteRotation()
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

void BDFloaterPoser::onJointPastePosition()
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

void BDFloaterPoser::onJointPasteScale()
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

void BDFloaterPoser::onJointMirror()
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

void BDFloaterPoser::onJointSymmetrize(bool from)
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
            //     ELSE
            //     Flip the our bone's rotation and apply it to the mirror bone (if available).
            if (mirror_joint)
            {
                LLVector3 mirror_rot;
                LLQuaternion mirror_rot_quat;
                LLScrollListItem* item2 = mJointScrolls[BD_JOINTS]->getItemByLabel(mirror_joint_name, FALSE, BD_COL_NAME);
                if (from)
                {
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
                else
                {
                    mirror_rot_quat = joint->getTargetRotation();
                    LLQuaternion inv_mirror_rot_quat = LLQuaternion(-mirror_rot_quat.mQ[VX], mirror_rot_quat.mQ[VY], -mirror_rot_quat.mQ[VZ], mirror_rot_quat.mQ[VW]);
                    inv_mirror_rot_quat.getEulerAngles(&mirror_rot[VX], &mirror_rot[VY], &mirror_rot[VZ]);
                    mirror_joint->setTargetRotation(inv_mirror_rot_quat);

                    LLScrollListCell* col_rot_x = item2->getColumn(BD_COL_ROT_X);
                    LLScrollListCell* col_rot_y = item2->getColumn(BD_COL_ROT_Y);
                    LLScrollListCell* col_rot_z = item2->getColumn(BD_COL_ROT_Z);

                    col_rot_x->setValue(ll_round(mirror_rot.mV[VX], 0.001f));
                    col_rot_y->setValue(ll_round(mirror_rot.mV[VY], 0.001f));
                    col_rot_z->setValue(ll_round(mirror_rot.mV[VZ], 0.001f));
                }
            }
        }
    }
}

void BDFloaterPoser::onJointCopyTransforms()
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

bool BDFloaterPoser::onJointContextMenuEnable(const LLSD& param)
{
    std::string action = param.asString();
    if (action == "clipboard")
    {
        return mClipboard.has("rot");
    }
    if (action == "enable_bone")
    {
        LLScrollListItem* item = mAvatarScroll->getFirstSelected();
        if (!item) return false;

        LLVOAvatar* avatar = (LLVOAvatar*)item->getUserdata();
        if (!avatar || avatar->isDead()) return false;

        item = mJointScrolls[BD_JOINTS]->getFirstSelected();
        if (item)
        {
            LLJoint* joint = (LLJoint*)item->getUserdata();
            if (joint)
            {
                BDPosingMotion* motion = (BDPosingMotion*)avatar->findMotion(ANIM_BD_POSING_MOTION);
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

void BDFloaterPoser::onJointContextMenuAction(const LLSD& param)
{
    LLScrollListItem* item = mAvatarScroll->getFirstSelected();
    if (!item) return;

    LLVOAvatar* avatar = (LLVOAvatar*)item->getUserdata();
    if (!avatar || avatar->isDead()) return;

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
    else if (action == "symmetrize_from")
    {
        onJointSymmetrize(true);
    }
    else if (action == "symmetrize_to")
    {
        onJointSymmetrize(false);
    }
    else if (action == "mirror")
    {
        onJointMirror();
    }
    else if (action == "recapture")
    {
        onJointRecapture();
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

//BD - This is used to collect all default values at the beginning to revert to later on.
void BDFloaterPoser::onCollectDefaults()
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

        //BD - All bones support positions now.
        //     We always get the values but we don't write them out as they are not relevant for the
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

////////////////////////////////
//BD - Misc Functions
////////////////////////////////

void BDFloaterPoser::loadPoseRotations(std::string name, LLVector3 *rotations)
{
    LLScrollListItem* item = mAvatarScroll->getFirstSelected();
    if (!item) return;

    LLVOAvatar* avatar = (LLVOAvatar*)item->getUserdata();
    if (!avatar || avatar->isDead()) return;

    std::string filename;
    if (!name.empty())
    {
        filename = gDirUtilp->getExpandedFilename(LL_PATH_POSES, LLDir::escapePathString(name) + ".xml");
    }

    LLSD pose;
    llifstream infile;
    infile.open(filename);
    if (!infile.is_open())
    {
        LL_WARNS("Posing") << "Cannot find file in: " << filename << LL_ENDL;
        return;
    }

    while (!infile.eof())
    {
        S32 count = LLSDSerialize::fromXML(pose, infile);
        if (count == LLSDParser::PARSE_FAILURE)
        {
            LL_WARNS("Posing") << "Failed to parse file: " << filename << LL_ENDL;
            return;
        }

        LLJoint* joint = avatar->getJoint(pose["bone"].asString());
        if (joint)
        {
            S32 joint_num = joint->getJointNum();
            if (pose["rotation"].isDefined())
            {
                rotations[joint_num].setValue(pose["rotation"]);
            }
        }
    }
    infile.close();
    return;
}

void BDFloaterPoser::loadPosePositions(std::string name, LLVector3 *positions)
{
    LLScrollListItem* item = mAvatarScroll->getFirstSelected();
    if (!item) return;

    LLVOAvatar* avatar = (LLVOAvatar*)item->getUserdata();
    if (!avatar || avatar->isDead()) return;

    std::string filename;
    if (!name.empty())
    {
        filename = gDirUtilp->getExpandedFilename(LL_PATH_POSES, LLDir::escapePathString(name) + ".xml");
    }

    LLSD pose;
    llifstream infile;
    infile.open(filename);
    if (!infile.is_open())
    {
        LL_WARNS("Posing") << "Cannot find file in: " << filename << LL_ENDL;
        return;
    }

    while (!infile.eof())
    {
        S32 count = LLSDSerialize::fromXML(pose, infile);
        if (count == LLSDParser::PARSE_FAILURE)
        {
            LL_WARNS("Posing") << "Failed to parse file: " << filename << LL_ENDL;
            return;
        }

        LLJoint* joint = avatar->getJoint(pose["bone"].asString());
        if (joint)
        {
            S32 joint_num = joint->getJointNum();
            //BD - Position information is only ever written when it is actually safe to do.
            //     It's safe to assume that IF information is available it's safe to apply.
            if (pose["position"].isDefined())
            {
                positions[joint_num].setValue(pose["position"]);
            }
        }
    }
    infile.close();
    return;
}

void BDFloaterPoser::loadPoseScales(std::string name, LLVector3 *scales)
{
    LLScrollListItem* item = mAvatarScroll->getFirstSelected();
    if (!item) return;

    LLVOAvatar* avatar = (LLVOAvatar*)item->getUserdata();
    if (!avatar || avatar->isDead()) return;

    std::string filename;
    if (!name.empty())
    {
        filename = gDirUtilp->getExpandedFilename(LL_PATH_POSES, LLDir::escapePathString(name) + ".xml");
    }

    LLSD pose;
    llifstream infile;
    infile.open(filename);
    if (!infile.is_open())
    {
        LL_WARNS("Posing") << "Cannot find file in: " << filename << LL_ENDL;
        return;
    }

    while (!infile.eof())
    {
        S32 count = LLSDSerialize::fromXML(pose, infile);
        if (count == LLSDParser::PARSE_FAILURE)
        {
            LL_WARNS("Posing") << "Failed to parse file: " << filename << LL_ENDL;
            return;
        }

        LLJoint* joint = avatar->getJoint(pose["bone"].asString());
        if (joint)
        {
            S32 joint_num = joint->getJointNum();
            //BD - Bone Scales
            if (pose["scale"].isDefined())
            {
                scales[joint_num].setValue(pose["scale"]);
            }
        }
    }
    infile.close();
    return;
}

void BDFloaterPoser::onUpdateLayout()
{
    if (!this->isMinimized())
    {
        bool poses_expanded = getChild<LLButton>("extend")->getValue();
        getChild<LLUICtrl>("poses_layout")->setVisible(poses_expanded);

        S32 collapsed_width = getChild<LLPanel>("min_panel")->getRect().getWidth();
        S32 expanded_width = getChild<LLPanel>("max_panel")->getRect().getWidth();
        S32 floater_width = poses_expanded ? expanded_width : collapsed_width;
        this->reshape(floater_width, this->getRect().getHeight());
    }
}

//BD - Animesh
void BDFloaterPoser::onAvatarsSelect()
{
    //BD - Whenever we select an avatar in the list, check if the selected Avatar is still
    //     valid and/or if new avatars have become valid for posing.
    onAvatarsRefresh();

    //BD - Now that we selected an avatar we can refresh the joint list to have all bones
    //     mapped to that avatar so we can immediately start posing them or continue doing so.
    //     This will automatically invoke a onJointControlsRefresh()
    onJointRefresh();

    //BD - Now that we support animating multiple avatars we also need to refresh all controls
    //     and animation/pose lists for them when we switch to make it as easy as possible to
    //     quickly switch back and forth and make edits.
    onUpdateLayout();

    onPoseControlsRefresh();

    //BD - Disable the Start Posing button if we haven't loaded yet.
    LLScrollListItem* item = mAvatarScroll->getFirstSelected();
    if (item)
    {
        LLVOAvatar* avatar = (LLVOAvatar*)item->getUserdata();
        if (avatar && avatar->isSelf())
            mStartPosingBtn->setEnabled(gAgentAvatarp->isFullyLoaded());
    }

    mStartPosingBtn->setFlashing(true, true);
}

void BDFloaterPoser::onAvatarsRefresh()
{
    //BD - Flag all items first, we're going to unflag them when they are valid.
    for (LLScrollListItem* item : mAvatarScroll->getAllData())
    {
        if (item)
        {
            item->setFlagged(TRUE);
        }
    }

    bool create_new = true;
    for (LLCharacter* character : LLCharacter::sInstances)
    {
        create_new = true;
        LLVOAvatar* avatar = dynamic_cast<LLVOAvatar*>(character);
        if (avatar && !avatar->isControlAvatar()
            && avatar->isSelf())
        {
            LLUUID uuid = avatar->getID();
            for (LLScrollListItem* item : mAvatarScroll->getAllData())
            {
                if (avatar == item->getUserdata())
                {
                    item->setFlagged(FALSE);
                    //BD - When we refresh it might happen that we don't have a name for someone
                    //     yet, when this happens the list entry won't be purged and rebuild as
                    //     it will be updated with this part, so we have to update the name in
                    //     case it was still being resolved last time we refreshed and created the
                    //     initial list entry. This prevents the name from missing forever.
                    if (item->getColumn(1)->getValue().asString().empty())
                    {
                        LLAvatarName av_name;
                        LLAvatarNameCache::get(uuid, &av_name);
                        item->getColumn(1)->setValue(av_name.getDisplayName());
                    }

                    create_new = false;
                    break;
                }
            }

            if (create_new)
            {
                LLAvatarName av_name;
                LLAvatarNameCache::get(uuid, &av_name);

                LLSD row;
                row["columns"][0]["column"] = "icon";
                row["columns"][0]["type"] = "icon";
                row["columns"][0]["value"] = getString("icon_category");
                row["columns"][1]["column"] = "name";
                row["columns"][1]["value"] = av_name.getDisplayName();
                row["columns"][2]["column"] = "uuid";
                row["columns"][2]["value"] = avatar->getID();
                row["columns"][3]["column"] = "control_avatar";
                row["columns"][3]["value"] = false;
                LLScrollListItem* item = mAvatarScroll->addElement(row);
                item->setUserdata(avatar);

                //BD - We're just here to find ourselves, break out immediately when we are done.
                //break;
            }
        }
    }

    //BD - Animesh Support
    //     Search through all control avatars.
    for (auto character : LLControlAvatar::sInstances)
    {
        create_new = true;
        LLControlAvatar* avatar = dynamic_cast<LLControlAvatar*>(character);
        if (avatar && !avatar->isDead() && (avatar->getRegion() == gAgent.getRegion()))
        {
            LLUUID uuid = avatar->getID();
            for (LLScrollListItem* item : mAvatarScroll->getAllData())
            {
                if (item)
                {
                    if (avatar == item->getUserdata())
                    {
                        //BD - Avatar is still valid unflag it from removal.
                        item->setFlagged(FALSE);
                        create_new = false;
                        break;
                    }
                }
            }

            if (create_new)
            {
                //BD - Avatar was not listed yet, create a new entry.
                LLSD row;
                row["columns"][0]["column"] = "icon";
                row["columns"][0]["type"] = "icon";
                row["columns"][0]["value"] = getString("icon_object");
                row["columns"][1]["column"] = "name";
                row["columns"][1]["value"] = avatar->getFullname();
                row["columns"][2]["column"] = "uuid";
                row["columns"][2]["value"] = avatar->getID();
                row["columns"][3]["column"] = "control_avatar";
                row["columns"][3]["value"] = true;
                LLScrollListItem* item = mAvatarScroll->addElement(row);
                item->setUserdata(avatar);
            }
        }
    }

    //BD - Now safely delete all items so we can start adding the missing ones.
    mAvatarScroll->deleteFlaggedItems();

    //BD - Make sure we don't have a scrollbar unless we need it.
    mAvatarScroll->updateLayout();
    //mAvatarScroll->refreshLineHeight();
}
