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
#include "llagent.h"
#include "llavatarnamecache.h"
#include "llcheckboxctrl.h"
#include "llcommonutils.h"
#include "llcontrolavatar.h"
#include "lldiriterator.h"
#include "llsdserialize.h"
#include "llscrolllistctrl.h"
#include "llsliderctrl.h"
#include "lltabcontainer.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "llwindow.h"
#include "llvoavatarself.h"

namespace
{
constexpr char             POSE_INTERNAL_FORMAT_FILE_MASK[]    = "*.xml";
constexpr char             POSE_INTERNAL_FORMAT_FILE_EXT[]     = ".xml";
constexpr char             POSE_SAVE_SUBDIRECTORY[]            = "poses";
constexpr std::string_view POSE_PRESETS_HANDS_SUBDIRECTORY     = "hand_presets";
constexpr char             XML_LIST_HEADER_STRING_PREFIX[]     = "header_";
constexpr char             XML_LIST_TITLE_STRING_PREFIX[]      = "title_";
constexpr char             XML_JOINT_TRANSFORM_STRING_PREFIX[] = "joint_transform_";
constexpr std::string_view POSER_ADVANCEDWINDOWSTATE_SAVE_KEY  = "FSPoserAdvancedWindowState";
constexpr std::string_view POSER_TRACKPAD_SENSITIVITY_SAVE_KEY = "FSPoserTrackpadSensitivity";
constexpr std::string_view POSER_STOPPOSINGWHENCLOSED_SAVE_KEY = "FSPoserStopPosingWhenClosed";
constexpr std::string_view POSER_RESETBASEROTONEDIT_SAVE_KEY   = "FSPoserResetBaseRotationOnEdit";
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
    mCommitCallbackRegistrar.add("Poser.ToggleAdvancedPanel", [this](LLUICtrl*, const LLSD&) { onToggleAdvancedPanel(); });

    mCommitCallbackRegistrar.add("Poser.UndoLastRotation", [this](LLUICtrl*, const LLSD&) { onUndoLastRotation(); });
    mCommitCallbackRegistrar.add("Poser.RedoLastRotation", [this](LLUICtrl*, const LLSD&) { onRedoLastRotation(); });
    mCommitCallbackRegistrar.add("Poser.ToggleMirrorChanges", [this](LLUICtrl*, const LLSD&) { onToggleMirrorChange(); });
    mCommitCallbackRegistrar.add("Poser.ToggleSympatheticChanges", [this](LLUICtrl*, const LLSD&) { onToggleSympatheticChange(); });
    mCommitCallbackRegistrar.add("Poser.ToggleDeltaModeChanges", [this](LLUICtrl*, const LLSD &) { onToggleDeltaModeChange(); });
    mCommitCallbackRegistrar.add("Poser.AdjustTrackPadSensitivity", [this](LLUICtrl*, const LLSD&) { onAdjustTrackpadSensitivity(); });

    mCommitCallbackRegistrar.add("Poser.PositionSet", [this](LLUICtrl*, const LLSD&) { onAvatarPositionSet(); });
    mCommitCallbackRegistrar.add("Poser.SetToTPose", [this](LLUICtrl*, const LLSD&) { onSetAvatarToTpose(); });

    mCommitCallbackRegistrar.add("Poser.Advanced.PositionSet", [this](LLUICtrl*, const LLSD&) { onAdvancedPositionSet(); });
    mCommitCallbackRegistrar.add("Poser.Advanced.ScaleSet", [this](LLUICtrl*, const LLSD&) { onAdvancedScaleSet(); });
    mCommitCallbackRegistrar.add("Poser.UndoLastPosition", [this](LLUICtrl*, const LLSD&) { onUndoLastPosition(); });
    mCommitCallbackRegistrar.add("Poser.RedoLastPosition", [this](LLUICtrl*, const LLSD&) { onRedoLastPosition(); });
    mCommitCallbackRegistrar.add("Poser.ResetPosition", [this](LLUICtrl*, const LLSD&) { onResetPosition(); });
    mCommitCallbackRegistrar.add("Poser.ResetScale", [this](LLUICtrl*, const LLSD&) { onResetScale(); });
    mCommitCallbackRegistrar.add("Poser.UndoLastScale", [this](LLUICtrl*, const LLSD&) { onUndoLastScale(); });
    mCommitCallbackRegistrar.add("Poser.RedoLastScale", [this](LLUICtrl*, const LLSD&) { onRedoLastScale(); });

