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
#include <string_view>
#include <boost/algorithm/string.hpp>
#include "fsfloaterposer.h"
#include "fsposeranimator.h"
#include "llagent.h"
#include "llavatarname.h"
#include "llavatarnamecache.h"
#include "lldiriterator.h"
#include "llsdserialize.h"
#include "fsvirtualtrackpad.h"
#include "llfloater.h"
#include "llviewercontrol.h"
#include "llcontrolavatar.h"
#include "llstring.h"
#include "llviewerwindow.h"
#include "llwindow.h"
#include "llcommonutils.h"
#include "llscrolllistctrl.h"
#include "llsliderctrl.h"
#include "lltabcontainer.h"
#include "llcheckboxctrl.h"
#include <boost/algorithm/string.hpp>

namespace
{
constexpr std::string_view POSE_INTERNAL_FORMAT_FILE_MASK     = "*.xml";
constexpr std::string_view POSE_INTERNAL_FORMAT_FILE_EXT      = ".xml";
constexpr std::string_view POSE_EXTERNAL_FORMAT_FILE_EXT      = ".bvh";
constexpr std::string_view POSE_SAVE_SUBDIRECTORY             = "poses";
constexpr std::string_view POSE_PRESETS_HANDS_SUBDIRECTORY    = "poses\\hand_presets";
constexpr std::string_view XML_LIST_HEADER_STRING_PREFIX      = "header_";
constexpr std::string_view XML_LIST_TITLE_STRING_PREFIX       = "title_";
constexpr std::string_view XML_JOINT_TRANSFORM_STRING_PREFIX  = "joint_transform_";
constexpr std::string_view POSER_ADVANCEDWINDOWSTATE_SAVE_KEY = "FSPoserAdvancedWindowState";
constexpr std::string_view POSER_ALSOSAVEBVHFILE_SAVE_KEY     = "FSPoserSaveBvhFileAlso";
constexpr std::string_view POSER_TRACKPAD_SENSITIVITY_SAVE_KEY = "FSPoserTrackpadSensitivity";

constexpr std::string_view POSER_AVATAR_PANEL_JOINTSPARENT = "joints_parent_panel";
constexpr std::string_view POSER_AVATAR_PANEL_TRACKBALL = "trackball_panel";
constexpr std::string_view POSER_AVATAR_PANEL_ADVANCED = "advanced_parent_panel";
constexpr std::string_view POSER_AVATAR_TABGROUP_JOINTS = "joints_tabs";
constexpr std::string_view POSER_AVATAR_TAB_POSITION = "positionRotation_panel";
constexpr std::string_view POSER_AVATAR_TAB_BODY = "body_joints_panel";
constexpr std::string_view POSER_AVATAR_TAB_FACE = "face_joints_panel";
constexpr std::string_view POSER_AVATAR_TAB_HANDS = "hands_tabs";
constexpr std::string_view POSER_AVATAR_TAB_HANDJOINTS = "hands_joints_panel";
constexpr std::string_view POSER_AVATAR_TAB_MISC = "misc_joints_panel";
constexpr std::string_view POSER_AVATAR_TAB_VOLUMES = "collision_volumes_panel";


// standard controls
constexpr std::string_view POSER_AVATAR_TRACKBALL_NAME   = "limb_rotation";
constexpr std::string_view POSER_TRACKPAD_SENSITIVITY_SLIDER_NAME = "trackpad_sensitivity_slider";
constexpr std::string_view POSER_AVATAR_SLIDER_YAW_NAME  = "limb_yaw"; // turning your nose left or right
constexpr std::string_view POSER_AVATAR_SLIDER_PITCH_NAME  = "limb_pitch"; // pointing your nose up or down
constexpr std::string_view POSER_AVATAR_SLIDER_ROLL_NAME = "limb_roll"; // your ear touches your shoulder
constexpr std::string_view POSER_AVATAR_TOGGLEBUTTON_MIRROR = "button_toggleMirrorRotation";
constexpr std::string_view POSER_AVATAR_TOGGLEBUTTON_SYMPATH = "button_toggleSympatheticRotation";
constexpr std::string_view POSER_AVATAR_BUTTON_REDO = "button_redo_change";
constexpr std::string_view POSER_AVATAR_BUTTON_DELTAMODE = "delta_mode_toggle";
constexpr std::string_view POSER_AVATAR_SLIDER_POSX_NAME = "av_position_inout";
constexpr std::string_view POSER_AVATAR_SLIDER_POSY_NAME = "av_position_leftright";
constexpr std::string_view POSER_AVATAR_SLIDER_POSZ_NAME = "av_position_updown";


// Advanced controls
constexpr std::string_view POSER_AVATAR_ADV_SLIDER_ROTX_NAME = "Advanced_Rotation_X";
constexpr std::string_view POSER_AVATAR_ADV_SLIDER_ROTY_NAME = "Advanced_Rotation_Y";
constexpr std::string_view POSER_AVATAR_ADV_SLIDER_ROTZ_NAME = "Advanced_Rotation_Z";
constexpr std::string_view POSER_AVATAR_ADV_SLIDER_POSX_NAME = "Advanced_Position_X";
constexpr std::string_view POSER_AVATAR_ADV_SLIDER_POSY_NAME = "Advanced_Position_Y";
constexpr std::string_view POSER_AVATAR_ADV_SLIDER_POSZ_NAME = "Advanced_Position_Z";
constexpr std::string_view POSER_AVATAR_ADV_SLIDER_SCALEX_NAME = "Advanced_Scale_X";
constexpr std::string_view POSER_AVATAR_ADV_SLIDER_SCALEY_NAME = "Advanced_Scale_Y";
constexpr std::string_view POSER_AVATAR_ADV_SLIDER_SCALEZ_NAME = "Advanced_Scale_Z";
constexpr std::string_view POSER_AVATAR_ADV_BUTTON_NAME  = "start_stop_posing_button";
constexpr std::string_view POSER_AVATAR_ADVANCED_SAVEOPTIONSPANEL_NAME = "save_file_options";
constexpr std::string_view POSER_AVATAR_ADVANCED_SAVEBVHCHECKBOX_NAME = "also_save_bvh_checkbox";

constexpr std::string_view POSER_AVATAR_SCROLLLIST_AVATARSELECTION   = "avatarSelection_scroll";
constexpr std::string_view POSER_AVATAR_STARTSTOP_POSING_BUTTON_NAME = "start_stop_posing_button";
constexpr std::string_view POSER_AVATAR_ADVANCED_TOGGLEBUTTON_NAME   = "toggleAdvancedPanel";
constexpr std::string_view POSER_AVATAR_PANEL_BUTTON_FLIPPOSE_NAME   = "FlipPose_avatar";
constexpr std::string_view POSER_AVATAR_PANEL_BUTTON_FLIPJOINT_NAME  = "FlipJoint_avatar";
constexpr std::string_view POSER_AVATAR_PANEL_BUTTON_RECAPTURE_NAME  = "button_RecaptureParts";
constexpr std::string_view POSER_AVATAR_PANEL_BUTTON_TOGGLEPOSING_NAME  = "toggle_PosingSelectedBones";

constexpr std::string_view POSER_AVATAR_TOGGLEBUTTON_LOADSAVE        = "toggleLoadSavePanel";
constexpr std::string_view POSER_AVATAR_PANEL_LOADSAVE_NAME          = "poses_loadSave";
constexpr std::string_view POSER_AVATAR_SCROLLLIST_LOADSAVE_NAME     = "poses_scroll";
constexpr std::string_view POSER_AVATAR_BUTTON_BROWSEFOLDER_NAME     = "open_poseDir_button";
constexpr std::string_view POSER_AVATAR_BUTTON_LOAD_NAME             = "load_poses_button";
constexpr std::string_view POSER_AVATAR_BUTTON_SAVE_NAME             = "save_poses_button";
constexpr std::string_view POSER_AVATAR_LINEEDIT_FILESAVENAME        = "pose_save_name";

constexpr std::string_view POSER_AVATAR_SCROLLLIST_HIDDEN_NAME         = "entireAv_joint_scroll";
constexpr std::string_view POSER_AVATAR_SCROLLLIST_BODYJOINTS_NAME     = "body_joints_scroll";
constexpr std::string_view POSER_AVATAR_SCROLLLIST_FACEJOINTS_NAME     = "face_joints_scroll";
constexpr std::string_view POSER_AVATAR_SCROLLLIST_HANDJOINTS_NAME     = "hand_joints_scroll";
constexpr std::string_view POSER_AVATAR_SCROLLLIST_MISCJOINTS_NAME     = "misc_joints_scroll";
constexpr std::string_view POSER_AVATAR_SCROLLLIST_VOLUMES_NAME        = "collision_volumes_scroll";
constexpr std::string_view POSER_AVATAR_SCROLLLIST_HAND_PRESETS_NAME   = "hand_presets_scroll";
}

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



FSFloaterPoser::~FSFloaterPoser() {}

