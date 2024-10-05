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

static const std::string POSE_INTERNAL_FORMAT_FILE_MASK     = "*.xml";
static const std::string POSE_INTERNAL_FORMAT_FILE_EXT      = ".xml";
static const std::string POSE_SAVE_SUBDIRECTORY             = "poses";
static const std::string XML_LIST_HEADER_STRING_PREFIX      = "header_";
static const std::string XML_LIST_TITLE_STRING_PREFIX       = "title_";
static const std::string XML_JOINT_TRANSFORM_STRING_PREFIX  = "joint_transform_";
static const std::string POSER_ADVANCEDWINDOWSTATE_SAVE_KEY = "FSPoserAdvancedWindowState";

static const std::string POSER_AVATAR_PANEL_JOINTSPARENT = "joints_parent_panel";
static const std::string POSER_AVATAR_PANEL_TRACKBALL = "trackball_panel";
static const std::string POSER_AVATAR_PANEL_ADVANCED = "advanced_parent_panel";
static const std::string POSER_AVATAR_TABGROUP_JOINTS = "joints_tabs";
static const std::string POSER_AVATAR_TAB_POSITION = "positionRotation_panel";
static const std::string POSER_AVATAR_TAB_BODY = "body_joints_panel";
static const std::string POSER_AVATAR_TAB_FACE = "face_joints_panel";
static const std::string POSER_AVATAR_TAB_HANDS = "hands_joints_panel";
static const std::string POSER_AVATAR_TAB_MISC = "misc_joints_panel";

// standard controls
static const std::string POSER_AVATAR_TRACKBALL_NAME   = "limb_rotation";
static const std::string POSER_AVATAR_SLIDER_YAW_NAME  = "limb_yaw"; // turning your nose left or right
static const std::string POSER_AVATAR_SLIDER_PITCH_NAME  = "limb_pitch"; // pointing your nose up or down
static const std::string POSER_AVATAR_SLIDER_ROLL_NAME = "limb_roll"; // your ear touches your shoulder
static const std::string POSER_AVATAR_TOGGLEBUTTON_TRACKPADSENSITIVITY = "button_toggleTrackPadSensitivity";
static const std::string POSER_AVATAR_TOGGLEBUTTON_MIRROR = "button_toggleMirrorRotation";
static const std::string POSER_AVATAR_TOGGLEBUTTON_SYMPATH = "button_toggleSympatheticRotation";
static const std::string POSER_AVATAR_BUTTON_REDO = "button_redo_change";
static const std::string POSER_AVATAR_SLIDER_POSX_NAME = "av_position_inout";
static const std::string POSER_AVATAR_SLIDER_POSY_NAME = "av_position_leftright";
static const std::string POSER_AVATAR_SLIDER_POSZ_NAME = "av_position_updown";

// Advanced controls
static const std::string POSER_AVATAR_ADV_SLIDER_ROTX_NAME = "Advanced_Rotation_X";
static const std::string POSER_AVATAR_ADV_SLIDER_ROTY_NAME = "Advanced_Rotation_Y";
static const std::string POSER_AVATAR_ADV_SLIDER_ROTZ_NAME = "Advanced_Rotation_Z";
static const std::string POSER_AVATAR_ADV_SLIDER_POSX_NAME = "Advanced_Position_X";
static const std::string POSER_AVATAR_ADV_SLIDER_POSY_NAME = "Advanced_Position_Y";
static const std::string POSER_AVATAR_ADV_SLIDER_POSZ_NAME = "Advanced_Position_Z";
static const std::string POSER_AVATAR_ADV_SLIDER_SCALEX_NAME = "Advanced_Scale_X";
static const std::string POSER_AVATAR_ADV_SLIDER_SCALEY_NAME = "Advanced_Scale_Y";
static const std::string POSER_AVATAR_ADV_SLIDER_SCALEZ_NAME = "Advanced_Scale_Z";
static const std::string POSER_AVATAR_ADV_BUTTON_NAME  = "start_stop_posing_button";

