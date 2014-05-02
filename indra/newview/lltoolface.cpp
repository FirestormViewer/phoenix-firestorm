/** 
 * @file lltoolface.cpp
 * @brief A tool to manipulate faces
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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

#include "llviewerprecompiledheaders.h"

// File includes
#include "lltoolface.h" 

// Library includes
#include "llfloaterreg.h"
#include "v3math.h"

// Viewer includes
#include "llviewercontrol.h"
#include "llselectmgr.h"
#include "llviewerobject.h"
#include "llviewerwindow.h"
#include "llfloatertools.h"
// [RLVa:KB] - Checked: 2010-04-11 (RLVa-1.2.0e)
#include "rlvhandler.h"
// [/RLVa:KB]

// <FS:Zi> Add control to drag texture faces around
#include "llpanelface.h"
#include "llspinctrl.h"
#include "llkeyboard.h"
#include "llwindow.h"

BOOL LLToolFace::mTextureGrabbed=FALSE;
LLViewerObject* LLToolFace::mTextureObject=NULL;
S32 LLToolFace::mFaceGrabbed=0;

#undef TEXTURE_GRAB_UPDATE_REGULARLY	// continuous update of texture grabbing, might cause twitching
// </FS:Zi>

//
// Member functions
//

LLToolFace::LLToolFace()
:	LLTool(std::string("Texture"))
{ }


LLToolFace::~LLToolFace()
{ }


BOOL LLToolFace::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	if (!LLSelectMgr::getInstance()->getSelection()->isEmpty())
	{
		// You should already have an object selected from the mousedown.
		// If so, show its properties
		LLFloaterReg::showInstance("build", "Texture");
		return TRUE;
	}
	else
	{
		// Nothing selected means the first mouse click was probably
		// bad, so try again.
		return FALSE;
	}
}


BOOL LLToolFace::handleMouseDown(S32 x, S32 y, MASK mask)
{
	mGrabX=x;
	mGrabY=y;

	gViewerWindow->pickAsync(x, y, mask, pickCallback);
	return TRUE;
}

void LLToolFace::pickCallback(const LLPickInfo& pick_info)
{
	LLViewerObject* hit_obj	= pick_info.getObject();

	if (hit_obj)
	{
		S32 hit_face = pick_info.mObjectFace;
		
		if (hit_obj->isAvatar())
		{
			// ...clicked on an avatar, so don't do anything
			return;
		}

// [RLVa:KB] - Checked: 2010-11-29 (RLVa-1.3.0c) | Modified: RLVa-1.3.0c
		if ( (rlv_handler_t::isEnabled()) &&
			 ( (!gRlvHandler.canEdit(hit_obj)) || 
			   ((gRlvHandler.hasBehaviour(RLV_BHVR_FARTOUCH)) && (!gRlvHandler.canTouch(hit_obj, pick_info.mObjectOffset))) ) )
		{
			return;
		}
// [/RLVa:KB]

		// ...clicked on a world object, try to pick the appropriate face

		if (pick_info.mKeyMask & MASK_SHIFT)
		{
			// If object not selected, need to inform sim
			if ( !hit_obj->isSelected() )
			{
				// object wasn't selected so add the object and face
				LLSelectMgr::getInstance()->selectObjectOnly(hit_obj, hit_face);
			}
			else if (!LLSelectMgr::getInstance()->getSelection()->contains(hit_obj, hit_face) )
			{
				// object is selected, but not this face, so add it.
				LLSelectMgr::getInstance()->addAsIndividual(hit_obj, hit_face);
			}
			else
			{
				// object is selected, as is this face, so remove the face.
				LLSelectMgr::getInstance()->remove(hit_obj, hit_face);

				// BUG: If you remove the last face, the simulator won't know about it.
			}
		}
		else
		{
			// clicked without modifiers, select only
			// this face
			LLSelectMgr::getInstance()->deselectAll();
			LLSelectMgr::getInstance()->selectObjectOnly(hit_obj, hit_face);
		}

		// <FS:Zi> Add control to drag texture faces around
		if(gSavedSettings.getBOOL("FSExperimentalDragTexture"))
		{
			// if the same object and face was clicked as before, assume the user wants to grab it
			if(LLToolFace::mTextureObject==hit_obj && LLToolFace::mFaceGrabbed==pick_info.mObjectFace)
			{
				LLToolFace::mTextureGrabbed=TRUE;
				gViewerWindow->hideCursor();
			}
			else
			{
				LLToolFace::mTextureGrabbed=FALSE;
				gViewerWindow->showCursor();
			}
		}

		// remember the object being selected so we can check for grabbing later
		LLToolFace::mTextureObject=hit_obj;
		// also remember the selected object face
		LLToolFace::mFaceGrabbed=pick_info.mObjectFace;
		// </FS:Zi>
	}
	else
	{
		if (!(pick_info.mKeyMask == MASK_SHIFT))
		{
			LLSelectMgr::getInstance()->deselectAll();
		}
	}
}


void LLToolFace::handleSelect()
{
	// From now on, draw faces
	LLSelectMgr::getInstance()->setTEMode(TRUE);
}


void LLToolFace::handleDeselect()
{
	// Stop drawing faces
	LLSelectMgr::getInstance()->setTEMode(FALSE);

	stopGrabbing();	// <FS:Zi> Add control to drag texture faces around
}


void LLToolFace::render()
{
	// <FS:Zi> Add control to drag texture faces around
	// // for now, do nothing

#ifdef TEXTURE_GRAB_UPDATE_REGULARLY
	static S32 updateCounter=0;
	static BOOL teDirty=FALSE;
#endif

	// do nothing if no texture was grabbed or no associated object was found, or the object is no modify
	if(!LLToolFace::mTextureGrabbed || !LLToolFace::mTextureObject || !mTextureObject->permModify())
	{
		return;
	}

	// This is a rather lazy way of doing this, but the methods tried further down
	// didn't lead to the necessary accuracy. More research will be done to see if
	// it can be improved somehow.

	// get the mouse distance from the original grabbing point
	S32 dx=gViewerWindow->getCurrentMouseX()-mGrabX;
	S32 dy=mGrabY-gViewerWindow->getCurrentMouseY();

	// if no movement took place, stop here
	if(dx==0 && dy==0)
	{
		return;
	}

	// move the mouse back to the grabbing point
	gViewerWindow->getWindow()->setCursorPosition(LLCoordWindow(mGrabX,gViewerWindow->getWindowHeightRaw()-mGrabY-1));

	F32 u;
	F32 v;
	F32 scaleU;
	F32 scaleV;

	// get texture coordinates and scale from the currently edited face
	mTextureObject->getTE(mFaceGrabbed)->getOffset(&u,&v);
	mTextureObject->getTE(mFaceGrabbed)->getScale(&scaleU,&scaleV);

	// get the status of modifier keys
	MASK mask=gKeyboard->currentMask(FALSE);

	// scale mode
//	if(mask & MASK_ALT)
	if(gKeyboard->getKeyDown(KEY_CAPSLOCK))
	{
		// set the value scaling based on SHIFT and CTRL keys being used
		F32 scale=10.0f;
		if(mask & MASK_CONTROL)
			scale=100.0f;
		else if(mask & MASK_SHIFT)
			scale=1000.0f;

		// recalculate texture scale
		scaleU-=(F32) dx/scale;
		scaleV+=(F32) dy/scale;

		// Bounds check 
		if(scaleU<0.0f)  scaleU=0.0f;
		if(scaleV<0.0f)  scaleV=0.0f;

		// apply new texture scale
		mTextureObject->setTEScale(mFaceGrabbed,scaleU,scaleV);
	}
	// drag mode
	else
	{
		// set the value scaling based on SHIFT and CTRL keys being used
		F32 scale=100.0f;
		if(mask & MASK_CONTROL)
			scale=1000.0f;
		else if(mask & MASK_SHIFT)
			scale=10000.0f;

		// recalculate texture position
		u-=(F32) dx/scale*scaleU;
		v+=(F32) dy/scale*scaleV;

		// Bounds check and roll over
		if     (u> 1.0f)  u-=1.0f;
		else if(u<-1.0f)  u+=1.0f;
		if     (v> 1.0f)  v-=1.0f;
		else if(v<-1.0f)  v+=1.0f;

		// apply new texture position
		mTextureObject->setTEOffset(mFaceGrabbed,u,v);
	}

	// update the build floater to reflect the new values
	LLFloaterTools* toolsFloater=(LLFloaterTools*) LLFloaterReg::findInstance("build");
	if(toolsFloater)
	{
		LLPanelFace* panelFace=toolsFloater->mPanelFace;
		if(panelFace)
		{
			panelFace->refresh();
		}
	}

#ifdef TEXTURE_GRAB_UPDATE_REGULARLY
	teDirty=TRUE;

	updateCounter++;
	if(updateCounter==10)
	{
		mTextureObject->sendTEUpdate();
		updateCounter=0;
		teDirty=FALSE;
	}
#endif
	// this is one way I would rather do it than tracking mouse deltas, because it
	// would take rotations into account. However, mSTCoords seems too coarse to be useful
/*
	S32 x=gViewerWindow->getCurrentMouseX();
	S32 y=gViewerWindow->getCurrentMouseY();

	const LLPickInfo& pick=gViewerWindow->pickImmediate(x,y,TRUE);

	if(pick.getObjectID()!=LLToolFace::mTextureObject->getID() || pick.mObjectFace!=LLToolFace::mFaceGrabbed)
	{
		return;
	}

	llwarns << x << " " << y << " " << pick.mSTCoords.mV[VX] << " " <<  pick.mSTCoords.mV[VY] << llendl;
*/

	// this is another way I would rather do it than tracking mouse deltas, because it
	// would take rotations into account. However, mSTCoords seems too coarse to be useful
