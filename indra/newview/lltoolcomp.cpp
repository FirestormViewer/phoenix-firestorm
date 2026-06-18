/**
 * @file lltoolcomp.cpp
 * @brief Composite tools
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

#include "lltoolcomp.h"

#include "llfloaterreg.h"
#include "llgl.h"
#include "indra_constants.h"

#include "llmanip.h"
#include "llmaniprotate.h"
#include "llmanipscale.h"
#include "llmaniptranslate.h"
#include "llmenugl.h"           // for right-click menu hack
#include "llselectmgr.h"
#include "lltoolfocus.h"
#include "lltoolgrab.h"
#include "lltoolgun.h"
#include "lltoolmgr.h"
#include "lltoolselectrect.h"
#include "lltoolplacer.h"
#include "llviewerinput.h"
#include "llviewermenu.h"
#include "llviewerobject.h"
#include "llviewerwindow.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llfloatertools.h"
#include "llviewercontrol.h"
#include "llsmoothstep.h"
#include "llrender2dutils.h"    // for gl_rect_2d (ADS vignette)

// NaCl - Rightclick-mousewheel zoom
#include "llviewercamera.h"
// NaCl End

extern LLControlGroup gSavedSettings;

// we use this in various places instead of NULL
static LLPointer<LLTool> sNullTool(new LLTool(std::string("null"), NULL));

//-----------------------------------------------------------------------
// LLToolComposite

//static
void LLToolComposite::setCurrentTool( LLTool* new_tool )
{
    if( mCur != new_tool )
    {
        if( mSelected )
        {
            mCur->handleDeselect();
            mCur = new_tool;
            mCur->handleSelect();
        }
        else
        {
            mCur = new_tool;
        }
    }
}

LLToolComposite::LLToolComposite(const std::string& name)
    : LLTool(name),
      mCur(sNullTool),
      mDefault(sNullTool),
      mSelected(false),
      mMouseDown(false), mManip(NULL), mSelectRect(NULL)
{
}

// Returns to the default tool
bool LLToolComposite::handleMouseUp(S32 x, S32 y, MASK mask)
{
    bool handled = mCur->handleMouseUp( x, y, mask );
    if( handled )
    {
        setCurrentTool( mDefault );
    }
 return handled;
}

void LLToolComposite::onMouseCaptureLost()
{
    mCur->onMouseCaptureLost();
    setCurrentTool( mDefault );
}

bool LLToolComposite::isSelecting()
{
    return mCur == mSelectRect;
}

void LLToolComposite::handleSelect()
{
    if (!gSavedSettings.getBOOL("EditLinkedParts"))
    {
        LLSelectMgr::getInstance()->promoteSelectionToRoot();
    }
    mCur = mDefault;
    mCur->handleSelect();
    mSelected = true;
}

void LLToolComposite::handleDeselect()
{
    mCur->handleDeselect();
    mCur = mDefault;
    mSelected = false;
}

//----------------------------------------------------------------------------
// LLToolCompInspect
//----------------------------------------------------------------------------

LLToolCompInspect::LLToolCompInspect()
: LLToolComposite(std::string("Inspect")),
  mIsToolCameraActive(false)
{
    mSelectRect     = new LLToolSelectRect(this);
    mDefault = mSelectRect;
}


LLToolCompInspect::~LLToolCompInspect()
{
    delete mSelectRect;
    mSelectRect = NULL;
}

bool LLToolCompInspect::handleMouseDown(S32 x, S32 y, MASK mask)
{
    bool handled = false;

    if (mCur == LLToolCamera::getInstance())
    {
        handled = mCur->handleMouseDown(x, y, mask);
    }
    else
    {
        mMouseDown = true;
        gViewerWindow->pickAsync(x, y, mask, pickCallback);
        handled = true;
    }

    return handled;
}

bool LLToolCompInspect::handleMouseUp(S32 x, S32 y, MASK mask)
{
    bool handled = LLToolComposite::handleMouseUp(x, y, mask);
    mIsToolCameraActive = getCurrentTool() == LLToolCamera::getInstance();
    return handled;
}

void LLToolCompInspect::pickCallback(const LLPickInfo& pick_info)
{
    LLViewerObject* hit_obj = pick_info.getObject();
    LLToolCompInspect * tool_inspectp = LLToolCompInspect::getInstance();

    if (!tool_inspectp->mMouseDown)
    {
        // fast click on object, but mouse is already up...just do select
        tool_inspectp->mSelectRect->handleObjectSelection(pick_info, gSavedSettings.getBOOL("EditLinkedParts"), false);
        return;
    }

    LLSelectMgr * mgr_selectp = LLSelectMgr::getInstance();
    if( hit_obj && mgr_selectp->getSelection()->getObjectCount()) {
        LLEditMenuHandler::gEditMenuHandler = mgr_selectp;
    }

    tool_inspectp->setCurrentTool( tool_inspectp->mSelectRect );
    tool_inspectp->mIsToolCameraActive = false;
    tool_inspectp->mSelectRect->handlePick( pick_info );
}

bool LLToolCompInspect::handleDoubleClick(S32 x, S32 y, MASK mask)
{
    return true;
}

bool LLToolCompInspect::handleKey(KEY key, MASK mask)
{
    bool handled = false;

    if(KEY_ALT == key)
    {
        setCurrentTool(LLToolCamera::getInstance());
        mIsToolCameraActive = true;
        handled = true;
    }
    else
    {
        handled = LLToolComposite::handleKey(key, mask);
    }

    return handled;
}

void LLToolCompInspect::onMouseCaptureLost()
{
    LLToolComposite::onMouseCaptureLost();
    mIsToolCameraActive = false;
}

void LLToolCompInspect::keyUp(KEY key, MASK mask)
{
    if (KEY_ALT == key && mCur == LLToolCamera::getInstance())
    {
        setCurrentTool(mDefault);
        mIsToolCameraActive = false;
    }
}

//----------------------------------------------------------------------------
// LLToolCompTranslate
//----------------------------------------------------------------------------

LLToolCompTranslate::LLToolCompTranslate()
    : LLToolComposite(std::string("Move"))
{
    mManip      = new LLManipTranslate(this);
    mSelectRect     = new LLToolSelectRect(this);

    mCur            = mManip;
    mDefault        = mManip;
}

LLToolCompTranslate::~LLToolCompTranslate()
{
    delete mManip;
    mManip = NULL;

    delete mSelectRect;
    mSelectRect = NULL;
}

bool LLToolCompTranslate::handleHover(S32 x, S32 y, MASK mask)
{
    if( !mCur->hasMouseCapture() )
    {
        setCurrentTool( mManip );
    }
    return mCur->handleHover( x, y, mask );
}


bool LLToolCompTranslate::handleMouseDown(S32 x, S32 y, MASK mask)
{
    mMouseDown = true;
    gViewerWindow->pickAsync(x, y, mask, pickCallback, /*bool pick_transparent*/ false, LLFloaterReg::instanceVisible("build"), false,
        gSavedSettings.getBOOL("SelectReflectionProbes"));;
    return true;
}