static const std::string POSER_AVATAR_SCROLLLIST_AVATARSELECTION   = "avatarSelection_scroll";
static const std::string POSER_AVATAR_STARTSTOP_POSING_BUTTON_NAME = "start_stop_posing_button";
static const std::string POSER_AVATAR_ADVANCED_TOGGLEBUTTON_NAME   = "toggleAdvancedPanel";
static const std::string POSER_AVATAR_PANEL_BUTTON_FLIPPOSE_NAME   = "FlipPose_avatar";
static const std::string POSER_AVATAR_PANEL_BUTTON_FLIPJOINT_NAME  = "FlipJoint_avatar";
static const std::string POSER_AVATAR_PANEL_BUTTON_RECAPTURE_NAME  = "button_RecaptureParts";
static const std::string POSER_AVATAR_PANEL_BUTTON_TOGGLEPOSING_NAME  = "toggle_PosingSelectedBones";

static const std::string POSER_AVATAR_TOGGLEBUTTON_LOADSAVE        = "toggleLoadSavePanel";
static const std::string POSER_AVATAR_PANEL_LOADSAVE_NAME          = "poses_loadSave";
static const std::string POSER_AVATAR_SCROLLLIST_LOADSAVE_NAME     = "poses_scroll";
static const std::string POSER_AVATAR_BUTTON_BROWSEFOLDER_NAME     = "open_poseDir_button";
static const std::string POSER_AVATAR_BUTTON_LOAD_NAME             = "load_poses_button";
static const std::string POSER_AVATAR_BUTTON_SAVE_NAME             = "save_poses_button";
static const std::string POSER_AVATAR_LINEEDIT_FILESAVENAME        = "pose_save_name";

static const std::string POSER_AVATAR_SCROLLLIST_HIDDEN_NAME         = "entireAv_joint_scroll";
static const std::string POSER_AVATAR_SCROLLLIST_BODYJOINTS_NAME     = "body_joints_scroll";
static const std::string POSER_AVATAR_SCROLLLIST_FACEJOINTS_NAME     = "face_joints_scroll";
static const std::string POSER_AVATAR_SCROLLLIST_HANDJOINTS_NAME     = "hand_joints_scroll";
static const std::string POSER_AVATAR_SCROLLLIST_MISCJOINTS_NAME     = "misc_joints_scroll";

