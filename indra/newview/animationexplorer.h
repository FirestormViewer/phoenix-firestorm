/** 
 * @file ao.h
 * @brief Animation Explorer floater declaration
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * Copyright (C) 2013, Zi Ree @ Second Life
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

#ifndef ANIMATIONEXPLORER_H
#define ANIMATIONEXPLORER_H

#include "llfloater.h"
#include "llfloaterbvhpreview.h"	// for LLPreviewAnimation
#include "llsingleton.h"

// --------------------------------------------------------------------------
// RecentAnimationList: holds a list of recently placed animations to query
// by the animation explorer, so we don't have to keep the full floater
// loaded all the time.
// --------------------------------------------------------------------------

class AnimationExplorer;

class RecentAnimationList
:	public LLSingleton<RecentAnimationList>
{
	LLSINGLETON(RecentAnimationList);
	~RecentAnimationList();

public:
	struct AnimationEntry
	{
		LLUUID animationID;		// asset ID of the animation
		LLUUID playedBy;		// object/agent who played this animation
		F64 time;				// time in seconds since viewer start when the animation started
	};

	std::deque<AnimationEntry> mAnimationList;

	void addAnimation(const LLUUID& id, const LLUUID& playedBy);		// called in llviewermessage.cpp
	void requestList(AnimationExplorer* explorer);					// request animation list
};

// --------------------------------------------------------------------------
// AnimationExplorer: floater that shows recently played animations and gives
// options to preview, stop animations and revoke animation permissions
// --------------------------------------------------------------------------

class LLButton;
class LLCheckBoxCtrl;
class LLMessageSystem;
class LLScrollListCtrl;
class LLView;

class AnimationExplorer
:	public LLFloater
{
	friend class LLFloaterReg;

	private:
		AnimationExplorer(const LLSD& key);
		~AnimationExplorer();

	public:
		/*virtual*/ BOOL postBuild();
		void addAnimation(const LLUUID& id, const LLUUID& playedBy, F64 time);	// called from RecentAnimationList

		// copied from llfloaterbvhpreview.h
		BOOL handleMouseDown(S32 x, S32 y, MASK mask);
		BOOL handleMouseUp(S32 x, S32 y, MASK mask);
		BOOL handleHover(S32 x, S32 y, MASK mask);
		BOOL handleScrollWheel(S32 x, S32 y, S32 clicks); 
		void onMouseCaptureLost();

		void requestNameCallback(LLMessageSystem* msg);		// object name query callback

	protected:
		LLScrollListCtrl* mAnimationScrollList;
		LLButton* mStopButton;
		LLButton* mRevokeButton;
		LLButton* mStopAndRevokeButton;
		LLCheckBoxCtrl* mNoOwnedAnimationsCheckBox;

		LLView* mPreviewCtrl;	// dummy control on the floater where the avatar preview should go
		LLPointer<LLPreviewAnimation> mAnimationPreview;	// actual avatar preview

		S32 mLastMouseX;
		S32 mLastMouseY;

		LLUUID mCurrentAnimationID;		// currently selected animation's asset ID
		LLUUID mCurrentObject;			// object ID that played the currently selected animation

		std::vector<LLUUID> mRequestedIDs;			// list of object IDs we requested named for
		std::map<LLUUID, std::string> mKnownIDs;		// known list of names for object IDs

		void draw();
		void update();								// request list update from RecentAnimationList
		void updateList(F64 current_timestamp);		// update times and playing status in animation list
		void startMotion(const LLUUID& motionID);

		void onSelectAnimation();
		void onStopPressed();
		void onRevokePressed();
		void onStopAndRevokePressed();
		void onOwnedCheckToggled();
};

#endif // ANIMATIONEXPLORER_H