void LLToolCompTranslate::pickCallback(const LLPickInfo& pick_info)
{
    LLViewerObject* hit_obj = pick_info.getObject();

    LLToolCompTranslate::getInstance()->mManip->highlightManipulators(pick_info.mMousePt.mX, pick_info.mMousePt.mY);
    if (!LLToolCompTranslate::getInstance()->mMouseDown)
    {
        // fast click on object, but mouse is already up...just do select
        LLToolCompTranslate::getInstance()->mSelectRect->handleObjectSelection(pick_info, gSavedSettings.getBOOL("EditLinkedParts"), false);
        return;
    }

    if( hit_obj || LLToolCompTranslate::getInstance()->mManip->getHighlightedPart() != LLManip::LL_NO_PART )
    {
        if (LLToolCompTranslate::getInstance()->mManip->getSelection()->getObjectCount())
        {
            LLEditMenuHandler::gEditMenuHandler = LLSelectMgr::getInstance();
        }

        bool can_move = LLToolCompTranslate::getInstance()->mManip->canAffectSelection();

        if( LLManip::LL_NO_PART != LLToolCompTranslate::getInstance()->mManip->getHighlightedPart() && can_move)
        {
            LLToolCompTranslate::getInstance()->setCurrentTool( LLToolCompTranslate::getInstance()->mManip );
            LLToolCompTranslate::getInstance()->mManip->handleMouseDownOnPart( pick_info.mMousePt.mX, pick_info.mMousePt.mY, pick_info.mKeyMask );
        }
        else
        {
            LLToolCompTranslate::getInstance()->setCurrentTool( LLToolCompTranslate::getInstance()->mSelectRect );
            LLToolCompTranslate::getInstance()->mSelectRect->handlePick( pick_info );

            // *TODO: add toggle to trigger old click-drag functionality
            // LLToolCompTranslate::getInstance()->mManip->handleMouseDownOnPart( XY_part, x, y, mask);
        }
    }
    else
    {
        LLToolCompTranslate::getInstance()->setCurrentTool( LLToolCompTranslate::getInstance()->mSelectRect );
        LLToolCompTranslate::getInstance()->mSelectRect->handlePick( pick_info );
    }
}

bool LLToolCompTranslate::handleMouseUp(S32 x, S32 y, MASK mask)
{
    mMouseDown = false;
    return LLToolComposite::handleMouseUp(x, y, mask);
}

LLTool* LLToolCompTranslate::getOverrideTool(MASK mask)
{
    if (mask == MASK_CONTROL)
    {
        return LLToolCompRotate::getInstance();
    }
    else if (mask == (MASK_CONTROL | MASK_SHIFT))
    {
        return LLToolCompScale::getInstance();
    }
    return LLToolComposite::getOverrideTool(mask);
}

bool LLToolCompTranslate::handleDoubleClick(S32 x, S32 y, MASK mask)
{
    if (mManip->getSelection()->isEmpty() && mManip->getHighlightedPart() == LLManip::LL_NO_PART)
    {
        // You should already have an object selected from the mousedown.
        // If so, show its properties
        LLFloaterReg::showInstance("build", "Contents");
        return true;
    }
    // Nothing selected means the first mouse click was probably
    // bad, so try again.
    // This also consumes the event to prevent things like double-click
    // teleport from triggering.
    return handleMouseDown(x, y, mask);
}


void LLToolCompTranslate::render()
{
    mCur->render(); // removing this will not draw the RGB arrows and guidelines

    if( mCur != mManip )
    {
        LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE);
        mManip->renderGuidelines();
    }
}


//-----------------------------------------------------------------------
// LLToolCompScale

LLToolCompScale::LLToolCompScale()
    : LLToolComposite(std::string("Stretch"))
{
    mManip = new LLManipScale(this);
    mSelectRect = new LLToolSelectRect(this);

    mCur = mManip;
    mDefault = mManip;
}

LLToolCompScale::~LLToolCompScale()
{
    delete mManip;
    delete mSelectRect;
}

bool LLToolCompScale::handleHover(S32 x, S32 y, MASK mask)
{
    if( !mCur->hasMouseCapture() )
    {
        setCurrentTool(mManip );
    }
    return mCur->handleHover( x, y, mask );
}


bool LLToolCompScale::handleMouseDown(S32 x, S32 y, MASK mask)
{
    mMouseDown = true;
    gViewerWindow->pickAsync(x, y, mask, pickCallback);
    return true;
}