FSFloaterPoser::FSFloaterPoser(const LLSD& key) : LLFloater(key)
{
    // bind requests, other controls are find-and-binds, see postBuild()
    mCommitCallbackRegistrar.add("Poser.RefreshAvatars", boost::bind(&FSFloaterPoser::onAvatarsRefresh, this));
    mCommitCallbackRegistrar.add("Poser.StartStopAnimating", boost::bind(&FSFloaterPoser::onPoseStartStop, this));
    mCommitCallbackRegistrar.add("Poser.ToggleLoadSavePanel", boost::bind(&FSFloaterPoser::onToggleLoadSavePanel, this));
    mCommitCallbackRegistrar.add("Poser.ToggleAdvancedPanel", boost::bind(&FSFloaterPoser::onToggleAdvancedPanel, this));

    mCommitCallbackRegistrar.add("Poser.UndoLastRotation", boost::bind(&FSFloaterPoser::onUndoLastRotation, this));
    mCommitCallbackRegistrar.add("Poser.RedoLastRotation", boost::bind(&FSFloaterPoser::onRedoLastRotation, this));
    mCommitCallbackRegistrar.add("Poser.ToggleMirrorChanges", boost::bind(&FSFloaterPoser::onToggleMirrorChange, this));
    mCommitCallbackRegistrar.add("Poser.ToggleSympatheticChanges", boost::bind(&FSFloaterPoser::onToggleSympatheticChange, this));
    mCommitCallbackRegistrar.add("Poser.ToggleTrackPadSensitivity", boost::bind(&FSFloaterPoser::refreshTrackpadCursor, this));

    mCommitCallbackRegistrar.add("Poser.PositionSet", boost::bind(&FSFloaterPoser::onAvatarPositionSet, this));

    mCommitCallbackRegistrar.add("Poser.Advanced.PositionSet", boost::bind(&FSFloaterPoser::onAdvancedPositionSet, this));
    mCommitCallbackRegistrar.add("Poser.Advanced.RotationSet", boost::bind(&FSFloaterPoser::onAdvancedRotationSet, this));
    mCommitCallbackRegistrar.add("Poser.Advanced.ScaleSet", boost::bind(&FSFloaterPoser::onAdvancedScaleSet, this));

    mCommitCallbackRegistrar.add("Pose.Save", boost::bind(&FSFloaterPoser::onClickPoseSave, this));
    mCommitCallbackRegistrar.add("Pose.Menu", boost::bind(&FSFloaterPoser::onPoseMenuAction, this, _2));
    mCommitCallbackRegistrar.add("Poser.BrowseCache", boost::bind(&FSFloaterPoser::onClickBrowsePoseCache, this));

    mCommitCallbackRegistrar.add("Poser.FlipPose", boost::bind(&FSFloaterPoser::onClickFlipPose, this));
    mCommitCallbackRegistrar.add("Poser.FlipJoint", boost::bind(&FSFloaterPoser::onClickFlipSelectedJoints, this));
    mCommitCallbackRegistrar.add("Poser.RecaptureSelectedBones", boost::bind(&FSFloaterPoser::onClickRecaptureSelectedBones, this));
    mCommitCallbackRegistrar.add("Poser.TogglePosingSelectedBones", boost::bind(&FSFloaterPoser::onClickToggleSelectedBoneEnabled, this));
    mCommitCallbackRegistrar.add("Pose.PoseJointsReset", boost::bind(&FSFloaterPoser::onPoseJointsReset, this));
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
                setRotationChangeButtons(false, false);
            });

    LLScrollListCtrl *scrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_AVATARSELECTION);
    if (scrollList)
    {
        scrollList->setCommitOnSelectionChange(true);
        scrollList->setCommitCallback(boost::bind(&FSFloaterPoser::onAvatarSelect, this));
    }

    scrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_BODYJOINTS_NAME);
    if (scrollList)
    {
        scrollList->setCommitOnSelectionChange(true);
        scrollList->setCommitCallback(boost::bind(&FSFloaterPoser::onJointSelect, this));
    }

    scrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_FACEJOINTS_NAME);
    if (scrollList)
    {
        scrollList->setCommitOnSelectionChange(true);
        scrollList->setCommitCallback(boost::bind(&FSFloaterPoser::onJointSelect, this));
    }

    scrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_HANDJOINTS_NAME);
    if (scrollList)
    {
        scrollList->setCommitOnSelectionChange(true);
        scrollList->setCommitCallback(boost::bind(&FSFloaterPoser::onJointSelect, this));
    }

    scrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_MISCJOINTS_NAME);
    if (scrollList)
    {
        scrollList->setCommitOnSelectionChange(true);
        scrollList->setCommitCallback(boost::bind(&FSFloaterPoser::onJointSelect, this));
    }

    scrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_LOADSAVE_NAME);
    if (scrollList)
    {
        scrollList->setCommitOnSelectionChange(true);
        scrollList->setCommitCallback(boost::bind(&FSFloaterPoser::onPoseFileSelect, this));
    }

    bool advButtonState = gSavedSettings.getBOOL(POSER_ADVANCEDWINDOWSTATE_SAVE_KEY);
    if (advButtonState)
    {
        LLButton *advancedButton = getChild<LLButton>(POSER_AVATAR_ADVANCED_TOGGLEBUTTON_NAME);
        if (advancedButton)
            advancedButton->setValue(true);
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
}

void FSFloaterPoser::onClose(bool app_quitting)
{
    LLButton *advancedButton = getChild<LLButton>(POSER_AVATAR_ADVANCED_TOGGLEBUTTON_NAME);
    if (advancedButton)
        gSavedSettings.setBOOL(POSER_ADVANCEDWINDOWSTATE_SAVE_KEY, advancedButton->getValue().asBoolean());
}

