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


#ifndef BD_FLOATER_POSE_CREATOR_H
#define BD_FLOATER_POSE_CREATOR_H

#include "llfloater.h"
#include "fsscrolllistctrl.h"
#include "llsliderctrl.h"
#include "lltabcontainer.h"
#include "llkeyframemotion.h"
#include "lltoggleablemenu.h"
#include "llmenubutton.h"
#include "llspinctrl.h"
#include "bdposingmotion.h"

#include "llviewerobject.h"

class BDFloaterPoseCreator :
    public LLFloater
{
    friend class LLFloaterReg;
private:
    BDFloaterPoseCreator(const LLSD& key);
    /*virtual*/ ~BDFloaterPoseCreator();
    /*virtual*/ bool postBuild();
    /*virtual*/ void draw();
    /*virtual*/ void onOpen(const LLSD& key);
    /*virtual*/ void onClose(bool app_quitting);

    //BD - Posing
    bool onClickPoseSave(const LLSD& param);
    void onPoseStart();
    bool onPoseExport();
    void onPoseImport();
    void onPoseStartStop();
    void onPoseReapply();
    void onEditAnimationInfo(const LLSD& param);
    void onInterpolationChange(LLUICtrl* ctrl);
    void onAnimationDurationCheck();

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
    void onCollectDefaults();
    void onJointContextMenuAction(const LLSD& param);
    bool onJointContextMenuEnable(const LLSD& param);
    //BD - Joints - Utilities
    void onJointPasteRotation();
    void onJointPastePosition();
    void onJointPasteScale();
    void onJointMirror();
    void onJointSymmetrize();
    void onJointCopyTransforms();
    //BD - Keyframes
    void onKeyframeSelect();
    void onKeyframeAdd();
    void onKeyframeAdd(F32 time, LLJoint* joint);
    void onKeyframeRemove();
    void onKeyframeTime();
    void onKeyframeRefresh();
    void onKeyframesRebuild();
    void onKeyframeResetAll();

    //BD - Misc
    LLKeyframeMotion* onReadyTempMotion(std::string filename = "_poser_temp.anim", bool eternal = false);
    void onCreateTempMotion();

    //BD - Mirror Bone
    void toggleMirrorMode(LLUICtrl* ctrl) { mMirrorMode = ctrl->getValue().asBoolean(); }
    void toggleEasyRotations(LLUICtrl* ctrl) { mEasyRotations = ctrl->getValue().asBoolean(); }

    //BD - Flip Poses
    void onFlipPose();

private:
    //BD - Posing
    LLTabContainer*                             mJointTabs;
    LLTabContainer*                             mModifierTabs;
    FSScrollListCtrl*                           mKeyframeScroll;
    FSScrollListCtrl*                           mTimelineScroll;
    LLHandle<LLToggleableMenu>                  mSaveMenuHandle;

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
    bool                                        mAutoDuration;

    LLUUID                                      mTempMotionID;

    //BD - Mirror Bone
    bool                                        mMirrorMode;

    LLButton*                                   mStartPosingBtn;

    LLSD                                        mClipboard;
};

#endif