bool FSFloaterPoser::postBuild()
{
    // find-and-binds
    getChild<LLUICtrl>(POSER_AVATAR_TRACKBALL_NAME)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onLimbTrackballChanged(); });
    getChild<LLUICtrl>(POSER_AVATAR_SLIDER_YAW_NAME)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onLimbYawPitchRollChanged(); });
    getChild<LLUICtrl>(POSER_AVATAR_SLIDER_PITCH_NAME)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onLimbYawPitchRollChanged(); });
    getChild<LLUICtrl>(POSER_AVATAR_SLIDER_ROLL_NAME)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onLimbYawPitchRollChanged(); });
    getChild<LLTabContainer>(POSER_AVATAR_TABGROUP_JOINTS)
        ->setCommitCallback(
            [this](LLUICtrl*, const LLSD&)
            {
                onJointSelect();
                setRotationChangeButtons(false, false, false);
            });

    LLScrollListCtrl *scrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_AVATARSELECTION);
    if (scrollList)
    {
        scrollList->setCommitOnSelectionChange(true);
        scrollList->setCommitCallback([this](LLUICtrl *, const LLSD &) { onAvatarSelect(); });
    }

    scrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_BODYJOINTS_NAME);
    if (scrollList)
    {
        scrollList->setCommitOnSelectionChange(true);
        scrollList->setCommitCallback([this](LLUICtrl *, const LLSD &) { onJointSelect(); });
    }

    scrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_FACEJOINTS_NAME);
    if (scrollList)
    {
        scrollList->setCommitOnSelectionChange(true);
        scrollList->setCommitCallback([this](LLUICtrl *, const LLSD &) { onJointSelect(); });
    }

    scrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_HANDJOINTS_NAME);
    if (scrollList)
    {
        scrollList->setCommitOnSelectionChange(true);
        scrollList->setCommitCallback([this](LLUICtrl *, const LLSD &) { onJointSelect(); });
    }

    scrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_MISCJOINTS_NAME);
    if (scrollList)
    {
        scrollList->setCommitOnSelectionChange(true);
        scrollList->setCommitCallback([this](LLUICtrl *, const LLSD &) { onJointSelect(); });
    }

    scrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_VOLUMES_NAME);
    if (scrollList)
    {
        scrollList->setCommitOnSelectionChange(true);
        scrollList->setCommitCallback([this](LLUICtrl *, const LLSD &) { onJointSelect(); });
    }

    scrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_LOADSAVE_NAME);
    if (scrollList)
    {
        scrollList->setCommitOnSelectionChange(true);
        scrollList->setCommitCallback([this](LLUICtrl *, const LLSD &) { onPoseFileSelect(); });
    }

    bool advButtonState = gSavedSettings.getBOOL(POSER_ADVANCEDWINDOWSTATE_SAVE_KEY);
    if (advButtonState)
    {
        LLButton *advancedButton = getChild<LLButton>(POSER_AVATAR_ADVANCED_TOGGLEBUTTON_NAME);
        if (advancedButton)
            advancedButton->setValue(true);
    }

    bool saveBvhCheckboxState = gSavedSettings.getBOOL(POSER_ALSOSAVEBVHFILE_SAVE_KEY);
    if (saveBvhCheckboxState)
    {
        LLCheckBoxCtrl* saveBvhCheckbox = getChild<LLCheckBoxCtrl>(POSER_AVATAR_ADVANCED_SAVEBVHCHECKBOX_NAME);
        if (saveBvhCheckbox)
            saveBvhCheckbox->set(true);
    }

    LLSliderCtrl* trackpadSensitivitySlider = getChild<LLSliderCtrl>(POSER_TRACKPAD_SENSITIVITY_SLIDER_NAME);
    if (trackpadSensitivitySlider)
    {
        F32 trackPadSensitivity = gSavedSettings.getF32(POSER_TRACKPAD_SENSITIVITY_SAVE_KEY);
        trackpadSensitivitySlider->setValue(trackPadSensitivity);
    }

    LLLineEditor *poseSaveName = getChild<LLLineEditor>(POSER_AVATAR_LINEEDIT_FILESAVENAME);
    if (poseSaveName)
        poseSaveName->setPrevalidate(&LLTextValidate::validateASCIIPrintableNoPipe);

    return true;
}

void FSFloaterPoser::draw()
{
    LLFloater::draw();
}

void FSFloaterPoser::onOpen(const LLSD& key)
{
    onAvatarsRefresh();
    refreshJointScrollListMembers();
    onJointSelect();
    onOpenSetAdvancedPanel();
    refreshPoseScroll(POSER_AVATAR_SCROLLLIST_HAND_PRESETS_NAME, POSE_PRESETS_HANDS_SUBDIRECTORY);
}

void FSFloaterPoser::onClose(bool app_quitting)
{
    LLButton *advancedButton = getChild<LLButton>(POSER_AVATAR_ADVANCED_TOGGLEBUTTON_NAME);
    if (advancedButton)
        gSavedSettings.setBOOL(POSER_ADVANCEDWINDOWSTATE_SAVE_KEY, advancedButton->getValue().asBoolean());

    LLCheckBoxCtrl* saveBvhCheckbox = getChild<LLCheckBoxCtrl>(POSER_AVATAR_ADVANCED_SAVEBVHCHECKBOX_NAME);
    if (saveBvhCheckbox)
        gSavedSettings.setBOOL(POSER_ALSOSAVEBVHFILE_SAVE_KEY, saveBvhCheckbox->getValue());
}

void FSFloaterPoser::refreshPoseScroll(std::string_view scrollListName, std::string_view subDirectory)
{
    if (scrollListName.empty() || subDirectory.empty())
        return;

    LLScrollListCtrl* posesScrollList = getChild<LLScrollListCtrl>(scrollListName);
    if (!posesScrollList)
        return;

    posesScrollList->clearRows();

    std::string   dir = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, std::string(subDirectory));
    std::string file;
    LLDirIterator dir_iter(dir, std::string(POSE_INTERNAL_FORMAT_FILE_MASK));
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
    LLScrollListCtrl *posesScrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_LOADSAVE_NAME);
    if (!posesScrollList)
        return;

    LLScrollListItem *item = posesScrollList->getFirstSelected();
    if (!item)
        return;

    LLVOAvatar* avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    bool enableButtons = _poserAnimator.isPosingAvatar(avatar);

    LLButton *loadPosesButton = getChild<LLButton>(POSER_AVATAR_BUTTON_LOAD_NAME);
    if (loadPosesButton)
        loadPosesButton->setEnabled(enableButtons);

    LLButton *savePosesButton = getChild<LLButton>(POSER_AVATAR_BUTTON_SAVE_NAME);
    if (savePosesButton)
        savePosesButton->setEnabled(enableButtons);

    std::string poseName = item->getColumn(0)->getValue().asString();
    if (poseName.empty())
        return;

    LLLineEditor *poseSaveName = getChild<LLLineEditor>(POSER_AVATAR_LINEEDIT_FILESAVENAME);
    if (!poseSaveName)
        return;

    poseSaveName->setEnabled(enableButtons);
    LLStringExplicit name = LLStringExplicit(poseName);
    poseSaveName->setText(name);
}

void FSFloaterPoser::onClickPoseSave()
{
    LLUICtrl *poseSaveName = getChild<LLUICtrl>(POSER_AVATAR_LINEEDIT_FILESAVENAME);
    if (!poseSaveName)
        return;

    std::string filename = poseSaveName->getValue().asString();
    if (filename.empty())
        return;

    LLVOAvatar* avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    bool successfulSave = savePoseToXml(avatar, filename);
    if (successfulSave)
    {
        refreshPoseScroll(POSER_AVATAR_SCROLLLIST_LOADSAVE_NAME, POSE_SAVE_SUBDIRECTORY);
        setUiSelectedAvatarSaveFileName(filename);
        // TODO: provide feedback for save

        LLCheckBoxCtrl* saveBvhCheckbox = getChild<LLCheckBoxCtrl>(POSER_AVATAR_ADVANCED_SAVEBVHCHECKBOX_NAME);
        if (!saveBvhCheckbox)
            return;

        bool alsoSaveAsBvh = saveBvhCheckbox->getValue().asBoolean();
        if (alsoSaveAsBvh)
            savePoseToBvh(avatar, filename);
    }
}

bool FSFloaterPoser::savePoseToBvh(LLVOAvatar* avatar, std::string poseFileName)
{
    if (poseFileName.empty())
        return false;

    if (!_poserAnimator.isPosingAvatar(avatar))
        return false;

    bool writeSuccess = false;

    try
    {
        std::string pathname = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, std::string(POSE_SAVE_SUBDIRECTORY));
        if (!gDirUtilp->fileExists(pathname))
        {
            LL_WARNS("Poser") << "Couldn't find folder: " << pathname << " - creating one." << LL_ENDL;
            LLFile::mkdir(pathname);
        }

        std::string fullSavePath =
            gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, std::string(POSE_SAVE_SUBDIRECTORY), poseFileName + std::string(POSE_EXTERNAL_FORMAT_FILE_EXT));

        llofstream file;
        file.open(fullSavePath.c_str());
        if (!file.is_open())
        {
            LL_WARNS("Poser") << "Unable to save pose!" << LL_ENDL;
            return false;
        }

        writeSuccess = _poserAnimator.writePoseAsBvh(&file, avatar);

        file.close();
    }
    catch (const std::exception& e)
    {
        LL_WARNS("Posing") << "Exception caught in SaveToBVH: " << e.what() << LL_ENDL;
        return false;
    }


    return true;
}

bool FSFloaterPoser::savePoseToXml(LLVOAvatar* avatar, std::string poseFileName)
{
    if (poseFileName.empty())
        return false;

    if (!_poserAnimator.isPosingAvatar(avatar))
        return false;

    try
    {
        std::string pathname = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, std::string(POSE_SAVE_SUBDIRECTORY));
        if (!gDirUtilp->fileExists(pathname))
        {
            LL_WARNS("Poser") << "Couldn't find folder: " << pathname << " - creating one." << LL_ENDL;
            LLFile::mkdir(pathname);
        }

        std::string fullSavePath =
            gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, std::string(POSE_SAVE_SUBDIRECTORY), poseFileName + std::string(POSE_INTERNAL_FORMAT_FILE_EXT));

        LLSD record;
        S32 version = 3;
        record["version"]["value"] = version;

        for (const FSPoserAnimator::FSPoserJoint& pj : _poserAnimator.PoserJoints)
        {
            std::string  bone_name = pj.jointName();

            LLVector3 vec3 = _poserAnimator.getJointRotation(avatar, pj, SWAP_NOTHING, NEGATE_NOTHING);

            record[bone_name]     = pj.jointName();   
            record[bone_name]["rotation"] = vec3.getValue();

            vec3                          = _poserAnimator.getJointPosition(avatar, pj);
            record[bone_name]["position"] = vec3.getValue();

            vec3                       = _poserAnimator.getJointScale(avatar, pj);
            record[bone_name]["scale"] = vec3.getValue();

            record[bone_name]["enabled"] = _poserAnimator.isPosingAvatarJoint(avatar, pj);
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

    if (!_poserAnimator.isPosingAvatar(avatar))
        return;

    for (auto item : selectedJoints)
    {
        bool currentlyPosing = _poserAnimator.isPosingAvatarJoint(avatar, *item);
        _poserAnimator.setPosingAvatarJoint(avatar, *item, !currentlyPosing);
    }

    refreshRotationSliders();
    refreshTrackpadCursor();
    refreshTextEmbiggeningOnAllScrollLists();
}