void LLToolCompScale::pickCallback(const LLPickInfo& pick_info)
{
    LLViewerObject* hit_obj = pick_info.getObject();

    LLToolCompScale::getInstance()->mManip->highlightManipulators(pick_info.mMousePt.mX, pick_info.mMousePt.mY);
    if (!LLToolCompScale::getInstance()->mMouseDown)
    {
        // fast click on object, but mouse is already up...just do select
        LLToolCompScale::getInstance()->mSelectRect->handleObjectSelection(pick_info, gSavedSettings.getBOOL("EditLinkedParts"), false);

        return;
    }

    if( hit_obj || LLToolCompScale::getInstance()->mManip->getHighlightedPart() != LLManip::LL_NO_PART)
    {
        if (LLToolCompScale::getInstance()->mManip->getSelection()->getObjectCount())
        {
            LLEditMenuHandler::gEditMenuHandler = LLSelectMgr::getInstance();
        }
        if( LLManip::LL_NO_PART != LLToolCompScale::getInstance()->mManip->getHighlightedPart() )
        {
            LLToolCompScale::getInstance()->setCurrentTool( LLToolCompScale::getInstance()->mManip );
            LLToolCompScale::getInstance()->mManip->handleMouseDownOnPart( pick_info.mMousePt.mX, pick_info.mMousePt.mY, pick_info.mKeyMask );
        }
        else
        {
            LLToolCompScale::getInstance()->setCurrentTool( LLToolCompScale::getInstance()->mSelectRect );
            LLToolCompScale::getInstance()->mSelectRect->handlePick( pick_info );
        }
    }
    else
    {
        LLToolCompScale::getInstance()->setCurrentTool( LLToolCompScale::getInstance()->mSelectRect );
        LLToolCompScale::getInstance()->mSelectRect->handlePick( pick_info );
    }
}

bool LLToolCompScale::handleMouseUp(S32 x, S32 y, MASK mask)
{
    mMouseDown = false;
    return LLToolComposite::handleMouseUp(x, y, mask);
}

LLTool* LLToolCompScale::getOverrideTool(MASK mask)
{
    if (mask == MASK_CONTROL)
    {
        return LLToolCompRotate::getInstance();
    }

    return LLToolComposite::getOverrideTool(mask);
}


bool LLToolCompScale::handleDoubleClick(S32 x, S32 y, MASK mask)
{
    if (!mManip->getSelection()->isEmpty() && mManip->getHighlightedPart() == LLManip::LL_NO_PART)
    {
        // You should already have an object selected from the mousedown.
        // If so, show its properties
        LLFloaterReg::showInstance("build", "Contents");
        return true;
    }
    else
    {
        // Nothing selected means the first mouse click was probably
        // bad, so try again.
        return handleMouseDown(x, y, mask);
    }
}


void LLToolCompScale::render()
{
    mCur->render();

    if( mCur != mManip )
    {
        LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE);
        mManip->renderGuidelines();
    }
}

// <FS:Zi> Add middle mouse control for switching uniform scaling on the fly
bool LLToolCompScale::handleMiddleMouseDown(S32 x, S32 y, MASK mask)
{
    LLToolCompScale::getInstance()->mManip->handleMiddleMouseDown(x,y,mask);
    return handleMouseDown(x,y,mask);
}

bool LLToolCompScale::handleMiddleMouseUp(S32 x, S32 y, MASK mask)
{
    LLToolCompScale::getInstance()->mManip->handleMiddleMouseUp(x,y,mask);
    return handleMouseUp(x,y,mask);
}
// </FS:Zi>

//-----------------------------------------------------------------------
// LLToolCompCreate

LLToolCompCreate::LLToolCompCreate()
    : LLToolComposite(std::string("Create"))
{
    mPlacer = new LLToolPlacer();
    mSelectRect = new LLToolSelectRect(this);

    mCur = mPlacer;
    mDefault = mPlacer;
    mObjectPlacedOnMouseDown = false;
}


LLToolCompCreate::~LLToolCompCreate()
{
    delete mPlacer;
    delete mSelectRect;
}


bool LLToolCompCreate::handleMouseDown(S32 x, S32 y, MASK mask)
{
    bool handled = false;
    mMouseDown = true;

    if ( (mask == MASK_SHIFT) || (mask == MASK_CONTROL) )
    {
        gViewerWindow->pickAsync(x, y, mask, pickCallback);
        handled = true;
    }
    else
    {
        setCurrentTool( mPlacer );
        handled = mPlacer->placeObject( x, y, mask );
    }

    mObjectPlacedOnMouseDown = true;

    return handled;
}

void LLToolCompCreate::pickCallback(const LLPickInfo& pick_info)
{
    // *NOTE: We mask off shift and control, so you cannot
    // multi-select multiple objects with the create tool.
    MASK mask = (pick_info.mKeyMask & ~MASK_SHIFT);
    mask = (mask & ~MASK_CONTROL);

    LLToolCompCreate::getInstance()->setCurrentTool( LLToolCompCreate::getInstance()->mSelectRect );
    LLToolCompCreate::getInstance()->mSelectRect->handlePick( pick_info );
}

bool LLToolCompCreate::handleDoubleClick(S32 x, S32 y, MASK mask)
{
    return handleMouseDown(x, y, mask);
}

bool LLToolCompCreate::handleMouseUp(S32 x, S32 y, MASK mask)
{
    bool handled = false;

    if ( mMouseDown && !mObjectPlacedOnMouseDown && !(mask == MASK_SHIFT) && !(mask == MASK_CONTROL) )
    {
        setCurrentTool( mPlacer );
        handled = mPlacer->placeObject( x, y, mask );
    }

    mObjectPlacedOnMouseDown = false;
    mMouseDown = false;

    if (!handled)
    {
        handled = LLToolComposite::handleMouseUp(x, y, mask);
    }

    return handled;
}

//-----------------------------------------------------------------------
// LLToolCompRotate

LLToolCompRotate::LLToolCompRotate()
    : LLToolComposite(std::string("Rotate"))
{
    mManip = new LLManipRotate(this);
    mSelectRect = new LLToolSelectRect(this);

    mCur = mManip;
    mDefault = mManip;
}


LLToolCompRotate::~LLToolCompRotate()
{
    delete mManip;
    delete mSelectRect;
}

bool LLToolCompRotate::handleHover(S32 x, S32 y, MASK mask)
{
    if( !mCur->hasMouseCapture() )
    {
        setCurrentTool( mManip );
    }
    return mCur->handleHover( x, y, mask );
}


bool LLToolCompRotate::handleMouseDown(S32 x, S32 y, MASK mask)
{
    mMouseDown = true;
    gViewerWindow->pickAsync(x, y, mask, pickCallback);
    return true;
}

