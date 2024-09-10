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

static const std::string POSE_INTERNAL_FORMAT_FILE_MASK     = "*.xml";
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
static const std::string POSER_AVATAR_SLIDER_AZI_NAME  = "limb_azimuth";
static const std::string POSER_AVATAR_SLIDER_ELE_NAME  = "limb_elevation";
static const std::string POSER_AVATAR_SLIDER_ROLL_NAME = "limb_roll";
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
    mCommitCallbackRegistrar.add("Poser.ToggleMirrorChanges", boost::bind(&FSFloaterPoser::onToggleMirrorChange, this));
    mCommitCallbackRegistrar.add("Poser.ToggleSympatheticChanges", boost::bind(&FSFloaterPoser::onToggleSympatheticChange, this));

    mCommitCallbackRegistrar.add("Poser.PositionSet", boost::bind(&FSFloaterPoser::onAvatarPositionSet, this));

    mCommitCallbackRegistrar.add("Poser.Advanced.PositionSet", boost::bind(&FSFloaterPoser::onAdvancedPositionSet, this));
    mCommitCallbackRegistrar.add("Poser.Advanced.RotationSet", boost::bind(&FSFloaterPoser::onAdvancedRotationSet, this));
    mCommitCallbackRegistrar.add("Poser.Advanced.ScaleSet", boost::bind(&FSFloaterPoser::onAdvancedScaleSet, this));

    mCommitCallbackRegistrar.add("Pose.Save", boost::bind(&FSFloaterPoser::onClickPoseSave, this));
    mCommitCallbackRegistrar.add("Pose.Load", boost::bind(&FSFloaterPoser::onPoseLoad, this));
    mCommitCallbackRegistrar.add("Pose.Menu", boost::bind(&FSFloaterPoser::onPoseMenuAction, this, _2));
    mCommitCallbackRegistrar.add("Pose.Delete", boost::bind(&FSFloaterPoser::onPoseDelete, this));
}

FSFloaterPoser::~FSFloaterPoser()
{
    clearRecentlySetRotations();
}

bool FSFloaterPoser::postBuild()
{
    // find-and-binds
    getChild<LLUICtrl>(POSER_AVATAR_TRACKBALL_NAME)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onLimbTrackballChanged(); });
    getChild<LLUICtrl>(POSER_AVATAR_SLIDER_AZI_NAME)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onLimbAziEleRollChanged(); });
    getChild<LLUICtrl>(POSER_AVATAR_SLIDER_ELE_NAME)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onLimbAziEleRollChanged(); });
    getChild<LLUICtrl>(POSER_AVATAR_SLIDER_ROLL_NAME)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onLimbAziEleRollChanged(); });

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
            gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, POSE_SAVE_SUBDIRECTORY, poseFileName + POSE_INTERNAL_FORMAT_FILE_MASK);

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

void FSFloaterPoser::onPoseLoad()
{
    LLScrollListCtrl *posesScrollList = getChild<LLScrollListCtrl>(POSER_AVATAR_SCROLLLIST_LOADSAVE_NAME);
    if (!posesScrollList)
        return;

    LLScrollListItem *item = posesScrollList->getFirstSelected();
    if (!item)
        return;

    std::string pose_name = item->getColumn(0)->getValue().asString();

    // TODO: gDragonAnimator.loadPose(pose_name);
    refreshJointScrollListMembers();
}

// TODO: implement
// Needs UI button.
// Even better, forget this and pop open an OS file handler window so the user can better manage files than our buttons ever could.
// No delete button means no accidental deletion.
// If one has to go to the OS, actions are out of our context.
// OS probably offers undo-delete/move/etc.
// In which case we might want a refresh button to cue user to re-scrape the poses dir.
void FSFloaterPoser::onPoseDelete()
{
    /*for (auto item : mPoseScroll->getAllSelected())
    {
        std::string filename = item->getColumn(0)->getValue().asString();
        std::string dirname = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "poses");

        if (gDirUtilp->deleteFilesInDir(dirname, filename + ".xml") < 1)
        {
            LL_WARNS("Posing") << "Cannot remove file: " << filename << LL_ENDL;
        }
    }
    onPoseRefresh();*/
}

void FSFloaterPoser::onPoseMenuAction(const LLSD &param) { onPoseLoadSelective(param); }

