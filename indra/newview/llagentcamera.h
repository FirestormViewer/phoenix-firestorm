/**
 * @file llagent.h
 * @brief LLAgent class header file
 *
 * $LicenseInfo:firstyear=2000&license=viewerlgpl$
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

#ifndef LL_LLAGENTCAMERA_H
#define LL_LLAGENTCAMERA_H

#include "llfollowcam.h"            // Ventrella
#include "llhudeffectlookat.h"      // EPointAtType
#include "llhudeffectpointat.h"     // ELookAtType

class LLPickInfo;
class LLVOAvatarSelf;
class LLControlVariable;

//--------------------------------------------------------------------
// Types
//--------------------------------------------------------------------
enum ECameraMode
{
    CAMERA_MODE_THIRD_PERSON,
    CAMERA_MODE_MOUSELOOK,
    CAMERA_MODE_CUSTOMIZE_AVATAR,
    CAMERA_MODE_FOLLOW
};

/** Camera Presets for CAMERA_MODE_THIRD_PERSON */
enum ECameraPreset
{
    /** Default preset, what the Third Person Mode actually was */
    CAMERA_PRESET_REAR_VIEW,

    /** "Looking at the Avatar from the front" */
    CAMERA_PRESET_FRONT_VIEW,

    /** "Above and to the left, over the shoulder, pulled back a little on the zoom" */
    CAMERA_PRESET_GROUP_VIEW,

    /** Current view when a preset is saved */
    CAMERA_PRESET_CUSTOM,

// <FS:PP> Third Person Perspective camera
    /** "Based on Penny Patton's optimized camera settings" */
    CAMERA_PRESET_TPP_VIEW,
// </FS:PP>

// [RLVa:KB] - @setcam_eyeoffset and @setcam_focusoffset
    /* Used by RLVa */
    CAMERA_RLV_SETCAM_VIEW,
// [/RLVa:KB]
};

//------------------------------------------------------------------------
// LLAgentCamera
//------------------------------------------------------------------------
class LLAgentCamera
{
    LOG_CLASS(LLAgentCamera);

public:
    //--------------------------------------------------------------------
    // Constructors / Destructors
    //--------------------------------------------------------------------
public:
    LLAgentCamera();
    virtual         ~LLAgentCamera();
    void            init();
    void            cleanup();
    void            setAvatarObject(LLVOAvatarSelf* avatar);
    bool            isInitialized() { return mInitialized; }
private:
    bool            mInitialized;


    //--------------------------------------------------------------------
    // Mode
    //--------------------------------------------------------------------
public:
    void            changeCameraToDefault();
    void            changeCameraToMouselook(bool animate = true);
    void            changeCameraToThirdPerson(bool animate = true);
    void            changeCameraToCustomizeAvatar(); // Trigger transition animation
    void            changeCameraToFollow(bool animate = true);  // Ventrella
    bool            cameraThirdPerson() const       { return (mCameraMode == CAMERA_MODE_THIRD_PERSON && mLastCameraMode == CAMERA_MODE_THIRD_PERSON); }
    bool            cameraMouselook() const         { return (mCameraMode == CAMERA_MODE_MOUSELOOK && mLastCameraMode == CAMERA_MODE_MOUSELOOK); }
    bool            cameraCustomizeAvatar() const   { return (mCameraMode == CAMERA_MODE_CUSTOMIZE_AVATAR /*&& !mCameraAnimating*/); }
    bool            cameraFollow() const            { return (mCameraMode == CAMERA_MODE_FOLLOW && mLastCameraMode == CAMERA_MODE_FOLLOW); }
    ECameraMode     getCameraMode() const           { return mCameraMode; }
    ECameraMode     getLastCameraMode() const       { return mLastCameraMode; }
    void            updateCamera();                 // Call once per frame to update camera location/orientation
    void            resetCamera();                  // Slam camera into its default position
    void            updateLastCamera();             // Set last camera to current camera

private:
    ECameraMode     mCameraMode;                    // Target mode after transition animation is done
    ECameraMode     mLastCameraMode;