void FSFloaterPoser::onClickFlipSelectedJoints()
{
    auto selectedJoints = getUiSelectedPoserJoints();
    if (selectedJoints.size() < 1)
        return;

    LLVOAvatar *avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    if (!_poserAnimator.isPosingAvatar(avatar))
        return;

    for (auto item : selectedJoints)
    {
        // need to be posing the joint to flip
        bool currentlyPosingJoint = _poserAnimator.isPosingAvatarJoint(avatar, *item);
        if (!currentlyPosingJoint)
            continue;

        // need to be posing opposite joint too, or don't flip
        auto oppositeJoint = _poserAnimator.getPoserJointByName(item->mirrorJointName());
        if (oppositeJoint)
        {
            bool currentlyPosingOppositeJoint = _poserAnimator.isPosingAvatarJoint(avatar, *oppositeJoint);
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

        _poserAnimator.reflectJoint(avatar, item);
    }

    refreshRotationSliders();
    refreshTrackpadCursor();
}

void FSFloaterPoser::onClickFlipPose()
{
    LLVOAvatar *avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    if (!_poserAnimator.isPosingAvatar(avatar))
        return;

    _poserAnimator.flipEntirePose(avatar);

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

    if (!_poserAnimator.isPosingAvatar(avatar))
        return;

    for (auto item : selectedJoints)
    {
        bool currentlyPosing = _poserAnimator.isPosingAvatarJoint(avatar, *item);
        if (currentlyPosing)
            continue;

        LLVector3 newRotation = _poserAnimator.getJointRotation(avatar, *item, getJointTranslation(item->jointName()),
                                                                getJointNegation(item->jointName()), true);
        LLVector3 newPosition = _poserAnimator.getJointPosition(avatar, *item, true);
        LLVector3 newScale    = _poserAnimator.getJointScale(avatar, *item, true);

        _poserAnimator.setPosingAvatarJoint(avatar, *item, true);

        _poserAnimator.setJointRotation(avatar, item, newRotation, NONE, getJointTranslation(item->jointName()),
                                        getJointNegation(item->jointName()));
        _poserAnimator.setJointPosition(avatar, item, newPosition, NONE);
        _poserAnimator.setJointScale(avatar, item, newScale, NONE);
    }

    refreshRotationSliders();
    refreshTrackpadCursor();
    refreshTextEmbiggeningOnAllScrollLists();
}

void FSFloaterPoser::onClickBrowsePoseCache()
{
    std::string pathname = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, std::string(POSE_SAVE_SUBDIRECTORY));
    if (!gDirUtilp->fileExists(pathname))
        LLFile::mkdir(pathname);

    gViewerWindow->getWindow()->openFile(pathname);
}

void FSFloaterPoser::onPoseJointsReset()
{
    // This is a double-click function: it needs to run twice within some amount of time to complete.
    auto timeIntervalSinceLastClick = std::chrono::system_clock::now() - _timeLastClickedJointReset;
    _timeLastClickedJointReset = std::chrono::system_clock::now();
    if (timeIntervalSinceLastClick > _doubleClickInterval)
        return;

    LLVOAvatar *avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    if (!_poserAnimator.isPosingAvatar(avatar))
        return;

    auto selectedJoints = getUiSelectedPoserJoints();
    if (selectedJoints.size() < 1)
        return;

    for (auto item : selectedJoints)
    {
        bool currentlyPosing = _poserAnimator.isPosingAvatarJoint(avatar, *item);
        if (currentlyPosing)
            _poserAnimator.resetAvatarJoint(avatar, *item);
    }

    refreshRotationSliders();
    refreshTrackpadCursor();
}

void FSFloaterPoser::onPoseMenuAction(const LLSD &param)
{
    std::string loadStyle = param.asString();
    if (loadStyle.empty())
        return;

    LLScrollListCtrl *posesScrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_LOADSAVE_NAME);
    if (!posesScrollList)
        return;

    LLScrollListItem *item = posesScrollList->getFirstSelected();
    if (!item)
        return;

    std::string poseName = item->getColumn(0)->getValue().asString();

    E_LoadPoseMethods loadType = ROT_POS_AND_SCALES; // the default is to load everything
    if (boost::iequals(loadStyle, "rotation"))
        loadType = ROTATIONS;
    else if (boost::iequals(loadStyle, "position"))
        loadType = POSITIONS;
    else if (boost::iequals(loadStyle, "scale"))
        loadType = SCALES;
    else if (boost::iequals(loadStyle, "rot_pos"))
        loadType = ROTATIONS_AND_POSITIONS;
    else if (boost::iequals(loadStyle, "rot_scale"))
        loadType = ROTATIONS_AND_SCALES;
    else if (boost::iequals(loadStyle, "pos_scale"))
        loadType = POSITIONS_AND_SCALES;
    else if (boost::iequals(loadStyle, "all"))
        loadType = ROT_POS_AND_SCALES;

    LLVOAvatar* avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    loadPoseFromXml(avatar, poseName, loadType);
    refreshJointScrollListMembers();
}

void FSFloaterPoser::onClickLoadLeftHandPose()
{
    // This is a double-click function: it needs to run twice within some amount of time to complete.
    auto timeIntervalSinceLastClick = std::chrono::system_clock::now() - _timeLastClickedJointReset;
    _timeLastClickedJointReset      = std::chrono::system_clock::now();
    if (timeIntervalSinceLastClick > _doubleClickInterval)
        return;

    onClickLoadHandPose(false);
}

void FSFloaterPoser::onClickLoadRightHandPose()
{
    // This is a double-click function: it needs to run twice within some amount of time to complete.
    auto timeIntervalSinceLastClick = std::chrono::system_clock::now() - _timeLastClickedJointReset;
    _timeLastClickedJointReset      = std::chrono::system_clock::now();
    if (timeIntervalSinceLastClick > _doubleClickInterval)
        return;

    onClickLoadHandPose(true);
}

void FSFloaterPoser::onClickLoadHandPose(bool isRightHand)
{
    LLScrollListCtrl* handPosesScrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_HAND_PRESETS_NAME);
    if (!handPosesScrollList)
        return;

    LLScrollListItem* item = handPosesScrollList->getFirstSelected();
    if (!item)
        return;

    std::string poseName = item->getColumn(0)->getValue().asString();
    std::string pathname = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, std::string(POSE_PRESETS_HANDS_SUBDIRECTORY));
    if (!gDirUtilp->fileExists(pathname))
        return;

    std::string fullPath =
        gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, std::string(POSE_PRESETS_HANDS_SUBDIRECTORY), poseName + std::string(POSE_INTERNAL_FORMAT_FILE_EXT));

    LLVOAvatar* avatar   = getUiSelectedAvatar();
    if (!avatar)
        return;
    if (!_poserAnimator.isPosingAvatar(avatar))
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

                const FSPoserAnimator::FSPoserJoint* poserJoint = _poserAnimator.getPoserJointByName(name);
                if (!poserJoint)
                    continue;

                vec3.setValue(control_map["rotation"]);
                _poserAnimator.setJointRotation(avatar, poserJoint, vec3, NONE, SWAP_NOTHING, NEGATE_NOTHING);
            }
        }
    }
    catch ( const std::exception& e )
    {
        LL_WARNS("Posing") << "Threw an exception trying to load a hand pose: " << poseName << " exception: " << e.what() << LL_ENDL;
    }

}

