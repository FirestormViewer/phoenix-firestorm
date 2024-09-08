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


#ifndef BD_FLOATER_POSER_H
#define BD_FLOATER_POSER_H

#include "llfloater.h"
#include "fsscrolllistctrl.h"
#include "llsliderctrl.h"
#include "lltabcontainer.h"
#include "llkeyframemotion.h"
#include "lltoggleablemenu.h"
#include "llmenubutton.h"

#include "llviewerobject.h"

typedef enum E_BoneTypes
{
    JOINTS = 0,
    COLLISION_VOLUMES = 1,
    ATTACHMENT_BONES = 2
} E_BoneTypes;

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

class BDFloaterPoser :
    public LLFloater
{
    friend class LLFloaterReg;
private:
    BDFloaterPoser(const LLSD& key);
    /*virtual*/ ~BDFloaterPoser();
    /*virtual*/ bool postBuild();
    /*virtual*/ void draw();
    /*virtual*/ void onOpen(const LLSD& key);
    /*virtual*/ void onClose(bool app_quitting);

    //BD - Posing
    bool onClickPoseSave(const LLSD& param);
    void onPoseStart();
    void onPoseDelete();
    void onPoseRefresh();
    void onPoseControlsRefresh();
    bool onPoseSave();
    void onPoseLoad();
    void onPoseLoadSelective(const LLSD& param);
    void onPoseMenuAction(const LLSD& param);

    //BD - Joints
    void onJointRefresh();
    void onJointSet(LLUICtrl* ctrl, const LLSD& param);
    void onJointPosSet(LLUICtrl* ctrl, const LLSD& param);
    void onJointScaleSet(LLUICtrl* ctrl, const LLSD& param);
    void onJointChangeState();
    void onJointControlsRefresh();
    void onJointRotPosScaleReset();
    void onJointRotationReset();
    void onJointPositionReset();
    void onJointScaleReset();
    void onJointRotationRevert();
    void onJointRecapture();
    void onCollectDefaults();
    void onJointContextMenuAction(const LLSD& param);
    bool onJointContextMenuEnable(const LLSD& param);
    //BD - Joints - Utilities
    void onJointPasteRotation();
    void onJointPastePosition();
    void onJointPasteScale();
    void onJointMirror();
    void onJointSymmetrize(bool from = true);
    void onJointCopyTransforms();

    //BD - Misc
    void onUpdateLayout();

    //BD - Mirror Bone
    void toggleMirrorMode(LLUICtrl* ctrl) { mMirrorMode = ctrl->getValue().asBoolean(); }
    void toggleEasyRotations(LLUICtrl* ctrl) { mEasyRotations = ctrl->getValue().asBoolean(); }

    //BD - Flip Poses
    void onFlipPose();

    void onPoseSymmetrize(const LLSD& param);

    //BD - Animesh
    void onAvatarsRefresh();
    void onAvatarsSelect();

    //BD
    void loadPoseRotations(std::string name, LLVector3 *rotations);
    void loadPosePositions(std::string name, LLVector3 *rotations);
    void loadPoseScales(std::string name, LLVector3 *rotations);

private:
    //BD - Posing
    FSScrollListCtrl*                           mPoseScroll;
    LLTabContainer*                             mJointTabs;
    LLTabContainer*                             mModifierTabs;

    std::array<LLUICtrl*, 3>                    mRotationSliders;
    std::array<LLSliderCtrl*, 3>                mPositionSliders;
    std::array<LLSliderCtrl*, 3>                mScaleSliders;
    std::array<FSScrollListCtrl*, 3>            mJointScrolls;

    //BD - I really didn't want to do this this way but we have to.
    //     It's the easiest way doing this.
    std::map<const std::string, LLQuaternion>   mDefaultRotations;
    std::map<const std::string, LLVector3>      mDefaultScales;
    std::map<const std::string, LLVector3>      mDefaultPositions;

    //BD - Misc
    bool                                        mDelayRefresh;
    bool                                        mEasyRotations;

    //BD - Mirror Bone
    bool                                        mMirrorMode;

    //BD - Animesh
    FSScrollListCtrl*                           mAvatarScroll;

    LLButton*                                   mStartPosingBtn;
    LLMenuButton*                               mLoadPosesBtn;
    LLButton*                                   mSavePosesBtn;

    LLSD                                        mClipboard;
};

#endif
