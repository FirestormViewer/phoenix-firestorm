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


#ifndef FS_FLOATER_POSER_H
#define FS_FLOATER_POSER_H

#include "llfloater.h"
#include "fsposeranimator.h"

/// <summary>
/// Describes how to load a pose file.
/// </summary>
typedef enum E_LoadPoseMethods
{
    ROTATIONS               = 1,
    POSITIONS               = 2,
    SCALES                  = 3,
    ROTATIONS_AND_POSITIONS = 4,
    ROTATIONS_AND_SCALES    = 5,
    POSITIONS_AND_SCALES    = 6,
    ROT_POS_AND_SCALES      = 7,
    HAND_RIGHT              = 8,
    HAND_LEFT               = 9,
    FACE_ONLY               = 10,
} E_LoadPoseMethods;

/// <summary>
/// Describes the columns of the avatars scroll-list.
/// </summary>
typedef enum E_Columns
{
    COL_ICON = 0,
    COL_NAME = 1,
    COL_UUID = 2,
    COL_SAVE = 3,
} E_Columns;

/// <summary>
/// A class containing the UI fiddling for the Poser Floater.
/// Please don't do LLJoint stuff here, fsposingmotion (the LLMotion derivative) is the class for that.
/// </summary>
class FSFloaterPoser : public LLFloater
{
    friend class LLFloaterReg;
    FSFloaterPoser(const LLSD &key);

  private:
    /*virtual*/ ~FSFloaterPoser();
    /*virtual*/ bool postBuild();
    /*virtual*/ void draw();
    /*virtual*/ void onOpen(const LLSD& key);
    /*virtual*/ void onClose(bool app_quitting);

    /// <summary>
    /// The amount of deflection 'one unit' on the trackpad translates to in radians.
    /// The trackpad ordinarily has a range of +1..-1; multiplied by PI, gives PI to -PI, or all 360 degrees of deflection.
    /// </summary>
    const F32 normalTrackpadRangeInRads = F_PI;
    
    /// <summary>
    /// Refreshes the supplied pose list from the supplued subdirectory.
    /// </summary>
    void refreshPoseScroll(std::string_view scrollListName, std::string_view subDirectory);

    /// <summary>
    /// (Dis)Enables all of the posing controls; such as when you can't pose for reasons.
    /// </summary>
    /// <param name="enable">Whether to enable the pose controls.</param>
    void poseControlsEnable(bool enable);

    /// <summary>
    /// Refreshes all of the 'joint/bones/thingos' lists (lists-zah, all of them).
    /// </summary>
    void refreshJointScrollListMembers();

    /// <summary>
    /// Adds a 'header' menu item to the supplied scroll list, handy for demarking clusters of joints on the UI.
    /// </summary>
    /// <param name="jointName">
    /// The well-known name of the joint, eg: mChest
    /// This does a lookup into the poser XML for a friendly header title by joint name, if it exists.
    /// </param>
    /// <param name="bodyJointsScrollList">The scroll list to add the header-row to.</param>
    void AddHeaderRowToScrollList(std::string jointName, LLScrollListCtrl *bodyJointsScrollList);

    /// <summary>
    /// Generates the data for a row to add to a scroll-list.
    /// The supplied joint name is looked up in the UI XML to find a friendly name.
    /// </summary>
    /// <param name="jointName">The well-known joint name of the joint to add the row for, eg: mChest.</param>
    /// <param name="isHeaderRow">Whether the joint is one which should come immediately after a header.</param>
    /// <returns>The data required to make the row.</returns>
    LLSD createRowForJoint(std::string jointName, bool isHeaderRow);

    /// <summary>
    /// Gets the collection of poser joints currently selected on the active bones-tab of the UI.
    /// </summary>
    /// <returns>The selected joints</returns>
    std::vector<FSPoserAnimator::FSPoserJoint *> getUiSelectedPoserJoints() const;

    /// <summary>
    /// Gets a detectable avatar by its UUID.
    /// </summary>
    /// <param name="avatarToFind">The ID of the avatar to find.</param>
    /// <returns>The avatar, if found, otherwise nullptr.</returns>
    LLVOAvatar* getAvatarByUuid(LLUUID avatarToFind);