void FSFloaterPoser::refreshPosesScroll()
{
    LLScrollListCtrl *posesScrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_LOADSAVE_NAME);
    if (!posesScrollList)
        return;

    posesScrollList->clearRows();

    std::string dir = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "poses");
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
        refreshPosesScroll();
        setUiSelectedAvatarSaveFileName(filename);
        // TODO: provide feedback for save
    }
}

bool FSFloaterPoser::savePoseToXml(LLVOAvatar* avatar, std::string poseFileName)
{
    if (poseFileName.empty())
        return false;

    if (!_poserAnimator.isPosingAvatar(avatar))
        return false;

    try
    {
        std::string pathname = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, POSE_SAVE_SUBDIRECTORY);
        if (!gDirUtilp->fileExists(pathname))
        {
            LL_WARNS("Posing") << "Couldn't find folder: " << pathname << " - creating one." << LL_ENDL;
            LLFile::mkdir(pathname);
        }

        std::string fullSavePath =
            gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, POSE_SAVE_SUBDIRECTORY, poseFileName + POSE_INTERNAL_FORMAT_FILE_EXT);

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
            LL_WARNS("GroupMute") << "Unable to save pose!" << LL_ENDL;
            return false;
        }
        LLSDSerialize::toPrettyXML(record, file);
        file.close();
    }
    catch (...)
    {
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
        LLVector3 newPosition = _poserAnimator.getJointPosition(avatar, *item);
        LLVector3 newScale = _poserAnimator.getJointScale(avatar, *item);

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
    std::string pathname = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, POSE_SAVE_SUBDIRECTORY);
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

