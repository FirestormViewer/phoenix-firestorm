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
#include "llvirtualtrackball.h"
#include "llfloater.h"
#include "llviewercontrol.h"
#include "llcontrolavatar.h"
#include "llstring.h"
#include "llviewerwindow.h"
#include "llwindow.h"

static const std::string POSE_INTERNAL_FORMAT_FILE_MASK     = "*.xml";
static const std::string POSE_INTERNAL_FORMAT_FILE_EXT      = ".xml";
static const std::string POSE_SAVE_SUBDIRECTORY             = "poses";
static const std::string XML_LIST_HEADER_STRING_PREFIX      = "header_";
static const std::string XML_LIST_TITLE_STRING_PREFIX       = "title_";
static const std::string POSER_ADVANCEDWINDOWSTATE_SAVE_KEY = "FSPoserAdvancedWindowState";

static const std::string POSER_AVATAR_PANEL_JOINTSPARENT = "joints_parent_panel";
static const std::string POSER_AVATAR_PANEL_TRACKBALL = "trackball_panel";
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
static const std::string POSER_AVATAR_TOGGLEBUTTON_MIRROR = "button_toggleMirrorRotation";
static const std::string POSER_AVATAR_TOGGLEBUTTON_SYMPATH = "button_toggleSympatheticRotation";
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
static const std::string POSER_AVATAR_PANEL_ADVANCED_NAME          = "poses_AdvancedControls";

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

const LLVector3          VectorZero(1.0f, 0.0f, 0.0f);

FSFloaterPoser::FSFloaterPoser(const LLSD& key) : LLFloater(key)
{
    // bind requests, other controls are find-and-binds, see postBuild()
    mCommitCallbackRegistrar.add("Poser.RefreshAvatars", boost::bind(&FSFloaterPoser::onAvatarsRefresh, this));
    mCommitCallbackRegistrar.add("Poser.StartStopAnimating", boost::bind(&FSFloaterPoser::onPoseStartStop, this));
    mCommitCallbackRegistrar.add("Poser.ToggleLoadSavePanel", boost::bind(&FSFloaterPoser::onToggleLoadSavePanel, this));
    mCommitCallbackRegistrar.add("Poser.ToggleAdvancedPanel", boost::bind(&FSFloaterPoser::onToggleAdvancedPanel, this));

    mCommitCallbackRegistrar.add("Poser.UndoLastRotation", boost::bind(&FSFloaterPoser::onUndoLastRotation, this));
    mCommitCallbackRegistrar.add("Poser.ToggleMirrorChanges", boost::bind(&FSFloaterPoser::onToggleMirrorChange, this));
    mCommitCallbackRegistrar.add("Poser.ToggleSympatheticChanges", boost::bind(&FSFloaterPoser::onToggleSympatheticChange, this));

    mCommitCallbackRegistrar.add("Poser.PositionSet", boost::bind(&FSFloaterPoser::onAvatarPositionSet, this));

    mCommitCallbackRegistrar.add("Poser.Advanced.PositionSet", boost::bind(&FSFloaterPoser::onAdvancedPositionSet, this));
    mCommitCallbackRegistrar.add("Poser.Advanced.RotationSet", boost::bind(&FSFloaterPoser::onAdvancedRotationSet, this));
    mCommitCallbackRegistrar.add("Poser.Advanced.ScaleSet", boost::bind(&FSFloaterPoser::onAdvancedScaleSet, this));

    mCommitCallbackRegistrar.add("Pose.Save", boost::bind(&FSFloaterPoser::onClickPoseSave, this));
    mCommitCallbackRegistrar.add("Pose.Menu", boost::bind(&FSFloaterPoser::onPoseMenuAction, this, _2));
    mCommitCallbackRegistrar.add("Poser.BrowseCache", boost::bind(&FSFloaterPoser::onClickBrowsePoseCache, this));

    mCommitCallbackRegistrar.add("Poser.TrackBallMove", boost::bind(&FSFloaterPoser::onLimbTrackballChanged, this)); // so I can debug
}

FSFloaterPoser::~FSFloaterPoser()
{
    clearRecentlySetRotations();
}