    mCommitCallbackRegistrar.add("Poser.Save", [this](LLUICtrl*, const LLSD&) { onClickPoseSave(); });
    mCommitCallbackRegistrar.add("Pose.Menu", [this](LLUICtrl*, const LLSD& data) { onPoseMenuAction(data); });
    mCommitCallbackRegistrar.add("Poser.BrowseCache", [this](LLUICtrl*, const LLSD&) { onClickBrowsePoseCache(); });
    mCommitCallbackRegistrar.add("Poser.LoadLeftHand", [this](LLUICtrl*, const LLSD&) { onClickLoadLeftHandPose(); });
    mCommitCallbackRegistrar.add("Poser.LoadRightHand", [this](LLUICtrl*, const LLSD&) { onClickLoadRightHandPose(); });

    mCommitCallbackRegistrar.add("Poser.FlipPose", [this](LLUICtrl*, const LLSD&) { onClickFlipPose(); });
    mCommitCallbackRegistrar.add("Poser.FlipJoint", [this](LLUICtrl*, const LLSD&) { onClickFlipSelectedJoints(); });
    mCommitCallbackRegistrar.add("Poser.RecaptureSelectedBones", [this](LLUICtrl*, const LLSD&) { onClickRecaptureSelectedBones(); });
    mCommitCallbackRegistrar.add("Poser.TogglePosingSelectedBones", [this](LLUICtrl*, const LLSD&) { onClickToggleSelectedBoneEnabled(); });
    mCommitCallbackRegistrar.add("Poser.PoseJointsReset", [this](LLUICtrl*, const LLSD&) { onPoseJointsReset(); });
}

