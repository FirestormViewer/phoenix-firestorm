/** 
 * @file llfloaterbvhpreview.h
 * @brief LLFloaterBvhPreview class definition
 *
 * $LicenseInfo:firstyear=2004&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
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

#ifndef LL_LLFLOATERBVHPREVIEW_H
#define LL_LLFLOATERBVHPREVIEW_H

#include "llassettype.h"
#include "llfloaternamedesc.h"
#include "lldynamictexture.h"
#include "llcharacter.h"
#include "llquaternion.h"
#include "llextendedstatus.h"

class LLVOAvatar;
class LLViewerJointMesh;
// <FS> Preview on own avatar
class LLFloaterBvhPreview;

class LLPreviewAnimation : public LLViewerDynamicTexture
{
protected:
	virtual ~LLPreviewAnimation();

public:
	LLPreviewAnimation(S32 width, S32 height);	

	/*virtual*/ S8 getType() const ;

	BOOL	render();
	void	requestUpdate();
	void	rotate(F32 yaw_radians, F32 pitch_radians);
	void	zoom(F32 zoom_delta);
	void	setZoom(F32 zoom_amt);
	void	pan(F32 right, F32 up);
	virtual BOOL needsUpdate() { return mNeedsUpdate; }

	LLVOAvatar* getDummyAvatar() { return mDummyAvatar; }
	// <FS> Preview on own avatar
	LLVOAvatar* getPreviewAvatar(LLFloaterBvhPreview* floaterp);

protected:
	BOOL				mNeedsUpdate;
	F32					mCameraDistance;
	F32					mCameraYaw;
	F32					mCameraPitch;
	F32					mCameraZoom;
	LLVector3			mCameraOffset;
	LLVector3			mCameraRelPos;
	LLPointer<LLVOAvatar>			mDummyAvatar;
};

class LLFloaterBvhPreview : public LLFloaterNameDesc
{
	// <FS> Preview on own avatar
	friend class LLPreviewAnimation;

public:
	LLFloaterBvhPreview(const std::string& filename);
	virtual ~LLFloaterBvhPreview();
	
	BOOL postBuild();

	BOOL handleMouseDown(S32 x, S32 y, MASK mask);
	BOOL handleMouseUp(S32 x, S32 y, MASK mask);
	BOOL handleHover(S32 x, S32 y, MASK mask);
	BOOL handleScrollWheel(S32 x, S32 y, S32 clicks); 
	void onMouseCaptureLost();

	void refresh();

	void onBtnPlay();
	void onBtnPause();	
	void onBtnStop();
	void onSliderMove();
	void onCommitBaseAnim();
	void onCommitLoop();
	void onCommitLoopIn();
	void onCommitLoopOut();
	bool validateLoopIn(const LLSD& data);
	bool validateLoopOut(const LLSD& data);
	void onCommitName();
	void onCommitHandPose();
	void onCommitEmote();
	void onCommitPriority();
	void onCommitEaseIn();
	void onCommitEaseOut();
	bool validateEaseIn(const LLSD& data);
	bool validateEaseOut(const LLSD& data);
	static void	onBtnOK(void*);
	// <FS> Reload animation from disk
	static void	onBtnReload(void*);
	static void onSaveComplete(const LLUUID& asset_uuid,
									   LLAssetType::EType type,
									   void* user_data,
									   S32 status, LLExtStat ext_status);
	// <FS:Sei> FIRE-17277: Allow entering Loop In/Loop Out as frames
	void onCommitLoopInFrames();
	void onCommitLoopOutFrames();
	bool validateLoopInFrames(const LLSD& data);
	bool validateLoopOutFrames(const LLSD& data);
	// </FS:Sei>
private:
	void setAnimCallbacks() ;
    std::map <std::string, std::string> getJointAliases();

	// <FS> Reload animation from disk
	BOOL loadBVH();
	void unloadMotion();
	// </FS>

protected:
	void			draw();
	void			resetMotion();

	LLPointer< LLPreviewAnimation > mAnimPreview;
	S32					mLastMouseX;
	S32					mLastMouseY;
	LLButton*			mPlayButton;
	LLButton*			mPauseButton;	
	LLButton*			mStopButton;
	LLRect				mPreviewRect;
	LLRectf				mPreviewImageRect;
	LLAssetID			mMotionID;
	LLTransactionID		mTransactionID;
	LLAnimPauseRequest	mPauseRequest;

	std::map<std::string, LLUUID>	mIDList;

	// <FS> Preview on own avatar
	bool mUseOwnAvatar;

	// <FS:Ansariel> FIRE-2083: Slider in upload animation floater doesn't work
	LLFrameTimer		mTimer;

	// <FS:Sei> FIRE-17277: Allow entering Loop In/Loop Out as frames
	S32					mNumFrames;
};

#endif  // LL_LLFLOATERBVHPREVIEW_H