bool FSFloaterPoser::postBuild()
{
    // find-and-binds
    getChild<LLUICtrl>(POSER_AVATAR_TRACKBALL_NAME)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onLimbTrackballChanged(); });
    getChild<LLUICtrl>(POSER_AVATAR_SLIDER_YAW_NAME)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onLimbYawPitchRollChanged(); });
    getChild<LLUICtrl>(POSER_AVATAR_SLIDER_PITCH_NAME)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onLimbYawPitchRollChanged(); });
    getChild<LLUICtrl>(POSER_AVATAR_SLIDER_ROLL_NAME)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onLimbYawPitchRollChanged(); });

    LLScrollListCtrl *scrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_AVATARSELECTION);
    if (scrollList)
    {
        scrollList->setCommitOnSelectionChange(TRUE);
        scrollList->setCommitCallback(boost::bind(&FSFloaterPoser::onAvatarsSelect, this));
    }

    scrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_BODYJOINTS_NAME);
    if (scrollList)
    {
        scrollList->setCommitOnSelectionChange(TRUE);
        scrollList->setCommitCallback(boost::bind(&FSFloaterPoser::onJointSelect, this));
    }

    scrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_FACEJOINTS_NAME);
    if (scrollList)
    {
        scrollList->setCommitOnSelectionChange(TRUE);
        scrollList->setCommitCallback(boost::bind(&FSFloaterPoser::onJointSelect, this));
    }

    scrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_HANDJOINTS_NAME);
    if (scrollList)
    {
        scrollList->setCommitOnSelectionChange(TRUE);
        scrollList->setCommitCallback(boost::bind(&FSFloaterPoser::onJointSelect, this));
    }

    scrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_MISCJOINTS_NAME);
    if (scrollList)
    {
        scrollList->setCommitOnSelectionChange(TRUE);
        scrollList->setCommitCallback(boost::bind(&FSFloaterPoser::onJointSelect, this));
    }

    scrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_LOADSAVE_NAME);
    if (scrollList)
    {
        scrollList->setCommitOnSelectionChange(TRUE);
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

    return TRUE;
}

void FSFloaterPoser::draw()
{
    LLFloater::draw();
}

void FSFloaterPoser::onOpen(const LLSD& key)
{
    onAvatarsRefresh();
    refreshJointScrollListMembers();
}

void FSFloaterPoser::onClose(bool app_quitting)
{
    LLButton *advancedButton = getChild<LLButton>(POSER_AVATAR_ADVANCED_TOGGLEBUTTON_NAME);
    if (advancedButton)
        gSavedSettings.setBOOL(POSER_ADVANCEDWINDOWSTATE_SAVE_KEY, advancedButton->getValue());
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

    LLButton *loadPosesButton = getChild<LLButton>(POSER_AVATAR_BUTTON_LOAD_NAME);
    if (loadPosesButton)
        loadPosesButton->setEnabled(true);

    LLButton *savePosesButton = getChild<LLButton>(POSER_AVATAR_BUTTON_SAVE_NAME);
    if (savePosesButton)
        savePosesButton->setEnabled(true);

    std::string pose_name = item->getColumn(0)->getValue().asString();
    if (pose_name.empty())
        return;

    LLLineEditor *poseSaveName = getChild<LLLineEditor>(POSER_AVATAR_LINEEDIT_FILESAVENAME);
    if (!poseSaveName)
        return;

    poseSaveName->setEnabled(true);
    LLStringExplicit name = LLStringExplicit(pose_name);
    poseSaveName->setText(name);
}

void FSFloaterPoser::onClickPoseSave()
{
    LLUICtrl   *poseSaveName = getChild<LLUICtrl>(POSER_AVATAR_LINEEDIT_FILESAVENAME);
    if (!poseSaveName)
        return;

    std::string filename = poseSaveName->getValue().asString();
    if (filename.empty())
        return;

    bool successfulSave = savePoseToXml(filename);
    if (successfulSave)
    {
        refreshPosesScroll();
        // TODO: provide feedback for save
    }
}

bool FSFloaterPoser::savePoseToXml(std::string poseFileName)
{
    if (poseFileName.empty())
        return false;

    LLVOAvatar *avatar = getUiSelectedAvatar();
    if (!avatar)
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

            LLVector3 vec3 = _poserAnimator.getJointRotation(avatar, pj);

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

void FSFloaterPoser::onClickBrowsePoseCache()
{
    std::string pathname = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, POSE_SAVE_SUBDIRECTORY);
    if (!gDirUtilp->fileExists(pathname))
        LLFile::mkdir(pathname);

    gViewerWindow->getWindow()->openFile(pathname);
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

    loadPoseFromXml(poseName, loadType);
    refreshJointScrollListMembers();
}