    /// <summary>
    /// Gets the currently selected avatar or animesh.
    /// </summary>
    /// <returns>The currently selected avatar or animesh.</returns>
    LLVOAvatar *getUiSelectedAvatar();

    /// <summary>
    /// Gets the current bone-deflection style: encapsulates 'anything else you want to do' while you're manipulating a joint.
    /// Such as: fiddle the opposite joint too.
    /// </summary>
    /// <returns>A E_BoneDeflectionStyles member.</returns>
    E_BoneDeflectionStyles getUiSelectedBoneDeflectionStyle();

    /// <summary>
    /// Gets the collection of UUIDs for nearby avatars.
    /// </summary>
    /// <returns>A the collection of UUIDs for nearby avatars.</returns>
    uuid_vec_t getNearbyAvatarsAndAnimeshes();

    /// <summary>
    /// Gets a collection of UUIDs for avatars currently being presented on the UI.
    /// </summary>
    /// <returns>A the collection of UUIDs.</returns>
    uuid_vec_t getCurrentlyListedAvatarsAndAnimeshes();

    /// <summary>
    /// Gets the scroll-list index of the supplied avatar.
    /// </summary>
    /// <param name="toFind">The avatar UUID to find on the avatars scroll list.</param>
    /// <returns>The scroll-list index for the supplied avatar, if found, otherwise -1.</returns>
    S32 getAvatarListIndexForUuid(LLUUID toFind);

    /// <summary>
    /// There are several control-callbacks manipulating rotations etc, they all devolve to these.
    /// In these are the appeals to the posing business layer.
    /// </summary>
    /// <remarks>
    /// Using a set, then a get does not guarantee the value you just set.
    /// There may be +/- PI difference two axes, because harmonics.
    /// Thus keep your UI synced with less gets.
    /// </remarks>
    void setSelectedJointsRotation(F32 yawInRadians, F32 pitchInRadians, F32 rollInRadians);
    void setSelectedJointsPosition(F32 x, F32 y, F32 z);
    void setSelectedJointsScale(F32 x, F32 y, F32 z);

    /// <summary>
    /// Yeilds the rotation of the first selected joint (one may multi-select).
    /// </summary>
    /// <remarks>
    /// Using a set, then a get does not guarantee the value you just set.
    /// There may be +/- PI difference two axes, because harmonics.
    /// Thus keep your UI synced with less gets.
    /// </remarks>
    LLVector3 getRotationOfFirstSelectedJoint();
    LLVector3 getPositionOfFirstSelectedJoint();
    LLVector3 getScaleOfFirstSelectedJoint();

    // Pose load/save
    void onToggleLoadSavePanel();
    void onClickPoseSave();
    void onPoseFileSelect();
    bool savePoseToXml(LLVOAvatar* avatar, std::string posePath);
    bool savePoseToBvh(LLVOAvatar* avatar, std::string posePath);
    void onClickBrowsePoseCache();
    void onPoseMenuAction(const LLSD& param);
    void loadPoseFromXml(LLVOAvatar* avatar, std::string poseFileName, E_LoadPoseMethods loadMethod);
    void setPoseSaveFileTextBoxToUiSelectedAvatarSaveFileName();
    void setUiSelectedAvatarSaveFileName(std::string saveFileName);
    void showOrHideAdvancedSaveOptions();

    // UI Event Handlers:
    void onAvatarsRefresh();
    void onAvatarSelect();
    void onJointSelect();
    void onToggleAdvancedPanel();
    void onToggleMirrorChange();
    void onToggleSympatheticChange();
    void onToggleDeltaModeChange();
    void setRotationChangeButtons(bool mirror, bool sympathetic, bool togglingDelta);
    void onUndoLastRotation();
    void onRedoLastRotation();
    void onUndoLastPosition();
    void onRedoLastPosition();
    void onUndoLastScale();
    void onRedoLastScale();
    void onResetPosition();
    void onResetScale();
    void enableOrDisableRedoButton();
    void onPoseStartStop();
    void onLimbTrackballChanged();
    void onLimbYawPitchRollChanged();
    void onAvatarPositionSet();
    void onAdvancedPositionSet();
    void onAdvancedScaleSet();
    void onClickToggleSelectedBoneEnabled();
    void onClickRecaptureSelectedBones();
    void onClickFlipPose();
    void onClickFlipSelectedJoints();
    void onPoseJointsReset();
    void onOpenSetAdvancedPanel();
    void onAdjustTrackpadSensitivity();
    void onClickLoadLeftHandPose();
    void onClickLoadRightHandPose();
    void onClickLoadHandPose(bool isRightHand);