void LLToolCompRotate::pickCallback(const LLPickInfo& pick_info)
{
    LLViewerObject* hit_obj = pick_info.getObject();

    LLToolCompRotate::getInstance()->mManip->highlightManipulators(pick_info.mMousePt.mX, pick_info.mMousePt.mY);
    if (!LLToolCompRotate::getInstance()->mMouseDown)
    {
        // fast click on object, but mouse is already up...just do select
        LLToolCompRotate::getInstance()->mSelectRect->handleObjectSelection(pick_info, gSavedSettings.getBOOL("EditLinkedParts"), false);
        return;
    }

    if( hit_obj || LLToolCompRotate::getInstance()->mManip->getHighlightedPart() != LLManip::LL_NO_PART)
    {
        if (LLToolCompRotate::getInstance()->mManip->getSelection()->getObjectCount())
        {
            LLEditMenuHandler::gEditMenuHandler = LLSelectMgr::getInstance();
        }
        if( LLManip::LL_NO_PART != LLToolCompRotate::getInstance()->mManip->getHighlightedPart() )
        {
            LLToolCompRotate::getInstance()->setCurrentTool( LLToolCompRotate::getInstance()->mManip );
            LLToolCompRotate::getInstance()->mManip->handleMouseDownOnPart( pick_info.mMousePt.mX, pick_info.mMousePt.mY, pick_info.mKeyMask );
        }
        else
        {
            LLToolCompRotate::getInstance()->setCurrentTool( LLToolCompRotate::getInstance()->mSelectRect );
            LLToolCompRotate::getInstance()->mSelectRect->handlePick( pick_info );
        }
    }
    else
    {
        LLToolCompRotate::getInstance()->setCurrentTool( LLToolCompRotate::getInstance()->mSelectRect );
        LLToolCompRotate::getInstance()->mSelectRect->handlePick( pick_info );
    }
}

bool LLToolCompRotate::handleMouseUp(S32 x, S32 y, MASK mask)
{
    mMouseDown = false;
    return LLToolComposite::handleMouseUp(x, y, mask);
}

LLTool* LLToolCompRotate::getOverrideTool(MASK mask)
{
    if (mask == (MASK_CONTROL | MASK_SHIFT))
    {
        return LLToolCompScale::getInstance();
    }
    return LLToolComposite::getOverrideTool(mask);
}

bool LLToolCompRotate::handleDoubleClick(S32 x, S32 y, MASK mask)
{
    if (!mManip->getSelection()->isEmpty() && mManip->getHighlightedPart() == LLManip::LL_NO_PART)
    {
        // You should already have an object selected from the mousedown.
        // If so, show its properties
        LLFloaterReg::showInstance("build", "Contents");
        return true;
    }
    else
    {
        // Nothing selected means the first mouse click was probably
        // bad, so try again.
        return handleMouseDown(x, y, mask);
    }
}


void LLToolCompRotate::render()
{
    mCur->render();

    if( mCur != mManip )
    {
        LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE);
        mManip->renderGuidelines();
    }
}


//-----------------------------------------------------------------------
// LLToolCompGun

LLToolCompGun::LLToolCompGun()
    : LLToolComposite(std::string("Mouselook")),
      mTargetFOV(0.f),
      mCurrentFOV(0.f),
      mTransitionStartFOV(0.f),
      mIsZoomTransitioning(false),
      mIsZoomed(false),
      mBaseFOV(0.f),
      mZoomedFOV(0.f),
      mZoomProportion(1.f),
      mIsADS(false),
      mTransitionIsADS(false),
      mADSFOV(0.f),
      mADSFromOTS(false),
      mLastTapWasQuick(false)
{
    mGun = new LLToolGun(this);
    mGrab = new LLToolGrabBase(this);
    mNull = sNullTool;

    setCurrentTool(mGun);
    mDefault = mGun;
}


LLToolCompGun::~LLToolCompGun()
{
    delete mGun;
    mGun = NULL;

    delete mGrab;
    mGrab = NULL;

    // don't delete a static object
    // delete mNull;
    mNull = NULL;
}

bool LLToolCompGun::handleHover(S32 x, S32 y, MASK mask)
{
    // Smooth FOV transition for mouselook zoom
    if (mIsZoomTransitioning)
    {
        // ADS transitions use their own smoothing setting; the normal hold-zoom
        // uses the existing one. Both are milliseconds (0 = instant).
        static LLCachedControl<F32> mlTransition(gSavedSettings, "MouselookZoomTransitionSpeed");
        static LLCachedControl<F32> adsTransition(gSavedSettings, "FSADSZoomTransitionSpeed");
        F32 duration = llclamp((F32)(mTransitionIsADS ? adsTransition : mlTransition), 0.f, 5000.f) / 1000.f;

        if (duration <= 0.f)
        {
            // Instant transition
            mCurrentFOV = mTargetFOV;
            mIsZoomTransitioning = false;
            mZoomProportion = 1.f;
            gSavedSettings.setF32("CameraAngle", mCurrentFOV);
        }
        else
        {
            // Scale duration by how far we actually zoomed so that a
            // partial zoom-out finishes proportionally faster.
            F32 effectiveDuration = duration * llclamp(mZoomProportion, 0.f, 1.f);
            if (effectiveDuration <= 0.f)
            {
                effectiveDuration = duration;
            }

            F32 elapsed = (F32)mTransitionTimer.getElapsedTimeF32();
            F32 t = llclamp(elapsed / effectiveDuration, 0.f, 1.f);

            // Smooth-step for a more natural feel
            F32 smooth_t = llsmoothstep(0.f, 1.f, t);
            mCurrentFOV = mTransitionStartFOV + (mTargetFOV - mTransitionStartFOV) * smooth_t;

            if (t >= 1.f)
            {
                mCurrentFOV = mTargetFOV;
                mIsZoomTransitioning = false;
                mZoomProportion = 1.f;
            }

            gSavedSettings.setF32("CameraAngle", mCurrentFOV);
        }
    }
    
    // *NOTE: This hack is here to make mouselook kick in again after
    // item selected from context menu.
    if ( mCur == mNull && !gPopupMenuView->getVisible() )
    {
        LLSelectMgr::getInstance()->deselectAll();
        setCurrentTool( (LLTool*) mGrab );
    }

    // Note: if the tool changed, we can't delegate the current mouse event
    // after the change because tools can modify the mouse during selection and deselection.
    // Instead we let the current tool handle the event and then make the change.
    // The new tool will take effect on the next frame.

    mCur->handleHover( x, y, mask );

    // If mouse button not down...
    if( !gViewerWindow->getLeftMouseDown())
    {
        // let ALT switch from gun to grab
        if ( mCur == mGun && (mask & MASK_ALT) )
        {
            setCurrentTool( (LLTool*) mGrab );
        }
        else if ( mCur == mGrab && !(mask & MASK_ALT) )
        {
            setCurrentTool( (LLTool*) mGun );
            setMouseCapture(true);
        }
    }

    return true;
}