void FSFloaterPoser::loadPoseFromXml(std::string poseFileName, E_LoadPoseMethods loadMethod)
{
    LLVOAvatar *avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    if (!_poserAnimator.isPosingAvatar(avatar))
        return;

    std::string pathname = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, POSE_SAVE_SUBDIRECTORY);
    if (!gDirUtilp->fileExists(pathname))
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
                    _poserAnimator.setJointRotation(avatar, poserJoint, vec3, NONE);
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
            }
        }
    }
    catch (...)
    {
        LL_WARNS("Posing") << "Everything caught fire trying to load the pose: " << poseFileName << LL_ENDL;
    }
}

void FSFloaterPoser::onPoseStartStop()
{
    LLScrollListCtrl *avatarScrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_AVATARSELECTION);
    if (!avatarScrollList)
        return;

    LLScrollListItem *item = avatarScrollList->getFirstSelected();
    if (!item)
        return;

    LLVOAvatar* avatar = (LLVOAvatar*)item->getUserdata();

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
    onAvatarsSelect();
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

    // TODO: isSelf? or permission request? maybe through Bridge? 

    return true;
}

void FSFloaterPoser::poseControlsEnable(bool enable)
{
    LLUICtrl *jointsParentPanel = getChild<LLUICtrl>(POSER_AVATAR_PANEL_JOINTSPARENT);
    if (jointsParentPanel)
        jointsParentPanel->setEnabled(enable);

    LLUICtrl *trackballPanel = getChild<LLUICtrl>(POSER_AVATAR_PANEL_TRACKBALL);
    if (trackballPanel)
        trackballPanel->setEnabled(enable);

    LLButton *yourPosesButton = getChild<LLButton>(POSER_AVATAR_TOGGLEBUTTON_LOADSAVE);
    if (yourPosesButton)
        yourPosesButton->setEnabled(enable);
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

    bool loadSavePanelExpanded = yourPosesButton->getValue();
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

void FSFloaterPoser::onToggleMirrorChange()
{
    LLButton *toggleMirrorButton = getChild<LLButton>(POSER_AVATAR_TOGGLEBUTTON_MIRROR);
    if (!toggleMirrorButton)
        return;

    LLButton *toggleSympatheticButton = getChild<LLButton>(POSER_AVATAR_TOGGLEBUTTON_SYMPATH);
    if (!toggleSympatheticButton)
        return;

    bool useMirror = toggleMirrorButton->getValue();
    bool useSympathetic = toggleSympatheticButton->getValue();

    if (useMirror && useSympathetic)
        toggleSympatheticButton->setValue(false);
}

void FSFloaterPoser::onToggleSympatheticChange()
{
    LLButton *toggleMirrorButton = getChild<LLButton>(POSER_AVATAR_TOGGLEBUTTON_MIRROR);
    if (!toggleMirrorButton)
        return;
    LLButton *toggleSympatheticButton = getChild<LLButton>(POSER_AVATAR_TOGGLEBUTTON_SYMPATH);
    if (!toggleSympatheticButton)
        return;

    bool useMirror      = toggleMirrorButton->getValue();
    bool useSympathetic = toggleSympatheticButton->getValue();

    if (useMirror && useSympathetic)
        toggleMirrorButton->setValue(false);
}

void FSFloaterPoser::onUndoLastRotation()
{
    auto size = _lastSetRotations.size();
    if (size < 2)
        return;

    _lastSetRotations.pop();
    LLVector3 lastRotation = _lastSetRotations.top();

    setSelectedJointsRotation(lastRotation.mV[VX], lastRotation.mV[VY], lastRotation.mV[VZ]);
}

void FSFloaterPoser::clearRecentlySetRotations()
{
    while (!_lastSetRotations.empty())
        _lastSetRotations.pop();
}

void FSFloaterPoser::addRotationToRecentlySet(F32 yawInRadians, F32 pitchInRadians, F32 rollInRadians)
{
    _lastSetRotations.push(LLVector3(yawInRadians, pitchInRadians, rollInRadians));
}

void FSFloaterPoser::onToggleAdvancedPanel()
{
    if (this->isMinimized())
        return;

    // Get the "Advanced" button toggle state, find the Advanced panel, and set its visibility
    LLButton *advancedButton = getChild<LLButton>(POSER_AVATAR_ADVANCED_TOGGLEBUTTON_NAME);
    if (!advancedButton)
        return;

    bool      advancedPanelExpanded = advancedButton->getValue();
    LLUICtrl *advancedPanel         = getChild<LLUICtrl>(POSER_AVATAR_PANEL_ADVANCED_NAME);
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

    if (toggleMirrorButton->getValue())
        return MIRROR;
    if (toggleSympatheticButton->getValue())
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

    return (LLVOAvatar *) item->getUserdata();
}

void FSFloaterPoser::onAdvancedPositionSet()
{
    LLSliderCtrl *xposAdvSlider = getChild<LLSliderCtrl>(POSER_AVATAR_ADV_SLIDER_POSX_NAME);
    LLSliderCtrl *yposAdvSlider = getChild<LLSliderCtrl>(POSER_AVATAR_ADV_SLIDER_POSY_NAME);
    LLSliderCtrl *zposAdvSlider = getChild<LLSliderCtrl>(POSER_AVATAR_ADV_SLIDER_POSZ_NAME);
    if (!xposAdvSlider || !yposAdvSlider || !zposAdvSlider)
        return;

    F32 posX = xposAdvSlider->getValue().asReal();
    F32 posY = yposAdvSlider->getValue().asReal();
    F32 posZ = zposAdvSlider->getValue().asReal();

    setSelectedJointsPosition(posX, posY, posZ);
}

void FSFloaterPoser::onAdvancedRotationSet()
{
    LLSliderCtrl *xRotAdvSlider = getChild<LLSliderCtrl>(POSER_AVATAR_ADV_SLIDER_ROTX_NAME);
    LLSliderCtrl *yRotAdvSlider = getChild<LLSliderCtrl>(POSER_AVATAR_ADV_SLIDER_ROTY_NAME);
    LLSliderCtrl *zRotAdvSlider = getChild<LLSliderCtrl>(POSER_AVATAR_ADV_SLIDER_ROTZ_NAME);
    if (!xRotAdvSlider || !yRotAdvSlider || !zRotAdvSlider)
        return;

    F32 yaw   = xRotAdvSlider->getValue().asReal();
    F32 pitch = yRotAdvSlider->getValue().asReal();
    F32 roll      = zRotAdvSlider->getValue().asReal();

    setSelectedJointsRotation(yaw, pitch, roll);
}

void FSFloaterPoser::onAdvancedScaleSet()
{
    LLSliderCtrl *scalex = getChild<LLSliderCtrl>(POSER_AVATAR_ADV_SLIDER_SCALEX_NAME);
    LLSliderCtrl *scaley = getChild<LLSliderCtrl>(POSER_AVATAR_ADV_SLIDER_SCALEY_NAME);
    LLSliderCtrl *scalez = getChild<LLSliderCtrl>(POSER_AVATAR_ADV_SLIDER_SCALEZ_NAME);
    if (!scalex || !scaley || !scalez)
        return;

    F32 scX = scalex->getValue().asReal();
    F32 scY = scaley->getValue().asReal();
    F32 scZ = scalez->getValue().asReal();

    setSelectedJointsScale(scX, scY, scZ);
}

void FSFloaterPoser::onAvatarPositionSet()
{
    LLSliderCtrl *xposSlider = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_POSX_NAME);
    LLSliderCtrl *yposSlider = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_POSY_NAME);
    LLSliderCtrl *zposSlider = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_POSZ_NAME);
    if (!xposSlider || !yposSlider || !zposSlider)
        return;

    F32 posX = xposSlider->getValue().asReal();
    F32 posY = yposSlider->getValue().asReal();
    F32 posZ = zposSlider->getValue().asReal();

    setSelectedJointsPosition(posX, posY, posZ);
}