void FSFloaterPoser::loadPoseFromXml(LLVOAvatar* avatar, std::string poseFileName, E_LoadPoseMethods loadMethod)
{
    std::string pathname = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, std::string(POSE_SAVE_SUBDIRECTORY));
    if (!gDirUtilp->fileExists(pathname))
        return;

    if (!_poserAnimator.isPosingAvatar(avatar))
        return;

    std::string fullPath =
        gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, std::string(POSE_SAVE_SUBDIRECTORY), poseFileName + std::string(POSE_INTERNAL_FORMAT_FILE_EXT));

    bool loadRotations = loadMethod == ROTATIONS || loadMethod == ROTATIONS_AND_POSITIONS || loadMethod == ROTATIONS_AND_SCALES ||
                         loadMethod == ROT_POS_AND_SCALES;
    bool loadPositions = loadMethod == POSITIONS || loadMethod == ROTATIONS_AND_POSITIONS || loadMethod == POSITIONS_AND_SCALES ||
                         loadMethod == ROT_POS_AND_SCALES;
    bool loadScales    = loadMethod == SCALES || loadMethod == POSITIONS_AND_SCALES || loadMethod == ROTATIONS_AND_SCALES ||
                         loadMethod == ROT_POS_AND_SCALES;
    bool loadHandsOnly = loadMethod == HAND_RIGHT || loadMethod == HAND_LEFT;

    try
    {
        LLSD       pose;
        llifstream infile;
        LLVector3  vec3;
        bool       enabled;

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
                std::string const &name        = itr->first;
                LLSD const        &control_map = itr->second;

                if (loadHandsOnly && name.find("Hand") == std::string::npos)
                    continue;

                const FSPoserAnimator::FSPoserJoint *poserJoint = _poserAnimator.getPoserJointByName(name);
                if (!poserJoint)
                    continue;

                if (loadHandsOnly && control_map.has("rotation"))
                {
                    vec3.setValue(control_map["rotation"]);

                    _poserAnimator.setJointRotation(avatar, poserJoint, vec3, NONE, SWAP_NOTHING, NEGATE_NOTHING);
                    continue;
                }

                if (loadRotations && control_map.has("rotation"))
                {
                    vec3.setValue(control_map["rotation"]);
                    _poserAnimator.setJointRotation(avatar, poserJoint, vec3, NONE, SWAP_NOTHING, NEGATE_NOTHING); // If we keep defaults BD poses mostly load, except fingers
                }

                if (loadPositions && control_map.has("position"))
                {
                    vec3.setValue(control_map["position"]);
                    _poserAnimator.setJointPosition(avatar, poserJoint, vec3, NONE);
                }

                if (loadScales && control_map.has("scale"))
                {
                    vec3.setValue(control_map["scale"]);
                    _poserAnimator.setJointScale(avatar, poserJoint, vec3, NONE);
                }

                if (control_map.has("enabled"))
                {
                    enabled = control_map["enabled"].asBoolean();
                    _poserAnimator.setPosingAvatarJoint(avatar, *poserJoint, enabled);
                }
            }
        }
    }    
    catch ( const std::exception & e )
    {
        LL_WARNS("Posing") << "Everything caught fire trying to load the pose: " << poseFileName << " exception: " << e.what() << LL_ENDL;
    }

    onJointSelect();
}

void FSFloaterPoser::onPoseStartStop()
{
    LLVOAvatar* avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    bool arePosingSelected = _poserAnimator.isPosingAvatar(avatar);
    if (arePosingSelected)
    {
        _poserAnimator.stopPosingAvatar(avatar);
    }
    else
    {
        if (!couldAnimateAvatar(avatar))
            return;

        if (!havePermissionToAnimateAvatar(avatar))
            return;

        _poserAnimator.tryPosingAvatar(avatar);
    }

    onAvatarsRefresh();
    onAvatarSelect();
}

bool FSFloaterPoser::couldAnimateAvatar(LLVOAvatar *avatar)
{
    if (!avatar || avatar->isDead())
        return false;
    if (avatar->getRegion() != gAgent.getRegion())
        return false;

    return true;
}

bool FSFloaterPoser::havePermissionToAnimateAvatar(LLVOAvatar *avatar)
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
    LLUICtrl *somePanel = getChild<LLUICtrl>(POSER_AVATAR_PANEL_JOINTSPARENT);
    if (somePanel)
        somePanel->setEnabled(enable);

    somePanel = getChild<LLUICtrl>(POSER_AVATAR_PANEL_ADVANCED);
    if (somePanel)
        somePanel->setEnabled(enable);

    LLUICtrl *trackballPanel = getChild<LLUICtrl>(POSER_AVATAR_PANEL_TRACKBALL);
    if (trackballPanel)
        trackballPanel->setEnabled(enable);

    LLButton *someButton = getChild<LLButton>(POSER_AVATAR_PANEL_BUTTON_FLIPPOSE_NAME);
    if (someButton)
        someButton->setEnabled(enable);

    someButton = getChild<LLButton>(POSER_AVATAR_PANEL_BUTTON_FLIPJOINT_NAME);
    if (someButton)
        someButton->setEnabled(enable);

    someButton = getChild<LLButton>(POSER_AVATAR_PANEL_BUTTON_RECAPTURE_NAME);
    if (someButton)
        someButton->setEnabled(enable);

    someButton = getChild<LLButton>(POSER_AVATAR_PANEL_BUTTON_TOGGLEPOSING_NAME);
    if (someButton)
        someButton->setEnabled(enable);

    someButton = getChild<LLButton>(POSER_AVATAR_BUTTON_LOAD_NAME);
    if (someButton)
        someButton->setEnabled(enable);

    someButton = getChild<LLButton>(POSER_AVATAR_BUTTON_SAVE_NAME);
    if (someButton)
        someButton->setEnabled(enable);

    LLLineEditor* poseSaveName = getChild<LLLineEditor>(POSER_AVATAR_LINEEDIT_FILESAVENAME);
    if (poseSaveName)
        poseSaveName->setEnabled(enable);
}

void FSFloaterPoser::refreshJointScrollListMembers()
{
    LLVOAvatar *avatar = getUiSelectedAvatar();

    LLScrollListCtrl *hiddenScrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_HIDDEN_NAME);
    LLScrollListCtrl *bodyJointsScrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_BODYJOINTS_NAME);
    LLScrollListCtrl *faceJointsScrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_FACEJOINTS_NAME);
    LLScrollListCtrl *handsJointsScrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_HANDJOINTS_NAME);
    LLScrollListCtrl *miscJointsScrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_MISCJOINTS_NAME);
    LLScrollListCtrl *collisionVolumesScrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_VOLUMES_NAME);
    if (!bodyJointsScrollList || !faceJointsScrollList || !handsJointsScrollList || !miscJointsScrollList || !collisionVolumesScrollList)
        return;

    bodyJointsScrollList->clearRows();
    faceJointsScrollList->clearRows();
    handsJointsScrollList->clearRows();
    miscJointsScrollList->clearRows();
    collisionVolumesScrollList->clearRows();

    std::vector<FSPoserAnimator::FSPoserJoint>::const_iterator poserJoint_iter;
    for (poserJoint_iter = _poserAnimator.PoserJoints.begin();
         poserJoint_iter != _poserAnimator.PoserJoints.end();
         ++poserJoint_iter)
    {
        LLSD row  = createRowForJoint(poserJoint_iter->jointName(), false);
        if (!row)
            continue;

        LLScrollListItem *item = nullptr;
        bool hasListHeader = hasString(std::string(XML_LIST_HEADER_STRING_PREFIX) + poserJoint_iter->jointName());

        switch (poserJoint_iter->boneType())
        {
            case WHOLEAVATAR:
                item = hiddenScrollList->addElement(row);
                hiddenScrollList->selectFirstItem();
                break;

            case BODY:
                if (hasListHeader)
                    AddHeaderRowToScrollList(poserJoint_iter->jointName(), bodyJointsScrollList);

                item = bodyJointsScrollList->addElement(row);
                break;

            case FACE:
                if (hasListHeader)
                    AddHeaderRowToScrollList(poserJoint_iter->jointName(), faceJointsScrollList);

                item = faceJointsScrollList->addElement(row);
                break;

            case HANDS:
                if (hasListHeader)
                    AddHeaderRowToScrollList(poserJoint_iter->jointName(), handsJointsScrollList);

                item = handsJointsScrollList->addElement(row);
                break;

            case MISC:
                if (hasListHeader)
                    AddHeaderRowToScrollList(poserJoint_iter->jointName(), miscJointsScrollList);

                item = miscJointsScrollList->addElement(row);
                break;

            case COL_VOLUMES:
                if (hasListHeader)
                    AddHeaderRowToScrollList(poserJoint_iter->jointName(), collisionVolumesScrollList);

                item = collisionVolumesScrollList->addElement(row);
                break;
        }

        if (item)
        {
            item->setUserdata((void*) &*poserJoint_iter);

            if (_poserAnimator.isPosingAvatarJoint(avatar, *poserJoint_iter))
                ((LLScrollListText *) item->getColumn(COL_NAME))->setFontStyle(LLFontGL::BOLD);
            else
                ((LLScrollListText *) item->getColumn(COL_NAME))->setFontStyle(LLFontGL::NORMAL);
        }
    }
}

void FSFloaterPoser::AddHeaderRowToScrollList(std::string jointName, LLScrollListCtrl *bodyJointsScrollList)
{
    LLSD headerRow = createRowForJoint(jointName, true);
    if (!headerRow)
        return;

    LLScrollListItem *hdrRow = bodyJointsScrollList->addElement(headerRow);
    hdrRow->setEnabled(FALSE);
}

LLSD FSFloaterPoser::createRowForJoint(std::string jointName, bool isHeaderRow)
{
    if (jointName.empty())
        return NULL;

    std::string headerValue = "";
    if (hasString("icon_category") && hasString("icon_bone"))
        headerValue = isHeaderRow ? getString("icon_category") : getString("icon_bone");

    std::string jointValue    = jointName;
    std::string parameterName = (isHeaderRow ? std::string(XML_LIST_HEADER_STRING_PREFIX) : std::string(XML_LIST_TITLE_STRING_PREFIX)) + jointName;
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
    if (this->isMinimized())
        return;

    // Get the load/save button toggle state, find the load/save panel, and set its visibility
    LLButton *yourPosesButton = getChild<LLButton>(POSER_AVATAR_TOGGLEBUTTON_LOADSAVE);
    if (!yourPosesButton)
        return;

    bool      loadSavePanelExpanded = yourPosesButton->getValue().asBoolean();
    LLUICtrl *loadSavePanel         = getChild<LLUICtrl>(POSER_AVATAR_PANEL_LOADSAVE_NAME);
    if (!loadSavePanel)
        return;

    loadSavePanel->setVisible(loadSavePanelExpanded);
    LLButton *loadPosesButton = getChild<LLButton>(POSER_AVATAR_BUTTON_LOAD_NAME);
    if (loadPosesButton)
        loadPosesButton->setVisible(loadSavePanelExpanded);

    LLButton *savePosesButton = getChild<LLButton>(POSER_AVATAR_BUTTON_SAVE_NAME);
    if (savePosesButton)
        savePosesButton->setVisible(loadSavePanelExpanded);

    LLButton *exploreFolderButton = getChild<LLButton>(POSER_AVATAR_BUTTON_BROWSEFOLDER_NAME);
    if (exploreFolderButton)
        exploreFolderButton->setVisible(loadSavePanelExpanded);

    // change the width of the Poser panel for the (dis)appearance of the load/save panel
    S32 currentWidth = this->getRect().getWidth();
    S32 loadSavePanelWidth = loadSavePanel->getRect().getWidth();

    S32 poserFloaterHeight = this->getRect().getHeight();
    S32 poserFloaterWidth  = loadSavePanelExpanded ? currentWidth + loadSavePanelWidth : currentWidth - loadSavePanelWidth;

    if (poserFloaterWidth < 0)
        return;

    this->reshape(poserFloaterWidth, poserFloaterHeight);

    if (loadSavePanelExpanded)
        refreshPoseScroll(POSER_AVATAR_SCROLLLIST_LOADSAVE_NAME, POSE_SAVE_SUBDIRECTORY);

    showOrHideAdvancedSaveOptions();
}