    //--------------------------------------------------------------------
    // Preset
    //--------------------------------------------------------------------
public:
    void switchCameraPreset(ECameraPreset preset);
    ECameraPreset getCameraPreset() const { return mCameraPreset; }
    /** Determines default camera offset depending on the current camera preset */
    LLVector3 getCameraOffsetInitial();
// [RLVa:KB] - @setcam_eyeoffsetscale
    /** Determines default camera offset scale depending on the current camera preset */
    F32 getCameraOffsetScale() const;
// [/RLVa:KB]
    /** Determines default focus offset depending on the current camera preset */
    LLVector3d getFocusOffsetInitial();

    LLVector3 getCurrentCameraOffset();
    LLVector3d getCurrentFocusOffset();
    LLQuaternion getCurrentAvatarRotation();
    bool isJoystickCameraUsed();
    void setInitSitRot(LLQuaternion sit_rot) { mInitSitRot = sit_rot; };
    void rotateToInitSitRot();

private:
    /** Determines maximum camera distance from target for mouselook, opposite to LAND_MIN_ZOOM */
    // <FS:Ansariel> FIRE-23470: Fix camera controls zoom glitch
    //F32 getCameraMaxZoomDistance();
    F32 getCameraMaxZoomDistance(bool allow_disabled_constraints = false);
    // </FS:Ansariel>

    /** Camera preset in Third Person Mode */
    ECameraPreset mCameraPreset;

// [RLVa:KB] - @setcam_eyeoffset
    LLPointer<LLControlVariable> mRlvCameraOffsetInitialControl;
// [/RLVa:KB]

// [RLVa:KB] - @setcam_eyeoffsetscale
    LLPointer<LLControlVariable> mRlvCameraOffsetScaleControl;
// [/RLVa:KB]

//  LLPointer<LLControlVariable> mFocusOffsetInitial;
// [RLVa:KB] - @setcam_focusoffset
    LLPointer<LLControlVariable> mRlvFocusOffsetInitialControl;
// [/RLVa:KB]

    LLQuaternion mInitSitRot;

    //--------------------------------------------------------------------
    // Position
    //--------------------------------------------------------------------
public:
    LLVector3d      getCameraPositionGlobal() const;
    const LLVector3 &getCameraPositionAgent() const;
    LLVector3d      calcCameraPositionTargetGlobal(bool *hit_limit = NULL); // Calculate the camera position target
    F32             getCameraMinOffGround();        // Minimum height off ground for this mode, meters
    void            setCameraCollidePlane(const LLVector4 &plane) { mCameraCollidePlane = plane; }
    bool            calcCameraMinDistance(F32 &obj_min_distance);
    F32             getCurrentCameraBuildOffset() const { return (F32)mCameraFocusOffset.length(); }
    void            clearCameraLag() { mCameraLag.clearVec(); }
    const LLVector3& getCameraUpVector() const { return mCameraUpVector; }
private:
    LLVector3       getAvatarRootPosition();

    F32             mCurrentCameraDistance;         // Current camera offset from avatar
    F32             mTargetCameraDistance;          // Target camera offset from avatar
    F32             mCameraFOVZoomFactor;           // Amount of fov zoom applied to camera when zeroing in on an object
    F32             mCameraCurrentFOVZoomFactor;    // Interpolated fov zoom
    LLVector4       mCameraCollidePlane;            // Colliding plane for camera
    F32             mCameraZoomFraction;            // Mousewheel driven fraction of zoom
    LLVector3       mCameraVirtualPositionAgent;    // Camera virtual position (target) before performing FOV zoom
    LLVector3d      mCameraSmoothingLastPositionGlobal;
    LLVector3d      mCameraSmoothingLastPositionAgent;
    bool            mCameraSmoothingStop;
    LLVector3       mCameraLag;                     // Third person camera lag
    LLVector3       mCameraUpVector;                // Camera's up direction in world coordinates (determines the 'roll' of the view)

    //--------------------------------------------------------------------
    // Follow
    //--------------------------------------------------------------------
public:
    bool            isfollowCamLocked();
private:
    LLFollowCam     mFollowCam;             // Ventrella

    //--------------------------------------------------------------------
    // Sit
    //--------------------------------------------------------------------
public:
    void            setupSitCamera();
    bool            sitCameraEnabled()      { return mSitCameraEnabled; }
    void            setSitCamera(const LLUUID &object_id,
                                 const LLVector3 &camera_pos = LLVector3::zero, const LLVector3 &camera_focus = LLVector3::zero);
private:
    LLPointer<LLViewerObject> mSitCameraReferenceObject; // Object to which camera is related when sitting
    bool            mSitCameraEnabled;      // Use provided camera information when sitting?
    LLVector3       mSitCameraPos;          // Root relative camera pos when sitting
    LLVector3       mSitCameraFocus;        // Root relative camera target when sitting