void FSFloaterPoser::loadPoseFromXml(LLVOAvatar* avatar, std::string poseFileName, E_LoadPoseMethods loadMethod)
{
    std::string pathname = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, POSE_SAVE_SUBDIRECTORY);
    if (!gDirUtilp->fileExists(pathname))
        return;

    if (!_poserAnimator.isPosingAvatar(avatar))
        return;

    std::string fullPath =
        gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, POSE_SAVE_SUBDIRECTORY, poseFileName + POSE_INTERNAL_FORMAT_FILE_EXT);

    bool loadRotations = loadMethod == ROTATIONS || loadMethod == ROTATIONS_AND_POSITIONS || loadMethod == ROTATIONS_AND_SCALES ||
                         loadMethod == ROT_POS_AND_SCALES;
    bool loadPositions = loadMethod == POSITIONS || loadMethod == ROTATIONS_AND_POSITIONS || loadMethod == POSITIONS_AND_SCALES ||
                         loadMethod == ROT_POS_AND_SCALES;
    bool loadScales = loadMethod == SCALES || loadMethod == POSITIONS_AND_SCALES || loadMethod == ROTATIONS_AND_SCALES ||
                      loadMethod == ROT_POS_AND_SCALES;

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

                const FSPoserAnimator::FSPoserJoint *poserJoint = _poserAnimator.getPoserJointByName(name);
                if (!poserJoint)
                    continue;

                if (loadRotations && control_map.has("rotation"))
                {
                    vec3.setValue(control_map["rotation"]);
                    _poserAnimator.setJointRotation(avatar, poserJoint, vec3, NONE, SWAP_NOTHING, NEGATE_NOTHING); // If we keep defaults it will load BD poses
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
    catch (...)
    {
        LL_WARNS("Posing") << "Everything caught fire trying to load the pose: " << poseFileName << LL_ENDL;
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

#ifdef NDEBUG
    return true;
#endif
    return avatar->isSelf();
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
    if (!bodyJointsScrollList || !faceJointsScrollList || !handsJointsScrollList || !miscJointsScrollList)
        return;

    bodyJointsScrollList->clearRows();
    faceJointsScrollList->clearRows();
    handsJointsScrollList->clearRows();
    miscJointsScrollList->clearRows();

    std::vector<FSPoserAnimator::FSPoserJoint>::const_iterator poserJoint_iter;
    for (poserJoint_iter = _poserAnimator.PoserJoints.begin();
         poserJoint_iter != _poserAnimator.PoserJoints.end();
         ++poserJoint_iter)
    {
        LLSD row  = createRowForJoint(poserJoint_iter->jointName(), false);
        if (!row)
            continue;

        LLScrollListItem *item = nullptr;
        bool hasListHeader = hasString(XML_LIST_HEADER_STRING_PREFIX + poserJoint_iter->jointName());

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
        refreshPosesScroll();
}

void FSFloaterPoser::onToggleMirrorChange() { setRotationChangeButtons(true, false); }

void FSFloaterPoser::onToggleSympatheticChange() { setRotationChangeButtons(false, true); }

void FSFloaterPoser::setRotationChangeButtons(bool togglingMirror, bool togglingSympathetic)
{
    LLButton *toggleMirrorButton = getChild<LLButton>(POSER_AVATAR_TOGGLEBUTTON_MIRROR);
    if (!toggleMirrorButton)
        return;
    LLButton *toggleSympatheticButton = getChild<LLButton>(POSER_AVATAR_TOGGLEBUTTON_SYMPATH);
    if (!toggleSympatheticButton)
        return;

    if (!togglingMirror && !togglingSympathetic) // turn off both buttons
    {
        toggleMirrorButton->setValue(false);
        toggleSympatheticButton->setValue(false);
        return;
    }

    bool useMirror      = toggleMirrorButton->getValue().asBoolean();
    bool useSympathetic = toggleSympatheticButton->getValue().asBoolean();
    if (useMirror && useSympathetic) // if both buttons are down, turn one of them off
    {
        if (togglingSympathetic)
            toggleMirrorButton->setValue(false);

        if (togglingMirror)
            toggleSympatheticButton->setValue(false);
    }
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

    bool shouldEnableRedoButton = false;
    for (auto item : selectedJoints)
    {
        bool currentlyPosing = _poserAnimator.isPosingAvatarJoint(avatar, *item);
        if (currentlyPosing)
            _poserAnimator.undoLastJointRotation(avatar, *item, getUiSelectedBoneDeflectionStyle());

        shouldEnableRedoButton |= _poserAnimator.canRedoJointRotation(avatar, *item);
    }

    enableOrDisableRedoButton();
    refreshRotationSliders();
    refreshTrackpadCursor();
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

    bool shouldEnableRedoButton = false;
    for (auto item : selectedJoints)
    {
        bool currentlyPosing = _poserAnimator.isPosingAvatarJoint(avatar, *item);
        if (currentlyPosing)
            _poserAnimator.redoLastJointRotation(avatar, *item, getUiSelectedBoneDeflectionStyle());

        shouldEnableRedoButton |= _poserAnimator.canRedoJointRotation(avatar, *item);
    }

    enableOrDisableRedoButton();
    refreshRotationSliders();
    refreshTrackpadCursor();
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

    onJointSelect();
}

std::vector<FSPoserAnimator::FSPoserJoint *> FSFloaterPoser::getUiSelectedPoserJoints()
{
    std::vector<FSPoserAnimator::FSPoserJoint *> joints;

    LLTabContainer *jointTabGroup = getChild<LLTabContainer>(POSER_AVATAR_TABGROUP_JOINTS);
    if (!jointTabGroup)
        return joints;

    std::string activeTabName = jointTabGroup->getCurrentPanel()->getName();
    if (activeTabName.empty())
        return joints;

    std::string scrollListName;

    if (boost::iequals(activeTabName, POSER_AVATAR_TAB_POSITION))
        scrollListName = POSER_AVATAR_SCROLLLIST_HIDDEN_NAME;
    else if (boost::iequals(activeTabName, POSER_AVATAR_TAB_BODY))
        scrollListName = POSER_AVATAR_SCROLLLIST_BODYJOINTS_NAME;
    else if (boost::iequals(activeTabName, POSER_AVATAR_TAB_FACE))
        scrollListName = POSER_AVATAR_SCROLLLIST_FACEJOINTS_NAME;
    else if (boost::iequals(activeTabName, POSER_AVATAR_TAB_HANDS))
        scrollListName = POSER_AVATAR_SCROLLLIST_HANDJOINTS_NAME;
    else if (boost::iequals(activeTabName, POSER_AVATAR_TAB_MISC))
        scrollListName = POSER_AVATAR_SCROLLLIST_MISCJOINTS_NAME;

    if (scrollListName.empty())
        return joints;

    LLScrollListCtrl *scrollList = getChild<LLScrollListCtrl>(scrollListName);
    if (!scrollList)
        return joints;

    for (auto item : scrollList->getAllSelected())
    {
        FSPoserAnimator::FSPoserJoint *userData = (FSPoserAnimator::FSPoserJoint *) item->getUserdata();
        if (userData)
            joints.push_back(userData);
    }

    return joints;
}

E_BoneDeflectionStyles FSFloaterPoser::getUiSelectedBoneDeflectionStyle()
{
    LLButton *toggleMirrorButton = getChild<LLButton>(POSER_AVATAR_TOGGLEBUTTON_MIRROR);
    if (!toggleMirrorButton)
        return NONE;
    LLButton *toggleSympatheticButton = getChild<LLButton>(POSER_AVATAR_TOGGLEBUTTON_SYMPATH);
    if (!toggleSympatheticButton)
        return NONE;

    if (toggleMirrorButton->getValue().asBoolean())
        return MIRROR;
    if (toggleSympatheticButton->getValue().asBoolean())
        return SYMPATHETIC;

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
}

void FSFloaterPoser::onAdvancedRotationSet()
{
    LLSliderCtrl *xRotAdvSlider = getChild<LLSliderCtrl>(POSER_AVATAR_ADV_SLIDER_ROTX_NAME);
    LLSliderCtrl *yRotAdvSlider = getChild<LLSliderCtrl>(POSER_AVATAR_ADV_SLIDER_ROTY_NAME);
    LLSliderCtrl *zRotAdvSlider = getChild<LLSliderCtrl>(POSER_AVATAR_ADV_SLIDER_ROTZ_NAME);
    if (!xRotAdvSlider || !yRotAdvSlider || !zRotAdvSlider)
        return;

    F32 yaw   = (F32) xRotAdvSlider->getValue().asReal();
    F32 pitch = (F32) yRotAdvSlider->getValue().asReal();
    F32 roll  = (F32) zRotAdvSlider->getValue().asReal();

    setSelectedJointsRotation(yaw, pitch, roll);
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

    F32 yaw, pitch, roll = 0.0;
    yaw  = trackPadPos.mV[VX];
    pitch = trackPadPos.mV[VY];

    LLButton *toggleSensitivityButton = getChild<LLButton>(POSER_AVATAR_TOGGLEBUTTON_TRACKPADSENSITIVITY);
    if (toggleSensitivityButton)
    {
        bool moreSensitive = toggleSensitivityButton->getValue().asBoolean();
        if (moreSensitive)
        {
            yaw *= trackPadHighSensitivity;
            pitch *= trackPadHighSensitivity;
        }
        else
        {
            yaw *= trackPadDefaultSensitivity;
            pitch *= trackPadDefaultSensitivity;
        }
    }

    // if the trackpad is in 'infinite scroll' mode, it can produce normalized-values outside the range of the sliders; this wraps them to by the slider full-scale
    while (yaw > 1)
        yaw -= 2;
    while (yaw < -1)
        yaw += 2;
    while (pitch > 1)
        pitch -= 2;
    while (pitch < -1)
        pitch += 2;
    
    yaw *= normalTrackpadRangeInRads;
    pitch *= normalTrackpadRangeInRads;

    LLSliderCtrl *rollSlider = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_ROLL_NAME);
    if (rollSlider)
        roll = (F32) rollSlider->getValue().asReal();  // roll starts from its own slider

    roll += trackPadPos.mV[VZ];
    if (rollSlider)
        rollSlider->setValue(roll);

    roll *= DEG_TO_RAD;

    setSelectedJointsRotation(yaw, pitch, roll);

    // WARNING!
    // as tempting as it is to refactor the following to refreshRotationSliders(), don't.
    // getRotationOfFirstSelectedJoint/setSelectedJointsRotation are
    // not necessarily symmetric functions (see their remarks).
    LLSliderCtrl *yawSlider   = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_YAW_NAME);
    LLSliderCtrl *pitchSlider = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_PITCH_NAME);
    if (!yawSlider || !pitchSlider)
        return;

    yawSlider->setValue(yaw *= RAD_TO_DEG);
    pitchSlider->setValue(pitch *= RAD_TO_DEG);
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

    yaw /= normalTrackpadRangeInRads;
    pitch /= normalTrackpadRangeInRads;
    LLButton *toggleSensitivityButton = getChild<LLButton>(POSER_AVATAR_TOGGLEBUTTON_TRACKPADSENSITIVITY);
    if (toggleSensitivityButton)
    {
        bool moreSensitive = toggleSensitivityButton->getValue().asBoolean();
        if (moreSensitive)
        {
            yaw /= trackPadHighSensitivity;
            pitch /= trackPadHighSensitivity;
        }
        else
        {
            yaw /= trackPadDefaultSensitivity;
            pitch /= trackPadDefaultSensitivity;
        }
    }

    trackBall->setValue(yaw, pitch);
}