bool LLToolCompGun::handleMouseDown(S32 x, S32 y, MASK mask)
{
    // if the left button is grabbed, don't put up the pie menu
    if (gAgent.leftButtonGrabbed() && gViewerInput.isLMouseHandlingDefault(MODE_FIRST_PERSON))
    {
        gAgent.setControlFlags(AGENT_CONTROL_ML_LBUTTON_DOWN);
        return false;
    }

    // On mousedown, start grabbing
    gGrabTransientTool = this;
    LLToolMgr::getInstance()->getCurrentToolset()->selectTool( (LLTool*) mGrab );

    return LLToolGrab::getInstance()->handleMouseDown(x, y, mask);
}


bool LLToolCompGun::handleDoubleClick(S32 x, S32 y, MASK mask)
{
    // if the left button is grabbed, don't put up the pie menu
    if (gAgent.leftButtonGrabbed() && gViewerInput.isLMouseHandlingDefault(MODE_FIRST_PERSON))
    {
        gAgent.setControlFlags(AGENT_CONTROL_ML_LBUTTON_DOWN);
        return false;
    }

    // On mousedown, start grabbing
    gGrabTransientTool = this;
    LLToolMgr::getInstance()->getCurrentToolset()->selectTool( (LLTool*) mGrab );

    return LLToolGrab::getInstance()->handleDoubleClick(x, y, mask);
}


bool LLToolCompGun::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
    mRMBDownTimer.reset(); // measure this press's duration (to tell a tap from a hold)

    // Returning true will suppress the context menu
    // NaCl - Rightclick-mousewheel zoom
    if (!(gKeyboard->currentMask(true) & MASK_ALT))
    {
        // Double-tap-hold ADS: a right press that lands within the double-tap
        // window of the previous release is the held second tap. Zoom to the
        // separate ADS FOV (with ADS smoothing) instead of the normal zoom.
        static LLCachedControl<bool> ads_enable(gSavedSettings, "FSDoubleTapADS", true);
        static LLCachedControl<F32> ads_window(gSavedSettings, "FSADSDoubleTapTime", 0.25f);
        // Only the second of two genuine quick taps within the window counts: the
        // first press must have been a tap (not a hold-zoom). Stops a normal
        // hold-then-click, or two spaced-out clicks, from firing ADS by accident.
        if (ads_enable && mLastTapWasQuick && mLastRMBUpTimer.getElapsedTimeF32() < (F32)ads_window)
        {
            mLastTapWasQuick = false; // consume the gesture

            // Core: from OTS, ADS dives into first-person mouselook (and pops back
            // out on release); when already first person, it just applies the zoom.
            mADSFromOTS = gAgentCamera.cameraOTS();
            if (mADSFromOTS)
            {
                gAgentCamera.changeCameraToMouselook(true);
            }

            // mBaseFOV already holds the un-zoomed FOV from the first tap; only
            // capture it if we somehow started from a clean state.
            if (!mIsZoomed && !mIsZoomTransitioning && !mIsADS)
            {
                mBaseFOV = gSavedSettings.getF32("CameraAngle");
            }
            static LLCachedControl<F32> ads_fov(gSavedSettings, "FSADSZoomFOV", 0.5f);
            mADSFOV = (F32)ads_fov;
            mTargetFOV = mADSFOV;
            mCurrentFOV = gSavedSettings.getF32("CameraAngle");
            mTransitionStartFOV = mCurrentFOV;
            mTransitionTimer.reset();
            mIsZoomTransitioning = true;
            mTransitionIsADS = true;
            mIsADS = true;
            mIsZoomed = false; // ADS supersedes the normal hold-zoom
            mZoomProportion = 1.f;
            return true;
        }

        // Only capture base FOV if we're not zoomed AND not transitioning
        // This prevents capturing mid-transition FOV if user clicks during zoom-out
        if (!mIsZoomed && !mIsZoomTransitioning)
        {
            mBaseFOV = gSavedSettings.getF32("CameraAngle");

            // Get zoomed FOV from settings
            LLVector3 _NACL_MLFovValues = gSavedSettings.getVector3("_NACL_MLFovValues");
            mZoomedFOV = _NACL_MLFovValues.mV[VY];
        }

        // Start zoom-in transition
        mTargetFOV = mZoomedFOV;
        mCurrentFOV = gSavedSettings.getF32("CameraAngle");
        mTransitionStartFOV = mCurrentFOV;
        mTransitionTimer.reset();
        mIsZoomTransitioning = true;
        mTransitionIsADS = false;
        mIsZoomed = true;
        mZoomProportion = 1.f; // Full zoom expected

        return true;
    }
    // NaCl End

    // <FS:Ansariel> Enable context/pie menu in mouselook
    //return true;
    return (!gSavedSettings.getBOOL("FSEnableRightclickMenuInMouselook"));
    // </FS:Ansariel>
}
// NaCl - Rightclick-mousewheel zoom
bool LLToolCompGun::handleRightMouseUp(S32 x, S32 y, MASK mask)
{
    // A press counts as a quick tap only if it was short (not a hold-zoom); the
    // next press needs this to register a double-tap.
    static LLCachedControl<F32> ads_window(gSavedSettings, "FSADSDoubleTapTime", 0.25f);
    mLastTapWasQuick = (mRMBDownTimer.getElapsedTimeF32() < (F32)ads_window);
    mLastRMBUpTimer.reset(); // open the double-tap window for the next press

    // Releasing the held second tap exits ADS, easing back to the base FOV.
    if (mIsADS)
    {
        // Return to OTS if ADS was entered from there.
        if (mADSFromOTS)
        {
            gAgentCamera.changeCameraToOTS();
            mADSFromOTS = false;
        }
        F32 currentActualFOV = gSavedSettings.getF32("CameraAngle");
        F32 totalZoomDistance = fabsf(mBaseFOV - mADSFOV);
        mZoomProportion = (totalZoomDistance > 0.001f)
            ? llclamp(fabsf(mBaseFOV - currentActualFOV) / totalZoomDistance, 0.f, 1.f)
            : 1.f;
        mTargetFOV = mBaseFOV;
        mCurrentFOV = currentActualFOV;
        mTransitionStartFOV = mCurrentFOV;
        mTransitionTimer.reset();
        mIsZoomTransitioning = true;
        mTransitionIsADS = true;
        mIsADS = false;
        return true;
    }

    // Only zoom out if we're currently zoomed in
    if (mIsZoomed)
    {
        // Get current actual FOV
        F32 currentActualFOV = gSavedSettings.getF32("CameraAngle");

        // Calculate zoom proportion (how far did we actually zoom)
        // If base=60, zoomed=30, current=45: proportion = (60-45)/(60-30) = 0.5
        F32 totalZoomDistance = fabsf(mBaseFOV - mZoomedFOV);
        if (totalZoomDistance > 0.001f)
        {
            F32 currentZoomDistance = fabsf(mBaseFOV - currentActualFOV);
            mZoomProportion = currentZoomDistance / totalZoomDistance;
            mZoomProportion = llclamp(mZoomProportion, 0.f, 1.f);
        }
        else
        {
            mZoomProportion = 1.f; // No zoom distance, treat as full
        }

        // Start zoom-out transition from current position
        mTargetFOV = mBaseFOV;
        mCurrentFOV = currentActualFOV;
        mTransitionStartFOV = mCurrentFOV;
        mTransitionTimer.reset();
        mIsZoomTransitioning = true;
        mTransitionIsADS = false;
        mIsZoomed = false;  // No longer in zoomed state
    }
    return true;
}
// NaCl End