void FSFloaterPoser::showOrHideAdvancedSaveOptions()
{
    LLButton* yourPosesButton = getChild<LLButton>(POSER_AVATAR_TOGGLEBUTTON_LOADSAVE);
    if (!yourPosesButton)
        return;

    LLButton* advancedButton = getChild<LLButton>(POSER_AVATAR_ADVANCED_TOGGLEBUTTON_NAME);
    if (!advancedButton)
        return;

    LLUICtrl* advSavePanel = getChild<LLUICtrl>(POSER_AVATAR_ADVANCED_SAVEOPTIONSPANEL_NAME);
    if (!advSavePanel)
        return;

    bool loadSavePanelExpanded = yourPosesButton->getValue().asBoolean();
    bool advancedPanelExpanded = advancedButton->getValue().asBoolean();

    advSavePanel->setVisible(loadSavePanelExpanded && advancedPanelExpanded);
}

void FSFloaterPoser::onToggleMirrorChange() { setRotationChangeButtons(true, false, false); }

void FSFloaterPoser::onToggleSympatheticChange() { setRotationChangeButtons(false, true, false); }

void FSFloaterPoser::onToggleDeltaModeChange() { setRotationChangeButtons(false, false, true); }

void FSFloaterPoser::setRotationChangeButtons(bool togglingMirror, bool togglingSympathetic, bool togglingDelta)
{
    LLButton *toggleMirrorButton = getChild<LLButton>(POSER_AVATAR_TOGGLEBUTTON_MIRROR);
    if (!toggleMirrorButton)
        return;
    LLButton *toggleSympatheticButton = getChild<LLButton>(POSER_AVATAR_TOGGLEBUTTON_SYMPATH);
    if (!toggleSympatheticButton)
        return;
    LLButton* deltaModeToggleButton = getChild<LLButton>(POSER_AVATAR_BUTTON_DELTAMODE);
    if (!deltaModeToggleButton)
        return;

    if (togglingSympathetic || togglingDelta)
        toggleMirrorButton->setValue(false);

    if (togglingMirror || togglingDelta)
        toggleSympatheticButton->setValue(false);

    if (togglingMirror || togglingSympathetic)
        deltaModeToggleButton->setValue(false);

    refreshTrackpadCursor();
}

void FSFloaterPoser::onUndoLastRotation()
{
    LLVOAvatar* avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    if (!_poserAnimator.isPosingAvatar(avatar))
        return;

    auto selectedJoints = getUiSelectedPoserJoints();
    if (selectedJoints.size() < 1)
        return;

    for (auto item : selectedJoints)
    {
        bool currentlyPosing = _poserAnimator.isPosingAvatarJoint(avatar, *item);
        if (currentlyPosing)
            _poserAnimator.undoLastJointRotation(avatar, *item, getUiSelectedBoneDeflectionStyle());
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

    if (!_poserAnimator.isPosingAvatar(avatar))
        return;

    auto selectedJoints = getUiSelectedPoserJoints();
    if (selectedJoints.size() < 1)
        return;

    for (auto item : selectedJoints)
    {
        bool currentlyPosing = _poserAnimator.isPosingAvatarJoint(avatar, *item);
        if (currentlyPosing)
            _poserAnimator.undoLastJointPosition(avatar, *item, getUiSelectedBoneDeflectionStyle());
    }

    refreshAdvancedPositionSliders();
    refreshAvatarPositionSliders();
}

void FSFloaterPoser::onUndoLastScale()
{
    LLVOAvatar* avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    if (!_poserAnimator.isPosingAvatar(avatar))
        return;

    auto selectedJoints = getUiSelectedPoserJoints();
    if (selectedJoints.size() < 1)
        return;

    for (auto item : selectedJoints)
    {
        bool currentlyPosing = _poserAnimator.isPosingAvatarJoint(avatar, *item);
        if (currentlyPosing)
            _poserAnimator.undoLastJointScale(avatar, *item, getUiSelectedBoneDeflectionStyle());
    }

    refreshAdvancedScaleSliders();
}

void FSFloaterPoser::onResetPosition()
{
    // This is a double-click function: it needs to run twice within some amount of time to complete.
    auto timeIntervalSinceLastClick = std::chrono::system_clock::now() - _timeLastClickedJointReset;
    _timeLastClickedJointReset      = std::chrono::system_clock::now();
    if (timeIntervalSinceLastClick > _doubleClickInterval)
        return;

    LLVOAvatar* avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    if (!_poserAnimator.isPosingAvatar(avatar))
        return;

    auto selectedJoints = getUiSelectedPoserJoints();
    if (selectedJoints.size() < 1)
        return;

    for (auto item : selectedJoints)
    {
        bool currentlyPosing = _poserAnimator.isPosingAvatarJoint(avatar, *item);
        if (currentlyPosing)
            _poserAnimator.resetJointPosition(avatar, *item, getUiSelectedBoneDeflectionStyle());
    }

    refreshAdvancedPositionSliders();
    refreshAvatarPositionSliders();
}

void FSFloaterPoser::onResetScale()
{
    // This is a double-click function: it needs to run twice within some amount of time to complete.
    auto timeIntervalSinceLastClick = std::chrono::system_clock::now() - _timeLastClickedJointReset;
    _timeLastClickedJointReset      = std::chrono::system_clock::now();
    if (timeIntervalSinceLastClick > _doubleClickInterval)
        return;

    LLVOAvatar* avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    if (!_poserAnimator.isPosingAvatar(avatar))
        return;

    auto selectedJoints = getUiSelectedPoserJoints();
    if (selectedJoints.size() < 1)
        return;

    for (auto item : selectedJoints)
    {
        bool currentlyPosing = _poserAnimator.isPosingAvatarJoint(avatar, *item);
        if (currentlyPosing)
            _poserAnimator.resetJointScale(avatar, *item, getUiSelectedBoneDeflectionStyle());
    }

    refreshAdvancedScaleSliders();
}

void FSFloaterPoser::onRedoLastRotation()
{
    LLVOAvatar* avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    if (!_poserAnimator.isPosingAvatar(avatar))
        return;

    auto selectedJoints = getUiSelectedPoserJoints();
    if (selectedJoints.size() < 1)
        return;

    for (auto item : selectedJoints)
    {
        bool currentlyPosing = _poserAnimator.isPosingAvatarJoint(avatar, *item);
        if (currentlyPosing)
            _poserAnimator.redoLastJointRotation(avatar, *item, getUiSelectedBoneDeflectionStyle());
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

    if (!_poserAnimator.isPosingAvatar(avatar))
        return;

    auto selectedJoints = getUiSelectedPoserJoints();
    if (selectedJoints.size() < 1)
        return;

    for (auto item : selectedJoints)
    {
        bool currentlyPosing = _poserAnimator.isPosingAvatarJoint(avatar, *item);
        if (currentlyPosing)
            _poserAnimator.redoLastJointPosition(avatar, *item, getUiSelectedBoneDeflectionStyle());
    }

    refreshAdvancedPositionSliders();
    refreshAvatarPositionSliders();
}

void FSFloaterPoser::onRedoLastScale()
{
    LLVOAvatar* avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    if (!_poserAnimator.isPosingAvatar(avatar))
        return;

    auto selectedJoints = getUiSelectedPoserJoints();
    if (selectedJoints.size() < 1)
        return;

    for (auto item : selectedJoints)
    {
        bool currentlyPosing = _poserAnimator.isPosingAvatarJoint(avatar, *item);
        if (currentlyPosing)
            _poserAnimator.redoLastJointScale(avatar, *item, getUiSelectedBoneDeflectionStyle());
    }

    refreshAdvancedScaleSliders();
}

void FSFloaterPoser::enableOrDisableRedoButton()
{
    LLButton* redoButton = getChild<LLButton>(POSER_AVATAR_BUTTON_REDO);
    if (!redoButton)
        return;

    LLVOAvatar* avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    if (!_poserAnimator.isPosingAvatar(avatar))
        return;

    auto selectedJoints = getUiSelectedPoserJoints();
    if (selectedJoints.size() < 1)
        return;

    bool shouldEnableRedoButton = false;
    for (auto item : selectedJoints)
    {
        bool currentlyPosing = _poserAnimator.isPosingAvatarJoint(avatar, *item);
        if (currentlyPosing)
            shouldEnableRedoButton |= _poserAnimator.canRedoJointRotation(avatar, *item);
    }

    redoButton->setEnabled(shouldEnableRedoButton);
}

void FSFloaterPoser::onOpenSetAdvancedPanel()
{
    LLButton* advancedButton = getChild<LLButton>(POSER_AVATAR_ADVANCED_TOGGLEBUTTON_NAME);
    if (!advancedButton)
        return;

    bool advancedPanelExpanded = advancedButton->getValue().asBoolean();
    if (advancedPanelExpanded)
        onToggleAdvancedPanel();
}

void FSFloaterPoser::onToggleAdvancedPanel()
{
    if (this->isMinimized())
        return;

    LLButton *advancedButton = getChild<LLButton>(POSER_AVATAR_ADVANCED_TOGGLEBUTTON_NAME);
    if (!advancedButton)
        return;

    bool      advancedPanelExpanded = advancedButton->getValue().asBoolean();
    LLUICtrl* advancedPanel         = getChild<LLUICtrl>(POSER_AVATAR_PANEL_ADVANCED);
    if (!advancedPanel)
        return;

    advancedPanel->setVisible(advancedPanelExpanded);

    // change the height of the Poser panel
    S32 currentHeight       = this->getRect().getHeight();
    S32 advancedPanelHeight = advancedPanel->getRect().getHeight();

    S32 poserFloaterHeight = advancedPanelExpanded ? currentHeight + advancedPanelHeight : currentHeight - advancedPanelHeight;
    S32 poserFloaterWidth  = this->getRect().getWidth();

    if (poserFloaterHeight < 0)
        return;

    this->reshape(poserFloaterWidth, poserFloaterHeight);
    showOrHideAdvancedSaveOptions();
    onJointSelect();
}

std::vector<FSPoserAnimator::FSPoserJoint *> FSFloaterPoser::getUiSelectedPoserJoints() const
{
    std::vector<FSPoserAnimator::FSPoserJoint *> joints;

    LLTabContainer *tabGroup = getChild<LLTabContainer>(POSER_AVATAR_TABGROUP_JOINTS);
    if (!tabGroup)
    {
        return joints;
    }

    std::string activeTabName = tabGroup->getCurrentPanel()->getName();
    if (activeTabName.empty())
    {
        return joints;
    }

    std::string scrollListName;

    if (boost::iequals(activeTabName, POSER_AVATAR_TAB_POSITION))
    {
        scrollListName = POSER_AVATAR_SCROLLLIST_HIDDEN_NAME;
    }
    else if (boost::iequals(activeTabName, POSER_AVATAR_TAB_BODY))
    {
        scrollListName = POSER_AVATAR_SCROLLLIST_BODYJOINTS_NAME;
    }
    else if (boost::iequals(activeTabName, POSER_AVATAR_TAB_FACE))
    {
        scrollListName = POSER_AVATAR_SCROLLLIST_FACEJOINTS_NAME;
    }
    else if (boost::iequals(activeTabName, POSER_AVATAR_TAB_HANDS))
    {
        tabGroup = getChild<LLTabContainer>(POSER_AVATAR_TAB_HANDS);
        if (!tabGroup)
        {
            return joints;
        }

        activeTabName = tabGroup->getCurrentPanel()->getName();
        if (activeTabName.empty())
        {
            return joints;
        }

        if (boost::iequals(activeTabName, POSER_AVATAR_TAB_HANDJOINTS))
        {
            scrollListName = POSER_AVATAR_SCROLLLIST_HANDJOINTS_NAME;
        }
    }
    else if (boost::iequals(activeTabName, POSER_AVATAR_TAB_MISC))
    {
        scrollListName = POSER_AVATAR_SCROLLLIST_MISCJOINTS_NAME;
    }
    else if (boost::iequals(activeTabName, POSER_AVATAR_TAB_VOLUMES))
    {
        scrollListName = POSER_AVATAR_SCROLLLIST_VOLUMES_NAME;
    }

    if (scrollListName.empty())
    {
        return joints;
    }

    LLScrollListCtrl *scrollList = getChild<LLScrollListCtrl>(scrollListName);
    if (!scrollList)
    {
        return joints;
    }
    for (auto item : scrollList->getAllSelected())
    {
        auto *userData = static_cast<FSPoserAnimator::FSPoserJoint *>(item->getUserdata());
        if (userData)
        {
            joints.push_back(userData);
        }
    }

    return joints;
}

E_BoneDeflectionStyles FSFloaterPoser::getUiSelectedBoneDeflectionStyle()
{

    // Use early return to reduce nesting and improve readability
    auto* toggleMirrorButton = getChild<LLButton>(POSER_AVATAR_TOGGLEBUTTON_MIRROR);
    auto* toggleSympatheticButton = getChild<LLButton>(POSER_AVATAR_TOGGLEBUTTON_SYMPATH);
    auto* deltaModeToggleButton = getChild<LLButton>(POSER_AVATAR_BUTTON_DELTAMODE);
    if (!toggleMirrorButton || !toggleSympatheticButton || !deltaModeToggleButton)
    {
        return NONE;
    }

    if (toggleMirrorButton->getValue().asBoolean())
    {
        return MIRROR;
    }
    if (toggleSympatheticButton->getValue().asBoolean())
    {
        return SYMPATHETIC;
    }
    if (deltaModeToggleButton->getValue().asBoolean())
    {
        return DELTAMODE;
    }

    return NONE;
}

LLVOAvatar* FSFloaterPoser::getUiSelectedAvatar()
{
    LLScrollListCtrl *avatarScrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_AVATARSELECTION);
    if (!avatarScrollList)
        return nullptr;

    LLScrollListItem *item = avatarScrollList->getFirstSelected();
    if (!item)
        return nullptr;

    LLScrollListCell* cell = item->getColumn(COL_UUID);
    if (!cell)
        return nullptr;

    LLUUID selectedAvatarId = cell->getValue().asUUID();

    return getAvatarByUuid(selectedAvatarId);
}

void FSFloaterPoser::setPoseSaveFileTextBoxToUiSelectedAvatarSaveFileName()
{
    LLScrollListCtrl* avatarScrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_AVATARSELECTION);
    if (!avatarScrollList)
        return;

    LLScrollListItem* item = avatarScrollList->getFirstSelected();
    if (!item)
        return;

    LLScrollListCell* cell = item->getColumn(COL_SAVE);
    if (!cell)
        return;

    std::string lastSetName = cell->getValue().asString();
    if (lastSetName.empty())
        return;

    LLLineEditor* poseSaveName = getChild<LLLineEditor>(POSER_AVATAR_LINEEDIT_FILESAVENAME);
    if (!poseSaveName)
        return;

    LLStringExplicit name = LLStringExplicit(lastSetName);
    poseSaveName->setText(name);
}