/// <summary>
/// The trackball controller is not friendly to photographers (or normal people).
/// This method could be streamlined but at the high cost of picking apart what it does.
/// The simplest thing to do would be to reimplement the code behind the slider!
/// TLDR: we just want the trackball to behave like a 2-axis slider.
///
/// BEWARE! Changes to behaviour here require their inverse to be applied on the slider-callback. 
/// </summary>
void FSFloaterPoser::onLimbTrackballChanged()
{
    LLVirtualTrackball *trackBall = getChild<LLVirtualTrackball>(POSER_AVATAR_TRACKBALL_NAME);
    if (!trackBall)
        return;

    LLSliderCtrl *pitchSlider = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_PITCH_NAME); // up/down
    LLSliderCtrl *yawSlider = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_YAW_NAME); // left right
    LLSliderCtrl *rollSlider = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_ROLL_NAME);
    if (!yawSlider || !pitchSlider || !rollSlider)
        return;

    LLQuaternion trackBallQuat = trackBall->getRotation();

    // Convert the quaternion to a cartesian (x,y,z) point on a unit-sphere
    LLVector3 cartesionPoint = VectorZero * trackBallQuat; // VX is +up/-down screen; VY is +right/-left screen; VZ is +in/-out; all are ranged 1..-1

    F32 yaw, pitch, roll;
    if (cartesionPoint.mV[VZ] >= 0)  // the sun is in front of the trackball, easy math
    {
        yaw   = cartesionPoint.mV[VX] * F_PI_BY_TWO;
        pitch = cartesionPoint.mV[VY] * F_PI_BY_TWO;
    }
    else // when the sun is behind the trackball (VZ < 0), we want to keep increasing the angle
    {
        // this is a first pass, and does not consider sensitivity changes around the edges.
        // it could be worth disallowing VZ < 0, or only allowing as an advanced feature.

        if (cartesionPoint.mV[VX] >= 0) // sun is in top hemisphere
            yaw = F_PI_BY_TWO * (2 - cartesionPoint.mV[VX]);
        else
            yaw = -1 * F_PI_BY_TWO - F_PI_BY_TWO * (1 + cartesionPoint.mV[VX]);

        if (cartesionPoint.mV[VY] >= 0)  // sun is in screen-right hemisphere
            pitch = F_PI_BY_TWO * (2 - cartesionPoint.mV[VY]);
        else
            pitch = -1 * F_PI_BY_TWO - F_PI_BY_TWO * (1 + cartesionPoint.mV[VY]);
    }

    roll = rollSlider->getValue().asReal(); // roll comes from the slider
    roll *= DEG_TO_RAD;

    setSelectedJointsRotation(yaw, pitch, roll);

    yaw *= RAD_TO_DEG;
    pitch *= RAD_TO_DEG;
    yawSlider->setValue(yaw);
    pitchSlider->setValue(pitch);
}