bool FSFloaterPoser::postBuild()
{
    mAvatarTrackball = getChild<FSVirtualTrackpad>("limb_rotation");
    mAvatarTrackball->setCommitCallback([this](LLUICtrl *, const LLSD &) { onLimbTrackballChanged(); });

    mLimbYawSlider = getChild<LLSliderCtrl>("limb_yaw");
    mLimbYawSlider->setCommitCallback([this](LLUICtrl *, const LLSD &) { onLimbYawPitchRollChanged(); });

    mLimbPitchSlider = getChild<LLSliderCtrl>("limb_pitch");
    mLimbPitchSlider->setCommitCallback([this](LLUICtrl *, const LLSD &) { onLimbYawPitchRollChanged(); });

    mLimbRollSlider = getChild<LLSliderCtrl>("limb_roll");
    mLimbRollSlider->setCommitCallback([this](LLUICtrl *, const LLSD &) { onLimbYawPitchRollChanged(); });

    mJointsTabs = getChild<LLTabContainer>("joints_tabs");
    mJointsTabs->setCommitCallback(
        [this](LLUICtrl*, const LLSD&)
        {
            onJointTabSelect();
            setRotationChangeButtons(false, false, false);
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

    mToggleAdvancedPanelBtn = getChild<LLButton>("toggleAdvancedPanel");
    if (gSavedSettings.getBOOL(POSER_ADVANCEDWINDOWSTATE_SAVE_KEY))
        mToggleAdvancedPanelBtn->setValue(true);

    mTrackpadSensitivitySlider = getChild<LLSliderCtrl>("trackpad_sensitivity_slider");
    mTrackpadSensitivitySlider->setValue(gSavedSettings.getF32(POSER_TRACKPAD_SENSITIVITY_SAVE_KEY));

    mPoseSaveNameEditor = getChild<LLLineEditor>("pose_save_name");
    mPoseSaveNameEditor->setPrevalidate(&LLTextValidate::validateASCIIPrintableNoPipe);

    mHandPresetsScrollList = getChild<LLScrollListCtrl>("hand_presets_scroll");

    mPosXSlider = getChild<LLSliderCtrl>("av_position_inout");
    mPosYSlider = getChild<LLSliderCtrl>("av_position_leftright");
    mPosZSlider = getChild<LLSliderCtrl>("av_position_updown");

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

    mFlipPoseBtn = getChild<LLButton>("FlipPose_avatar");
    mFlipJointBtn = getChild<LLButton>("FlipJoint_avatar");
    mRecaptureBtn = getChild<LLButton>("button_RecaptureParts");
    mTogglePosingBonesBtn = getChild<LLButton>("toggle_PosingSelectedBones");

    mToggleMirrorRotationBtn = getChild<LLButton>("button_toggleMirrorRotation");
    mToggleSympatheticRotationBtn = getChild<LLButton>("button_toggleSympatheticRotation");
    mToggleDeltaModeBtn = getChild<LLButton>("delta_mode_toggle");
    mRedoChangeBtn = getChild<LLButton>("button_redo_change");
    mSetToTposeButton = getChild<LLButton>("set_t_pose_button");

    mJointsParentPnl = getChild<LLPanel>("joints_parent_panel");
    mAdvancedParentPnl = getChild<LLPanel>("advanced_parent_panel");
    mTrackballPnl = getChild<LLPanel>("trackball_panel");
    mPositionRotationPnl = getChild<LLPanel>("positionRotation_panel");
    mBodyJointsPnl = getChild<LLPanel>("body_joints_panel");
    mFaceJointsPnl = getChild<LLPanel>("face_joints_panel");
    mHandsTabs = getChild<LLTabContainer>("hands_tabs");
    mHandsJointsPnl = mHandsTabs->getChild<LLPanel>("hands_joints_panel");
    mMiscJointsPnl = getChild<LLPanel>("misc_joints_panel");
    mCollisionVolumesPnl = getChild<LLPanel>("collision_volumes_panel");

    return true;
}

void FSFloaterPoser::onOpen(const LLSD& key)
{
    onAvatarsRefresh();
    refreshJointScrollListMembers();
    onJointTabSelect();
    onOpenSetAdvancedPanel();
    refreshPoseScroll(mHandPresetsScrollList, POSE_PRESETS_HANDS_SUBDIRECTORY);
    startPosingSelf();

    LLFloater::onOpen(key);
}

void FSFloaterPoser::onClose(bool app_quitting)
{
    if (mToggleAdvancedPanelBtn)
        gSavedSettings.setBOOL(POSER_ADVANCEDWINDOWSTATE_SAVE_KEY, mToggleAdvancedPanelBtn->getValue().asBoolean());

    if (gSavedSettings.getBOOL(POSER_STOPPOSINGWHENCLOSED_SAVE_KEY))
        stopPosingSelf();

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
    if (isDeltaSave)
        mLoadPosesBtn->setLabel("Load Diff");
    else
        mLoadPosesBtn->setLabel("Load Pose");
}

void FSFloaterPoser::onClickPoseSave()
{
    std::string filename = mPoseSaveNameEditor->getValue().asString();
    if (filename.empty())
        return;

    LLVOAvatar* avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    bool successfulSave = savePoseToXml(avatar, filename);
    if (successfulSave)
    {
        refreshPoseScroll(mPosesScrollList);
        setUiSelectedAvatarSaveFileName(filename);
        // TODO: provide feedback for save
    }
}

bool FSFloaterPoser::savePoseToXml(LLVOAvatar* avatar, const std::string& poseFileName)
{
    if (poseFileName.empty())
        return false;

    if (!mPoserAnimator.isPosingAvatar(avatar))
        return false;

    try
    {
        std::string pathname = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, POSE_SAVE_SUBDIRECTORY);
        if (!gDirUtilp->fileExists(pathname))
        {
            LL_WARNS("Poser") << "Couldn't find folder: " << pathname << " - creating one." << LL_ENDL;
            LLFile::mkdir(pathname);
        }

        std::string fullSavePath =
            gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, POSE_SAVE_SUBDIRECTORY, poseFileName + POSE_INTERNAL_FORMAT_FILE_EXT);

        bool savingDiff = !mPoserAnimator.allBaseRotationsAreZero(avatar);

        LLSD record;
        record["version"]["value"] = (S32)5;
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

    refreshRotationSliders();
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

    refreshRotationSliders();
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

    refreshRotationSliders();
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
    refreshRotationSliders();
    refreshTrackpadCursor();
    refreshTextHighlightingOnJointScrollLists();
}

void FSFloaterPoser::onClickBrowsePoseCache()
{
    std::string pathname = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, POSE_SAVE_SUBDIRECTORY);
    if (!gDirUtilp->fileExists(pathname))
        LLFile::mkdir(pathname);

    gViewerWindow->getWindow()->openFile(pathname);
}