void FSFloaterPoser::setUiSelectedAvatarSaveFileName(std::string saveFileName)
{
    LLScrollListCtrl *avatarScrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_AVATARSELECTION);
    if (!avatarScrollList)
        return;

    LLScrollListItem *item = avatarScrollList->getFirstSelected();
    if (!item)
        return;

    LLScrollListCell* cell = item->getColumn(COL_SAVE);
    if (!cell)
        return;

    return cell->setValue(saveFileName);
}

LLVOAvatar* FSFloaterPoser::getAvatarByUuid(LLUUID avatarToFind)
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
    LLSliderCtrl *xposAdvSlider = getChild<LLSliderCtrl>(POSER_AVATAR_ADV_SLIDER_POSX_NAME);
    LLSliderCtrl *yposAdvSlider = getChild<LLSliderCtrl>(POSER_AVATAR_ADV_SLIDER_POSY_NAME);
    LLSliderCtrl *zposAdvSlider = getChild<LLSliderCtrl>(POSER_AVATAR_ADV_SLIDER_POSZ_NAME);
    if (!xposAdvSlider || !yposAdvSlider || !zposAdvSlider)
        return;

    F32 posX = (F32) xposAdvSlider->getValue().asReal();
    F32 posY = (F32) yposAdvSlider->getValue().asReal();
    F32 posZ = (F32) zposAdvSlider->getValue().asReal();

    setSelectedJointsPosition(posX, posY, posZ);
    refreshAvatarPositionSliders();
}

void FSFloaterPoser::onAdvancedScaleSet()
{
    LLSliderCtrl *scalex = getChild<LLSliderCtrl>(POSER_AVATAR_ADV_SLIDER_SCALEX_NAME);
    LLSliderCtrl *scaley = getChild<LLSliderCtrl>(POSER_AVATAR_ADV_SLIDER_SCALEY_NAME);
    LLSliderCtrl *scalez = getChild<LLSliderCtrl>(POSER_AVATAR_ADV_SLIDER_SCALEZ_NAME);
    if (!scalex || !scaley || !scalez)
        return;

    F32 scX = (F32) scalex->getValue().asReal();
    F32 scY = (F32) scaley->getValue().asReal();
    F32 scZ = (F32) scalez->getValue().asReal();

    setSelectedJointsScale(scX, scY, scZ);
}

void FSFloaterPoser::onAvatarPositionSet()
{
    LLSliderCtrl *xposSlider = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_POSX_NAME);
    LLSliderCtrl *yposSlider = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_POSY_NAME);
    LLSliderCtrl *zposSlider = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_POSZ_NAME);
    if (!xposSlider || !yposSlider || !zposSlider)
        return;

    F32 posX = (F32) xposSlider->getValue().asReal();
    F32 posY = (F32) yposSlider->getValue().asReal();
    F32 posZ = (F32) zposSlider->getValue().asReal();

    setSelectedJointsPosition(posX, posY, posZ);
    refreshAdvancedPositionSliders();
}

