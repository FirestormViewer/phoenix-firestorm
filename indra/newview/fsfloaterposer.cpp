/**
 * @file fsfloaterposer.cpp
 * @brief View Model for posing your (and other) avatar(s).
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

#include "fsfloaterposer.h"
#include "fsposeranimator.h"
#include "fsvirtualtrackpad.h"
#include "v4color.h"
#include "llagent.h"
#include "llavatarnamecache.h"
#include "llcheckboxctrl.h"
#include "llcommonutils.h"
#include "llcontrolavatar.h"
#include "llnotificationsutil.h"
#include "lldiriterator.h"
#include "llsdserialize.h"
#include "llscrolllistctrl.h"
#include "llsliderctrl.h"
#include "lltabcontainer.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "llwindow.h"
#include "llvoavatarself.h"
#include "llinventoryfunctions.h"
#include "lltoolcomp.h"

namespace
{
constexpr char             POSE_INTERNAL_FORMAT_FILE_MASK[]    = "*.xml";
constexpr char             POSE_INTERNAL_FORMAT_FILE_EXT[]     = ".xml";
constexpr char             POSE_EXTERNAL_FORMAT_FILE_EXT[]     = ".bvh";
constexpr char             POSE_SAVE_SUBDIRECTORY[]            = "poses";
constexpr std::string_view POSE_PRESETS_HANDS_SUBDIRECTORY     = "hand_presets";
constexpr char             XML_LIST_HEADER_STRING_PREFIX[]     = "header_";
constexpr char             XML_LIST_TITLE_STRING_PREFIX[]      = "title_";
constexpr char             XML_JOINT_TRANSFORM_STRING_PREFIX[] = "joint_transform_";
constexpr char             XML_JOINT_DELTAROT_STRING_PREFIX[]  = "joint_delta_rotate_";
constexpr char             BVH_JOINT_TRANSFORM_STRING_PREFIX[] = "bvh_joint_transform_";
constexpr std::string_view POSER_TRACKPAD_SENSITIVITY_SAVE_KEY = "FSPoserTrackpadSensitivity";
constexpr std::string_view POSER_STOPPOSINGWHENCLOSED_SAVE_KEY = "FSPoserStopPosingWhenClosed";
constexpr std::string_view POSER_RESETBASEROTONEDIT_SAVE_KEY   = "FSPoserResetBaseRotationOnEdit";
constexpr std::string_view POSER_SAVEEXTERNALFORMAT_SAVE_KEY   = "FSPoserSaveExternalFileAlso";
constexpr std::string_view POSER_SAVECONFIRMREQUIRED_SAVE_KEY  = "FSPoserOnSaveConfirmOverwrite";
constexpr char             ICON_SAVE_OK[]                      = "icon_rotation_is_own_work";
constexpr char             ICON_SAVE_FAILED[]                  = "icon_save_failed_button";

}  // namespace

/// <summary>
/// The amount of deflection 'one unit' on the trackpad translates to in radians.
/// The trackpad ordinarily has a range of +1..-1; multiplied by PI, gives PI to -PI, or all 360 degrees of deflection.
/// </summary>
constexpr F32 NormalTrackpadRangeInRads = F_PI;

FSFloaterPoser::FSFloaterPoser(const LLSD& key) : LLFloater(key)
{
    // Bind requests, other controls are find-and-binds, see postBuild()
    mCommitCallbackRegistrar.add("Poser.RefreshAvatars", [this](LLUICtrl*, const LLSD&) { onAvatarsRefresh(); });
    mCommitCallbackRegistrar.add("Poser.StartStopAnimating", [this](LLUICtrl*, const LLSD&) { onPoseStartStop(); });
    mCommitCallbackRegistrar.add("Poser.ToggleLoadSavePanel", [this](LLUICtrl*, const LLSD&) { onToggleLoadSavePanel(); });
    mCommitCallbackRegistrar.add("Poser.ToggleVisualManipulators", [this](LLUICtrl*, const LLSD&) { onToggleVisualManipulators(); });

    mCommitCallbackRegistrar.add("Poser.UndoLastRotation", [this](LLUICtrl*, const LLSD&) { onUndoLastChange(); });
    mCommitCallbackRegistrar.add("Poser.RedoLastRotation", [this](LLUICtrl*, const LLSD&) { onRedoLastChange(); });
    mCommitCallbackRegistrar.add("Poser.ToggleMirrorChanges", [this](LLUICtrl*, const LLSD&) { onToggleMirrorChange(); });
    mCommitCallbackRegistrar.add("Poser.ToggleSympatheticChanges", [this](LLUICtrl*, const LLSD&) { onToggleSympatheticChange(); });
    mCommitCallbackRegistrar.add("Poser.AdjustTrackPadSensitivity", [this](LLUICtrl*, const LLSD&) { onAdjustTrackpadSensitivity(); });

    mCommitCallbackRegistrar.add("Poser.PositionSet", [this](LLUICtrl*, const LLSD&) { onPositionSet(); });
    mCommitCallbackRegistrar.add("Poser.SetToTPose", [this](LLUICtrl*, const LLSD&) { onSetAvatarToTpose(); });

    mCommitCallbackRegistrar.add("Poser.Advanced.ScaleSet", [this](LLUICtrl*, const LLSD&) { onScaleSet(); });
    mCommitCallbackRegistrar.add("Poser.UndoLastPosition", [this](LLUICtrl*, const LLSD&) { onUndoLastChange(); });
    mCommitCallbackRegistrar.add("Poser.RedoLastPosition", [this](LLUICtrl*, const LLSD&) { onRedoLastChange(); });
    mCommitCallbackRegistrar.add("Poser.ResetJoint", [this](LLUICtrl*, const LLSD& data) { onResetJoint(data); });

    mCommitCallbackRegistrar.add("Poser.Save", [this](LLUICtrl*, const LLSD&) { onClickPoseSave(); });
    mCommitCallbackRegistrar.add("Pose.Menu", [this](LLUICtrl*, const LLSD& data) { onPoseMenuAction(data); });
    mCommitCallbackRegistrar.add("Poser.BrowseCache", [this](LLUICtrl*, const LLSD&) { onClickBrowsePoseCache(); });
    mCommitCallbackRegistrar.add("Poser.LoadLeftHand", [this](LLUICtrl*, const LLSD&) { onClickLoadLeftHandPose(); });
    mCommitCallbackRegistrar.add("Poser.LoadRightHand", [this](LLUICtrl*, const LLSD&) { onClickLoadRightHandPose(); });

    mCommitCallbackRegistrar.add("Poser.FlipPose", [this](LLUICtrl*, const LLSD&) { onClickFlipPose(); });
    mCommitCallbackRegistrar.add("Poser.FlipJoint", [this](LLUICtrl*, const LLSD&) { onClickFlipSelectedJoints(); });
    mCommitCallbackRegistrar.add("Poser.RecaptureSelectedBones", [this](LLUICtrl*, const LLSD&) { onClickRecaptureSelectedBones(); });
    mCommitCallbackRegistrar.add("Poser.TogglePosingSelectedBones", [this](LLUICtrl*, const LLSD&) { onClickToggleSelectedBoneEnabled(); });

    mCommitCallbackRegistrar.add("Poser.CommitSpinner", [this](LLUICtrl* spinner, const LLSD& data) { onCommitSpinner(spinner, data); });
    mCommitCallbackRegistrar.add("Poser.CommitSlider", [this](LLUICtrl* slider, const LLSD& data) { onCommitSlider(slider, data); });
    mCommitCallbackRegistrar.add("Poser.Symmetrize", [this](LLUICtrl*, const LLSD& data) { onClickSymmetrize(data); });
}

bool FSFloaterPoser::postBuild()
{
    mAvatarTrackball = getChild<FSVirtualTrackpad>("limb_rotation");
    mAvatarTrackball->setCommitCallback([this](LLUICtrl *, const LLSD &) { onTrackballChanged(); });

    mJointsTabs = getChild<LLTabContainer>("joints_tabs");
    mJointsTabs->setCommitCallback(
        [this](LLUICtrl*, const LLSD&)
        {
            onJointTabSelect();
            setRotationChangeButtons(false, false);
        });        

    mAvatarSelectionScrollList = getChild<LLScrollListCtrl>("avatarSelection_scroll");
    mAvatarSelectionScrollList->setCommitOnSelectionChange(true);
    mAvatarSelectionScrollList->setCommitCallback([this](LLUICtrl *, const LLSD &) { onAvatarSelect(); });

    mBodyJointsScrollList = getChild<LLScrollListCtrl>("body_joints_scroll");
    mBodyJointsScrollList->setCommitOnSelectionChange(true);
    mBodyJointsScrollList->setCommitCallback([this](LLUICtrl *, const LLSD &) { onJointTabSelect(); });

    mFaceJointsScrollList = getChild<LLScrollListCtrl>("face_joints_scroll");
    mFaceJointsScrollList->setCommitOnSelectionChange(true);
    mFaceJointsScrollList->setCommitCallback([this](LLUICtrl *, const LLSD &) { onJointTabSelect(); });

    mHandJointsScrollList = getChild<LLScrollListCtrl>("hand_joints_scroll");
    mHandJointsScrollList->setCommitOnSelectionChange(true);
    mHandJointsScrollList->setCommitCallback([this](LLUICtrl *, const LLSD &) { onJointTabSelect(); });

    mMiscJointsScrollList = getChild<LLScrollListCtrl>("misc_joints_scroll");
    mMiscJointsScrollList->setCommitOnSelectionChange(true);
    mMiscJointsScrollList->setCommitCallback([this](LLUICtrl *, const LLSD &) { onJointTabSelect(); });

    mCollisionVolumesScrollList = getChild<LLScrollListCtrl>("collision_volumes_scroll");
    mCollisionVolumesScrollList->setCommitOnSelectionChange(true);
    mCollisionVolumesScrollList->setCommitCallback([this](LLUICtrl *, const LLSD &) { onJointTabSelect(); });

    mEntireAvJointScroll = getChild<LLScrollListCtrl>("entireAv_joint_scroll");

    mPosesScrollList = getChild<LLScrollListCtrl>("poses_scroll");
    mPosesScrollList->setCommitOnSelectionChange(true);
    mPosesScrollList->setCommitCallback([this](LLUICtrl *, const LLSD &) { onPoseFileSelect(); });

    mToggleVisualManipulators = getChild<LLButton>("toggleVisualManipulators");
    mToggleVisualManipulators->setToggleState(true);

    mTrackpadSensitivitySlider = getChild<LLSliderCtrl>("trackpad_sensitivity_slider");

    mPoseSaveNameEditor = getChild<LLLineEditor>("pose_save_name");
    mPoseSaveNameEditor->setPrevalidate(&LLTextValidate::validateASCIIPrintableNoPipe);

    mHandPresetsScrollList = getChild<LLScrollListCtrl>("hand_presets_scroll");

    mPosXSlider = getChild<LLSliderCtrl>("av_position_inout");
    mPosYSlider = getChild<LLSliderCtrl>("av_position_leftright");
    mPosZSlider = getChild<LLSliderCtrl>("av_position_updown");

    mAdvRotXSlider = getChild<LLSliderCtrl>("limb_pitch_slider");
    mAdvRotYSlider = getChild<LLSliderCtrl>("limb_yaw_slider");
    mAdvRotZSlider = getChild<LLSliderCtrl>("limb_roll_slider");

    mAdvPosXSlider = getChild<LLSliderCtrl>("Advanced_Position_X");
    mAdvPosYSlider = getChild<LLSliderCtrl>("Advanced_Position_Y");
    mAdvPosZSlider = getChild<LLSliderCtrl>("Advanced_Position_Z");

    mAdvScaleXSlider = getChild<LLSliderCtrl>("Advanced_Scale_X");
    mAdvScaleYSlider = getChild<LLSliderCtrl>("Advanced_Scale_Y");
    mAdvScaleZSlider = getChild<LLSliderCtrl>("Advanced_Scale_Z");

    mPosesLoadSavePnl = getChild<LLPanel>("poses_loadSave");
    mStartStopPosingBtn = getChild<LLButton>("start_stop_posing_button");
    mToggleLoadSavePanelBtn = getChild<LLButton>("toggleLoadSavePanel");
    mBrowserFolderBtn = getChild<LLButton>("open_poseDir_button");
    mLoadPosesBtn = getChild<LLButton>("load_poses_button");
    mSavePosesBtn = getChild<LLButton>("save_poses_button");
    mSavePosesBtn->setMouseLeaveCallback([this](LLUICtrl*, const LLSD&) { onMouseLeaveSavePoseBtn(); });

    mFlipPoseBtn = getChild<LLButton>("FlipPose_avatar");
    mFlipJointBtn = getChild<LLButton>("FlipJoint_avatar");
    mRecaptureBtn = getChild<LLButton>("button_RecaptureParts");
    mTogglePosingBonesBtn = getChild<LLButton>("toggle_PosingSelectedBones");

    mToggleMirrorRotationBtn = getChild<LLButton>("button_toggleMirrorRotation");
    mToggleSympatheticRotationBtn = getChild<LLButton>("button_toggleSympatheticRotation");
    mToggleDeltaModeBtn = getChild<LLButton>("delta_mode_toggle");
    mRedoChangeBtn = getChild<LLButton>("button_redo_change");
    mUndoChangeBtn = getChild<LLButton>("undo_change");
    mSetToTposeButton = getChild<LLButton>("set_t_pose_button");

    mJointsParentPnl = getChild<LLPanel>("joints_parent_panel");
    mTrackballPnl = getChild<LLPanel>("trackball_panel");
    mPositionRotationPnl = getChild<LLPanel>("positionRotation_panel");
    mBodyJointsPnl = getChild<LLPanel>("body_joints_panel");
    mFaceJointsPnl = getChild<LLPanel>("face_joints_panel");
    mHandsTabs = getChild<LLTabContainer>("hands_tabs");
    mHandsJointsPnl = mHandsTabs->getChild<LLPanel>("hands_joints_panel");
    mMiscJointsPnl = getChild<LLPanel>("misc_joints_panel");
    mCollisionVolumesPnl = getChild<LLPanel>("collision_volumes_panel");

    mAlsoSaveBvhCbx = getChild<LLCheckBoxCtrl>("also_save_bvh_checkbox");
    mResetBaseRotCbx = getChild<LLCheckBoxCtrl>("reset_base_rotation_on_edit_checkbox");
    mResetBaseRotCbx->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickSetBaseRotZero(); });

    mTrackpadSensitivitySpnr = getChild<LLUICtrl>("trackpad_sensitivity_spinner");
    mYawSpnr                 = getChild<LLUICtrl>("limb_yaw_spinner");
    mYawSpnr->setCommitCallback([this](LLUICtrl*, const LLSD&) { onYawPitchRollChanged(); });
    mPitchSpnr               = getChild<LLUICtrl>("limb_pitch_spinner");
    mPitchSpnr->setCommitCallback([this](LLUICtrl*, const LLSD&) { onYawPitchRollChanged(); });
    mRollSpnr = getChild<LLUICtrl>("limb_roll_spinner");
    mRollSpnr->setCommitCallback([this](LLUICtrl*, const LLSD&) { onYawPitchRollChanged(); });
    mUpDownSpnr              = getChild<LLUICtrl>("av_position_updown_spinner");
    mLeftRightSpnr           = getChild<LLUICtrl>("av_position_leftright_spinner");
    mInOutSpnr               = getChild<LLUICtrl>("av_position_inout_spinner");
    mAdvPosXSpnr             = getChild<LLUICtrl>("adv_posx_spinner");
    mAdvPosYSpnr             = getChild<LLUICtrl>("adv_posy_spinner");
    mAdvPosZSpnr             = getChild<LLUICtrl>("adv_posz_spinner");
    mScaleXSpnr              = getChild<LLUICtrl>("adv_scalex_spinner");
    mScaleYSpnr              = getChild<LLUICtrl>("adv_scaley_spinner");
    mScaleZSpnr              = getChild<LLUICtrl>("adv_scalez_spinner");

    mBtnJointRotate = getChild<LLButton>("button_joint_rotate_tool");

    return true;
}

void FSFloaterPoser::onOpen(const LLSD& key)
{
    createUserPoseDirectoryIfNeeded();
    onAvatarsRefresh();
    refreshJointScrollListMembers();
    onJointTabSelect();
    refreshPoseScroll(mHandPresetsScrollList, POSE_PRESETS_HANDS_SUBDIRECTORY);
    startPosingSelf();

    enableVisualManipulators();
    LLFloater::onOpen(key);
}


void FSFloaterPoser::onFocusReceived()
{
    LLEditMenuHandler::gEditMenuHandler = this;
}

void FSFloaterPoser::onFocusLost()
{
    if( LLEditMenuHandler::gEditMenuHandler == this )
    {
        LLEditMenuHandler::gEditMenuHandler = nullptr;
    }
}

void FSFloaterPoser::enableVisualManipulators()
{
    if (!gAgentAvatarp || gAgentAvatarp.isNull())
    {
        mToggleVisualManipulators->setToggleState(false);
        return;
    }

    if (LLToolMgr::getInstance()->getCurrentToolset() != gCameraToolset)
    {
        mLastToolset = LLToolMgr::getInstance()->getCurrentToolset();
    }

    LLToolMgr::getInstance()->setCurrentToolset(gPoserToolset);
    LLToolMgr::getInstance()->getCurrentToolset()->selectTool(FSToolCompPose::getInstance());
    FSToolCompPose::getInstance()->setAvatar(gAgentAvatarp);
}

void FSFloaterPoser::disableVisualManipulators()
{
    if (mLastToolset)
    {
        LLToolMgr::getInstance()->setCurrentToolset(mLastToolset);
    }
    FSToolCompPose::getInstance()->setAvatar(nullptr);
}

void FSFloaterPoser::onClose(bool app_quitting)
{
    if (gSavedSettings.getBOOL(POSER_STOPPOSINGWHENCLOSED_SAVE_KEY))
    {
        stopPosingAllAvatars();
    }

    disableVisualManipulators();
    LLFloater::onClose(app_quitting);
}

void FSFloaterPoser::refreshPoseScroll(LLScrollListCtrl* posesScrollList, std::optional<std::string_view> subDirectory)
{
    if (!posesScrollList)
        return;

    posesScrollList->clearRows();

    std::string dir = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, POSE_SAVE_SUBDIRECTORY);
    if (subDirectory.has_value())
        gDirUtilp->append(dir, std::string(subDirectory.value()));

    std::string file;
    LLDirIterator dir_iter(dir, POSE_INTERNAL_FORMAT_FILE_MASK);
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

        posesScrollList->addElement(row);
    }
}

void FSFloaterPoser::onPoseFileSelect()
{
    LLScrollListItem* item = mPosesScrollList->getFirstSelected();
    if (!item)
        return;

    LLVOAvatar* avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    bool enableButtons = mPoserAnimator.isPosingAvatar(avatar);
    mLoadPosesBtn->setEnabled(enableButtons);
    mSavePosesBtn->setEnabled(enableButtons);

    std::string poseName = item->getColumn(0)->getValue().asString();
    if (poseName.empty())
        return;

    LLStringExplicit name = LLStringExplicit(poseName);
    mPoseSaveNameEditor->setEnabled(enableButtons);
    mPoseSaveNameEditor->setText(name);

    bool isDeltaSave = !poseFileStartsFromTeePose(name);
    if (isDeltaSave && hasString("LoadDiffLabel"))
        mLoadPosesBtn->setLabel(getString("LoadDiffLabel"));
    else if (hasString("LoadPoseLabel"))
        mLoadPosesBtn->setLabel(getString("LoadPoseLabel"));
}

void FSFloaterPoser::doPoseSave(LLVOAvatar* avatar, const std::string& filename)
{
    if (!avatar)
        return;

    bool successfulSave = savePoseToXml(avatar, filename);
    if (successfulSave)
    {
        refreshPoseScroll(mPosesScrollList);
        setUiSelectedAvatarSaveFileName(filename);

        if (getSavingToBvh())
            savePoseToBvh(avatar, filename);

        if (hasString(ICON_SAVE_OK))
            mSavePosesBtn->setImageOverlay(getString(ICON_SAVE_OK), mSavePosesBtn->getImageOverlayHAlign());

        setSavePosesButtonText(!mPoserAnimator.allBaseRotationsAreZero(avatar));
    }
    else
    {
        if (hasString(ICON_SAVE_FAILED))
            mSavePosesBtn->setImageOverlay(getString(ICON_SAVE_FAILED), mSavePosesBtn->getImageOverlayHAlign());
    }    
}

void FSFloaterPoser::onClickPoseSave()
{
    std::string filename = mPoseSaveNameEditor->getValue().asString();
    if (filename.empty() && hasString(ICON_SAVE_FAILED))
    {
        mSavePosesBtn->setImageOverlay(getString(ICON_SAVE_FAILED), mSavePosesBtn->getImageOverlayHAlign());
        return;
    }
    
    LLVOAvatar* avatar = getUiSelectedAvatar();
    if (!avatar)
    return;
  
    // if prompts are disabled or file doesn't exist, do the save immediately:
    const bool prompt = gSavedSettings.getBOOL(POSER_SAVECONFIRMREQUIRED_SAVE_KEY);

    std::string fullPath = gDirUtilp->getExpandedFilename(
        LL_PATH_USER_SETTINGS, POSE_SAVE_SUBDIRECTORY, filename + POSE_INTERNAL_FORMAT_FILE_EXT);
    const bool exists = gDirUtilp->fileExists(fullPath);

    if (!prompt || !exists)
    {
        // new file or no overwrite guard
        doPoseSave(avatar, filename);
    }
    else
    {
        // show a modal dialog, passing the pose name along
        LLSD args;
        args["POSE_NAME"] = filename;
       
        LLNotificationsUtil::add("ConfirmPoserOverwrite", args, LLSD(), // no payload
            [this, avatar, filename](const LLSD& notification, const LLSD& response)
            {
                if (LLNotificationsUtil::getSelectedOption(notification, response) == 0)
                {
                    // user clicked “Yes”
                    doPoseSave(avatar, filename);
                }
                // else do nothing (cancel)
            });
    }
}

void FSFloaterPoser::onMouseLeaveSavePoseBtn()
{
    if (hasString("icon_save_button"))
        mSavePosesBtn->setImageOverlay(getString("icon_save_button"), mSavePosesBtn->getImageOverlayHAlign());

    LLVOAvatar* avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    setSavePosesButtonText(!mPoserAnimator.allBaseRotationsAreZero(avatar));
}

void FSFloaterPoser::createUserPoseDirectoryIfNeeded()
{
    std::string userPath =
        gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, POSE_SAVE_SUBDIRECTORY);

    std::string userHandPresetsPath = userPath + gDirUtilp->getDirDelimiter() + std::string(POSE_PRESETS_HANDS_SUBDIRECTORY);
    if (gDirUtilp->fileExists(userHandPresetsPath))
        return;

    try
    {
        if (!gDirUtilp->fileExists(userPath))
        {
            LL_WARNS("Poser") << "Couldn't find folder: " << userPath << " - creating one." << LL_ENDL;
            LLFile::mkdir(userPath);
        }

        if (!gDirUtilp->fileExists(userHandPresetsPath))
        {
            LL_WARNS("Poser") << "Couldn't find folder: " << userHandPresetsPath << " - creating one." << LL_ENDL;
            LLFile::mkdir(userHandPresetsPath);
        }

        std::string sourcePresetPath =
            gDirUtilp->getExpandedFilename(LL_PATH_EXECUTABLE, POSE_SAVE_SUBDIRECTORY, std::string(POSE_PRESETS_HANDS_SUBDIRECTORY));

        if (!gDirUtilp->fileExists(sourcePresetPath))
            return;

        auto posesToCopy = gDirUtilp->getFilesInDir(sourcePresetPath);
        for (const auto& pose : posesToCopy)
        {
            std::string source      = sourcePresetPath + gDirUtilp->getDirDelimiter() + pose;
            std::string destination = userHandPresetsPath + gDirUtilp->getDirDelimiter() + pose;

            if (!LLFile::copy(source, destination))
                LL_WARNS("Poser") << "Failed to copy " << source << " to " << destination << LL_ENDL;
        }
    }
    catch (const std::exception& e)
    {
        LL_WARNS("Posing") << "Exception caught trying to create: " << userPath << e.what() << LL_ENDL;
    }
}

bool FSFloaterPoser::savePoseToXml(LLVOAvatar* avatar, const std::string& poseFileName)
{
    if (poseFileName.empty())
        return false;

    if (!mPoserAnimator.isPosingAvatar(avatar))
        return false;

    createUserPoseDirectoryIfNeeded();

    try
    {
        bool savingDiff = !mPoserAnimator.allBaseRotationsAreZero(avatar);
        LLSD record;
        record["version"]["value"] = (S32)6;
        record["startFromTeePose"]["value"] = !savingDiff;

        LLVector3 rotation, position, scale, zeroVector;
        bool      baseRotationIsZero;

        for (const FSPoserAnimator::FSPoserJoint& pj : mPoserAnimator.PoserJoints)
        {
            std::string bone_name = pj.jointName();
            bool posingThisJoint  = mPoserAnimator.isPosingAvatarJoint(avatar, pj);

            record[bone_name]            = bone_name;
            record[bone_name]["enabled"] = posingThisJoint;
            if (!posingThisJoint)
                continue;

            if (!mPoserAnimator.tryGetJointSaveVectors(avatar, pj, &rotation, &position, &scale, &baseRotationIsZero))
                continue;

            bool jointRotPosScaleAllZero = rotation == zeroVector && position == zeroVector && scale == zeroVector;

            if (savingDiff && jointRotPosScaleAllZero)
                continue;

            record[bone_name]["jointBaseRotationIsZero"] = baseRotationIsZero;
            record[bone_name]["rotation"] = rotation.getValue();
            record[bone_name]["position"] = position.getValue();
            record[bone_name]["scale"]    = scale.getValue();
        }

        std::string fullSavePath =
            gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, POSE_SAVE_SUBDIRECTORY, poseFileName + POSE_INTERNAL_FORMAT_FILE_EXT);

        llofstream file;
        file.open(fullSavePath.c_str());
        if (!file.is_open())
        {
            LL_WARNS("Poser") << "Unable to save pose!" << LL_ENDL;
            return false;
        }
        LLSDSerialize::toPrettyXML(record, file);
        file.close();
    }
    catch (const std::exception& e)
    {
        LL_WARNS("Posing") << "Exception caught in saveToXml: " << e.what() << LL_ENDL;
        return false;
    }


    return true;
}

void FSFloaterPoser::onClickToggleSelectedBoneEnabled()
{
    auto selectedJoints = getUiSelectedPoserJoints();
    if (selectedJoints.size() < 1)
        return;

    LLVOAvatar *avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    if (!mPoserAnimator.isPosingAvatar(avatar))
        return;

    for (auto item : selectedJoints)
    {
        bool currentlyPosing = mPoserAnimator.isPosingAvatarJoint(avatar, *item);
        mPoserAnimator.setPosingAvatarJoint(avatar, *item, !currentlyPosing);
    }

    refreshRotationSlidersAndSpinners();
    refreshTrackpadCursor();
    refreshTextHighlightingOnJointScrollLists();
}

void FSFloaterPoser::onClickFlipSelectedJoints()
{
    auto selectedJoints = getUiSelectedPoserJoints();
    if (selectedJoints.size() < 1)
        return;

    LLVOAvatar *avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    if (!mPoserAnimator.isPosingAvatar(avatar))
        return;

    for (auto item : selectedJoints)
    {
        // need to be posing the joint to flip
        bool currentlyPosingJoint = mPoserAnimator.isPosingAvatarJoint(avatar, *item);
        if (!currentlyPosingJoint)
            continue;

        auto oppositeJoint = mPoserAnimator.getPoserJointByName(item->mirrorJointName());
        if (oppositeJoint)
        {
            bool currentlyPosingOppositeJoint = mPoserAnimator.isPosingAvatarJoint(avatar, *oppositeJoint);
            if (!currentlyPosingOppositeJoint)
                continue;
        }

        // If one selects a joint and its opposite, this would otherwise flip each of them sequentially to yeild no net result.
        // This allows the user to select a joint and its opposite, or only select one joint, and get the same result.
        if (std::find(selectedJoints.begin(), selectedJoints.end(), oppositeJoint) != selectedJoints.end())
        {
            if (!item->dontFlipOnMirror())
                continue;
        }

        mPoserAnimator.reflectJoint(avatar, item);
    }

    refreshRotationSlidersAndSpinners();
    enableOrDisableRedoAndUndoButton();
    refreshTrackpadCursor();
}

void FSFloaterPoser::onClickFlipPose()
{
    LLVOAvatar *avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    if (!mPoserAnimator.isPosingAvatar(avatar))
        return;

    mPoserAnimator.flipEntirePose(avatar);

    refreshRotationSlidersAndSpinners();
    refreshTrackpadCursor();
}

void FSFloaterPoser::onClickRecaptureSelectedBones()
{
    auto selectedJoints = getUiSelectedPoserJoints();
    if (selectedJoints.size() < 1)
        return;

    LLVOAvatar *avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    if (!mPoserAnimator.isPosingAvatar(avatar))
        return;

    for (auto item : selectedJoints)
    {
        bool currentlyPosing = mPoserAnimator.isPosingAvatarJoint(avatar, *item);
        if (currentlyPosing)
            continue;

        mPoserAnimator.recaptureJoint(avatar, *item, getJointTranslation(item->jointName()), getJointNegation(item->jointName()));
    }

    setSavePosesButtonText(true);
    refreshRotationSlidersAndSpinners();
    refreshTrackpadCursor();
    refreshTextHighlightingOnJointScrollLists();
    enableOrDisableRedoAndUndoButton();
}

void FSFloaterPoser::updatePosedBones(const std::string& jointName)
{
    LLVOAvatar *avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    if (!mPoserAnimator.isPosingAvatar(avatar))
        return;

    const FSPoserAnimator::FSPoserJoint* poserJoint = mPoserAnimator.getPoserJointByName(jointName);
    if (!poserJoint)
        return;

    mPoserAnimator.recaptureJointAsDelta(avatar, poserJoint, getUiSelectedBoneDeflectionStyle());

    refreshRotationSlidersAndSpinners();
    refreshPositionSlidersAndSpinners();
    refreshScaleSlidersAndSpinners();
    refreshTrackpadCursor();
    enableOrDisableRedoAndUndoButton();
    refreshTextHighlightingOnJointScrollLists();
}

void FSFloaterPoser::onClickBrowsePoseCache()
{
    createUserPoseDirectoryIfNeeded();

    std::string pathname = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, POSE_SAVE_SUBDIRECTORY);
    gViewerWindow->getWindow()->openFile(pathname);
}

void FSFloaterPoser::onClickSymmetrize(const S32 ID)
{
    LLVOAvatar* avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    if (!mPoserAnimator.isPosingAvatar(avatar))
        return;

    mPoserAnimator.symmetrizeLeftToRightOrRightToLeft(avatar, ID == 2);

    refreshRotationSlidersAndSpinners();
    enableOrDisableRedoAndUndoButton();
    refreshTrackpadCursor();
}

void FSFloaterPoser::onCommitSpinner(const LLUICtrl* spinner, const S32 id)
{
    if (!spinner)
        return;

    F32 value = (F32)spinner->getValue().asReal();

    switch (id)
    {
        case 0: // av_position_updown_spinner
        {
            mAdvPosZSpnr->setValue(value);
            onPositionSet();
            break;
        }
        case 1: // av_position_leftright
        {
            mAdvPosYSpnr->setValue(value);
            onPositionSet();
            break;
        }
        case 2: // av_position_inout_spinner
        {
            mAdvPosXSpnr->setValue(value);
            onPositionSet();
            break;
        }
        case 3: // trackpad_sensitivity_spinner
        {
            onAdjustTrackpadSensitivity();
            break;
        }
        case 7: // adv_posx_spinner
        case 8: // adv_posy_spinner
        case 9: // adv_posz_spinner
        {
            onPositionSet();
            break;
        }
        case 10: // adv_scalex_spinner
        case 11: // adv_scaley_spinner
        case 12: // adv_scalez_spinner
        {
            onScaleSet();
            break;
        }

        default:
            LL_WARNS("Posing") << "onCommitSpinner passed invalid parameter: " << id << LL_ENDL;
            break;
    }
}

void FSFloaterPoser::onCommitSlider(const LLUICtrl* slider, const S32 id)
{
    if (!slider)
        return;

    F32 value = (F32)slider->getValue().asReal();

    switch (id)
    {
        case 0: // av_position_updown
        case 9: // Advanced_Position_Z
        {
            mAdvPosZSpnr->setValue(value);
            onPositionSet();
            break;
        }

        case 1: // av_position_leftright
        case 8: // Advanced_Position_Y
        {
            mAdvPosYSpnr->setValue(value);
            onPositionSet();
            break;
        }

        case 2: // av_position_inout
        case 7: // Advanced_Position_X
        {
            mAdvPosXSpnr->setValue(value);
            onPositionSet();
            break;
        }

        case 4: // limb_pitch_slider
        {
            mPitchSpnr->setValue(value);
            onYawPitchRollChanged();
            break;
        }
        case 5: // limb_yaw_slider
        {
            mYawSpnr->setValue(value);
            onYawPitchRollChanged();
            break;
        }
        case 6: // limb_roll_slider
        {
            mRollSpnr->setValue(value);
            onYawPitchRollChanged();
            break;
        }

        case 10: // Advanced_Scale_X
        {
            mScaleXSpnr->setValue(value);
            onScaleSet();
            break;
        }
        case 11: // Advanced_Scale_Y
        {
            mScaleYSpnr->setValue(value);
            onScaleSet();
            break;
        }
        case 12: // Advanced_Scale_Z
        {
            mScaleZSpnr->setValue(value);
            onScaleSet();
            break;
        }

        default:
            LL_WARNS("Posing") << "onCommitSlider passed invalid parameter: " << id << LL_ENDL;
            break;
    }
}

void FSFloaterPoser::onPoseMenuAction(const LLSD& param)
{
    std::string loadStyle = param.asString();
    if (loadStyle.empty())
        return;

    LLScrollListItem* item = mPosesScrollList->getFirstSelected();
    if (!item)
        return;

    std::string poseName = item->getColumn(0)->getValue().asString();

    E_LoadPoseMethods loadType = ROT_POS_AND_SCALES; // the default is to load everything
    if (loadStyle == "rotation")
        loadType = ROTATIONS;
    else if (loadStyle == "position")
        loadType = POSITIONS;
    else if (loadStyle == "scale")
        loadType = SCALES;
    else if (loadStyle == "rot_pos")
        loadType = ROTATIONS_AND_POSITIONS;
    else if (loadStyle == "rot_scale")
        loadType = ROTATIONS_AND_SCALES;
    else if (loadStyle == "pos_scale")
        loadType = POSITIONS_AND_SCALES;
    else if (loadStyle == "all")
        loadType = ROT_POS_AND_SCALES;
    else if (loadStyle == "selective")
        loadType = SELECTIVE;
    else if (loadStyle == "selective_rot")
        loadType = SELECTIVE_ROT;

    LLVOAvatar* avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    loadPoseFromXml(avatar, poseName, loadType);
    onJointTabSelect();
    refreshJointScrollListMembers();
    setSavePosesButtonText(!mPoserAnimator.allBaseRotationsAreZero(avatar));
}

void FSFloaterPoser::onClickLoadLeftHandPose()
{
    onClickLoadHandPose(false);
}

void FSFloaterPoser::onClickLoadRightHandPose()
{
    onClickLoadHandPose(true);
}

void FSFloaterPoser::onClickLoadHandPose(bool isRightHand)
{
    LLScrollListItem* item = mHandPresetsScrollList->getFirstSelected();
    if (!item)
        return;

    std::string poseName = item->getColumn(0)->getValue().asString();
    std::string pathname = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, POSE_SAVE_SUBDIRECTORY, std::string(POSE_PRESETS_HANDS_SUBDIRECTORY));
    if (!gDirUtilp->fileExists(pathname))
        return;

    std::string fullPath =
        gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, POSE_SAVE_SUBDIRECTORY, std::string(POSE_PRESETS_HANDS_SUBDIRECTORY), poseName + POSE_INTERNAL_FORMAT_FILE_EXT);

    LLVOAvatar* avatar   = getUiSelectedAvatar();
    if (!avatar)
        return;
    if (!mPoserAnimator.isPosingAvatar(avatar))
        return;

    try
    {
        LLSD       pose;
        llifstream infile;
        LLVector3  vec3;

        infile.open(fullPath);
        if (!infile.is_open())
            return;

        while (!infile.eof())
        {
            S32 lineCount = LLSDSerialize::fromXML(pose, infile);
            if (lineCount == LLSDParser::PARSE_FAILURE)
            {
                LL_WARNS("Posing") << "Failed to parse loading a file for a hand: " << poseName << LL_ENDL;
                return;
            }

            for (LLSD::map_const_iterator itr = pose.beginMap(); itr != pose.endMap(); ++itr)
            {
                std::string const& name        = itr->first;
                LLSD const&        control_map = itr->second;

                if (name.find("Hand") == std::string::npos)
                    continue;
                if (isRightHand != (name.find("Left") == std::string::npos))
                    continue;

                const FSPoserAnimator::FSPoserJoint* poserJoint = mPoserAnimator.getPoserJointByName(name);
                if (!poserJoint)
                    continue;

                vec3.setValue(control_map["rotation"]);
                mPoserAnimator.loadJointRotation(avatar, poserJoint, true, vec3);
            }
        }
    }
    catch ( const std::exception& e )
    {
        LL_WARNS("Posing") << "Threw an exception trying to load a hand pose: " << poseName << " exception: " << e.what() << LL_ENDL;
    }
}

bool FSFloaterPoser::poseFileStartsFromTeePose(const std::string& poseFileName)
{
    std::string pathname = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, POSE_SAVE_SUBDIRECTORY);
    if (!gDirUtilp->fileExists(pathname))
        return false;

    std::string fullPath =
        gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, POSE_SAVE_SUBDIRECTORY, poseFileName + POSE_INTERNAL_FORMAT_FILE_EXT);

    try
    {
        LLSD       pose;
        llifstream infile;
        bool       startFromZeroRot = false;

        infile.open(fullPath);
        if (!infile.is_open())
            return false;

        S32 lineCount = LLSDSerialize::fromXML(pose, infile);
        if (lineCount == LLSDParser::PARSE_FAILURE)
            return startFromZeroRot;

        for (LLSD::map_const_iterator itr = pose.beginMap(); itr != pose.endMap(); ++itr)
        {
            std::string const& name        = itr->first;
            LLSD const&        control_map = itr->second;

            if (name == "startFromTeePose")
                startFromZeroRot = control_map["value"].asBoolean();
        }

        return startFromZeroRot;
    }
    catch (const std::exception& e)
    {
        LL_WARNS("Posing") << "Unable to load or parse the pose: " << poseFileName << " exception: " << e.what() << LL_ENDL;
    }

    return false;
}

void FSFloaterPoser::loadPoseFromXml(LLVOAvatar* avatar, const std::string& poseFileName, E_LoadPoseMethods loadMethod)
{
    std::string pathname = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, POSE_SAVE_SUBDIRECTORY);
    if (!gDirUtilp->fileExists(pathname))
        return;

    if (!mPoserAnimator.isPosingAvatar(avatar))
        return;

    std::string fullPath =
        gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, POSE_SAVE_SUBDIRECTORY, poseFileName + POSE_INTERNAL_FORMAT_FILE_EXT);

    bool loadRotations = loadMethod == ROTATIONS || loadMethod == ROTATIONS_AND_POSITIONS || loadMethod == ROTATIONS_AND_SCALES ||
                         loadMethod == ROT_POS_AND_SCALES || loadMethod == SELECTIVE || loadMethod == SELECTIVE_ROT;
    bool loadPositions = loadMethod == POSITIONS || loadMethod == ROTATIONS_AND_POSITIONS || loadMethod == POSITIONS_AND_SCALES ||
                         loadMethod == ROT_POS_AND_SCALES || loadMethod == SELECTIVE;
    bool loadScales    = loadMethod == SCALES || loadMethod == POSITIONS_AND_SCALES || loadMethod == ROTATIONS_AND_SCALES ||
                         loadMethod == ROT_POS_AND_SCALES || loadMethod == SELECTIVE;
    bool loadSelective = loadMethod == SELECTIVE || loadMethod == SELECTIVE_ROT;

    try
    {
        LLSD         pose;
        llifstream   infile;
        LLVector3    vec3;
        LLQuaternion quat;
        bool         enabled;
        bool         setJointBaseRotationToZero;
        S32          version = 0;
        bool startFromZeroRot = true;

        infile.open(fullPath);
        if (!infile.is_open())
            return;

        while (!infile.eof())
        {
            S32 lineCount = LLSDSerialize::fromXML(pose, infile);
            if (lineCount == LLSDParser::PARSE_FAILURE)
            {
                LL_WARNS("Posing") << "Failed to parse file: " << poseFileName << LL_ENDL;
                return;
            }

            for (LLSD::map_const_iterator itr = pose.beginMap(); itr != pose.endMap(); ++itr)
            {
                std::string const& name        = itr->first;
                LLSD const&        control_map = itr->second;

                if (name == "startFromTeePose")
                    startFromZeroRot = control_map["value"].asBoolean();

                if (name == "version")
                    version = (S32)control_map["value"].asInteger();
            }

            if (version > 5 && startFromZeroRot)
                mPoserAnimator.setAllAvatarStartingRotationsToZero(avatar);

            bool loadPositionsAndScalesAsDeltas = false;
            if (version > 3)
                loadPositionsAndScalesAsDeltas = true;

            for (LLSD::map_const_iterator itr = pose.beginMap(); itr != pose.endMap(); ++itr)
            {
                std::string const& name        = itr->first;
                LLSD const&        control_map = itr->second;

                const FSPoserAnimator::FSPoserJoint *poserJoint = mPoserAnimator.getPoserJointByName(name);
                if (!poserJoint)
                    continue;

                if (loadSelective && mPoserAnimator.isPosingAvatarJoint(avatar, *poserJoint))
                    continue;

                if (control_map.has("enabled"))
                {
                    enabled = control_map["enabled"].asBoolean();
                    mPoserAnimator.setPosingAvatarJoint(avatar, *poserJoint, enabled || loadSelective);
                }

                if (control_map.has("jointBaseRotationIsZero"))
                    setJointBaseRotationToZero = control_map["jointBaseRotationIsZero"].asBoolean();
                else
                    setJointBaseRotationToZero = startFromZeroRot;

                if (loadPositions && control_map.has("position"))
                {
                    vec3.setValue(control_map["position"]);
                    mPoserAnimator.loadJointPosition(avatar, poserJoint, loadPositionsAndScalesAsDeltas, vec3);
                }

                if (loadRotations && control_map.has("rotation"))
                {
                    vec3.setValue(control_map["rotation"]);
                    mPoserAnimator.loadJointRotation(avatar, poserJoint, setJointBaseRotationToZero, vec3);
                }

                if (loadScales && control_map.has("scale"))
                {
                    vec3.setValue(control_map["scale"]);
                    mPoserAnimator.loadJointScale(avatar, poserJoint, loadPositionsAndScalesAsDeltas, vec3);
                }
            }
        }
    }
    catch ( const std::exception & e )
    {
        LL_WARNS("Posing") << "Everything caught fire trying to load the pose: " << poseFileName << " exception: " << e.what() << LL_ENDL;
    }
}

void FSFloaterPoser::startPosingSelf()
{
    if (!gAgentAvatarp || gAgentAvatarp.isNull())
        return;

    setUiSelectedAvatar(gAgentAvatarp->getID());
    LLVOAvatar* avatar = getAvatarByUuid(gAgentAvatarp->getID());
    if (!avatar)
        return;

    bool arePosingSelected = mPoserAnimator.isPosingAvatar(avatar);
    if (!arePosingSelected && couldAnimateAvatar(avatar))
        mPoserAnimator.tryPosingAvatar(avatar);

    onAvatarSelect();
}

void FSFloaterPoser::stopPosingAllAvatars()
{
    if (!gAgentAvatarp || gAgentAvatarp.isNull() || !mAvatarSelectionScrollList)
        return;

    for (auto listItem : mAvatarSelectionScrollList->getAllData())
    {
        LLScrollListCell* cell = listItem->getColumn(COL_UUID);
        if (!cell)
            continue;

        LLUUID      selectedAvatarId = cell->getValue().asUUID();
        LLVOAvatar* listAvatar       = getAvatarByUuid(selectedAvatarId);

        if (mPoserAnimator.isPosingAvatar(listAvatar))
            mPoserAnimator.stopPosingAvatar(listAvatar);
    }

    onAvatarSelect();
}

void FSFloaterPoser::onPoseStartStop()
{
    LLVOAvatar* avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    bool arePosingSelected = mPoserAnimator.isPosingAvatar(avatar);
    if (arePosingSelected)
    {
        mPoserAnimator.stopPosingAvatar(avatar);
    }
    else
    {
        if (!couldAnimateAvatar(avatar))
            return;

        if (!havePermissionToAnimateAvatar(avatar))
            return;

        mPoserAnimator.tryPosingAvatar(avatar);
    }

    onAvatarsRefresh();
    onAvatarSelect();
}

bool FSFloaterPoser::couldAnimateAvatar(LLVOAvatar *avatar) const
{
    if (!avatar || avatar->isDead())
        return false;
    if (avatar->getRegion() != gAgent.getRegion())
        return false;

    return true;
}

bool FSFloaterPoser::havePermissionToAnimateAvatar(LLVOAvatar *avatar) const
{
    if (!avatar || avatar->isDead())
        return false;
    if (avatar->isSelf())
        return true;
    if (avatar->isControlAvatar())
        return true;

    return false;
}

void FSFloaterPoser::poseControlsEnable(bool enable)
{
    mTrackballPnl->setEnabled(enable);
    mFlipPoseBtn->setEnabled(enable);
    mFlipJointBtn->setEnabled(enable);
    mRecaptureBtn->setEnabled(enable);
    mTogglePosingBonesBtn->setEnabled(enable);
    mLoadPosesBtn->setEnabled(enable);
    mSavePosesBtn->setEnabled(enable);
    mPoseSaveNameEditor->setEnabled(enable);
}

void FSFloaterPoser::refreshJointScrollListMembers()
{
    mEntireAvJointScroll->clearRows();
    mBodyJointsScrollList->clearRows();
    mFaceJointsScrollList->clearRows();
    mHandJointsScrollList->clearRows();
    mMiscJointsScrollList->clearRows();
    mCollisionVolumesScrollList->clearRows();

    std::vector<FSPoserAnimator::FSPoserJoint>::const_iterator poserJoint_iter;
    for (poserJoint_iter = mPoserAnimator.PoserJoints.begin();
         poserJoint_iter != mPoserAnimator.PoserJoints.end();
         ++poserJoint_iter)
    {
        LLSD row = createRowForJoint(poserJoint_iter->jointName(), false);
        if (!row)
            continue;

        LLScrollListItem *item = nullptr;
        bool hasListHeader = hasString(XML_LIST_HEADER_STRING_PREFIX + poserJoint_iter->jointName());

        switch (poserJoint_iter->boneType())
        {
            case WHOLEAVATAR:
                item = mEntireAvJointScroll->addElement(row);
                mEntireAvJointScroll->selectFirstItem();
                break;

            case BODY:
                if (hasListHeader)
                    addHeaderRowToScrollList(poserJoint_iter->jointName(), mBodyJointsScrollList);

                item = mBodyJointsScrollList->addElement(row);
                break;

            case FACE:
                if (hasListHeader)
                    addHeaderRowToScrollList(poserJoint_iter->jointName(), mFaceJointsScrollList);

                item = mFaceJointsScrollList->addElement(row);
                break;

            case HANDS:
                if (hasListHeader)
                    addHeaderRowToScrollList(poserJoint_iter->jointName(), mHandJointsScrollList);

                item = mHandJointsScrollList->addElement(row);
                break;

            case MISC:
                if (hasListHeader)
                    addHeaderRowToScrollList(poserJoint_iter->jointName(), mMiscJointsScrollList);

                item = mMiscJointsScrollList->addElement(row);
                break;

            case COL_VOLUMES:
                if (hasListHeader)
                    addHeaderRowToScrollList(poserJoint_iter->jointName(), mCollisionVolumesScrollList);

                item = mCollisionVolumesScrollList->addElement(row);
                break;
        }

        if (item)
            item->setUserdata((void*) &*poserJoint_iter);
    }

    refreshTextHighlightingOnJointScrollLists();
}

void FSFloaterPoser::addHeaderRowToScrollList(const std::string& jointName, LLScrollListCtrl* bodyJointsScrollList)
{
    LLSD headerRow = createRowForJoint(jointName, true);
    if (!headerRow)
        return;

    LLScrollListItem *hdrRow = bodyJointsScrollList->addElement(headerRow);
    hdrRow->setEnabled(FALSE);
}

LLSD FSFloaterPoser::createRowForJoint(const std::string& jointName, bool isHeaderRow)
{
    if (jointName.empty())
        return NULL;

    std::string headerValue = "";
    if (isHeaderRow && hasString("icon_category"))
        headerValue = getString("icon_category");

    std::string jointValue    = jointName;
    std::string parameterName = (isHeaderRow ? XML_LIST_HEADER_STRING_PREFIX : XML_LIST_TITLE_STRING_PREFIX) + jointName;
    if (hasString(parameterName))
        jointValue = getString(parameterName);
    else
        return NULL;

    LLSD row;
    row["columns"][COL_ICON]["column"] = "icon";
    row["columns"][COL_ICON]["type"]   = "icon";
    row["columns"][COL_ICON]["value"]  = headerValue;
    row["columns"][COL_NAME]["column"] = "joint";
    row["columns"][COL_NAME]["value"]  = jointValue;

    return row;
}

void FSFloaterPoser::onToggleLoadSavePanel()
{
    if (isMinimized())
        return;

    // Get the load/save button toggle state, find the load/save panel, and set its visibility
    bool loadSavePanelExpanded = mToggleLoadSavePanelBtn->getValue().asBoolean();
    mPosesLoadSavePnl->setVisible(loadSavePanelExpanded);
    mLoadPosesBtn->setVisible(loadSavePanelExpanded);
    mSavePosesBtn->setVisible(loadSavePanelExpanded);
    mBrowserFolderBtn->setVisible(loadSavePanelExpanded);

    // change the width of the Poser panel for the (dis)appearance of the load/save panel
    S32 currentWidth       = getRect().getWidth();
    S32 loadSavePanelWidth = mPosesLoadSavePnl->getRect().getWidth();

    S32 poserFloaterHeight = getRect().getHeight();
    S32 poserFloaterWidth  = loadSavePanelExpanded ? currentWidth + loadSavePanelWidth : currentWidth - loadSavePanelWidth;

    if (poserFloaterWidth < 0)
        return;

    reshape(poserFloaterWidth, poserFloaterHeight);

    if (loadSavePanelExpanded)
        refreshPoseScroll(mPosesScrollList);
}

void FSFloaterPoser::onToggleMirrorChange()
{
    setRotationChangeButtons(true, false);
}

void FSFloaterPoser::onToggleSympatheticChange()
{
    setRotationChangeButtons(false, true);
}

void FSFloaterPoser::setRotationChangeButtons(bool togglingMirror, bool togglingSympathetic)
{
    if (togglingSympathetic)
        mToggleMirrorRotationBtn->setValue(false);

    if (togglingMirror)
        mToggleSympatheticRotationBtn->setValue(false);

    refreshTrackpadCursor();
}

void FSFloaterPoser::onUndoLastChange()
{
    LLVOAvatar* avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    if (!mPoserAnimator.isPosingAvatar(avatar))
        return;

    auto selectedJoints = getUiSelectedPoserJoints();
    if (selectedJoints.size() < 1)
        return;

    for (auto item : selectedJoints)
    {
        bool currentlyPosing = mPoserAnimator.isPosingAvatarJoint(avatar, *item);
        if (currentlyPosing)
            mPoserAnimator.undoLastJointChange(avatar, *item, getUiSelectedBoneDeflectionStyle());
    }

    enableOrDisableRedoAndUndoButton();
    refreshRotationSlidersAndSpinners();
    refreshPositionSlidersAndSpinners();
    refreshScaleSlidersAndSpinners();
    refreshTrackpadCursor();
}

void FSFloaterPoser::onSetAvatarToTpose()
{
    LLVOAvatar* avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    setSavePosesButtonText(false);
    mPoserAnimator.setAllAvatarStartingRotationsToZero(avatar);
    refreshTextHighlightingOnJointScrollLists();
    enableOrDisableRedoAndUndoButton();
}

void FSFloaterPoser::onResetJoint(const LLSD data)
{
    int resetType = data.asInteger();

    LLVOAvatar* avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    if (!mPoserAnimator.isPosingAvatar(avatar))
        return;

    auto selectedJoints = getUiSelectedPoserJoints();
    if (selectedJoints.size() < 1)
        return;

    for (auto item : selectedJoints)
    {
        bool currentlyPosing = mPoserAnimator.isPosingAvatarJoint(avatar, *item);
        if (!currentlyPosing)
            continue;

        mPoserAnimator.resetJoint(avatar, *item, getUiSelectedBoneDeflectionStyle());
    }

    refreshRotationSlidersAndSpinners();
    refreshPositionSlidersAndSpinners();
    refreshScaleSlidersAndSpinners();
    refreshTrackpadCursor();
    enableOrDisableRedoAndUndoButton();
}

void FSFloaterPoser::onRedoLastChange()
{
    LLVOAvatar* avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    if (!mPoserAnimator.isPosingAvatar(avatar))
        return;

    auto selectedJoints = getUiSelectedPoserJoints();
    if (selectedJoints.size() < 1)
        return;

    for (auto item : selectedJoints)
    {
        bool currentlyPosing = mPoserAnimator.isPosingAvatarJoint(avatar, *item);
        if (currentlyPosing)
            mPoserAnimator.redoLastJointChange(avatar, *item, getUiSelectedBoneDeflectionStyle());
    }

    enableOrDisableRedoAndUndoButton();
    refreshRotationSlidersAndSpinners();
    refreshTrackpadCursor();
    refreshScaleSlidersAndSpinners();
    refreshPositionSlidersAndSpinners();
}

void FSFloaterPoser::enableOrDisableRedoAndUndoButton()
{
    mRedoChangeBtn->setEnabled(false);
    mUndoChangeBtn->setEnabled(false);

    LLVOAvatar* avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    if (!mPoserAnimator.isPosingAvatar(avatar))
        return;

    auto selectedJoints = getUiSelectedPoserJoints();
    if (selectedJoints.size() < 1)
        return;

    bool shouldEnableRedoButton = false;
    bool shouldEnableUndoButton = false;

    for (auto item : selectedJoints)
    {
        bool currentlyPosing = mPoserAnimator.isPosingAvatarJoint(avatar, *item);
        if (!currentlyPosing)
            continue;

        shouldEnableRedoButton |= mPoserAnimator.canRedoOrUndoJointChange(avatar, *item);
        shouldEnableUndoButton |= mPoserAnimator.canRedoOrUndoJointChange(avatar, *item, true);
    }

    mRedoChangeBtn->setEnabled(shouldEnableRedoButton);
    mUndoChangeBtn->setEnabled(shouldEnableUndoButton);
}

void FSFloaterPoser::onToggleVisualManipulators()
{
    bool tools_enabled = mToggleVisualManipulators->getValue().asBoolean();

    if (tools_enabled)
    {
        enableVisualManipulators();
    }
    else
    {
        disableVisualManipulators();
    }
}

void FSFloaterPoser::selectJointByName(const std::string& jointName)
{
    LLTabContainer* tabContainer = mJointsTabs;
    std::vector<LLPanel*> panels = {
        mPositionRotationPnl,
        mBodyJointsPnl,
        mFaceJointsPnl,
        mHandsTabs,
        mMiscJointsPnl,
        mCollisionVolumesPnl
    };
    
    std::vector<LLScrollListCtrl*> scrollLists = {
        mEntireAvJointScroll,
        mBodyJointsScrollList,
        mFaceJointsScrollList,
        mHandJointsScrollList,
        mMiscJointsScrollList,
        mCollisionVolumesScrollList
    };

    bool found = false;
    for (S32 i = 0; i < tabContainer->getTabCount(); ++i)
    {
        LLPanel* panel = tabContainer->getPanelByIndex(i);
        tabContainer->selectTabPanel(panel);

        // Special handling for Hands tab
        if (panel == mHandsTabs)
        {
            mHandsTabs->selectTabPanel(mHandsJointsPnl);
        }

        for (auto scrollList : scrollLists)
        {
            scrollList->deselectAllItems();
        }

        auto scrollList = getScrollListForTab(panel);

        std::vector<LLScrollListItem*> items = scrollList->getAllData();
        for (auto item : items)
        {
            auto* userData = static_cast<FSPoserAnimator::FSPoserJoint*>(item->getUserdata());
            if (userData && userData->jointName() == jointName)
            {
                tabContainer->selectTab(i);
                scrollList->selectNthItem(scrollList->getItemIndex(item));
                scrollList->scrollToShowSelected();
                getUiSelectedPoserJoints();
                return; // Exit the loop once we've found and selected the joint
            }
        }
    }
    LL_WARNS() << "Joint not found: " << jointName << LL_ENDL;
}

LLScrollListCtrl* FSFloaterPoser::getScrollListForTab(LLPanel * tabPanel) const
{
    if (tabPanel == mPositionRotationPnl)
    {
        return mEntireAvJointScroll;
    }
    else if (tabPanel == mBodyJointsPnl)
    {
        return mBodyJointsScrollList;
    }
    else if (tabPanel == mFaceJointsPnl)
    {
        return mFaceJointsScrollList;
    }
    else if (tabPanel == mHandsTabs)
    {
        return mHandJointsScrollList;
    }
    else if (tabPanel == mMiscJointsPnl)
    {
        return mMiscJointsScrollList;
    }
    else if (tabPanel == mCollisionVolumesPnl)
    {
        return mCollisionVolumesScrollList;
    }

    LL_WARNS() << "Unknown tab panel: " << tabPanel << LL_ENDL;
    return nullptr;
}

std::vector<FSPoserAnimator::FSPoserJoint*> FSFloaterPoser::getUiSelectedPoserJoints() const
{
    std::vector<FSPoserAnimator::FSPoserJoint*> joints;

    auto activeTab = mJointsTabs->getCurrentPanel();
    if (!activeTab)
    {
        return joints;
    }

    LLScrollListCtrl* scrollList{ nullptr };
    
    scrollList = getScrollListForTab(activeTab);
    if (activeTab == mPositionRotationPnl)
    {
        mEntireAvJointScroll->selectFirstItem();
    }
    else if (activeTab == mHandsTabs )
    {
        auto activeHandsSubTab = mHandsTabs->getCurrentPanel();
        if (!activeHandsSubTab)
        {
            return joints;
        }
    }
    if (!scrollList)
    {
        return joints;
    }

    for (auto item : scrollList->getAllSelected())
    {
        auto* userData = static_cast<FSPoserAnimator::FSPoserJoint*>(item->getUserdata());
        if (userData)
        {
            joints.push_back(userData);
        }
    }

    updateManipWithFirstSelectedJoint(joints);

    return joints;
}

void FSFloaterPoser::updateManipWithFirstSelectedJoint(std::vector<FSPoserAnimator::FSPoserJoint*> joints) const
{
    auto avatarp = getUiSelectedAvatar();
    if (!avatarp)
        return;

    if (joints.size() >= 1)
        FSToolCompPose::getInstance()->setJoint(avatarp->getJoint(joints[0]->jointName()));
    else
        FSToolCompPose::getInstance()->setJoint(nullptr);
}

E_RotationStyle FSFloaterPoser::getUiSelectedBoneRotationStyle(const std::string& jointName) const
{
    if (jointName.empty())
        return ABSOLUTE_ROT;

    bool hasRotationStylePreferenceParameter = hasString(XML_JOINT_DELTAROT_STRING_PREFIX + jointName);
    if (!hasRotationStylePreferenceParameter)
        return ABSOLUTE_ROT;
 
    std::string paramValue = getString(XML_JOINT_DELTAROT_STRING_PREFIX + jointName);
    if (paramValue == "true")
        return DELTAIC_ROT;

    return ABSOLUTE_ROT;
}

E_BoneDeflectionStyles FSFloaterPoser::getUiSelectedBoneDeflectionStyle() const
{
    bool isDelta = mToggleDeltaModeBtn->getValue().asBoolean();

    if (mToggleMirrorRotationBtn->getValue().asBoolean())
        return isDelta ? MIRROR_DELTA : MIRROR;

    if (mToggleSympatheticRotationBtn->getValue().asBoolean())
        return isDelta ? SYMPATHETIC_DELTA : SYMPATHETIC;

    return isDelta ? DELTAMODE : NONE;
}

LLVOAvatar* FSFloaterPoser::getUiSelectedAvatar() const
{
    LLScrollListItem* item = mAvatarSelectionScrollList->getFirstSelected();
    if (!item)
        return nullptr;

    LLScrollListCell* cell = item->getColumn(COL_UUID);
    if (!cell)
        return nullptr;

    LLUUID selectedAvatarId = cell->getValue().asUUID();

    return getAvatarByUuid(selectedAvatarId);
}

void FSFloaterPoser::setUiSelectedAvatar(const LLUUID& avatarToSelect)
{
    for (auto listItem : mAvatarSelectionScrollList->getAllData())
    {
        LLScrollListCell* cell = listItem->getColumn(COL_UUID);
        if (!cell)
            continue;

        LLUUID avatarId = cell->getValue().asUUID();
        if (avatarId != avatarToSelect)
            continue;

        listItem->setSelected(true);
        break;
    }
}

void FSFloaterPoser::setPoseSaveFileTextBoxToUiSelectedAvatarSaveFileName()
{
    LLScrollListItem* item = mAvatarSelectionScrollList->getFirstSelected();
    if (!item)
        return;

    LLScrollListCell* cell = item->getColumn(COL_SAVE);
    if (!cell)
        return;

    std::string lastSetName = cell->getValue().asString();
    if (lastSetName.empty())
        return;

    LLStringExplicit name = LLStringExplicit(lastSetName);
    mPoseSaveNameEditor->setText(name);
}

void FSFloaterPoser::setUiSelectedAvatarSaveFileName(const std::string& saveFileName)
{
    LLScrollListItem* item = mAvatarSelectionScrollList->getFirstSelected();
    if (!item)
        return;

    LLScrollListCell* cell = item->getColumn(COL_SAVE);
    if (!cell)
        return;

    return cell->setValue(saveFileName);
}

LLVOAvatar* FSFloaterPoser::getAvatarByUuid(const LLUUID& avatarToFind) const
{
    for (LLCharacter* character : LLCharacter::sInstances)
    {
        if (avatarToFind != character->getID())
            continue;

        LLVOAvatar* avatar = dynamic_cast<LLVOAvatar*>(character);
        return avatar;
    }

    return nullptr;
}

void FSFloaterPoser::onPositionSet()
{
    F32 posX = (F32)mAdvPosXSpnr->getValue().asReal();
    F32 posY = (F32)mAdvPosYSpnr->getValue().asReal();
    F32 posZ = (F32)mAdvPosZSpnr->getValue().asReal();
    
    mInOutSpnr->setValue(posX);
    mLeftRightSpnr->setValue(posY);
    mUpDownSpnr->setValue(posZ);
    mAdvPosXSlider->setValue(posX);
    mAdvPosYSlider->setValue(posY);
    mAdvPosZSlider->setValue(posZ);
    mPosXSlider->setValue(posX);
    mPosYSlider->setValue(posY);
    mPosZSlider->setValue(posZ);

    setSelectedJointsPosition(posX, posY, posZ);
    refreshPositionSlidersAndSpinners();
    enableOrDisableRedoAndUndoButton();
}

void FSFloaterPoser::onScaleSet()
{
    F32 scX = (F32)mScaleXSpnr->getValue().asReal();
    F32 scY = (F32)mScaleYSpnr->getValue().asReal();
    F32 scZ = (F32)mScaleZSpnr->getValue().asReal();

    mAdvScaleXSlider->setValue(scX);
    mAdvScaleYSlider->setValue(scY);
    mAdvScaleZSlider->setValue(scZ);

    setSelectedJointsScale(scX, scY, scZ);
    refreshScaleSlidersAndSpinners();
    enableOrDisableRedoAndUndoButton();
}

void FSFloaterPoser::onTrackballChanged()
{
    LLVector3 trackPadDeltaPos;
    LLSD      deltaPosition = mAvatarTrackball->getValueDelta();

    if (deltaPosition.isArray() && deltaPosition.size() == 3)
        trackPadDeltaPos.setValue(deltaPosition);
    else
        return;

    F32 trackPadSensitivity = llmax(gSavedSettings.getF32(POSER_TRACKPAD_SENSITIVITY_SAVE_KEY), 0.0001f);

    trackPadDeltaPos[VX] *= NormalTrackpadRangeInRads * trackPadSensitivity * RAD_TO_DEG;
    trackPadDeltaPos[VY] *= NormalTrackpadRangeInRads * trackPadSensitivity * RAD_TO_DEG;
    trackPadDeltaPos[VZ] *= NormalTrackpadRangeInRads * RAD_TO_DEG;

    F32 axis1 = clipRange((F32)mYawSpnr->getValue().asReal() + trackPadDeltaPos[VX]);
    F32 axis2 = (F32)mPitchSpnr->getValue().asReal() + trackPadDeltaPos[VY];
    F32 axis3 = (F32)mRollSpnr->getValue().asReal() + trackPadDeltaPos[VZ];

    mYawSpnr->setValue(axis1);
    mPitchSpnr->setValue(axis2);
    mRollSpnr->setValue(axis3);

    onYawPitchRollChanged(true);
}

F32 FSFloaterPoser::clipRange(F32 value)
{
    F32 result = fmodf(value, 3600.f); // to avoid time consuming while loops
    while (result > 180.f)
        result -= 360.f;
    while (result < -180.f)
        result += 360.f;

    return result;
}

void FSFloaterPoser::onYawPitchRollChanged(bool skipUpdateTrackpad)
{
    LLVector3 absoluteRotation, deltaRotation;
    absoluteRotation.mV[VX] = (F32)mYawSpnr->getValue().asReal();
    absoluteRotation.mV[VY] = (F32)mPitchSpnr->getValue().asReal();
    absoluteRotation.mV[VZ] = (F32)mRollSpnr->getValue().asReal();

    mAdvRotXSlider->setValue(absoluteRotation.mV[VY]);
    mAdvRotYSlider->setValue(absoluteRotation.mV[VX]);
    mAdvRotZSlider->setValue(absoluteRotation.mV[VZ]);

    absoluteRotation *= DEG_TO_RAD;
    deltaRotation = absoluteRotation - mLastSliderRotation;
    mLastSliderRotation = absoluteRotation;

    setSelectedJointsRotation(absoluteRotation, deltaRotation);
    enableOrDisableRedoAndUndoButton();

    if (!skipUpdateTrackpad)
        refreshTrackpadCursor();
}

void FSFloaterPoser::onAdjustTrackpadSensitivity()
{
    refreshTrackpadCursor();
}

void FSFloaterPoser::refreshTrackpadCursor()
{
    F32 trackPadSensitivity = llmax(gSavedSettings.getF32(POSER_TRACKPAD_SENSITIVITY_SAVE_KEY), 0.0001f);
    F32 axis1 = (F32)mYawSpnr->getValue().asReal() * DEG_TO_RAD / NormalTrackpadRangeInRads / trackPadSensitivity;
    F32 axis2 = (F32)mPitchSpnr->getValue().asReal() * DEG_TO_RAD / NormalTrackpadRangeInRads / trackPadSensitivity;
    F32 axis3 = (F32)mRollSpnr->getValue().asReal() * DEG_TO_RAD / NormalTrackpadRangeInRads;

    mAvatarTrackball->setValue(axis1, axis2, axis3);
}

void FSFloaterPoser::refreshRotationSlidersAndSpinners()
{
    LLVector3 rotation = getRotationOfFirstSelectedJoint();

    mLastSliderRotation = rotation;
    mYawSpnr->setValue(rotation.mV[VX] *= RAD_TO_DEG);
    mPitchSpnr->setValue(rotation.mV[VY] *= RAD_TO_DEG);
    mRollSpnr->setValue(rotation.mV[VZ] *= RAD_TO_DEG);
    mAdvRotXSlider->setValue(rotation.mV[VY]);
    mAdvRotYSlider->setValue(rotation.mV[VX]);
    mAdvRotZSlider->setValue(rotation.mV[VZ]);
}

void FSFloaterPoser::refreshPositionSlidersAndSpinners()
{
    LLVector3 position = getPositionOfFirstSelectedJoint();

    mAdvPosXSlider->setValue(position.mV[VX]);
    mAdvPosXSpnr->setValue(position.mV[VX]);
    mAdvPosYSlider->setValue(position.mV[VY]);
    mAdvPosYSpnr->setValue(position.mV[VY]);
    mAdvPosZSlider->setValue(position.mV[VZ]);
    mAdvPosZSpnr->setValue(position.mV[VZ]);

    auto activeTab = mJointsTabs->getCurrentPanel();
    if (!activeTab)
        return;

    if (activeTab != mPositionRotationPnl)
        return; // if the active tab isn't the av position one, don't set anything.

    mPosXSlider->setValue(position.mV[VX]);
    mInOutSpnr->setValue(position.mV[VX]);
    mPosYSlider->setValue(position.mV[VY]);
    mLeftRightSpnr->setValue(position.mV[VY]);
    mPosZSlider->setValue(position.mV[VZ]);
    mUpDownSpnr->setValue(position.mV[VZ]);
}

void FSFloaterPoser::refreshScaleSlidersAndSpinners()
{
    LLVector3 rotation = getScaleOfFirstSelectedJoint();

    mAdvScaleXSlider->setValue(rotation.mV[VX]);
    mScaleXSpnr->setValue(rotation.mV[VX]);
    mAdvScaleYSlider->setValue(rotation.mV[VY]);
    mScaleYSpnr->setValue(rotation.mV[VY]);
    mAdvScaleZSlider->setValue(rotation.mV[VZ]);
    mScaleZSpnr->setValue(rotation.mV[VZ]);
}

void FSFloaterPoser::setSelectedJointsPosition(F32 x, F32 y, F32 z)
{
    LLVOAvatar *avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    if (!mPoserAnimator.isPosingAvatar(avatar))
        return;

    LLVector3 vec3 = LLVector3(x, y, z);
    E_BoneDeflectionStyles defl = getUiSelectedBoneDeflectionStyle();

    for (auto item : getUiSelectedPoserJoints())
    {
        bool currentlyPosingJoint = mPoserAnimator.isPosingAvatarJoint(avatar, *item);
        if (!currentlyPosingJoint)
            continue;

        mPoserAnimator.setJointPosition(avatar, item, vec3, defl);
    }
}

void FSFloaterPoser::setSelectedJointsRotation(const LLVector3& absoluteRot, const LLVector3& deltaRot)
{
    LLVOAvatar *avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    if (!mPoserAnimator.isPosingAvatar(avatar))
        return;

    auto                   selectedJoints   = getUiSelectedPoserJoints();
    bool                   savingToExternal = getWhetherToResetBaseRotationOnEdit();
    E_BoneDeflectionStyles defl             = getUiSelectedBoneDeflectionStyle();

    for (auto item : selectedJoints)
    {
        bool currentlyPosingJoint = mPoserAnimator.isPosingAvatarJoint(avatar, *item);
        if (!currentlyPosingJoint)
            continue;

        auto oppositeJoint = mPoserAnimator.getPoserJointByName(item->mirrorJointName());
        if (oppositeJoint)
        {
            bool oppositeJointAlsoSelectedOnUi =
                std::find(selectedJoints.begin(), selectedJoints.end(), oppositeJoint) != selectedJoints.end();

            bool deflectionDoesOppositeLimbs = !(defl == NONE || defl == DELTAMODE);
            if (oppositeJointAlsoSelectedOnUi && deflectionDoesOppositeLimbs && item->dontFlipOnMirror())
                continue;
        }

        mPoserAnimator.setJointRotation(avatar, item, absoluteRot, deltaRot, defl,
                                        getJointTranslation(item->jointName()), getJointNegation(item->jointName()), savingToExternal,
                                        getUiSelectedBoneRotationStyle(item->jointName()));
    }

    if (savingToExternal)
        refreshTextHighlightingOnJointScrollLists();
}

void FSFloaterPoser::setSelectedJointsScale(F32 x, F32 y, F32 z)
{
    LLVOAvatar *avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    if (!mPoserAnimator.isPosingAvatar(avatar))
        return;

    LLVector3              vec3 = LLVector3(x, y, z);
    E_BoneDeflectionStyles defl = getUiSelectedBoneDeflectionStyle();

    for (auto item : getUiSelectedPoserJoints())
    {
        bool currentlyPosingJoint = mPoserAnimator.isPosingAvatarJoint(avatar, *item);
        if (!currentlyPosingJoint)
            continue;

        mPoserAnimator.setJointScale(avatar, item, vec3, defl);
    }
}

LLVector3 FSFloaterPoser::getRotationOfFirstSelectedJoint() const
{
    LLVector3 rotation;
    auto selectedJoints = getUiSelectedPoserJoints();
    if (selectedJoints.size() < 1)
        return rotation;

    LLVOAvatar *avatar = getUiSelectedAvatar();
    if (!avatar)
        return rotation;

    if (!mPoserAnimator.isPosingAvatar(avatar))
        return rotation;

    rotation = mPoserAnimator.getJointRotation(avatar, *selectedJoints.front(), getJointTranslation(selectedJoints.front()->jointName()),
                                               getJointNegation(selectedJoints.front()->jointName()));

    return rotation;
}

LLVector3 FSFloaterPoser::getPositionOfFirstSelectedJoint() const
{
    LLVector3 position;
    auto selectedJoints = getUiSelectedPoserJoints();
    if (selectedJoints.size() < 1)
        return position;

    LLVOAvatar *avatar = getUiSelectedAvatar();
    if (!avatar)
        return position;

    if (!mPoserAnimator.isPosingAvatar(avatar))
        return position;

    position = mPoserAnimator.getJointPosition(avatar, *selectedJoints.front());
    return position;
}

LLVector3 FSFloaterPoser::getScaleOfFirstSelectedJoint() const
{
    LLVector3 scale;
    auto selectedJoints = getUiSelectedPoserJoints();
    if (selectedJoints.size() < 1)
        return scale;

    LLVOAvatar *avatar = getUiSelectedAvatar();
    if (!avatar)
        return scale;

    if (!mPoserAnimator.isPosingAvatar(avatar))
        return scale;

    scale = mPoserAnimator.getJointScale(avatar, *selectedJoints.front());
    return scale;
}

void FSFloaterPoser::onJointTabSelect()
{
    refreshPositionSlidersAndSpinners();
    refreshRotationSlidersAndSpinners(); 
    refreshTrackpadCursor();
    enableOrDisableRedoAndUndoButton();
    refreshScaleSlidersAndSpinners();
    onClickSetBaseRotZero();
}

E_BoneAxisTranslation FSFloaterPoser::getJointTranslation(const std::string& jointName) const
{
    if (jointName.empty())
        return SWAP_NOTHING;

    bool hasTransformParameter = hasString(XML_JOINT_TRANSFORM_STRING_PREFIX + jointName);
    if (!hasTransformParameter)
        return SWAP_NOTHING;

    std::string paramValue = getString(XML_JOINT_TRANSFORM_STRING_PREFIX + jointName);

    if (strstr(paramValue.c_str(), "SWAP_YAW_AND_ROLL"))
        return SWAP_YAW_AND_ROLL;
    else if (strstr(paramValue.c_str(), "SWAP_YAW_AND_PITCH"))
        return SWAP_YAW_AND_PITCH;
    else if (strstr(paramValue.c_str(), "SWAP_ROLL_AND_PITCH"))
        return SWAP_ROLL_AND_PITCH;
    else if (strstr(paramValue.c_str(), "SWAP_X2Y_Y2Z_Z2X"))
        return SWAP_X2Y_Y2Z_Z2X;
    else if (strstr(paramValue.c_str(), "SWAP_X2Z_Y2X_Z2Y"))
        return SWAP_X2Z_Y2X_Z2Y;
    else
        return SWAP_NOTHING;
}

S32 FSFloaterPoser::getJointNegation(const std::string& jointName) const
{
    S32 result = NEGATE_NOTHING;

    if (jointName.empty())
        return result;

    bool hasTransformParameter = hasString(XML_JOINT_TRANSFORM_STRING_PREFIX + jointName);
    if (!hasTransformParameter)
        return result;

    std::string paramValue = getString(XML_JOINT_TRANSFORM_STRING_PREFIX + jointName);

    if (strstr(paramValue.c_str(), "NEGATE_YAW"))
        result |= NEGATE_YAW;
    if (strstr(paramValue.c_str(), "NEGATE_PITCH"))
        result |= NEGATE_PITCH;
    if (strstr(paramValue.c_str(), "NEGATE_ROLL"))
        result |= NEGATE_ROLL;
    if (strstr(paramValue.c_str(), "NEGATE_ALL"))
        return NEGATE_ALL;

    return result;
}

/// <summary>
/// An event handler for selecting an avatar or animesh on the POSES_AVATAR_SCROLL_LIST_NAME.
/// In general this will refresh the views for joints or their proxies, and (dis/en)able elements of the view.
/// </summary>
void FSFloaterPoser::onAvatarSelect()
{
    LLVOAvatar* avatar = getUiSelectedAvatar();
    if(avatar)
    {
        FSToolCompPose::getInstance()->setAvatar(avatar);
    }
    mStartStopPosingBtn->setEnabled(couldAnimateAvatar(avatar));

    bool arePosingSelected = mPoserAnimator.isPosingAvatar(avatar);
    mStartStopPosingBtn->setValue(arePosingSelected);
    mSetToTposeButton->setEnabled(arePosingSelected);
    poseControlsEnable(arePosingSelected);
    refreshTextHighlightingOnAvatarScrollList();
    refreshTextHighlightingOnJointScrollLists();
    onJointTabSelect();
    setPoseSaveFileTextBoxToUiSelectedAvatarSaveFileName();
}

uuid_vec_t FSFloaterPoser::getNearbyAvatarsAndAnimeshes() const
{
    uuid_vec_t avatar_ids;

    for (LLCharacter* character : LLCharacter::sInstances)
    {
        LLVOAvatar* avatar = dynamic_cast<LLVOAvatar*>(character);
        if (!havePermissionToAnimateAvatar(avatar))
            continue;

        avatar_ids.emplace_back(character->getID());
    }

    return avatar_ids;
}

uuid_vec_t FSFloaterPoser::getCurrentlyListedAvatarsAndAnimeshes() const
{
    uuid_vec_t avatar_ids;

    for (auto listItem : mAvatarSelectionScrollList->getAllData())
    {
        LLScrollListCell* cell = listItem->getColumn(COL_UUID);
        if (!cell)
            continue;

        LLUUID avatarId = cell->getValue().asUUID();
        avatar_ids.emplace_back(avatarId);
    }

    return avatar_ids;
}

S32 FSFloaterPoser::getAvatarListIndexForUuid(const LLUUID& toFind) const
{
    S32 result = -1;
    for (auto listItem : mAvatarSelectionScrollList->getAllData())
    {
        result++;

        LLScrollListCell* cell = listItem->getColumn(COL_UUID);
        if (!cell)
            continue;

        LLUUID avatarId = cell->getValue().asUUID();
        if (avatarId == toFind)
            return result;
    }

    return -1;
}

void FSFloaterPoser::onAvatarsRefresh()
{
    uuid_vec_t avatarsToAddToList, avatarsToRemoveFromList;
    uuid_vec_t nearbyAvatarIds = getNearbyAvatarsAndAnimeshes();
    uuid_vec_t currentlyListedAvatars = getCurrentlyListedAvatarsAndAnimeshes();
    LLCommonUtils::computeDifference(nearbyAvatarIds, currentlyListedAvatars, avatarsToAddToList, avatarsToRemoveFromList);

    for (LLUUID toRemove : avatarsToRemoveFromList)
    {
        S32 indexToRemove = getAvatarListIndexForUuid(toRemove);
        if (indexToRemove >= 0)
            mAvatarSelectionScrollList->deleteSingleItem(indexToRemove);
    }

    std::string iconCatagoryName = "Inv_BodyShape";
    if (hasString("icon_category"))
        iconCatagoryName = getString("icon_category");

    std::string iconObjectName = "Inv_Object";
    if (hasString("icon_object"))
        iconObjectName = getString("icon_object");

    // Add non-Animesh avatars
    for (LLCharacter *character : LLCharacter::sInstances)
    {
        LLUUID uuid = character->getID();
        if (std::find(avatarsToAddToList.begin(), avatarsToAddToList.end(), uuid) == avatarsToAddToList.end())
            continue;

        LLVOAvatar *avatar = dynamic_cast<LLVOAvatar *>(character);
        if (!couldAnimateAvatar(avatar))
            continue;

        if (avatar->isControlAvatar())
            continue;

        LLAvatarName av_name;
        if (!LLAvatarNameCache::get(uuid, &av_name))
            continue;

        LLSD row;
        row["columns"][COL_ICON]["column"] = "icon";
        row["columns"][COL_ICON]["type"]   = "icon";
        row["columns"][COL_ICON]["value"]  = iconCatagoryName;
        row["columns"][COL_NAME]["column"] = "name";
        row["columns"][COL_NAME]["value"]  = av_name.getDisplayName();
        row["columns"][COL_UUID]["column"] = "uuid";
        row["columns"][COL_UUID]["value"]  = uuid;
        row["columns"][COL_SAVE]["column"] = "saveFileName";
        row["columns"][COL_SAVE]["value"]  = "";
        LLScrollListItem* item             = mAvatarSelectionScrollList->addElement(row);
    }

    // Add Animesh avatars
    for (auto character : LLControlAvatar::sInstances)
    {
        LLUUID uuid = character->getID();
        if (std::find(avatarsToAddToList.begin(), avatarsToAddToList.end(), uuid) == avatarsToAddToList.end())
            continue;

        LLControlAvatar *avatar = dynamic_cast<LLControlAvatar *>(character);
        if (!couldAnimateAvatar(avatar))
            continue;

        LLAvatarName av_name;
        std::string animeshName = getControlAvatarName(avatar);
        if (animeshName.empty())
            continue;

        LLSD row;
        row["columns"][COL_ICON]["column"] = "icon";
        row["columns"][COL_ICON]["type"]   = "icon";
        row["columns"][COL_ICON]["value"]  = iconObjectName;
        row["columns"][COL_NAME]["column"] = "name";
        row["columns"][COL_NAME]["value"]  = animeshName;
        row["columns"][COL_UUID]["column"] = "uuid";
        row["columns"][COL_UUID]["value"]  = avatar->getID();
        row["columns"][COL_SAVE]["column"] = "saveFileName";
        row["columns"][COL_SAVE]["value"]  = "";
        mAvatarSelectionScrollList->addElement(row);
    }

    mAvatarSelectionScrollList->updateLayout();
    refreshTextHighlightingOnAvatarScrollList();
}

std::string FSFloaterPoser::getControlAvatarName(const LLControlAvatar* avatar)
{
    if (!avatar)
        return "";

    const LLVOVolume*     rootVolume     = avatar->mRootVolp;
    const LLViewerObject* rootEditObject = (rootVolume) ? rootVolume->getRootEdit() : NULL;
    if (!rootEditObject)
        return "";

    const LLViewerInventoryItem* attachedItem =
        (rootEditObject->isAttachment()) ? gInventory.getItem(rootEditObject->getAttachmentItemID()) : NULL;

    if (attachedItem)
        return attachedItem->getName();

    if (rootEditObject->permYouOwner())
        return avatar->getFullname();

    return "";
}

void FSFloaterPoser::refreshTextHighlightingOnAvatarScrollList()
{
    for (auto listItem : mAvatarSelectionScrollList->getAllData())
    {
        LLScrollListCell* cell = listItem->getColumn(COL_UUID);
        if (!cell)
            continue;

        LLUUID selectedAvatarId = cell->getValue().asUUID();
        LLVOAvatar* listAvatar = getAvatarByUuid(selectedAvatarId);

        if (mPoserAnimator.isPosingAvatar(listAvatar))
            ((LLScrollListText *) listItem->getColumn(COL_NAME))->setFontStyle(LLFontGL::BOLD);
        else
            ((LLScrollListText *) listItem->getColumn(COL_NAME))->setFontStyle(LLFontGL::NORMAL);
    }
}

void FSFloaterPoser::refreshTextHighlightingOnJointScrollLists()
{
    LLVOAvatar *avatar = getUiSelectedAvatar();

    addBoldToScrollList(mBodyJointsScrollList, avatar);
    addBoldToScrollList(mFaceJointsScrollList, avatar);
    addBoldToScrollList(mHandJointsScrollList, avatar);
    addBoldToScrollList(mMiscJointsScrollList, avatar);
    addBoldToScrollList(mCollisionVolumesScrollList, avatar);
}

void FSFloaterPoser::setSavePosesButtonText(bool setAsSaveDiff)
{
    if (hasString("SavePoseLabel") && hasString("SaveDiffLabel"))
        setAsSaveDiff ? mSavePosesBtn->setLabel(getString("SaveDiffLabel")) : mSavePosesBtn->setLabel(getString("SavePoseLabel"));
}

void FSFloaterPoser::addBoldToScrollList(LLScrollListCtrl* list, LLVOAvatar* avatar)
{
    if (!avatar)
        return;

    if (!list)
        return;

    std::string iconValue   = "";
    bool considerExternalFormatSaving = getWhetherToResetBaseRotationOnEdit();

    if (considerExternalFormatSaving && hasString("icon_rotation_is_own_work"))
        iconValue = getString("icon_rotation_is_own_work");

    for (auto listItem : list->getAllData())
    {
        FSPoserAnimator::FSPoserJoint *userData = static_cast<FSPoserAnimator::FSPoserJoint *>(listItem->getUserdata());
        if (!userData)
            continue;

        if (considerExternalFormatSaving)
        {
            if (mPoserAnimator.baseRotationIsZero(avatar, *userData))
                ((LLScrollListText*) listItem->getColumn(COL_ICON))->setValue(iconValue);
            else
                ((LLScrollListText*) listItem->getColumn(COL_ICON))->setValue("");
        }

        if (mPoserAnimator.isPosingAvatarJoint(avatar, *userData))
            ((LLScrollListText *) listItem->getColumn(COL_NAME))->setFontStyle(LLFontGL::BOLD);
        else
            ((LLScrollListText *) listItem->getColumn(COL_NAME))->setFontStyle(LLFontGL::NORMAL);
    }
}

bool FSFloaterPoser::savePoseToBvh(LLVOAvatar* avatar, const std::string& poseFileName)
{
    if (poseFileName.empty())
        return false;

    if (!mPoserAnimator.isPosingAvatar(avatar))
        return false;

    bool writeSuccess = false;

    try
    {
        std::string pathname = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, POSE_SAVE_SUBDIRECTORY);
        if (!gDirUtilp->fileExists(pathname))
        {
            LL_WARNS("Poser") << "Couldn't find folder: " << pathname << " - creating one." << LL_ENDL;
            LLFile::mkdir(pathname);
        }

        std::string fullSavePath =
            gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, POSE_SAVE_SUBDIRECTORY, poseFileName + POSE_EXTERNAL_FORMAT_FILE_EXT);

        llofstream file;
        file.open(fullSavePath.c_str());
        if (!file.is_open())
        {
            LL_WARNS("Poser") << "Unable to save pose!" << LL_ENDL;
            return false;
        }

        writeSuccess = writePoseAsBvh(&file, avatar);

        file.close();
    }
    catch (const std::exception& e)
    {
        LL_WARNS("Posing") << "Exception caught in SaveToBVH: " << e.what() << LL_ENDL;
        return false;
    }

    return writeSuccess;
}

bool FSFloaterPoser::writePoseAsBvh(llofstream* fileStream, LLVOAvatar* avatar)
{
    if (!fileStream || !avatar)
        return false;

    *fileStream << "HIERARCHY" << std::endl;
    auto startingJoint = mPoserAnimator.getPoserJointByName("mPelvis");
    writeBvhFragment(fileStream, avatar, startingJoint, 0);
    *fileStream << "MOTION" << std::endl;
    *fileStream << "Frames:    2" << std::endl;
    *fileStream << "Frame Time: 1" << std::endl;
    writeFirstFrameOfBvhMotion(fileStream, startingJoint);
    *fileStream << std::endl;
    writeBvhMotion(fileStream, avatar, startingJoint);
    *fileStream << std::endl;

    return true;
}

void FSFloaterPoser::writeBvhFragment(llofstream* fileStream, LLVOAvatar* avatar, const FSPoserAnimator::FSPoserJoint* joint, S32 tabStops)
{
    if (!joint)
        return;

    auto saveAxis = getBvhJointTranslation(joint->jointName());

    switch (joint->boneType())
    {
        case WHOLEAVATAR:
            *fileStream << "ROOT " + joint->jointName() << std::endl;
            *fileStream << "{" << std::endl;
            *fileStream << getTabs(tabStops + 1) + "OFFSET " + joint->bvhOffset() << std::endl;
// <AS:chanayane> BVH fixes
            *fileStream << getTabs(tabStops + 1) + "CHANNELS 6 Xposition Yposition Zposition Zrotation Xrotation Yrotation" << std::endl;
// </AS:chanayane>
            break;

        default:
            *fileStream << getTabs(tabStops) + "JOINT " + joint->jointName() << std::endl;
            *fileStream << getTabs(tabStops) + "{" << std::endl;
            *fileStream << getTabs(tabStops + 1) + "OFFSET " + joint->bvhOffset() << std::endl;

            switch (saveAxis)
            {
                default:
                case SWAP_NOTHING:
                    *fileStream << getTabs(tabStops + 1) + "CHANNELS 3 Xrotation Yrotation Zrotation" << std::endl;
                    break;

                case SWAP_YAW_AND_ROLL:
                    *fileStream << getTabs(tabStops + 1) + "CHANNELS 3 Zrotation Yrotation Xrotation" << std::endl;
                    break;

                case SWAP_ROLL_AND_PITCH:
                    *fileStream << getTabs(tabStops + 1) + "CHANNELS 3 Xrotation Zrotation Yrotation" << std::endl;
                    break;

                case SWAP_X2Z_Y2X_Z2Y:
                    *fileStream << getTabs(tabStops + 1) + "CHANNELS 3 Yrotation Zrotation Xrotation" << std::endl;
                    break;

                case SWAP_X2Y_Y2Z_Z2X:
                    *fileStream << getTabs(tabStops + 1) + "CHANNELS 3 Zrotation Xrotation Yrotation" << std::endl;
                    break;

                case SWAP_YAW_AND_PITCH:
                    *fileStream << getTabs(tabStops + 1) + "CHANNELS 3 Yrotation Xrotation Zrotation" << std::endl;
                    break;
            }
            break;
    }

    size_t numberOfBvhChildNodes = joint->bvhChildren().size();
    if (numberOfBvhChildNodes > 0)
    {
        for (size_t index = 0; index != numberOfBvhChildNodes; ++index)
        {
            auto nextJoint = mPoserAnimator.getPoserJointByName(joint->bvhChildren()[index]);
            writeBvhFragment(fileStream, avatar, nextJoint, tabStops + 1);
        }
    }
    else
    {
        // append the End Site offset
        *fileStream << getTabs(tabStops + 1) + "End Site" << std::endl;
        *fileStream << getTabs(tabStops + 1) + "{" << std::endl;
        *fileStream << getTabs(tabStops + 2) + "OFFSET " + joint->bvhEndSite() << std::endl;
        *fileStream << getTabs(tabStops + 1) + "}" << std::endl;
    }

    *fileStream << getTabs(tabStops) + "}" << std::endl;
}

void FSFloaterPoser::writeFirstFrameOfBvhMotion(llofstream* fileStream, const FSPoserAnimator::FSPoserJoint* joint)
{
    if (!joint)
        return;

    switch (joint->boneType())
    {
        case WHOLEAVATAR:
            *fileStream << "0.000000 0.000000 0.000000 0.0 0.0 0.0";
            break;

        default:
            *fileStream << "  0.0 0.0 0.0";
            break;
    }

    size_t numberOfBvhChildNodes = joint->bvhChildren().size();
    for (size_t index = 0; index != numberOfBvhChildNodes; ++index)
    {
        auto nextJoint = mPoserAnimator.getPoserJointByName(joint->bvhChildren()[index]);
        writeFirstFrameOfBvhMotion(fileStream, nextJoint);
    }
}

void FSFloaterPoser::writeBvhMotion(llofstream* fileStream, LLVOAvatar* avatar, const FSPoserAnimator::FSPoserJoint* joint)
{
    if (!joint)
        return;

    auto rotation = mPoserAnimator.getJointRotation(avatar, *joint, SWAP_NOTHING, NEGATE_NOTHING);
    auto position = mPoserAnimator.getJointPosition(avatar, *joint);

    switch (joint->boneType())
    {
        case WHOLEAVATAR:
            *fileStream << positionToString(position) + " " + rotationToString(rotation);
            break;

        default:
            *fileStream << " " + rotationToString(rotation);
            break;
    }

    size_t numberOfBvhChildNodes = joint->bvhChildren().size();
    for (size_t index = 0; index != numberOfBvhChildNodes; ++index)
    {
        auto nextJoint = mPoserAnimator.getPoserJointByName(joint->bvhChildren()[index]);
        writeBvhMotion(fileStream, avatar, nextJoint);
    }
}

std::string FSFloaterPoser::positionToString(const LLVector3& val)
{
    const float metresToInches = 39.37008f;
    return std::to_string(metresToInches * val[VY]) + " " + std::to_string(metresToInches * val[VZ]) + " " + std::to_string(metresToInches * val[VX]);
}

std::string FSFloaterPoser::rotationToString(const LLVector3& val)
{
    return std::to_string(val[VX] * RAD_TO_DEG) + " " + std::to_string(val[VY] * RAD_TO_DEG) + " " + std::to_string(val[VZ] * RAD_TO_DEG);
}

std::string FSFloaterPoser::getTabs(S32 numOfTabstops)
{
    std::string tabSpaces;
    for (S32 i = 0; i < numOfTabstops; i++)
        tabSpaces += "\t";

    return tabSpaces;
}

E_BoneAxisTranslation FSFloaterPoser::getBvhJointTranslation(const std::string& jointName) const
{
    if (jointName.empty())
        return SWAP_X2Y_Y2Z_Z2X;

    bool hasTransformParameter = hasString(BVH_JOINT_TRANSFORM_STRING_PREFIX + jointName);
    if (!hasTransformParameter)
        return SWAP_X2Y_Y2Z_Z2X;

    std::string paramValue = getString(BVH_JOINT_TRANSFORM_STRING_PREFIX + jointName);

    if (strstr(paramValue.c_str(), "SWAP_YAW_AND_ROLL"))
        return SWAP_YAW_AND_ROLL;
    else if (strstr(paramValue.c_str(), "SWAP_YAW_AND_PITCH"))
        return SWAP_YAW_AND_PITCH;
    else if (strstr(paramValue.c_str(), "SWAP_ROLL_AND_PITCH"))
        return SWAP_ROLL_AND_PITCH;
    else if (strstr(paramValue.c_str(), "SWAP_X2Y_Y2Z_Z2X"))
        return SWAP_X2Y_Y2Z_Z2X;
    else if (strstr(paramValue.c_str(), "SWAP_X2Z_Y2X_Z2Y"))
        return SWAP_X2Z_Y2X_Z2Y;
    else
        return SWAP_NOTHING;
}

S32 FSFloaterPoser::getBvhJointNegation(const std::string& jointName) const
{
    S32 result = NEGATE_NOTHING;

    if (jointName.empty())
        return result;

    bool hasTransformParameter = hasString(BVH_JOINT_TRANSFORM_STRING_PREFIX + jointName);
    if (!hasTransformParameter)
        return result;

    std::string paramValue = getString(BVH_JOINT_TRANSFORM_STRING_PREFIX + jointName);

    if (strstr(paramValue.c_str(), "NEGATE_YAW"))
        result |= NEGATE_YAW;
    if (strstr(paramValue.c_str(), "NEGATE_PITCH"))
        result |= NEGATE_PITCH;
    if (strstr(paramValue.c_str(), "NEGATE_ROLL"))
        result |= NEGATE_ROLL;
    if (strstr(paramValue.c_str(), "NEGATE_ALL"))
        return NEGATE_ALL;

    return result;
}

bool FSFloaterPoser::getWhetherToResetBaseRotationOnEdit() { return gSavedSettings.getBOOL(POSER_RESETBASEROTONEDIT_SAVE_KEY); }

void FSFloaterPoser::onClickSetBaseRotZero() { mAlsoSaveBvhCbx->setEnabled(getWhetherToResetBaseRotationOnEdit()); }

bool FSFloaterPoser::getSavingToBvh()
{
    return getWhetherToResetBaseRotationOnEdit() && gSavedSettings.getBOOL(POSER_RESETBASEROTONEDIT_SAVE_KEY);
}