void FSFloaterPoser::onPoseJointsReset()
{
    if (notDoubleClicked())
        return;

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
            mPoserAnimator.resetAvatarJoint(avatar, *item);
    }

    refreshRotationSliders();
    refreshTrackpadCursor();
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

    LLVOAvatar* avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    loadPoseFromXml(avatar, poseName, loadType);
    onJointTabSelect();
    refreshJointScrollListMembers();
    setSavePosesButtonText(mPoserAnimator.allBaseRotationsAreZero(avatar));
}

bool FSFloaterPoser::notDoubleClicked()
{
    auto timeIntervalSinceLastExecution = std::chrono::system_clock::now() - mTimeLastExecutedDoubleClickMethod;
    mTimeLastExecutedDoubleClickMethod  = std::chrono::system_clock::now();

    return timeIntervalSinceLastExecution > mDoubleClickInterval;
}

void FSFloaterPoser::onClickLoadLeftHandPose()
{
    if (notDoubleClicked())
        return;

    onClickLoadHandPose(false);
}

void FSFloaterPoser::onClickLoadRightHandPose()
{
    if (notDoubleClicked())
        return;

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
                         loadMethod == ROT_POS_AND_SCALES;
    bool loadPositions = loadMethod == POSITIONS || loadMethod == ROTATIONS_AND_POSITIONS || loadMethod == POSITIONS_AND_SCALES ||
                         loadMethod == ROT_POS_AND_SCALES;
    bool loadScales    = loadMethod == SCALES || loadMethod == POSITIONS_AND_SCALES || loadMethod == ROTATIONS_AND_SCALES ||
                         loadMethod == ROT_POS_AND_SCALES;

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

                if (control_map.has("enabled"))
                {
                    enabled = control_map["enabled"].asBoolean();
                    mPoserAnimator.setPosingAvatarJoint(avatar, *poserJoint, enabled);
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
    setUiSelectedAvatar(gAgentAvatarp->getID());
    LLVOAvatar* avatar = getAvatarByUuid(gAgentAvatarp->getID());
    if (!avatar)
        return;

    bool arePosingSelected = mPoserAnimator.isPosingAvatar(avatar);
    if (!arePosingSelected && couldAnimateAvatar(avatar))
        mPoserAnimator.tryPosingAvatar(avatar);

    onAvatarSelect();
}

void FSFloaterPoser::stopPosingSelf()
{
    LLVOAvatar* avatar = getAvatarByUuid(gAgentAvatarp->getID());
    if (!avatar)
        return;

    bool arePosingSelected = mPoserAnimator.isPosingAvatar(avatar);
    if (!arePosingSelected)
        return;

    mPoserAnimator.stopPosingAvatar(avatar);
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
    mAdvancedParentPnl->setEnabled(enable);
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
    setRotationChangeButtons(true, false, false);
}

void FSFloaterPoser::onToggleSympatheticChange()
{
    setRotationChangeButtons(false, true, false);
}

void FSFloaterPoser::onToggleDeltaModeChange()
{
    setRotationChangeButtons(false, false, true);
}

void FSFloaterPoser::setRotationChangeButtons(bool togglingMirror, bool togglingSympathetic, bool togglingDelta)
{
    if (togglingSympathetic || togglingDelta)
        mToggleMirrorRotationBtn->setValue(false);

    if (togglingMirror || togglingDelta)
        mToggleSympatheticRotationBtn->setValue(false);

    if (togglingMirror || togglingSympathetic)
        mToggleDeltaModeBtn->setValue(false);

    refreshTrackpadCursor();
}

void FSFloaterPoser::onUndoLastRotation()
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
            mPoserAnimator.undoLastJointRotation(avatar, *item, getUiSelectedBoneDeflectionStyle());
    }

    enableOrDisableRedoButton();
    refreshRotationSliders();
    refreshTrackpadCursor();
}

void FSFloaterPoser::onUndoLastPosition()
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
            mPoserAnimator.undoLastJointPosition(avatar, *item, getUiSelectedBoneDeflectionStyle());
    }

    refreshAdvancedPositionSliders();
    refreshAvatarPositionSliders();
}