void FSFloaterPoser::onLimbYawPitchRollChanged()
{
    LLSliderCtrl *yawSlider = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_YAW_NAME);
    LLSliderCtrl *pitchSlider = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_PITCH_NAME);
    LLSliderCtrl *rollSlider = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_ROLL_NAME);
    if (!yawSlider || !pitchSlider || !rollSlider)
        return;

    F32 yaw   = yawSlider->getValue().asReal();
    F32 pitch = pitchSlider->getValue().asReal();
    F32 roll  = rollSlider->getValue().asReal();

    yaw *= DEG_TO_RAD;
    pitch *= DEG_TO_RAD;
    roll *= DEG_TO_RAD;
    
    setSelectedJointsRotation(yaw, pitch, roll);

    yaw *= F_PI;
    pitch *= F_PI;
    roll *= F_PI; // roll needs to be recalculated from unit sphere based on ranges of yaw and roll
    LLVector3    vec3 = LLVector3(yaw, pitch, roll);
    LLQuaternion quat;
    quat.unpackFromVector3(vec3);
    quat.setAngleAxis(pitch, 0, 1, 0);

    LLVirtualTrackball *trackBall = getChild<LLVirtualTrackball>(POSER_AVATAR_TRACKBALL_NAME);
    if (!trackBall)
        return;

    trackBall->setRotation(quat);
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

    addRotationToRecentlySet(yawInRadians, pitchInRadians, rollInRadians);

    E_BoneDeflectionStyles defl = getUiSelectedBoneDeflectionStyle();
    LLVector3              vec3 = LLVector3(yawInRadians, pitchInRadians, rollInRadians);

    for (auto item : getUiSelectedPoserJoints())
        _poserAnimator.setJointRotation(avatar, item, vec3, defl);
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