    //--------------------------------------------------------------------
    // Animation
    //--------------------------------------------------------------------
public:
    void            setCameraAnimating(bool b)          { mCameraAnimating = b; }
    bool            getCameraAnimating()                { return mCameraAnimating; }
    void            setAnimationDuration(F32 seconds);
    void            startCameraAnimation();
    void            stopCameraAnimation();
private:
    LLFrameTimer    mAnimationTimer;    // Seconds that transition animation has been active
    F32             mAnimationDuration; // In seconds
    bool            mCameraAnimating;                   // Camera is transitioning from one mode to another
    LLVector3d      mAnimationCameraStartGlobal;        // Camera start position, global coords
    LLVector3d      mAnimationFocusStartGlobal;         // Camera focus point, global coords

    //--------------------------------------------------------------------
    // Focus
    //--------------------------------------------------------------------
public:
    LLVector3d      calcFocusPositionTargetGlobal();
    LLVector3       calcFocusOffset(LLViewerObject *object, LLVector3 pos_agent, S32 x, S32 y);
    bool            getFocusOnAvatar() const        { return mFocusOnAvatar; }
    LLPointer<LLViewerObject>&  getFocusObject()    { return mFocusObject; }
    F32             getFocusObjectDist() const      { return mFocusObjectDist; }
    void            updateFocusOffset();
    void            validateFocusObject();
    void            setFocusGlobal(const LLPickInfo& pick);
    void            setFocusGlobal(const LLVector3d &focus, const LLUUID &object_id = LLUUID::null);
    void            setFocusOnAvatar(bool focus, bool animate, bool reset_axes = true);
    void            setCameraPosAndFocusGlobal(const LLVector3d& pos, const LLVector3d& focus, const LLUUID &object_id);
    void            clearFocusObject();
    void            setFocusObject(LLViewerObject* object);
    void            setAllowChangeToFollow(bool focus)  { mAllowChangeToFollow = focus; }
    void            setObjectTracking(bool track)   { mTrackFocusObject = track; }
    const LLVector3d &getFocusGlobal() const        { return mFocusGlobal; }
    const LLVector3d &getFocusTargetGlobal() const  { return mFocusTargetGlobal; }
private:
    LLVector3d      mCameraFocusOffset;             // Offset from focus point in build mode
    LLVector3d      mCameraFocusOffsetTarget;       // Target towards which we are lerping the camera's focus offset
    bool            mFocusOnAvatar;
    bool            mAllowChangeToFollow;
    LLVector3d      mFocusGlobal;
    LLVector3d      mFocusTargetGlobal;
    LLPointer<LLViewerObject> mFocusObject;
    F32             mFocusObjectDist;
    LLVector3       mFocusObjectOffset;
    bool            mTrackFocusObject;

    //--------------------------------------------------------------------
    // Lookat / Pointat
    //--------------------------------------------------------------------
public:
    void            updateLookAt(const S32 mouse_x, const S32 mouse_y);
    bool            setLookAt(ELookAtType target_type, LLViewerObject *object = NULL, LLVector3 position = LLVector3::zero);
    ELookAtType     getLookAtType();
    void            lookAtLastChat();
    void            slamLookAt(const LLVector3 &look_at); // Set the physics data
    bool            setPointAt(EPointAtType target_type, LLViewerObject *object = NULL, LLVector3 position = LLVector3::zero);
    EPointAtType    getPointAtType();
public:
    LLPointer<LLHUDEffectLookAt> mLookAt;
    LLPointer<LLHUDEffectPointAt> mPointAt;

    LLViewerObject* mPointAtObject;

    //--------------------------------------------------------------------
    // Third person
    //--------------------------------------------------------------------
public:
    LLVector3d      calcThirdPersonFocusOffset();
    void            setThirdPersonHeadOffset(LLVector3 offset)  { mThirdPersonHeadOffset = offset; }
private:
    LLVector3       mThirdPersonHeadOffset;                     // Head offset for third person camera position