void FSFloaterPoser::onLimbTrackballChanged()
{
    FSVirtualTrackpad *trackBall = getChild<FSVirtualTrackpad>(POSER_AVATAR_TRACKBALL_NAME);
    if (!trackBall)
        return;

    LLVector3 trackPadPos;
    LLSD position = trackBall->getValue();
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

    // if the trackpad is in 'infinite scroll' mode, it can produce normalized-values outside the range of the sliders; this wraps them to by the slider full-scale
    yaw   = unWrapScale(yaw) * normalTrackpadRangeInRads;
    pitch = unWrapScale(pitch) * normalTrackpadRangeInRads;
    roll  = unWrapScale(roll) * normalTrackpadRangeInRads;

    bool deltaMode = false;
    LLButton* deltaModeToggleButton = getChild<LLButton>(POSER_AVATAR_BUTTON_DELTAMODE);
    if (deltaModeToggleButton)
        deltaMode = deltaModeToggleButton->getValue().asBoolean();

    if (deltaMode)
    {
        F32 deltaYaw, deltaPitch, deltaRoll;
        LLSD deltaPosition = trackBall->getValueDelta();
        LLVector3 trackPadDeltaPos;
        if (deltaPosition.isArray() && deltaPosition.size() == 3)
        {
            trackPadDeltaPos.setValue(deltaPosition);
            deltaYaw   = trackPadDeltaPos[VX] * normalTrackpadRangeInRads;
            deltaPitch = trackPadDeltaPos[VY] * normalTrackpadRangeInRads;
            deltaRoll  = trackPadDeltaPos[VZ] * normalTrackpadRangeInRads;
            deltaYaw *= trackPadSensitivity;
            deltaPitch *= trackPadSensitivity;
        
            setSelectedJointsRotation(deltaYaw, deltaPitch, deltaRoll);
        }
    }
    else
        setSelectedJointsRotation(yaw, pitch, roll);

    // WARNING!
    // as tempting as it is to refactor the following to refreshRotationSliders(), don't.
    // getRotationOfFirstSelectedJoint/setSelectedJointsRotation are
    // not necessarily symmetric functions (see their remarks).
    LLSliderCtrl *yawSlider   = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_YAW_NAME);
    LLSliderCtrl *pitchSlider = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_PITCH_NAME);
    LLSliderCtrl* rollSlider  = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_ROLL_NAME);
    if (!yawSlider || !pitchSlider || !rollSlider)
        return;

    yawSlider->setValue(yaw *= RAD_TO_DEG);
    pitchSlider->setValue(pitch *= RAD_TO_DEG);
    rollSlider->setValue(roll *= RAD_TO_DEG);
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
    LLSliderCtrl *yawSlider = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_YAW_NAME);
    LLSliderCtrl *pitchSlider = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_PITCH_NAME);
    LLSliderCtrl *rollSlider = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_ROLL_NAME);
    if (!yawSlider || !pitchSlider || !rollSlider)
        return;

    F32 yaw   = (F32) yawSlider->getValue().asReal();
    F32 pitch = (F32) pitchSlider->getValue().asReal();
    F32 roll  = (F32) rollSlider->getValue().asReal();

    yaw *= DEG_TO_RAD;
    pitch *= DEG_TO_RAD;
    roll *= DEG_TO_RAD;

    setSelectedJointsRotation(yaw, pitch, roll);

    // WARNING!
    // as tempting as it is to refactor the following to refreshTrackpadCursor(), don't.
    // getRotationOfFirstSelectedJoint/setSelectedJointsRotation are
    // not necessarily symmetric functions (see their remarks).
    FSVirtualTrackpad *trackBall = getChild<FSVirtualTrackpad>(POSER_AVATAR_TRACKBALL_NAME);
    if (!trackBall)
        return;

    F32 trackPadSensitivity = llmax(gSavedSettings.getF32(POSER_TRACKPAD_SENSITIVITY_SAVE_KEY), 0.0001f);
    yaw /= trackPadSensitivity;
    pitch /= trackPadSensitivity;

    yaw /= normalTrackpadRangeInRads;
    pitch /= normalTrackpadRangeInRads;
    roll /= normalTrackpadRangeInRads;

    trackBall->setValue(yaw, pitch, roll);
}

void FSFloaterPoser::onAdjustTrackpadSensitivity()
{
    LLSliderCtrl* trackpadSensitivitySlider = getChild<LLSliderCtrl>(POSER_TRACKPAD_SENSITIVITY_SLIDER_NAME);
    if (!trackpadSensitivitySlider)
        return;

    gSavedSettings.setF32(POSER_TRACKPAD_SENSITIVITY_SAVE_KEY, (F32) trackpadSensitivitySlider->getValue().asReal());
    refreshTrackpadCursor();
}

void FSFloaterPoser::refreshTrackpadCursor()
{
    FSVirtualTrackpad *trackBall = getChild<FSVirtualTrackpad>(POSER_AVATAR_TRACKBALL_NAME);
    if (!trackBall)
        return;

    LLSliderCtrl* yawSlider   = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_YAW_NAME);
    LLSliderCtrl* pitchSlider = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_PITCH_NAME);
    LLSliderCtrl* rollSlider = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_ROLL_NAME);
    if (!yawSlider || !pitchSlider || !rollSlider)
        return;

    F32 axis1 = (F32) yawSlider->getValue().asReal();
    F32 axis2 = (F32) pitchSlider->getValue().asReal();
    F32 axis3 = (F32) rollSlider->getValue().asReal();

    axis1 *= DEG_TO_RAD;
    axis2 *= DEG_TO_RAD;
    axis3 *= DEG_TO_RAD;

    axis1 /= normalTrackpadRangeInRads;
    axis2 /= normalTrackpadRangeInRads;
    axis3 /= normalTrackpadRangeInRads;

    F32 trackPadSensitivity = llmax(gSavedSettings.getF32(POSER_TRACKPAD_SENSITIVITY_SAVE_KEY), 0.0001f);
    axis1 /= trackPadSensitivity;
    axis2 /= trackPadSensitivity;

    trackBall->setValue(axis1, axis2, axis3);
}

/// <summary>
/// This only sets the position sliders of the 'basic' view (not the advanced sliders).
/// </summary>
void FSFloaterPoser::refreshAvatarPositionSliders()
{
    LLTabContainer *jointTabGroup = getChild<LLTabContainer>(POSER_AVATAR_TABGROUP_JOINTS);
    if (!jointTabGroup)
        return;

    std::string activeTabName = jointTabGroup->getCurrentPanel()->getName();
    if (activeTabName.empty())
        return;

    if (!boost::iequals(activeTabName, POSER_AVATAR_TAB_POSITION))
        return; // if the active tab isn't the av position one, don't set anything.

    LLSliderCtrl *xSlider = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_POSX_NAME);
    LLSliderCtrl *ySlider = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_POSY_NAME);
    LLSliderCtrl *zSlider = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_POSZ_NAME);
    if (!xSlider || !ySlider || !zSlider)
        return;

    LLVector3 position = getPositionOfFirstSelectedJoint();

    xSlider->setValue(position.mV[VX]);
    ySlider->setValue(position.mV[VY]);
    zSlider->setValue(position.mV[VZ]);
}

void FSFloaterPoser::refreshRotationSliders()
{
    LLSliderCtrl *yawSlider   = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_YAW_NAME);
    LLSliderCtrl *pitchSlider = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_PITCH_NAME);
    LLSliderCtrl *rollSlider  = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_ROLL_NAME);
    if (!yawSlider || !pitchSlider || !rollSlider)
        return;

    LLVector3 rotation = getRotationOfFirstSelectedJoint();

    yawSlider->setValue(rotation.mV[VX] *= RAD_TO_DEG);
    pitchSlider->setValue(rotation.mV[VY] *= RAD_TO_DEG);
    rollSlider->setValue(rotation.mV[VZ] *= RAD_TO_DEG);
}

void FSFloaterPoser::refreshAdvancedPositionSliders()
{
    LLSliderCtrl *xSlider = getChild<LLSliderCtrl>(POSER_AVATAR_ADV_SLIDER_POSX_NAME);
    LLSliderCtrl *ySlider = getChild<LLSliderCtrl>(POSER_AVATAR_ADV_SLIDER_POSY_NAME);
    LLSliderCtrl *zSlider = getChild<LLSliderCtrl>(POSER_AVATAR_ADV_SLIDER_POSZ_NAME);
    if (!xSlider || !ySlider || !zSlider)
        return;

    LLVector3 position = getPositionOfFirstSelectedJoint();

    xSlider->setValue(position.mV[VX]);
    ySlider->setValue(position.mV[VY]);
    zSlider->setValue(position.mV[VZ]);
}

void FSFloaterPoser::refreshAdvancedScaleSliders()
{
    LLSliderCtrl *xSlider     = getChild<LLSliderCtrl>(POSER_AVATAR_ADV_SLIDER_SCALEX_NAME);
    LLSliderCtrl *ySlider    = getChild<LLSliderCtrl>(POSER_AVATAR_ADV_SLIDER_SCALEY_NAME);
    LLSliderCtrl *zSlider     = getChild<LLSliderCtrl>(POSER_AVATAR_ADV_SLIDER_SCALEZ_NAME);
    if (!xSlider || !ySlider || !zSlider)
        return;

    LLVector3 rotation = getScaleOfFirstSelectedJoint();

    xSlider->setValue(rotation.mV[VX]);
    ySlider->setValue(rotation.mV[VY]);
    zSlider->setValue(rotation.mV[VZ]);
}

void FSFloaterPoser::setSelectedJointsPosition(F32 x, F32 y, F32 z)
{
    LLVOAvatar *avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    if (!_poserAnimator.isPosingAvatar(avatar))
        return;

    E_BoneDeflectionStyles defl = getUiSelectedBoneDeflectionStyle();
    LLVector3              vec3 = LLVector3(x, y, z);

    for (auto item : getUiSelectedPoserJoints())
    {
        bool currentlyPosingJoint = _poserAnimator.isPosingAvatarJoint(avatar, *item);
        if (!currentlyPosingJoint)
            continue;

        _poserAnimator.setJointPosition(avatar, item, vec3, defl);
    }
}