    // UI Refreshments
    void refreshRotationSliders();
    void refreshAvatarPositionSliders();
    void refreshTrackpadCursor();
    void refreshAdvancedPositionSliders();
    void refreshAdvancedScaleSliders();

    /// <summary>
    /// Determines if we have permission to animate the supplied avatar.
    /// </summary>
    /// <param name="avatar">The avatar to animate.</param>
    /// <returns>True if we have permission to animate, otherwise false.</returns>
    bool havePermissionToAnimateAvatar(LLVOAvatar *avatar);

    /// <summary>
    /// Determines if we could animate the supplied avatar.
    /// </summary>
    /// <param name="avatar">The avatar to animate.</param>
    /// <returns>True if the avatar is non-null, not dead, in the same region as self, otherwise false.</returns>
    bool couldAnimateAvatar(LLVOAvatar *avatar);

    /// <summary>
    /// Our instance of the class which lets us do the business of manipulating the avatar.
    /// This separates that business from the code-behind the UI.
    /// </summary>
    FSPoserAnimator _poserAnimator;

    /// <summary>
    /// The supplied Joint name has a quaternion describing its rotation.
    /// This gets the kind of axial transformation required for 'easy' consumption of the joint's Euler angles on our UI.
    /// This facilitates 'conceptual' conversion of Euler frame to up/down, left/right and roll and is rather subjective.
    /// Thus, many of these 'conversions' are backed by values in the XML.
    /// </summary>
    /// <param name="jointName">The well-known name of the joint, eg: mChest.</param>
    /// <returns>The axial translation so the oily angles make better sense in terms of up/down/left/right/roll.</returns>
    /// <remarks>
    /// Euler angles aren't cartesian; they're one of 12 possible orderings or something, yes, yes.
    /// No the translation isn't untangling all of that, it's not needed until it is.
    /// We're not landing on Mars with this code, just offering a user reasonable thumb-twiddlings.
    /// </remarks>
    E_BoneAxisTranslation getJointTranslation(std::string jointName);

    /// <summary>
    /// Gets the collection of E_BoneAxisNegation values for the supplied joint.
    /// </summary>
    /// <param name="jointName">The name of the joind to get the axis transformation for.</param>
    /// <returns>The kind of axis transformation to perform.</returns>
    S32 getJointNegation(std::string jointName);

    /// <summary>
    /// The smallest text embiggens the noble selection.
    /// </summary>
    void refreshTextEmbiggeningOnAllScrollLists();

    /// <summary>
    /// Applies the appropriate font-face (such as bold) to the text of the supplied list, to indicate use.
    /// </summary>
    /// <param name="listName">The name of the list to adjust text-face for.</param>
    /// <param name="avatar">The avatar to whom the list is relevant.</param>
    void addBoldToScrollList(std::string listName, LLVOAvatar *avatar);

    /// <summary>
    /// The time when the last click of a button was made.
    /// Utilized for controls needing a 'double click do' function.
    /// </summary>
    std::chrono::system_clock::time_point _timeLastClickedJointReset = std::chrono::system_clock::now();

    /// <summary>
    /// The constant time interval, in seconds, a user must click twice within to successfully double-click a button.
    /// </summary>
    std::chrono::duration<double> const _doubleClickInterval = std::chrono::duration<double>(0.3);

    /// <summary>
    /// Unwraps a normalized value from the trackball to a slider value.
    /// </summary>
    /// <param name="scale">The scale value from the trackball.</param>
    /// <returns>A value appropriate for fitting a slider.</returns>
    static F32 unWrapScale(F32 scale);
};

#endif