bool LLToolCompGun::handleMouseUp(S32 x, S32 y, MASK mask)
{
    if (gViewerInput.isLMouseHandlingDefault(MODE_FIRST_PERSON))
    {
        gAgent.setControlFlags(AGENT_CONTROL_ML_LBUTTON_UP);
    }
    setCurrentTool( (LLTool*) mGun );
    return true;
}

void LLToolCompGun::onMouseCaptureLost()
{
    resetZoom(); // Clear zoom when mouse capture is lost
    
    if (mComposite)
    {
        mComposite->onMouseCaptureLost();
        return;
    }
    mCur->onMouseCaptureLost();
}

void    LLToolCompGun::handleSelect()
{
    LLToolComposite::handleSelect();
    setMouseCapture(true);
}

void    LLToolCompGun::handleDeselect()
{
    // Genuinely leaving mouselook: end ADS first so resetZoom restores the base FOV.
    mIsADS = false;
    mADSFromOTS = false;
    resetZoom(); // Clear zoom when exiting mouselook
    LLToolComposite::handleDeselect();
    setMouseCapture(false);
}

void LLToolCompGun::resetZoom()
{
    // While ADS is held it owns its own FOV and lifecycle. Entering ADS switches
    // camera modes (changeCameraToMouselook), whose tool/capture/animation churn
    // can incidentally call resetZoom mid-hold; honoring it would cancel the ADS
    // zoom and drop the "return to OTS on release" state, stranding the player in
    // first person. Ignore resets while ADS is active - it ends on button release
    // (handleRightMouseUp) or genuine mouselook exit (handleDeselect clears it first).
    if (mIsADS)
    {
        return;
    }

    // Reset NaCl zoom state - immediately restore base FOV if zoomed
    if (mIsZoomed || mIsZoomTransitioning)
    {
        gSavedSettings.setF32("CameraAngle", mBaseFOV);
    }

    // Reset transition state
    mIsZoomTransitioning = false;
    mIsZoomed = false;
    mIsADS = false;
    mADSFromOTS = false;
    mLastTapWasQuick = false;
    mTransitionIsADS = false;
    mTargetFOV = 0.f;
    mCurrentFOV = 0.f;
    mBaseFOV = 0.f;
    mZoomedFOV = 0.f;
    mADSFOV = 0.f;
    mZoomProportion = 1.f;
}

// Screen-edge darkening drawn under the crosshair while holding ADS, to mark the
// state. Stacked edge bands, strongest at the edge and easing to zero inward;
// corners overlap so they read darkest (vignette-like). No texture/shader needed.
static void drawADSVignette(F32 strength)
{
    const LLRect& view = gViewerWindow->getWorldViewRectScaled();
    const S32 w = view.getWidth();
    const S32 h = view.getHeight();
    const F32 a0 = llclamp(strength, 0.f, 1.f);
    if (w <= 0 || h <= 0 || a0 <= 0.f)
    {
        return;
    }

    const F32 depth = (F32)llmin(w, h) * 0.30f; // how far the darkening reaches inward
    const S32 N = 24;                           // gradient steps
    const F32 t = depth / (F32)N;

    LLGLEnable blend(GL_BLEND);
    for (S32 i = 0; i < N; ++i)
    {
        const F32 f = 1.f - (F32)i / (F32)N;    // 1 at the edge -> 0 inward
        const LLColor4 col(0.f, 0.f, 0.f, a0 * f * f);
        const S32 lo = (S32)(i * t);
        const S32 hi = (S32)((i + 1) * t);
        gl_rect_2d(0, hi, w, lo, col);             // bottom
        gl_rect_2d(0, h - lo, w, h - hi, col);     // top
        gl_rect_2d(lo, h, hi, 0, col);             // left
        gl_rect_2d(w - hi, h, w - lo, 0, col);     // right
    }
}