void FSFloaterPoser::onJointSelect()
{
    auto selectedJoints = getUiSelectedPoserJoints();
    if (selectedJoints.size() < 1)
        return;

    LLVOAvatar *avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    if (!_poserAnimator.isPosingAvatar(avatar))
        return;

    clearRecentlySetRotations();
    LLVector3 rotation = _poserAnimator.getJointRotation(avatar, *selectedJoints.front());
    F32       yaw   = rotation.mV[VX];
    F32       pitch = rotation.mV[VY];
    F32       roll      = rotation.mV[VZ];

    if (is_approx_zero(pitch))
        pitch = F_APPROXIMATELY_ZERO;

    LLQuaternion quat;
    quat.setAngleAxis(-pitch, 0, 1, 0);
    LLQuaternion az_quat;
    az_quat.setAngleAxis(F_TWO_PI - yaw, 0, 0, 1);
    quat *= az_quat;

    LLVirtualTrackball *trackBall = getChild<LLVirtualTrackball>(POSER_AVATAR_TRACKBALL_NAME);
    if (!trackBall)
        return;

    trackBall->setRotation(quat);

    LLSliderCtrl *yawSlider  = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_YAW_NAME);
    LLSliderCtrl *pitchSlider  = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_PITCH_NAME);
    LLSliderCtrl *rollSlider = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_ROLL_NAME);
    if (!yawSlider || !pitchSlider || !rollSlider)
        return;

    yawSlider->setValue(yaw *= RAD_TO_DEG);
    pitchSlider->setValue(pitch *= RAD_TO_DEG);
    rollSlider->setValue(roll *= RAD_TO_DEG);
}

/// <summary>
/// An event handler for selecting an avatar or animesh on the POSES_AVATAR_SCROLL_LIST_NAME.
/// In general this will refresh the views for joints or their proxies, and (dis/en)able elements of the view.
/// </summary>
void FSFloaterPoser::onAvatarsSelect()
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
}

void FSFloaterPoser::onAvatarsRefresh()
{
    LLScrollListCtrl *avatarScrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_AVATARSELECTION);
    if (!avatarScrollList)
        return;

    LLSD selectedName = avatarScrollList->getSelectedValue();
    avatarScrollList->clearRows();

    // Add non-Animesh avatars
    for (LLCharacter *character : LLCharacter::sInstances)
    {
        LLVOAvatar *avatar = dynamic_cast<LLVOAvatar *>(character);
        if (!couldAnimateAvatar(avatar))
            continue;

        if (avatar->isControlAvatar())
            continue;

        LLUUID       uuid = avatar->getID();
        LLAvatarName av_name;
        if (!LLAvatarNameCache::get(uuid, &av_name))
            continue;

        LLSD row;
        row["columns"][0]["column"] = "icon";
        row["columns"][0]["type"]   = "icon";
        row["columns"][0]["value"]  = getString("icon_category");
        row["columns"][1]["column"] = "name";
        row["columns"][1]["value"]  = av_name.getDisplayName();
        row["columns"][2]["column"] = "uuid";
        row["columns"][2]["value"]  = avatar->getID();
        row["columns"][3]["column"] = "control_avatar";
        row["columns"][3]["value"]  = false;
        LLScrollListItem *item      = avatarScrollList->addElement(row);
        item->setUserdata(avatar);

        if (_poserAnimator.isPosingAvatar(avatar))
            ((LLScrollListText *) item->getColumn(COL_NAME))->setFontStyle(LLFontGL::BOLD);
    }

    // Add Animesh avatars
    for (auto character : LLControlAvatar::sInstances)
    {
        LLControlAvatar *avatar = dynamic_cast<LLControlAvatar *>(character);
        if (!couldAnimateAvatar(avatar))
            continue;

        LLSD row;
        row["columns"][0]["column"] = "icon";
        row["columns"][0]["type"]   = "icon";
        row["columns"][0]["value"]  = getString("icon_object");
        row["columns"][1]["column"] = "name";
        row["columns"][1]["value"]  = avatar->getFullname();
        row["columns"][2]["column"] = "uuid";
        row["columns"][2]["value"]  = avatar->getID();
        row["columns"][3]["column"] = "control_avatar";
        row["columns"][3]["value"]  = true;
        LLScrollListItem *item      = avatarScrollList->addElement(row);
        item->setUserdata(avatar);

        if (_poserAnimator.isPosingAvatar(avatar))
            ((LLScrollListText *) item->getColumn(COL_NAME))->setFontStyle(LLFontGL::BOLD);
    }

    avatarScrollList->selectByValue(selectedName);
    avatarScrollList->updateLayout();
}