    //--------------------------------------------------------------------
    // Orbit
    //--------------------------------------------------------------------
public:
    void            cameraOrbitAround(const F32 radians);   // Rotate camera CCW radians about build focus point
    void            cameraOrbitOver(const F32 radians);     // Rotate camera forward radians over build focus point
    void            cameraOrbitIn(const F32 meters);        // Move camera in toward build focus point
// <FS:Chanayane> Camera roll (from Alchemy)
    void            cameraRollOver(const F32 radians);      // Roll the camera
// </FS:Chanayane>
    void            resetCameraOrbit();
    void            resetOrbitDiff();
// <FS:Chanayane> Camera roll (from Alchemy)
    void            resetCameraRoll();
// </FS:Chanayane>
    //--------------------------------------------------------------------
    // Zoom
    //--------------------------------------------------------------------
public:
    void            handleScrollWheel(S32 clicks);                          // Mousewheel driven zoom
    void            cameraZoomIn(const F32 factor);                         // Zoom in by fraction of current distance
    F32             getCameraZoomFraction(bool get_third_person = false);   // Get camera zoom as fraction of minimum and maximum zoom
    void            setCameraZoomFraction(F32 fraction);                    // Set camera zoom as fraction of minimum and maximum zoom
    F32             calcCameraFOVZoomFactor();
    F32             getAgentHUDTargetZoom();

    void            resetCameraZoomFraction();
    F32             getCurrentCameraZoomFraction() const { return mCameraZoomFraction; }

    //--------------------------------------------------------------------
    // Pan
    //--------------------------------------------------------------------
public:
    void            cameraPanIn(const F32 meters);
    void            cameraPanLeft(const F32 meters);
    void            cameraPanUp(const F32 meters);
    void            resetCameraPan();
    void            resetPanDiff();
    //--------------------------------------------------------------------
    // View
    //--------------------------------------------------------------------
public:
    // Called whenever the agent moves.  Puts camera back in default position, deselects items, etc.
// <FS:CR> FIRE-8798: Option to prevent camera reset on movement
    //void          resetView(bool reset_camera = true, bool change_camera = false);
    void            resetView(bool reset_camera = true, bool change_camera = false, bool movement = false);
// </FS:CR>
    // Called on camera movement.  Unlocks camera from the default position behind the avatar.
    void            unlockView();
public:
    F32             mDrawDistance;

    //--------------------------------------------------------------------
    // Mouselook
    //--------------------------------------------------------------------
public:
    bool            getForceMouselook() const           { return mForceMouselook; }
    void            setForceMouselook(bool mouselook)   { mForceMouselook = mouselook; }
private:
    bool            mForceMouselook;

    //--------------------------------------------------------------------
    // HUD
    //--------------------------------------------------------------------
public:
    F32             mHUDTargetZoom; // Target zoom level for HUD objects (used when editing)
    F32             mHUDCurZoom;    // Current animated zoom level for HUD objects

// [RLVa:KB] - Checked: RLVa-2.0.0
    //--------------------------------------------------------------------
    // RLVa
    //--------------------------------------------------------------------
protected:
    bool allowFocusOffsetChange(const LLVector3d& offsetFocus);
    bool clampCameraPosition(LLVector3d& posCamGlobal, const LLVector3d posCamRefGlobal, float nDistMin, float nDistMax);

    bool m_fRlvMaxDist;             // True if the camera is at max distance
    bool m_fRlvMinDist;             // True if the camera is at min distance
    LLVector3d m_posRlvRefGlobal;       // Current reference point for distance calculations
// [/RLVa:KB]

/********************************************************************************
 **                                                                            **
 **                    KEYS
 **/

public:
    S32             getAtKey() const        { return mAtKey; }
    S32             getWalkKey() const      { return mWalkKey; }
    S32             getLeftKey() const      { return mLeftKey; }
    S32             getUpKey() const        { return mUpKey; }
    F32             getYawKey() const       { return mYawKey; }
    F32             getPitchKey() const     { return mPitchKey; }

    void            setAtKey(S32 mag)       { mAtKey = mag; }
    void            setWalkKey(S32 mag)     { mWalkKey = mag; }
    void            setLeftKey(S32 mag)     { mLeftKey = mag; }
    void            setUpKey(S32 mag)       { mUpKey = mag; }
    void            setYawKey(F32 mag)      { mYawKey = mag; }
    void            setPitchKey(F32 mag)    { mPitchKey = mag; }