void FSFloaterPoser::onUndoLastScale()
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
            mPoserAnimator.undoLastJointScale(avatar, *item, getUiSelectedBoneDeflectionStyle());
    }

    refreshAdvancedScaleSliders();
}

void FSFloaterPoser::onSetAvatarToTpose()
{
    if (notDoubleClicked())
        return;

    LLVOAvatar* avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    setSavePosesButtonText(false);
    mPoserAnimator.setAllAvatarStartingRotationsToZero(avatar);
    refreshTextHighlightingOnJointScrollLists();
}

void FSFloaterPoser::onResetPosition()
{
    if (notDoubleClicked())
        return;

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
            mPoserAnimator.resetJointPosition(avatar, *item, getUiSelectedBoneDeflectionStyle());
    }

    refreshAdvancedPositionSliders();
    refreshAvatarPositionSliders();
}

void FSFloaterPoser::onResetScale()
{
    if (notDoubleClicked())
        return;

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
            mPoserAnimator.resetJointScale(avatar, *item, getUiSelectedBoneDeflectionStyle());
    }

    refreshAdvancedScaleSliders();
}

void FSFloaterPoser::onRedoLastRotation()
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
            mPoserAnimator.redoLastJointRotation(avatar, *item, getUiSelectedBoneDeflectionStyle());
    }

    enableOrDisableRedoButton();
    refreshRotationSliders();
    refreshTrackpadCursor();
}

void FSFloaterPoser::onRedoLastPosition()
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
            mPoserAnimator.redoLastJointPosition(avatar, *item, getUiSelectedBoneDeflectionStyle());
    }

    refreshAdvancedPositionSliders();
    refreshAvatarPositionSliders();
}

void FSFloaterPoser::onRedoLastScale()
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
            mPoserAnimator.redoLastJointScale(avatar, *item, getUiSelectedBoneDeflectionStyle());
    }

    refreshAdvancedScaleSliders();
}

void FSFloaterPoser::enableOrDisableRedoButton()
{
    LLVOAvatar* avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    if (!mPoserAnimator.isPosingAvatar(avatar))
        return;

    auto selectedJoints = getUiSelectedPoserJoints();
    if (selectedJoints.size() < 1)
        return;

    bool shouldEnableRedoButton = false;
    for (auto item : selectedJoints)
    {
        bool currentlyPosing = mPoserAnimator.isPosingAvatarJoint(avatar, *item);
        if (currentlyPosing)
            shouldEnableRedoButton |= mPoserAnimator.canRedoJointRotation(avatar, *item);
    }

    mRedoChangeBtn->setEnabled(shouldEnableRedoButton);
}

void FSFloaterPoser::onOpenSetAdvancedPanel()
{
    bool advancedPanelExpanded = mToggleAdvancedPanelBtn->getValue().asBoolean();
    if (advancedPanelExpanded)
        onToggleAdvancedPanel();
}