void LLToolCompGun::draw()
{
    // Draw the ADS vignette while ADS is held OR easing out, scaling its darkness
    // by how far the FOV has zoomed toward the ADS level. This makes it creep in
    // and fade out smoothly in lockstep with the zoom (same FSADSZoomTransitionSpeed
    // easing) rather than popping on/off.
    if (mIsADS || (mIsZoomTransitioning && mTransitionIsADS))
    {
        static LLCachedControl<bool> vig_fp(gSavedSettings, "FSADSVignetteFirstPerson", true);
        static LLCachedControl<bool> vig_ots(gSavedSettings, "FSADSVignetteOTS", true);
        const bool ots = gAgentCamera.cameraOTS();
        if ((ots && vig_ots) || (!ots && vig_fp))
        {
            // 0 at the base FOV, 1 at the full ADS FOV (which is the smaller value).
            F32 progress = 1.f;
            const F32 span = mBaseFOV - mADSFOV;
            if (fabsf(span) > 0.0001f)
            {
                const F32 currentFOV = gSavedSettings.getF32("CameraAngle");
                progress = llclamp((mBaseFOV - currentFOV) / span, 0.f, 1.f);
            }
            static LLCachedControl<F32> vig_strength(gSavedSettings, "FSADSVignetteStrength", 0.5f);
            drawADSVignette((F32)vig_strength * progress);
        }
    }
    mCur->draw();
}

bool LLToolCompGun::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
    // NaCl - Rightclick-mousewheel zoom
    if (mIsADS)
    {
        // Scroll adjusts the ADS zoom level while held, mirroring the normal zoom.
        F32 currentFOV = gSavedSettings.getF32("CameraAngle");
        F32 newFOV = llclamp(currentFOV + (F32)(clicks * 0.1f),
                             LLViewerCamera::getInstance()->getMinView(),
                             LLViewerCamera::getInstance()->getMaxView());
        gSavedSettings.setF32("CameraAngle", newFOV);
        mADSFOV = newFOV;
        mTargetFOV = newFOV;
        mCurrentFOV = newFOV;
        gSavedSettings.setF32("FSADSZoomFOV", newFOV); // remember the ADS level
    }
    else if (mIsZoomed)
    {
        // Get current FOV
        F32 currentFOV = gSavedSettings.getF32("CameraAngle");
        
        // Adjust FOV by scroll amount
        F32 newFOV = llclamp(currentFOV + (F32)(clicks * 0.1f), 
                             LLViewerCamera::getInstance()->getMinView(), 
                             LLViewerCamera::getInstance()->getMaxView());
        
        // Apply immediately for responsive feel
        gSavedSettings.setF32("CameraAngle", newFOV);
        
        // Update our zoom target and current position
        mZoomedFOV = newFOV;
        mTargetFOV = newFOV;
        mCurrentFOV = newFOV;
        
        // Save the zoomed FOV to settings for next time
        LLVector3 _NACL_MLFovValues = gSavedSettings.getVector3("_NACL_MLFovValues");
        _NACL_MLFovValues.mV[VY] = newFOV;
        gSavedSettings.setVector3("_NACL_MLFovValues", _NACL_MLFovValues);
    }
    else if (clicks > 0 && gSavedSettings.getBOOL("FSScrollWheelExitsMouselook"))
    // NaCl End
    {
        gAgentCamera.changeCameraToDefault();

    }
    return true;
}


#include "llviewerwindow.h"      // for gViewerWindow->pickAsync()
#include "llselectmgr.h"         // for LLSelectMgr
#include "llfloaterreg.h"        // for LLFloaterReg::showInstance()
#include "llviewermenu.h"        // for LLEditMenuHandler::gEditMenuHandler
#include "fsfloaterposer.h"

// If you want a standard static instance approach:
FSToolCompPose* FSToolCompPose::getInstance()
{
    // Meyers singleton pattern
    static FSToolCompPose instance;
    return &instance;
}

//-----------------------------------
// Constructor
FSToolCompPose::FSToolCompPose()
:   LLToolComposite(std::string("Pose"))
{
    // Create a joint manipulator
    mManip = new FSManipRotateJoint(this);

    // Possibly create a selection rectangle tool if you want 
    // to be able to box-select joints or objects 
    // (same usage as LLToolCompRotate does)
    // mSelectRect = new LLToolSelectRect(this);

    // Set the default and current subtool
    mCur = mManip;
    mDefault = mManip;
}

//-----------------------------------
// Destructor
FSToolCompPose::~FSToolCompPose()
{
    delete mManip;
    mManip = nullptr;

    delete mSelectRect;
    mSelectRect = nullptr;
}

//-----------------------------------
// Handle Hover
bool FSToolCompPose::handleHover(S32 x, S32 y, MASK mask)
{
    // If the current subtool hasn't captured the mouse,
    // switch to your manip subtool (like LLToolCompRotate).
    if (!mCur->hasMouseCapture())
    {
        setCurrentTool(mManip);
    }
    return mCur->handleHover(x, y, mask);
}

//-----------------------------------
// Handle MouseDown
bool FSToolCompPose::handleMouseDown(S32 x, S32 y, MASK mask)
{
    mMouseDown = true;

    // Kick off an async pick, which calls pickCallback when complete
    // so we can see if user clicked on a manip ring or not
    gViewerWindow->pickAsync(x, y, mask, pickCallback);
    return true;
}

//-----------------------------------
// The pickCallback
void FSToolCompPose::pickCallback(const LLPickInfo& pick_info)
{
    FSToolCompPose* self = FSToolCompPose::getInstance();
    FSManipRotateJoint* manip = self->mManip;

    if (!manip) return; // No manipulator available, exit

    // Highlight the manipulator based on the mouse position
    manip->highlightManipulators(pick_info.mMousePt.mX, pick_info.mMousePt.mY);

    if (!self->mMouseDown)
    {
        // No action needed if mouse is up; interaction is handled by highlight logic
        return;
    }

    // Check if a manipulator ring is highlighted
    if (manip->getHighlightedPart() != LLManip::LL_NO_PART)
    {
        // Switch to the manipulator tool for dragging
        self->setCurrentTool(manip);
        manip->handleMouseDownOnPart(pick_info.mMousePt.mX, pick_info.mMousePt.mY, pick_info.mKeyMask);
    }
    else
    {
        // If no ring is highlighted, reset interaction or do nothing
        LL_DEBUGS("FSToolCompPose") << "No manipulator ring selected" << LL_ENDL;
    }
}