// TODO: detangle
// This is useful when posing others with a pose we made on our avatar.
// This is so we don't wreck their shape with scale/position changes from our avatar, but give them rotations.
void FSFloaterPoser::onPoseLoadSelective(const LLSD& param)
{
    //LLScrollListItem* item = mPoseScroll->getFirstSelected();
    //if (!item) return;

    //std::string pose_name = item->getColumn(0)->getValue().asString();

    //S32 load_type = 0;
    //if (param.asString() == "rotation")
    //    load_type |= ROTATIONS;
    //else if (param.asString() == "position")
    //    load_type |= POSITIONS;
    //else if (param.asString() == "scale")
    //    load_type |= SCALES;
    //else if (param.asString() == "rot_pos")
    //    load_type |= ROTATIONS | POSITIONS;
    //else if (param.asString() == "rot_scale")
    //    load_type |= ROTATIONS | SCALES;
    //else if (param.asString() == "pos_scale")
    //    load_type |= POSITIONS | SCALES;
    //else if (param.asString() == "all")
    //    load_type |= ROTATIONS | POSITIONS | SCALES;

    //gDragonAnimator.loadPose(pose_name, load_type);
    //onJointRefresh();
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

void FSFloaterPoser::addRotationToRecentlySet(F32 aziInRadians, F32 eleInRadians, F32 rollInRadians)
{
    _lastSetRotations.push(LLVector3(aziInRadians, eleInRadians, rollInRadians));
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

    F32 azimuth   = xRotAdvSlider->getValue().asReal();
    F32 elevation = yRotAdvSlider->getValue().asReal();
    F32 roll      = zRotAdvSlider->getValue().asReal();

    setSelectedJointsRotation(azimuth, elevation, roll);
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

void FSFloaterPoser::onLimbTrackballChanged()
{
    LLVirtualTrackball *trackBall = getChild<LLVirtualTrackball>(POSER_AVATAR_TRACKBALL_NAME);
    if (!trackBall)
        return;

    LLSliderCtrl *aziSlider = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_AZI_NAME);
    LLSliderCtrl *eleSlider  = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_ELE_NAME);
    LLSliderCtrl *rollSlider = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_ROLL_NAME);
    if (!aziSlider || !eleSlider || !rollSlider)
        return;

    LLQuaternion quat = trackBall->getRotation();

    F32 azimuth, elevation;
    LLVirtualTrackball::getAzimuthAndElevationDeg(quat, azimuth, elevation);

    aziSlider->setValue(azimuth);
    eleSlider->setValue(elevation);

    F32 roll = rollSlider->getValue().asReal();
    azimuth *= DEG_TO_RAD;
    elevation *= DEG_TO_RAD;
    roll *= DEG_TO_RAD;

    setSelectedJointsRotation(azimuth, elevation, roll);
}

void FSFloaterPoser::onLimbAziEleRollChanged()
{
    LLSliderCtrl *aziSlider = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_AZI_NAME);
    LLSliderCtrl *eleSlider = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_ELE_NAME);
    LLSliderCtrl *rollSlider = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_ROLL_NAME);
    if (!aziSlider || !eleSlider || !rollSlider)
        return;

    F32 azimuth   = aziSlider->getValue().asReal();
    F32 elevation = eleSlider->getValue().asReal();
    F32 roll      = rollSlider->getValue().asReal();

    azimuth *= DEG_TO_RAD;
    elevation *= DEG_TO_RAD;
    roll *= DEG_TO_RAD;

    if (is_approx_zero(elevation))
        elevation = F_APPROXIMATELY_ZERO;
    
    setSelectedJointsRotation(azimuth, elevation, roll);

    LLQuaternion quat;
    quat.setAngleAxis(-elevation, 0, 1, 0);
    LLQuaternion az_quat;
    az_quat.setAngleAxis(F_TWO_PI - azimuth, 0, 0, 1);
    quat *= az_quat;

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

void FSFloaterPoser::setSelectedJointsRotation(F32 aziInRadians, F32 eleInRadians, F32 rollInRadians)
{
    LLVOAvatar *avatar = getUiSelectedAvatar();
    if (!avatar)
        return;

    if (!_poserAnimator.isPosingAvatar(avatar))
        return;

    addRotationToRecentlySet(aziInRadians, eleInRadians, rollInRadians);

    E_BoneDeflectionStyles defl = getUiSelectedBoneDeflectionStyle();
    LLVector3              vec3 = LLVector3(aziInRadians, eleInRadians, rollInRadians);

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
    F32       azimuth   = rotation.mV[VX];
    F32       elevation = rotation.mV[VY];
    F32       roll      = rotation.mV[VZ];

    if (is_approx_zero(elevation))
        elevation = F_APPROXIMATELY_ZERO;

    LLQuaternion quat;
    quat.setAngleAxis(-elevation, 0, 1, 0);
    LLQuaternion az_quat;
    az_quat.setAngleAxis(F_TWO_PI - azimuth, 0, 0, 1);
    quat *= az_quat;

    LLVirtualTrackball *trackBall = getChild<LLVirtualTrackball>(POSER_AVATAR_TRACKBALL_NAME);
    if (!trackBall)
        return;

    trackBall->setRotation(quat);

    LLSliderCtrl *aziSlider  = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_AZI_NAME);
    LLSliderCtrl *eleSlider  = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_ELE_NAME);
    LLSliderCtrl *rollSlider = getChild<LLSliderCtrl>(POSER_AVATAR_SLIDER_ROLL_NAME);
    if (!aziSlider || !eleSlider || !rollSlider)
        return;

    aziSlider->setValue(azimuth *= RAD_TO_DEG);
    eleSlider->setValue(elevation *= RAD_TO_DEG);
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