void FSFloaterPoser::onToggleAdvancedPanel()
{
    if (isMinimized())
        return;

    bool advancedPanelExpanded = mToggleAdvancedPanelBtn->getValue().asBoolean();

    mAdvancedParentPnl->setVisible(advancedPanelExpanded);

    // change the height of the Poser panel
    S32 currentHeight       = getRect().getHeight();
    S32 advancedPanelHeight = mAdvancedParentPnl->getRect().getHeight();

    S32 poserFloaterHeight = advancedPanelExpanded ? currentHeight + advancedPanelHeight : currentHeight - advancedPanelHeight;
    S32 poserFloaterWidth  = getRect().getWidth();

    if (poserFloaterHeight < 0)
        return;

    reshape(poserFloaterWidth, poserFloaterHeight);
    onJointTabSelect();
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

    if (activeTab == mPositionRotationPnl)
    {
        scrollList = mEntireAvJointScroll;
    }
    else if (activeTab == mBodyJointsPnl)
    {
        scrollList = mBodyJointsScrollList;
    }
    else if (activeTab == mFaceJointsPnl)
    {
        scrollList = mFaceJointsScrollList;
    }
    else if (activeTab == mHandsTabs)
    {
        auto activeHandsSubTab = mHandsTabs->getCurrentPanel();
        if (!activeHandsSubTab)
        {
            return joints;
        }

        if (activeHandsSubTab == mHandsJointsPnl)
        {
            scrollList = mHandJointsScrollList;
        }
    }
    else if (activeTab == mMiscJointsPnl)
    {
        scrollList = mMiscJointsScrollList;
    }
    else if (activeTab == mCollisionVolumesPnl)
    {
        scrollList = mCollisionVolumesScrollList;
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

    return joints;
}

E_BoneDeflectionStyles FSFloaterPoser::getUiSelectedBoneDeflectionStyle() const
{
    if (mToggleMirrorRotationBtn->getValue().asBoolean())
        return MIRROR;

    if (mToggleSympatheticRotationBtn->getValue().asBoolean())
        return SYMPATHETIC;

    if (mToggleDeltaModeBtn->getValue().asBoolean())
        return DELTAMODE;

    return NONE;
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

void FSFloaterPoser::onAdvancedPositionSet()
{
    F32 posX = (F32)mAdvPosXSlider->getValue().asReal();
    F32 posY = (F32)mAdvPosYSlider->getValue().asReal();
    F32 posZ = (F32)mAdvPosZSlider->getValue().asReal();

    setSelectedJointsPosition(posX, posY, posZ);
    refreshAvatarPositionSliders();
}

void FSFloaterPoser::onAdvancedScaleSet()
{
    F32 scX = (F32)mAdvScaleXSlider->getValue().asReal();
    F32 scY = (F32)mAdvScaleYSlider->getValue().asReal();
    F32 scZ = (F32)mAdvScaleZSlider->getValue().asReal();

    setSelectedJointsScale(scX, scY, scZ);
}

void FSFloaterPoser::onAvatarPositionSet()
{
    F32 posX = (F32)mPosXSlider->getValue().asReal();
    F32 posY = (F32)mPosYSlider->getValue().asReal();
    F32 posZ = (F32)mPosZSlider->getValue().asReal();

    setSelectedJointsPosition(posX, posY, posZ);
    refreshAdvancedPositionSliders();
}

void FSFloaterPoser::onLimbTrackballChanged()
{
    LLVector3 trackPadPos;
    LLSD position = mAvatarTrackball->getValue();
    if (position.isArray() && position.size() == 3)
        trackPadPos.setValue(position);
    else
        return;

    F32 yaw, pitch, roll;
    yaw  = trackPadPos.mV[VX];
    pitch = trackPadPos.mV[VY];
    roll  = trackPadPos.mV[VZ];

    F32 trackPadSensitivity = llmax(gSavedSettings.getF32(POSER_TRACKPAD_SENSITIVITY_SAVE_KEY), 0.0001f);
    yaw *= trackPadSensitivity;
    pitch *= trackPadSensitivity;

    yaw   = unWrapScale(yaw) * NormalTrackpadRangeInRads;
    pitch = unWrapScale(pitch) * NormalTrackpadRangeInRads;
    roll  = unWrapScale(roll) * NormalTrackpadRangeInRads;

    if (mToggleDeltaModeBtn->getValue().asBoolean())
    {
        F32 deltaYaw, deltaPitch, deltaRoll;
        LLSD deltaPosition = mAvatarTrackball->getValueDelta();
        LLVector3 trackPadDeltaPos;
        if (deltaPosition.isArray() && deltaPosition.size() == 3)
        {
            trackPadDeltaPos.setValue(deltaPosition);
            deltaYaw   = trackPadDeltaPos[VX] * NormalTrackpadRangeInRads;
            deltaPitch = trackPadDeltaPos[VY] * NormalTrackpadRangeInRads;
            deltaRoll  = trackPadDeltaPos[VZ] * NormalTrackpadRangeInRads;
            deltaYaw *= trackPadSensitivity;
            deltaPitch *= trackPadSensitivity;
        
            setSelectedJointsRotation(deltaYaw, deltaPitch, deltaRoll);
        }
    }
    else
    {
        setSelectedJointsRotation(yaw, pitch, roll);
    }

    // WARNING!
    // as tempting as it is to refactor the following to refreshRotationSliders(), don't.
    // getRotationOfFirstSelectedJoint/setSelectedJointsRotation are
    // not necessarily symmetric functions (see their remarks).
    mLimbYawSlider->setValue(yaw *= RAD_TO_DEG);
    mLimbPitchSlider->setValue(pitch *= RAD_TO_DEG);
    mLimbRollSlider->setValue(roll *= RAD_TO_DEG);
}

F32 FSFloaterPoser::unWrapScale(F32 scale)
{
    if (scale > -1.f && scale < 1.f)
        return scale;

    F32 result = fmodf(scale, 100.f);  // to avoid time consuming while loops
    while (result > 1)
        result -= 2;
    while (result < -1)
        result += 2;

    return result;
}

void FSFloaterPoser::onLimbYawPitchRollChanged()
{
    F32 yaw   = (F32)mLimbYawSlider->getValue().asReal();
    F32 pitch = (F32)mLimbPitchSlider->getValue().asReal();
    F32 roll  = (F32)mLimbRollSlider->getValue().asReal();

    yaw *= DEG_TO_RAD;
    pitch *= DEG_TO_RAD;
    roll *= DEG_TO_RAD;

    setSelectedJointsRotation(yaw, pitch, roll);

    // WARNING!
    // as tempting as it is to refactor the following to refreshTrackpadCursor(), don't.
    // getRotationOfFirstSelectedJoint/setSelectedJointsRotation are
    // not necessarily symmetric functions (see their remarks).
    F32 trackPadSensitivity = llmax(gSavedSettings.getF32(POSER_TRACKPAD_SENSITIVITY_SAVE_KEY), 0.0001f);
    yaw /= trackPadSensitivity;
    pitch /= trackPadSensitivity;

    yaw /= NormalTrackpadRangeInRads;
    pitch /= NormalTrackpadRangeInRads;
    roll /= NormalTrackpadRangeInRads;

    mAvatarTrackball->setValue(yaw, pitch, roll);
}

void FSFloaterPoser::onAdjustTrackpadSensitivity()
{
    gSavedSettings.setF32(POSER_TRACKPAD_SENSITIVITY_SAVE_KEY, (F32)mTrackpadSensitivitySlider->getValue().asReal());
    refreshTrackpadCursor();
}

void FSFloaterPoser::refreshTrackpadCursor()
{
    F32 axis1 = (F32)mLimbYawSlider->getValue().asReal();
    F32 axis2 = (F32)mLimbPitchSlider->getValue().asReal();
    F32 axis3 = (F32)mLimbRollSlider->getValue().asReal();

    axis1 *= DEG_TO_RAD;
    axis2 *= DEG_TO_RAD;
    axis3 *= DEG_TO_RAD;

    axis1 /= NormalTrackpadRangeInRads;
    axis2 /= NormalTrackpadRangeInRads;
    axis3 /= NormalTrackpadRangeInRads;

    F32 trackPadSensitivity = llmax(gSavedSettings.getF32(POSER_TRACKPAD_SENSITIVITY_SAVE_KEY), 0.0001f);
    axis1 /= trackPadSensitivity;
    axis2 /= trackPadSensitivity;

    mAvatarTrackball->setValue(axis1, axis2, axis3);
}

/// <summary>
/// This only sets the position sliders of the 'basic' view (not the advanced sliders).
/// </summary>
void FSFloaterPoser::refreshAvatarPositionSliders()
{
    auto activeTab = mJointsTabs->getCurrentPanel();
    if (!activeTab)
        return;

    if (activeTab != mPositionRotationPnl)
        return; // if the active tab isn't the av position one, don't set anything.

    LLVector3 position = getPositionOfFirstSelectedJoint();
    mPosXSlider->setValue(position.mV[VX]);
    mPosYSlider->setValue(position.mV[VY]);
    mPosZSlider->setValue(position.mV[VZ]);
}

void FSFloaterPoser::refreshRotationSliders()
{
    LLVector3 rotation = getRotationOfFirstSelectedJoint();

    mLimbYawSlider->setValue(rotation.mV[VX] *= RAD_TO_DEG);
    mLimbPitchSlider->setValue(rotation.mV[VY] *= RAD_TO_DEG);
    mLimbRollSlider->setValue(rotation.mV[VZ] *= RAD_TO_DEG);
}

void FSFloaterPoser::refreshAdvancedPositionSliders()
{
    LLVector3 position = getPositionOfFirstSelectedJoint();

    mAdvPosXSlider->setValue(position.mV[VX]);
    mAdvPosYSlider->setValue(position.mV[VY]);
    mAdvPosZSlider->setValue(position.mV[VZ]);
}

void FSFloaterPoser::refreshAdvancedScaleSliders()
{
    LLVector3 rotation = getScaleOfFirstSelectedJoint();

    mAdvScaleXSlider->setValue(rotation.mV[VX]);
    mAdvScaleYSlider->setValue(rotation.mV[VY]);
    mAdvScaleZSlider->setValue(rotation.mV[VZ]);
}

void FSFloaterPoser::setSelectedJointsPosition(F32 x, F32 y, F32 z)
{
    LLVOAvatar *avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    if (!mPoserAnimator.isPosingAvatar(avatar))
        return;

    E_BoneDeflectionStyles defl = getUiSelectedBoneDeflectionStyle();
    LLVector3              vec3 = LLVector3(x, y, z);

    for (auto item : getUiSelectedPoserJoints())
    {
        bool currentlyPosingJoint = mPoserAnimator.isPosingAvatarJoint(avatar, *item);
        if (!currentlyPosingJoint)
            continue;

        mPoserAnimator.setJointPosition(avatar, item, vec3, defl);
    }
}

void FSFloaterPoser::setSelectedJointsRotation(F32 yawInRadians, F32 pitchInRadians, F32 rollInRadians)
{
    LLVOAvatar *avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    if (!mPoserAnimator.isPosingAvatar(avatar))
        return;

    E_BoneDeflectionStyles defl             = getUiSelectedBoneDeflectionStyle();
    LLVector3              vec3             = LLVector3(yawInRadians, pitchInRadians, rollInRadians);
    auto                   selectedJoints   = getUiSelectedPoserJoints();
    bool                   savingToExternal = getWhetherToResetBaseRotationOnEdit();

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

            if (oppositeJointAlsoSelectedOnUi && item->dontFlipOnMirror())
                continue;
        }

        mPoserAnimator.setJointRotation(avatar, item, vec3, defl, getJointTranslation(item->jointName()),
                                        getJointNegation(item->jointName()), savingToExternal);
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

    E_BoneDeflectionStyles defl = getUiSelectedBoneDeflectionStyle();
    LLVector3              vec3 = LLVector3(x, y, z);

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
    refreshAvatarPositionSliders();
    refreshRotationSliders();
    refreshTrackpadCursor();
    enableOrDisableRedoButton();

    if (mToggleAdvancedPanelBtn->getValue().asBoolean())
    {
        refreshAdvancedPositionSliders();
        refreshAdvancedScaleSliders();
    }
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

        LLSD row;
        row["columns"][COL_ICON]["column"] = "icon";
        row["columns"][COL_ICON]["type"]   = "icon";
        row["columns"][COL_ICON]["value"]  = iconObjectName;
        row["columns"][COL_NAME]["column"] = "name";
        row["columns"][COL_NAME]["value"]  = avatar->getFullname();
        row["columns"][COL_UUID]["column"] = "uuid";
        row["columns"][COL_UUID]["value"]  = avatar->getID();
        row["columns"][COL_SAVE]["column"] = "saveFileName";
        row["columns"][COL_SAVE]["value"]  = "";
        mAvatarSelectionScrollList->addElement(row);
    }

    mAvatarSelectionScrollList->updateLayout();
    refreshTextHighlightingOnAvatarScrollList();
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
    setAsSaveDiff ? mSavePosesBtn->setLabel("Save Diff") : mSavePosesBtn->setLabel("Save Pose");
}

bool FSFloaterPoser::posingAnyoneOnScrollList()
{
    for (auto listItem : mAvatarSelectionScrollList->getAllData())
    {
        LLScrollListCell* cell = listItem->getColumn(COL_UUID);
        if (!cell)
            continue;

        LLUUID      selectedAvatarId = cell->getValue().asUUID();
        LLVOAvatar* listAvatar       = getAvatarByUuid(selectedAvatarId);

        if (mPoserAnimator.isPosingAvatar(listAvatar))
            return true;
    }

    return false;
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

bool FSFloaterPoser::getWhetherToResetBaseRotationOnEdit() { return gSavedSettings.getBOOL(POSER_RESETBASEROTONEDIT_SAVE_KEY); }