//-----------------------------------
// Handle MouseUp
bool FSToolCompPose::handleMouseUp(S32 x, S32 y, MASK mask)
{
    mMouseDown = false;
    // The base LLToolComposite sets mCur->handleMouseUp(...) 
    // and does other management
    return LLToolComposite::handleMouseUp(x, y, mask);
}

//-----------------------------------
// getOverrideTool
// If you want SHIFT+CTRL combos to do something else
LLTool* FSToolCompPose::getOverrideTool(MASK mask)
{
    if (mask == MASK_CONTROL)
    {
        return FSToolCompPoseTranslate::getInstance();
    }

    // Otherwise fallback
    return LLToolComposite::getOverrideTool(mask);
}

//-----------------------------------
// Handle DoubleClick
bool FSToolCompPose::handleDoubleClick(S32 x, S32 y, MASK mask)
{
    if (!mManip->getSelection()->isEmpty() &&
        mManip->getHighlightedPart() == LLManip::LL_NO_PART)
    {
        // Possibly show some pose properties or open the pose floater
        mPoser = dynamic_cast<FSFloaterPoser*>(LLFloaterReg::showInstance("fs_poser"));
        return true;
    }
    else
    {
        // If nothing selected, try a mouse down again
        return handleMouseDown(x, y, mask);
    }
}

//-----------------------------------
// render
void FSToolCompPose::render()
{
    // Render the current subtool
    mCur->render();

    // If the current subtool is not the manip, we can still
    // optionally draw manip guidelines in the background
    if (mCur != mManip)
    {
        mManip->renderGuidelines(); // or something similar if your manip has it
        LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE);
    }
}

FSToolCompPoseTranslate* FSToolCompPoseTranslate::getInstance()
{
    // Meyers singleton pattern
    static FSToolCompPoseTranslate instance;
    return &instance;
}

//-----------------------------------
// Constructor
FSToolCompPoseTranslate::FSToolCompPoseTranslate() : LLToolComposite(std::string("PoseTranslate"))
{
    // Create a joint manipulator
    mManip = new FSManipTranslateJoint(this);

    // Set the default and current subtool
    mCur = mManip;
    mDefault = mManip;
}

//-----------------------------------
// Destructor
FSToolCompPoseTranslate::~FSToolCompPoseTranslate()
{
    delete mManip;
    mManip = nullptr;

    delete mSelectRect;
    mSelectRect = nullptr;
}

//-----------------------------------
// Handle Hover
bool FSToolCompPoseTranslate::handleHover(S32 x, S32 y, MASK mask)
{
    // If the current subtool hasn't captured the mouse,
    // switch to your manip subtool (like LLToolCompRotate).
    if (!mCur->hasMouseCapture())
    {
        setCurrentTool(mManip);
    }
    return mCur->handleHover(x, y, mask);
}

//-----------------------------------
// Handle MouseDown
bool FSToolCompPoseTranslate::handleMouseDown(S32 x, S32 y, MASK mask)
{
    mMouseDown = true;

    // Kick off an async pick, which calls pickCallback when complete
    // so we can see if user clicked on a manip ring or not
    gViewerWindow->pickAsync(x, y, mask, pickCallback);
    return true;
}

//-----------------------------------
// The pickCallback
void FSToolCompPoseTranslate::pickCallback(const LLPickInfo& pick_info)
{
    FSToolCompPoseTranslate* self  = FSToolCompPoseTranslate::getInstance();
    FSManipTranslateJoint* manip = self->mManip;

    if (!manip) return; // No manipulator available, exit

    // Highlight the manipulator based on the mouse position
    manip->highlightManipulators(pick_info.mMousePt.mX, pick_info.mMousePt.mY);

    if (!self->mMouseDown)
    {
        // No action needed if mouse is up; interaction is handled by highlight logic
        return;
    }

    // Check if a manipulator ring is highlighted
    if (manip->getHighlightedPart() != LLManip::LL_NO_PART)
    {
        // Switch to the manipulator tool for dragging
        self->setCurrentTool(manip);
        manip->handleMouseDownOnPart(pick_info.mMousePt.mX, pick_info.mMousePt.mY, pick_info.mKeyMask);
    }
    else
    {
        // If no ring is highlighted, reset interaction or do nothing
        LL_DEBUGS("FSToolCompPose") << "No manipulator ring selected" << LL_ENDL;
    }
}


//-----------------------------------
// Handle MouseUp
bool FSToolCompPoseTranslate::handleMouseUp(S32 x, S32 y, MASK mask)
{
    mMouseDown = false;
    // The base LLToolComposite sets mCur->handleMouseUp(...) 
    // and does other management
    return LLToolComposite::handleMouseUp(x, y, mask);
}

//-----------------------------------
// getOverrideTool
// If you want SHIFT+CTRL combos to do something else
LLTool* FSToolCompPoseTranslate::getOverrideTool(MASK mask)
{
    if (mask == (MASK_CONTROL | MASK_SHIFT))
    {
        return LLToolCompScale::getInstance();
    }
    // Otherwise fallback
    return LLToolComposite::getOverrideTool(mask);
}

//-----------------------------------
// Handle DoubleClick
bool FSToolCompPoseTranslate::handleDoubleClick(S32 x, S32 y, MASK mask)
{
    if (!mManip->getSelection()->isEmpty() &&
        mManip->getHighlightedPart() == LLManip::LL_NO_PART)
    {
        // Possibly show some pose properties or open the pose floater
        mPoser = dynamic_cast<FSFloaterPoser*>(LLFloaterReg::showInstance("fs_poser"));
        return true;
    }
    else
    {
        // If nothing selected, try a mouse down again
        return handleMouseDown(x, y, mask);
    }
}

//-----------------------------------
// render
void FSToolCompPoseTranslate::render()
{
    // Render the current subtool
    mCur->render();

    // If the current subtool is not the manip, we can still
    // optionally draw manip guidelines in the background
    if (mCur != mManip)
    {
        mManip->renderGuidelines(); // or something similar if your manip has it
        LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE);
    }
}