void FSFloaterPoser::refreshTrackpadCursor()
{
    FSVirtualTrackpad *trackBall = getChild<FSVirtualTrackpad>(POSER_AVATAR_TRACKBALL_NAME);
    if (!trackBall)
        return;

    LLSliderCtrl* yawSlider   = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_YAW_NAME);
    LLSliderCtrl* pitchSlider = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_PITCH_NAME);
    if (!yawSlider || !pitchSlider)
        return;

    F32 axis1 = (F32) yawSlider->getValue().asReal();
    F32 axis2 = (F32) pitchSlider->getValue().asReal();

    axis1 *= DEG_TO_RAD;
    axis2 *= DEG_TO_RAD;

    axis1 /= normalTrackpadRangeInRads;
    axis2 /= normalTrackpadRangeInRads;
    LLButton *toggleSensitivityButton = getChild<LLButton>(POSER_AVATAR_TOGGLEBUTTON_TRACKPADSENSITIVITY);
    if (toggleSensitivityButton)
    {
        bool moreSensitive = toggleSensitivityButton->getValue().asBoolean();
        if (moreSensitive)
        {
            axis1 /= trackPadHighSensitivity;
            axis2 /= trackPadHighSensitivity;
        }
        else
        {
            axis1 /= trackPadDefaultSensitivity;
            axis2 /= trackPadDefaultSensitivity;
        }
    }

    trackBall->setValue(axis1, axis2);
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
        _poserAnimator.setJointPosition(avatar, item, vec3, defl);
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
        _poserAnimator
            .setJointRotation(avatar, item, vec3, defl, getJointTranslation(item->jointName()), getJointNegation(item->jointName()));
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
        _poserAnimator.setJointScale(avatar, item, vec3, defl);
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

    position = _poserAnimator.getJointPosition(avatar, *selectedJoints.front());  // TODO: inject translations like rotation does?
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

    scale = _poserAnimator.getJointScale(avatar, *selectedJoints.front());  // TODO: inject translations like rotation does?
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

S32 FSFloaterPoser::getJointNegation(std::string jointName)
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
    addBoldToScrollList(POSER_AVATAR_SCROLLLIST_BODYJOINTS_NAME, avatar);
    addBoldToScrollList(POSER_AVATAR_SCROLLLIST_FACEJOINTS_NAME, avatar);
    addBoldToScrollList(POSER_AVATAR_SCROLLLIST_HANDJOINTS_NAME, avatar);
    addBoldToScrollList(POSER_AVATAR_SCROLLLIST_MISCJOINTS_NAME, avatar);
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
        FSPoserAnimator::FSPoserJoint *userData = (FSPoserAnimator::FSPoserJoint *) listItem->getUserdata();
        if (userData)
        {
            if (_poserAnimator.isPosingAvatarJoint(avatar, *userData))
                ((LLScrollListText *) listItem->getColumn(COL_NAME))->setFontStyle(LLFontGL::BOLD);
            else
                ((LLScrollListText *) listItem->getColumn(COL_NAME))->setFontStyle(LLFontGL::NORMAL);
        }
    }
}
