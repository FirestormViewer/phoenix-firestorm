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

typedef enum E_Columns
{
    COL_ICON = 0,
    COL_NAME = 1,
    COL_ROT_X = 2,
    COL_ROT_Y = 3,
    COL_ROT_Z = 4,
    COL_POS_X = 5,
    COL_POS_Y = 6,
    COL_POS_Z = 7,
    COL_SCALE_X = 8,
    COL_SCALE_Y = 9,
    COL_SCALE_Z = 10
} E_Columns;

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

    // UI Service functions
    void refreshPosesScroll();
    void poseControlsEnable(bool enable);
    void refreshJointScrollListMembers();
    void AddHeaderRowToScrollList(std::string jointName, LLScrollListCtrl *bodyJointsScrollList);
    LLSD createRowForJoint(std::string jointName, bool isHeaderRow);
    std::vector<FSPoserAnimator::FSPoserJoint *> getUiSelectedPoserJoints();
    LLVOAvatar                                  *getUiSelectedAvatar();
    E_BoneDeflectionStyles                       getUiSelectedBoneDeflectionStyle();
    void                                         addRotationToRecentlySet(F32 aziInRadians, F32 eleInRadians, F32 rollInRadians);
    void                                         clearRecentlySetRotations();
    void setSelectedJointsRotation(F32 aziInRadians, F32 eleInRadians, F32 rollInRadians);
    void setSelectedJointsPosition(F32 x, F32 y, F32 z);
    void setSelectedJointsScale(F32 x, F32 y, F32 z);

    // Pose load/save
    void onToggleLoadSavePanel();
    void onClickPoseSave();
    void onPoseFileSelect();
    bool savePoseToXml(std::string posePath);
    void onPoseLoad();
    void onPoseLoadSelective(const LLSD& param);
    void onPoseMenuAction(const LLSD& param);

    // UI Event Handlers:
    void onAvatarsRefresh();
    void onAvatarsSelect();
    void onJointSelect();
    void onToggleAdvancedPanel();
    void onToggleMirrorChange();
    void onToggleSympatheticChange();
    void onUndoLastRotation();
    void onPoseStartStop();
    void onPoseDelete();
    void onLimbTrackballChanged();
    void onLimbAziEleRollChanged();
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

private:
    FSPoserAnimator       _poserAnimator;
    std::stack<LLVector3> _lastSetRotations;
};

#endif