    void            clearGeneralKeys();
    static S32      directionToKey(S32 direction); // Changes direction to -1/0/1

private:
    S32             mAtKey;             // Either 1, 0, or -1. Indicates that movement key is pressed
    S32             mWalkKey;           // Like AtKey, but causes less forward thrust
    S32             mLeftKey;
    S32             mUpKey;
    F32             mYawKey;
    F32             mPitchKey;

    //--------------------------------------------------------------------
    // Orbit
    //--------------------------------------------------------------------
public:
    F32             getOrbitLeftKey() const     { return mOrbitLeftKey; }
    F32             getOrbitRightKey() const    { return mOrbitRightKey; }
    F32             getOrbitUpKey() const       { return mOrbitUpKey; }
    F32             getOrbitDownKey() const     { return mOrbitDownKey; }
    F32             getOrbitInKey() const       { return mOrbitInKey; }
    F32             getOrbitOutKey() const      { return mOrbitOutKey; }
// <FS:Chanayane> Camera roll (from Alchemy)
    F32             getRollLeftKey() const      { return mRollLeftKey; }
    F32             getRollRightKey() const     { return mRollRightKey; }
// </FS:Chanayane>

    void            setOrbitLeftKey(F32 mag)    { mOrbitLeftKey = mag; }
    void            setOrbitRightKey(F32 mag)   { mOrbitRightKey = mag; }
    void            setOrbitUpKey(F32 mag)      { mOrbitUpKey = mag; }
    void            setOrbitDownKey(F32 mag)    { mOrbitDownKey = mag; }
    void            setOrbitInKey(F32 mag)      { mOrbitInKey = mag; }
    void            setOrbitOutKey(F32 mag)     { mOrbitOutKey = mag; }
// <FS:Chanayane> Camera roll (from Alchemy)
    void            setRollLeftKey(F32 mag)     { mRollLeftKey = mag; }
    void            setRollRightKey(F32 mag)    { mRollRightKey = mag; }
// </FS:Chanayane>

    void            clearOrbitKeys();
private:
    F32             mOrbitLeftKey;
    F32             mOrbitRightKey;
    F32             mOrbitUpKey;
    F32             mOrbitDownKey;
    F32             mOrbitInKey;
    F32             mOrbitOutKey;

    F32             mOrbitAroundRadians;
    F32             mOrbitOverAngle;

// <FS:Chanayane> Camera roll (from Alchemy)
    F32             mRollLeftKey;
    F32             mRollRightKey;
    F32             mRollAngle = 0.f;
// </FS:Chanayane>

    //--------------------------------------------------------------------
    // Pan
    //--------------------------------------------------------------------
public:
    F32             getPanLeftKey() const       { return mPanLeftKey; }
    F32             getPanRightKey() const  { return mPanRightKey; }
    F32             getPanUpKey() const     { return mPanUpKey; }
    F32             getPanDownKey() const       { return mPanDownKey; }
    F32             getPanInKey() const     { return mPanInKey; }
    F32             getPanOutKey() const        { return mPanOutKey; }

    void            setPanLeftKey(F32 mag)      { mPanLeftKey = mag; }
    void            setPanRightKey(F32 mag)     { mPanRightKey = mag; }
    void            setPanUpKey(F32 mag)        { mPanUpKey = mag; }
    void            setPanDownKey(F32 mag)      { mPanDownKey = mag; }
    void            setPanInKey(F32 mag)        { mPanInKey = mag; }
    void            setPanOutKey(F32 mag)       { mPanOutKey = mag; }

    void            clearPanKeys();
private:
    F32             mPanUpKey;
    F32             mPanDownKey;
    F32             mPanLeftKey;
    F32             mPanRightKey;
    F32             mPanInKey;
    F32             mPanOutKey;

    LLVector3d      mPanFocusDiff;

/**                    Keys
 **                                                                            **
 *******************************************************************************/

// <FS:Ansariel> FIRE-7758: Save/load camera position feature
public:
    void            storeCameraPosition();
    void            loadCameraPosition();
// </FS:Ansariel> FIRE-7758: Save/load camera position feature
};

extern LLAgentCamera gAgentCamera;

#endif
