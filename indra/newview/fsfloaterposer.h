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
#include "llscrolllistctrl.h"
#include "llsliderctrl.h"
#include "lltabcontainer.h"
#include "lltoggleablemenu.h"
#include "llmenubutton.h"
#include "fsposeranimator.h"

/// <summary>
/// Describes how to load a pose file.
/// </summary>
typedef enum E_LoadPoseMethods
{
    ROTATIONS               = 1,
    POSITIONS               = 2,
    SCALES                  = 4,
    ROTATIONS_AND_POSITIONS = 3,
    ROTATIONS_AND_SCALES    = 4,
    POSITIONS_AND_SCALES    = 5,
    ROT_POS_AND_SCALES      = 6
} E_LoadPoseMethods;

/// <summary>
/// Describes the columns of a scroll-list.
/// </summary>
typedef enum E_Columns
{
    COL_ICON = 0,
    COL_NAME = 1,
} E_Columns;

/// <summary>
/// A class containing the UI fiddling for the Poser Floater.
/// Please don't do LLJoint stuff here, fsposeranimator is the class for that.
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
    const F32 normalTrackballRangeInRads = F_PI;

    /// <summary>
    /// The counterpart to above, when using the trackpad in zoom-mode, its maximum deflection on either axis is plus/minus this.
    /// </summary>
    const F32 zoomedTrackballRangeInRads = F_PI_BY_TWO;
    
    /// <summary>
    /// Refreshes the pose load/save list.
    /// </summary>
    void refreshPosesScroll();

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
    std::vector<FSPoserAnimator::FSPoserJoint *> getUiSelectedPoserJoints();

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
    /// There are several control-callbacks manipulating rotations etc, they all devolve to these.
    /// In these are the appeals to the posing business layer.
    /// </summary>
    void setSelectedJointsRotation(F32 yawInRadians, F32 pitchInRadians, F32 rollInRadians);
    void setSelectedJointsPosition(F32 x, F32 y, F32 z);
    void setSelectedJointsScale(F32 x, F32 y, F32 z);

    // Pose load/save
    void onToggleLoadSavePanel();
    void onClickPoseSave();
    void onPoseFileSelect();
    bool savePoseToXml(std::string posePath);
    void onClickBrowsePoseCache();
    void onPoseMenuAction(const LLSD& param);
    void loadPoseFromXml(std::string poseFileName, E_LoadPoseMethods loadMethod);

    // UI Event Handlers:
    void onAvatarsRefresh();
    void onAvatarsSelect();
    void onJointSelect();
    void onToggleAdvancedPanel();
    void onToggleMirrorChange();
    void onToggleSympatheticChange();
    void onUndoLastRotation();
    void onPoseStartStop();
    void onLimbTrackballChanged();
    void onLimbYawPitchRollChanged();
    void onAvatarPositionSet();
    void onAdvancedPositionSet();
    void onAdvancedRotationSet();
    void onAdvancedScaleSet();

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
    FSPoserAnimator       _poserAnimator;

    /// <summary>
    /// The supplied llJoint has a quaternion (and alternatively oily angles) describing its rotation.
    /// This gets the kind of axial transformation required for viewing the joint's Euler angles on our UI.
    /// </summary>
    /// <param name="jointName">The well-known name of the joint, eg: mChest.</param>
    /// <returns>The axial translation so the oily angles make better sense in terms of up/down/left/right/roll.</returns>
    /// <remarks>
    /// Euler angles aren't cartesian; they're one of 12 possible orderings or something, yes, yes.
    /// No the translation isn't untangling all of that, it's not needed until it is.
    /// We're not landing on Mars with this code, just offering a user reasonable thumb-twiddlings.
    /// </remarks>
    E_BoneAxisTranslation FSFloaterPoser::getJointTranslation(std::string jointName);

    /// <summary>
    /// Gets the collection of E_BoneAxisNegation values for the supplied joint.
    /// </summary>
    /// <param name="jointName"></param>
    /// <returns></returns>
    S32                   FSFloaterPoser::getJointNegation(std::string jointName);

    /// <summary>
    /// The smallest text embiggens the noble selection.
    /// </summary>
    void refreshTextEmbiggeningOnAllScrollLists();
    void addBoldToScrollList(std::string listName, LLVOAvatar *avatar);
};

#endif