/*
	S32 x=gViewerWindow->getCurrentMouseX();
	S32 y=gViewerWindow->getCurrentMouseY();

	S32 gDebugRaycastFaceHit = -1;
	LLVector3 gDebugRaycastIntersection;
	LLVector2 gDebugRaycastTexCoord;
	LLVector3 gDebugRaycastNormal;
	LLVector3 gDebugRaycastBinormal;
	LLVector3 gDebugRaycastStart;
	LLVector3 gDebugRaycastEnd;

	gDebugRaycastObject = gViewerWindow->cursorIntersect(x,y, 512.f, mTextureObject, mFaceGrabbed, FALSE,
										  &gDebugRaycastFaceHit,
										  &gDebugRaycastIntersection,
										  &gDebugRaycastTexCoord,
										  &gDebugRaycastNormal,
										  &gDebugRaycastBinormal,
										  &gDebugRaycastStart,
										  &gDebugRaycastEnd
										 );

	if(!gDebugRaycastObject || gDebugRaycastFaceHit!=LLToolFace::mFaceGrabbed)
	{
		return;
	}
*/
}

void LLToolFace::stopGrabbing()
{
	if(mTextureObject)
	{
		mTextureObject->sendTEUpdate();
	}
	gViewerWindow->showCursor();

	LLToolFace::mTextureGrabbed=FALSE;
	LLToolFace::mTextureObject=NULL;
	LLToolFace::mFaceGrabbed=0;
}

BOOL LLToolFace::handleMouseUp(S32 x, S32 y, MASK mask)
{
	if(mTextureObject)
	{
		mTextureObject->sendTEUpdate();
	}
	gViewerWindow->showCursor();
	LLToolFace::mTextureGrabbed=FALSE;

	return TRUE;
}
// </FS:Zi>
