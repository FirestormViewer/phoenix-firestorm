/**
 * @file lltoolcomp.h
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

#ifndef LL_TOOLCOMP_H
#define LL_TOOLCOMP_H

#include "lltool.h"
#include "lltimer.h"

class LLManip;
class LLToolSelectRect;
class LLToolPlacer;
class LLPickInfo;

class LLView;
class LLTextBox;

//-----------------------------------------------------------------------
// LLToolComposite

class LLToolComposite : public LLTool
{
public:
    LLToolComposite(const std::string& name);

    virtual bool            handleMouseDown(S32 x, S32 y, MASK mask) = 0;   // Sets the current tool
    virtual bool            handleMouseUp(S32 x, S32 y, MASK mask);         // Returns to the default tool
    virtual bool            handleDoubleClick(S32 x, S32 y, MASK mask) = 0;

    // Map virtual functions to the currently active internal tool
    virtual bool            handleHover(S32 x, S32 y, MASK mask)            { return mCur->handleHover( x, y, mask ); }
    virtual bool            handleScrollWheel(S32 x, S32 y, S32 clicks)     { return mCur->handleScrollWheel( x, y, clicks ); }
    virtual bool            handleRightMouseDown(S32 x, S32 y, MASK mask)   { return mCur->handleRightMouseDown( x, y, mask ); }
    // NaCl - Rightclick-mousewheel zoom
    virtual bool            handleRightMouseUp(S32 x, S32 y, MASK mask)     { return mCur->handleRightMouseUp( x, y, mask ); }
    // NaCl End

    virtual LLViewerObject* getEditingObject()                              { return mCur->getEditingObject(); }
    virtual LLVector3d      getEditingPointGlobal()                         { return mCur->getEditingPointGlobal(); }
    virtual bool            isEditing()                                     { return mCur->isEditing(); }
    virtual void            stopEditing()                                   { mCur->stopEditing(); mCur = mDefault; }

    virtual bool            clipMouseWhenDown()                             { return mCur->clipMouseWhenDown(); }

    virtual void            handleSelect();
    virtual void            handleDeselect();

    virtual void            render()                                        { mCur->render(); }
    virtual void            draw()                                          { mCur->draw(); }

    virtual bool            handleKey(KEY key, MASK mask)                   { return mCur->handleKey( key, mask ); }

    virtual void            onMouseCaptureLost();

    virtual void            screenPointToLocal(S32 screen_x, S32 screen_y, S32* local_x, S32* local_y) const
                                { mCur->screenPointToLocal(screen_x, screen_y, local_x, local_y); }

    virtual void            localPointToScreen(S32 local_x, S32 local_y, S32* screen_x, S32* screen_y) const
                                { mCur->localPointToScreen(local_x, local_y, screen_x, screen_y); }

    bool                    isSelecting();
    LLTool*                 getCurrentTool()                                { return mCur; }

protected:
    void                    setCurrentTool( LLTool* new_tool );
    // In hover handler, call this to auto-switch tools
    void                    setToolFromMask( MASK mask, LLTool *normal );

protected:
    LLTool*                 mCur;       // The tool to which we're delegating.
    LLTool*                 mDefault;
    bool                    mSelected;
    bool                    mMouseDown;
    LLManip*                mManip;
    LLToolSelectRect*       mSelectRect;

public:
    static const std::string sNameComp;
};


//-----------------------------------------------------------------------
// LLToolCompTranslate

class LLToolCompInspect : public LLToolComposite, public LLSingleton<LLToolCompInspect>
{
    LLSINGLETON(LLToolCompInspect);
    virtual ~LLToolCompInspect();
public:

    // Overridden from LLToolComposite
    virtual bool        handleMouseDown(S32 x, S32 y, MASK mask) override;
    virtual bool        handleMouseUp(S32 x, S32 y, MASK mask) override;
    virtual bool        handleDoubleClick(S32 x, S32 y, MASK mask) override;
    virtual bool        handleKey(KEY key, MASK mask) override;
    virtual void        onMouseCaptureLost() override;
            void        keyUp(KEY key, MASK mask);

    static void pickCallback(const LLPickInfo& pick_info);

    bool isToolCameraActive() const { return mIsToolCameraActive; }

private:
    bool mIsToolCameraActive;
};

//-----------------------------------------------------------------------
// LLToolCompTranslate

class LLToolCompTranslate : public LLToolComposite, public LLSingleton<LLToolCompTranslate>
{
    LLSINGLETON(LLToolCompTranslate);
    virtual ~LLToolCompTranslate();
public:

    // Overridden from LLToolComposite
    virtual bool        handleMouseDown(S32 x, S32 y, MASK mask) override;
    virtual bool        handleDoubleClick(S32 x, S32 y, MASK mask) override;
    virtual bool        handleHover(S32 x, S32 y, MASK mask) override;
    virtual bool        handleMouseUp(S32 x, S32 y, MASK mask) override;            // Returns to the default tool
    virtual void        render() override;

    virtual LLTool*     getOverrideTool(MASK mask) override;

    static void pickCallback(const LLPickInfo& pick_info);
};

//-----------------------------------------------------------------------
// LLToolCompScale

class LLToolCompScale : public LLToolComposite, public LLSingleton<LLToolCompScale>
{
    LLSINGLETON(LLToolCompScale);
    virtual ~LLToolCompScale();
public:

    // Overridden from LLToolComposite
    virtual bool        handleMouseDown(S32 x, S32 y, MASK mask) override;
    virtual bool        handleDoubleClick(S32 x, S32 y, MASK mask) override;
    virtual bool        handleHover(S32 x, S32 y, MASK mask) override;
    virtual bool        handleMouseUp(S32 x, S32 y, MASK mask) override;            // Returns to the default tool
    virtual void        render() override;

    virtual LLTool*     getOverrideTool(MASK mask) override;

    static void pickCallback(const LLPickInfo& pick_info);

    // <FS:Zi> Add middle mouse control for switching uniform scaling on the fly
    virtual bool        handleMiddleMouseDown(S32 x, S32 y, MASK mask) override;
    virtual bool        handleMiddleMouseUp(S32 x, S32 y, MASK mask) override;
    // </FS:Zi>
};


//-----------------------------------------------------------------------
// LLToolCompRotate

class LLToolCompRotate : public LLToolComposite, public LLSingleton<LLToolCompRotate>
{
    LLSINGLETON(LLToolCompRotate);
    virtual ~LLToolCompRotate();
public:

    // Overridden from LLToolComposite
    virtual bool        handleMouseDown(S32 x, S32 y, MASK mask) override;
    virtual bool        handleDoubleClick(S32 x, S32 y, MASK mask) override;
    virtual bool        handleHover(S32 x, S32 y, MASK mask) override;
    virtual bool        handleMouseUp(S32 x, S32 y, MASK mask) override;
    virtual void        render() override;

    virtual LLTool*     getOverrideTool(MASK mask) override;

    static void pickCallback(const LLPickInfo& pick_info);

protected:
};

//-----------------------------------------------------------------------
// LLToolCompCreate

class LLToolCompCreate : public LLToolComposite, public LLSingleton<LLToolCompCreate>
{
    LLSINGLETON(LLToolCompCreate);
    virtual ~LLToolCompCreate();
public:

    // Overridden from LLToolComposite
    virtual bool        handleMouseDown(S32 x, S32 y, MASK mask) override;
    virtual bool        handleDoubleClick(S32 x, S32 y, MASK mask) override;
    virtual bool        handleMouseUp(S32 x, S32 y, MASK mask) override;

    static void pickCallback(const LLPickInfo& pick_info);
protected:
    LLToolPlacer*       mPlacer;
    bool                mObjectPlacedOnMouseDown;
};


//-----------------------------------------------------------------------
// LLToolCompGun

class LLToolGun;
class LLToolGrabBase;
class LLToolSelect;

class LLToolCompGun : public LLToolComposite, public LLSingleton<LLToolCompGun>
{
    LLSINGLETON(LLToolCompGun);
    virtual ~LLToolCompGun();
public:

    // Overridden from LLToolComposite
    virtual bool            handleHover(S32 x, S32 y, MASK mask) override;
    virtual bool            handleMouseDown(S32 x, S32 y, MASK mask) override;
    virtual bool            handleDoubleClick(S32 x, S32 y, MASK mask) override;
    virtual bool            handleRightMouseDown(S32 x, S32 y, MASK mask) override;
    // NaCl - Rightclick-mousewheel zoom
    virtual bool            handleRightMouseUp(S32 x, S32 y, MASK mask) override;
    // NaCl End
    virtual bool            handleMouseUp(S32 x, S32 y, MASK mask) override;
    virtual bool            handleScrollWheel(S32 x, S32 y, S32 clicks) override;
    virtual void            onMouseCaptureLost() override;
    virtual void            handleSelect() override;
    virtual void            handleDeselect() override;
    virtual LLTool*         getOverrideTool(MASK mask) override { return NULL; }

    bool                    isZoomed() const { return mIsZoomed; }
    bool                    isADS() const { return mIsADS; }
    void                    resetZoom(); // Reset zoom state when exiting mouselook

    // Strength (0-1) of the ADS screen-edge vignette this frame, honoring the
    // per-mode toggles. The displayed value eases toward the live ADS darkness on
    // its own clock (so it fades smoothly on release even if a normal zoom
    // interrupts, and does not depend on the FOV-zoom smoothing). Advanced once
    // per frame; consumed by the post-process pass in LLPipeline::renderVignette.
    F32                     getADSVignetteAmount();

protected:

    LLToolGun*          mGun;
    LLToolGrabBase*     mGrab;
    LLTool*             mNull;

    // Smooth zoom transition
    F32 mTargetFOV;
    F32 mCurrentFOV;
    F32 mTransitionStartFOV;  // FOV at the moment a transition began
    bool mIsZoomTransitioning;
    bool mIsZoomed;  // Are we currently in zoomed state?
    F32 mBaseFOV;  // FOV when zoom started
    F32 mZoomedFOV; // Target zoomed FOV
    F32 mZoomProportion; // How far we got into the zoom (0-1)
    LLTimer mTransitionTimer; // Tracks elapsed time since transition start

    // Double-tap-hold ADS (aim-down-sights): tap, then tap-and-hold the right
    // button to zoom to a separate FOV with its own smoothing; release to exit.
    bool    mIsADS;            // currently holding ADS
    bool    mTransitionIsADS;  // current FOV transition is an ADS zoom (gates the vignette)
    bool    mTransitionUseADSSmoothing; // FOV ease uses ADS smoothing, decoupled from the
                                        // vignette gate so a normal zoom interrupting an ADS
                                        // release can still ease smoothly (no vignette)
    F32     mADSFOV;           // ADS target FOV
    bool    mADSFromOTS;       // ADS entered from OTS -> restore OTS on release
    bool    mLastTapWasQuick;  // previous right press was a quick tap (not a hold)
    LLTimer mLastRMBUpTimer;   // time since last right-button release (double-tap window)
    LLTimer mRMBDownTimer;     // measures the current press's duration

    // Eased ADS vignette darkness (decoupled from the FOV-zoom smoothing so the
    // release always fades smoothly and a normal zoom interrupting it can't snap
    // it off). Advanced once per frame inside getADSVignetteAmount().
    F32     mADSVignette;      // currently displayed darkness (0-1)
    U32     mADSVignetteFrame; // frame counter of the last ease step
};

// Subclass of LLToolComposite
#include "fsmaniprotatejoint.h"   // For FSManipRotateJoint
#include "fsmaniptranslatejoint.h"
#include "fsfloaterposer.h"
class FSToolCompPose : public LLToolComposite
{
public:
    // Typical pattern: pass a name like "Pose"
    FSToolCompPose();
    virtual ~FSToolCompPose();

    // For some viewer patterns, we create a static singleton:
    static FSToolCompPose* getInstance();

    // Overriding base events:
    virtual bool handleHover(S32 x, S32 y, MASK mask) override;
    virtual bool handleMouseDown(S32 x, S32 y, MASK mask) override;
    virtual bool handleMouseUp(S32 x, S32 y, MASK mask) override;
    virtual bool handleDoubleClick(S32 x, S32 y, MASK mask) override;
    virtual void render() override;
    void setAvatar(LLVOAvatar* avatar) { mManip->setAvatar(avatar); };
    void setJoint(LLJoint* joint) { mManip->setJoint(joint); };
    void setReferenceFrame(E_PoserReferenceFrame frame) { mManip->setReferenceFrame(frame); };

    // Optional override if you have SHIFT/CTRL combos
    virtual LLTool* getOverrideTool(MASK mask) override;

    // The pick callback invoked on async pick
    static void pickCallback(const LLPickInfo& pick_info);
    void setPoserFloater(FSFloaterPoser* poser){ mPoser = poser; };
    FSFloaterPoser* getPoserFloater(){ return mPoser; };
protected:
    // Tools within this composite
    FSManipRotateJoint* mManip     = nullptr; 
    LLToolSelectRect*   mSelectRect= nullptr;
    FSFloaterPoser*     mPoser     = nullptr;

    // Track mouse state similarly to LLToolCompRotate
    bool                mMouseDown = false;
};

class FSToolCompPoseTranslate : public LLToolComposite
{
public:
    // Typical pattern: pass a name like "Pose"
    FSToolCompPoseTranslate();
    virtual ~FSToolCompPoseTranslate();

    // For some viewer patterns, we create a static singleton:
    static FSToolCompPoseTranslate* getInstance();

    // Overriding base events:
    virtual bool handleHover(S32 x, S32 y, MASK mask) override;
    virtual bool handleMouseDown(S32 x, S32 y, MASK mask) override;
    virtual bool handleMouseUp(S32 x, S32 y, MASK mask) override;
    virtual bool handleDoubleClick(S32 x, S32 y, MASK mask) override;
    virtual void render() override;
    void setAvatar(LLVOAvatar* avatar) { mManip->setAvatar(avatar); };
    void setJoint(LLJoint* joint) { mManip->setJoint(joint); };
    void setReferenceFrame(E_PoserReferenceFrame frame) { mManip->setReferenceFrame(frame); };

    // Optional override if you have SHIFT/CTRL combos
    virtual LLTool* getOverrideTool(MASK mask) override;

    // The pick callback invoked on async pick
    static void pickCallback(const LLPickInfo& pick_info);
    void setPoserFloater(FSFloaterPoser* poser){ mPoser = poser; };
    FSFloaterPoser* getPoserFloater(){ return mPoser; };
protected:
    // Tools within this composite
    FSManipTranslateJoint* mManip      = nullptr;
    LLToolSelectRect*      mSelectRect = nullptr;
    FSFloaterPoser*        mPoser      = nullptr;
    bool                   mMouseDown  = false;
};


#endif  // LL_TOOLCOMP_H