void FSFloaterPoser::setSelectedJointsRotation(F32 yawInRadians, F32 pitchInRadians, F32 rollInRadians)
{
    LLVOAvatar *avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    if (!_poserAnimator.isPosingAvatar(avatar))
        return;

    E_BoneDeflectionStyles defl = getUiSelectedBoneDeflectionStyle();
    LLVector3              vec3 = LLVector3(yawInRadians, pitchInRadians, rollInRadians);

    for (auto item : getUiSelectedPoserJoints())
    {
        bool currentlyPosingJoint = _poserAnimator.isPosingAvatarJoint(avatar, *item);
        if (!currentlyPosingJoint)
            continue;

        _poserAnimator.setJointRotation(avatar, item, vec3, defl, getJointTranslation(item->jointName()),
                                getJointNegation(item->jointName()));
    }
}

void FSFloaterPoser::setSelectedJointsScale(F32 x, F32 y, F32 z)
{
    LLVOAvatar *avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    if (!_poserAnimator.isPosingAvatar(avatar))
        return;

    E_BoneDeflectionStyles defl = getUiSelectedBoneDeflectionStyle();
    LLVector3              vec3 = LLVector3(x, y, z);

    for (auto item : getUiSelectedPoserJoints())
    {
        bool currentlyPosingJoint = _poserAnimator.isPosingAvatarJoint(avatar, *item);
        if (!currentlyPosingJoint)
            continue;

        _poserAnimator.setJointScale(avatar, item, vec3, defl);
    }
}

LLVector3 FSFloaterPoser::getRotationOfFirstSelectedJoint()
{
    LLVector3 rotation;
    auto selectedJoints = getUiSelectedPoserJoints();
    if (selectedJoints.size() < 1)
        return rotation;

    LLVOAvatar *avatar = getUiSelectedAvatar();
    if (!avatar)
        return rotation;

    if (!_poserAnimator.isPosingAvatar(avatar))
        return rotation;

    rotation = _poserAnimator.getJointRotation(avatar, *selectedJoints.front(), getJointTranslation(selectedJoints.front()->jointName()),
                                               getJointNegation(selectedJoints.front()->jointName()));

    return rotation;
}

LLVector3 FSFloaterPoser::getPositionOfFirstSelectedJoint()
{
    LLVector3 position;
    auto selectedJoints = getUiSelectedPoserJoints();
    if (selectedJoints.size() < 1)
        return position;

    LLVOAvatar *avatar = getUiSelectedAvatar();
    if (!avatar)
        return position;

    if (!_poserAnimator.isPosingAvatar(avatar))
        return position;

    position = _poserAnimator.getJointPosition(avatar, *selectedJoints.front());
    return position;
}

LLVector3 FSFloaterPoser::getScaleOfFirstSelectedJoint()
{
    LLVector3 scale;
    auto selectedJoints = getUiSelectedPoserJoints();
    if (selectedJoints.size() < 1)
        return scale;

    LLVOAvatar *avatar = getUiSelectedAvatar();
    if (!avatar)
        return scale;

    if (!_poserAnimator.isPosingAvatar(avatar))
        return scale;

    scale = _poserAnimator.getJointScale(avatar, *selectedJoints.front());
    return scale;
}

void FSFloaterPoser::onJointSelect()
{
    refreshAvatarPositionSliders();
    refreshRotationSliders();
    refreshTrackpadCursor();
    enableOrDisableRedoButton();

    LLButton *advancedButton = getChild<LLButton>(POSER_AVATAR_ADVANCED_TOGGLEBUTTON_NAME);
    if (!advancedButton)
        return;

    if (advancedButton->getValue().asBoolean())
    {
        refreshAdvancedPositionSliders();
        refreshAdvancedScaleSliders();
    }
}

E_BoneAxisTranslation FSFloaterPoser::getJointTranslation(std::string jointName)
{
    if (jointName.empty())
        return SWAP_NOTHING;

    bool hasTransformParameter = hasString(std::string(XML_JOINT_TRANSFORM_STRING_PREFIX) + jointName);
    if (!hasTransformParameter)
        return SWAP_NOTHING;

    std::string paramValue = getString(std::string(XML_JOINT_TRANSFORM_STRING_PREFIX) + jointName);

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

S32 FSFloaterPoser::getJointNegation(std::string jointName)
{
    S32 result = NEGATE_NOTHING;

    if (jointName.empty())
        return result;

    bool hasTransformParameter = hasString(std::string(XML_JOINT_TRANSFORM_STRING_PREFIX) + jointName);
    if (!hasTransformParameter)
        return result;

    std::string paramValue = getString(std::string(XML_JOINT_TRANSFORM_STRING_PREFIX) + jointName);

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
    LLButton *startStopButton = getChild<LLButton>(POSER_AVATAR_STARTSTOP_POSING_BUTTON_NAME);
    if (!startStopButton)
        return;

    LLVOAvatar *avatar = getUiSelectedAvatar();
    if (couldAnimateAvatar(avatar))
        startStopButton->setEnabled(true);
    else
        startStopButton->setEnabled(false);

    bool arePosingSelected = _poserAnimator.isPosingAvatar(avatar);
    startStopButton->setValue(arePosingSelected);
    poseControlsEnable(arePosingSelected);
    refreshTextEmbiggeningOnAllScrollLists();
    onJointSelect();
    setPoseSaveFileTextBoxToUiSelectedAvatarSaveFileName();
}

uuid_vec_t FSFloaterPoser::getNearbyAvatarsAndAnimeshes()
{
    uuid_vec_t avatar_ids;

    for (LLCharacter* character : LLCharacter::sInstances)
    {
        LLVOAvatar* avatar = dynamic_cast<LLVOAvatar*>(character);
        if (!couldAnimateAvatar(avatar))
            continue;

        avatar_ids.emplace_back(character->getID());
    }

    return avatar_ids;
}

uuid_vec_t FSFloaterPoser::getCurrentlyListedAvatarsAndAnimeshes()
{
    uuid_vec_t avatar_ids;

    LLScrollListCtrl* avatarScrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_AVATARSELECTION);
    if (!avatarScrollList)
        return avatar_ids;

    for (auto listItem : avatarScrollList->getAllData())
    {
        LLScrollListCell* cell = listItem->getColumn(COL_UUID);
        if (!cell)
            continue;

        LLUUID avatarId = cell->getValue().asUUID();
        avatar_ids.emplace_back(avatarId);
    }

    return avatar_ids;
}

S32 FSFloaterPoser::getAvatarListIndexForUuid(LLUUID toFind)
{
    LLScrollListCtrl* avatarScrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_AVATARSELECTION);
    if (!avatarScrollList)
        return -1;

    S32 result = -1;
    for (auto listItem : avatarScrollList->getAllData())
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
    LLScrollListCtrl *avatarScrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_AVATARSELECTION);
    if (!avatarScrollList)
        return;

    uuid_vec_t avatarsToAddToList, avatarsToRemoveFromList;
    uuid_vec_t nearbyAvatarIds = getNearbyAvatarsAndAnimeshes();
    uuid_vec_t currentlyListedAvatars = getCurrentlyListedAvatarsAndAnimeshes();
    LLCommonUtils::computeDifference(nearbyAvatarIds, currentlyListedAvatars, avatarsToAddToList, avatarsToRemoveFromList);

    for (LLUUID toRemove : avatarsToRemoveFromList)
    {
        S32 indexToRemove = getAvatarListIndexForUuid(toRemove);
        if (indexToRemove >= 0)
            avatarScrollList->deleteSingleItem(indexToRemove);
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
        LLScrollListItem *item      = avatarScrollList->addElement(row);
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
        avatarScrollList->addElement(row);
    }

    avatarScrollList->updateLayout();
    refreshTextEmbiggeningOnAllScrollLists();
}

void FSFloaterPoser::refreshTextEmbiggeningOnAllScrollLists()
{
    LLScrollListCtrl *scrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_AVATARSELECTION);
    if (scrollList)
    {
        // the avatars
        for (auto listItem : scrollList->getAllData())
        {
            LLScrollListCell* cell = listItem->getColumn(COL_UUID);
            if (!cell)
                continue;

            LLUUID selectedAvatarId = cell->getValue().asUUID();
            LLVOAvatar* listAvatar = getAvatarByUuid(selectedAvatarId);

            if (_poserAnimator.isPosingAvatar(listAvatar))
                ((LLScrollListText *) listItem->getColumn(COL_NAME))->setFontStyle(LLFontGL::BOLD);
            else
                ((LLScrollListText *) listItem->getColumn(COL_NAME))->setFontStyle(LLFontGL::NORMAL);
        }
    }
    
    LLVOAvatar *avatar = getUiSelectedAvatar();
    addBoldToScrollList(std::string(POSER_AVATAR_SCROLLLIST_BODYJOINTS_NAME), avatar);
    addBoldToScrollList(std::string(POSER_AVATAR_SCROLLLIST_FACEJOINTS_NAME), avatar);
    addBoldToScrollList(std::string(POSER_AVATAR_SCROLLLIST_HANDJOINTS_NAME), avatar);
    addBoldToScrollList(std::string(POSER_AVATAR_SCROLLLIST_MISCJOINTS_NAME), avatar);
    addBoldToScrollList(std::string(POSER_AVATAR_SCROLLLIST_VOLUMES_NAME), avatar);
}

void FSFloaterPoser::addBoldToScrollList(std::string listName, LLVOAvatar *avatar)
{
    if (!avatar)
        return;

    LLScrollListCtrl *scrollList = getChild<LLScrollListCtrl>(listName);
    if (!scrollList)
        return;

    for (auto listItem : scrollList->getAllData())
    {
        FSPoserAnimator::FSPoserJoint *userData = static_cast<FSPoserAnimator::FSPoserJoint *>(listItem->getUserdata());
        if (userData)
        {
            if (_poserAnimator.isPosingAvatarJoint(avatar, *userData))
                ((LLScrollListText *) listItem->getColumn(COL_NAME))->setFontStyle(LLFontGL::BOLD);
            else
                ((LLScrollListText *) listItem->getColumn(COL_NAME))->setFontStyle(LLFontGL::NORMAL);
        }
    }
}
