/**
 * @file llviewerjoystick.h
 * @brief Viewer joystick / NDOF device functionality.
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

#ifndef LL_LLVIEWERJOYSTICK_H
#define LL_LLVIEWERJOYSTICK_H

#include "stdtypes.h"
// [RLVa:Trish]
#include "rlvdefines.h"
#include <boost/signals2.hpp>
// [/RLVa:Trish]

#include <map>

#if LIB_NDOF
#if LL_DARWIN
#define TARGET_OS_MAC 1
#endif
#include "ndofdev_external.h"
#else
#define NDOF_Device void
#define NDOF_HotPlugResult S32
#endif

// <FS:Zi> FIRE-14344 - Using the same numbers all over the place is a bad idea, so let's
//                      define them here in case we need to change them at some point. Better
//                      would be to have it all figured out at runtime, but a lot of the code
//                      expects a fixed size of options, so that needs bigger changes :/
#define MAX_JOYSTICK_AXES       8
#define MAX_JOYSTICK_BUTTONS    16
// </FS:Zi>

typedef enum e_joystick_driver_state
{
    JDS_UNINITIALIZED,
    JDS_INITIALIZED,
    JDS_INITIALIZING
} EJoystickDriverState;

class LLViewerJoystick : public LLSingleton<LLViewerJoystick>
{
    LLSINGLETON(LLViewerJoystick);
    virtual ~LLViewerJoystick();
    LOG_CLASS(LLViewerJoystick);

public:
    void init(bool autoenable);
    void initDevice(LLSD &guid);
    bool initDevice(void * preffered_device /*LPDIRECTINPUTDEVICE8*/);
    bool initDevice(void * preffered_device /*LPDIRECTINPUTDEVICE8*/, const std::string &name, const LLSD &guid);
    void terminate();

    void updateStatus();
    void scanJoystick();
    void moveObjects(bool reset = false);
    void moveAvatar(bool reset = false);
    void moveFlycam(bool reset = false);
    F32 getJoystickAxis(U32 axis) const;
    U32 getJoystickButton(U32 button) const;
    bool isJoystickInitialized() const {return (mDriverState==JDS_INITIALIZED);}
    bool isLikeSpaceNavigator() const;
    void setNeedsReset(bool reset = true) { mResetFlag = reset; }
    void setCameraNeedsUpdate(bool b)     { mCameraUpdated = b; }
    bool getCameraNeedsUpdate() const     { return mCameraUpdated; }
    // Flycam's state now lives on LLAgentCamera (shared with the game-control
    // flycam); this just forwards.
    bool getOverrideCamera();
    bool toggleFlycam();
// [RLVa:Trish] - False while @setcam or @setcam_unlock forbids entering (or staying in) flycam
    static bool isFlycamAllowedByRlv();
// [/RLVa:Trish]
    void setSNDefaults();
    bool isDeviceUUIDSet();
    LLSD getDeviceUUID(); //unconverted, OS dependent value wrapped into LLSD, for comparison/search
    std::string getDeviceUUIDString(); // converted readable value for settings
    std::string getDescription();
    void saveDeviceIdToSettings();

    static bool is3DConnexionDevice(const std::string& device_name);

    // Summary of 3Dconnexion usage this session, for the viewer stats:
    // { enabled, devices: [ { name, used, connected }, ... ] }
    // 'devices' lists every 3Dconnexion device accepted at some point this session.
    LLSD getSessionStatsAsLLSD() const;

protected:
    void updateEnabled(bool autoenable);
    void handleRun(F32 inc);
    void agentSlide(F32 inc);
    void agentPush(F32 inc);
    void agentFly(F32 inc);
    void agentPitch(F32 pitch_inc);
    void agentYaw(F32 yaw_inc);
    void agentJump();
    void resetDeltas(S32 axis[]);
    void loadDeviceIdFromSettings();
// [RLVa:Trish]
    void onRlvBehaviourToggle(ERlvBehaviour behaviour, ERlvParamType type);
// [/RLVa:Trish]
    // Call after a successful ndof_init_first(): marks the driver initialized
    // only if the device is a 3Dconnexion one.  Returns true if accepted.
    bool acceptInitializedDevice();
#if LIB_NDOF
    static NDOF_HotPlugResult HotPlugAddCallback(NDOF_Device *dev);
    static void HotPlugRemovalCallback(NDOF_Device *dev);
#endif

private:
    // 3Dconnexion devices accepted this session (product name -> used), for viewer stats.
    std::map<std::string, bool> mSessionDevices;

// <FS:Zi> FIRE-14344 - Add button preview and allow for more than 6 axes
//  F32                     mAxes[6];
//  long                    mBtn[16];
    F32                     mAxes[MAX_JOYSTICK_AXES];
    long                    mBtn[MAX_JOYSTICK_BUTTONS];
// </FS:Zi>
    EJoystickDriverState    mDriverState { JDS_UNINITIALIZED };
    NDOF_Device             *mNdofDev { nullptr };

    // Windows: _GUID as U8 binary map
    // MacOS: long as an U8 binary map
    // Else: integer 1 for no device/ndof's default device
    LLSD                    mLastDeviceUUID;

    F32                     mPerfScale;
    U32                     mJoystickRun { 0 };
    bool                    mResetFlag { false };
    bool                    mCameraUpdated { true };
    bool                    mDeviceIs3DConnexion { false };
    boost::signals2::connection mRlvBehaviourConnection; // [RLVa:Trish]

    static F32              sLastDelta[7];
    static F32              sDelta[7];

    // This device's own feathered-delta state feeding
    // LLAgentCamera::applyNdofFlycamFrameDelta() (which owns the shared flycam
    // transform/engine, also used by the game-control flycam), kept separate
    // from sDelta/sLastDelta above (which moveAvatar()/moveObjects() still
    // use for non-flycam axis handling).
    F32                     mJoystickFlycamDelta[7] { 0,0,0,0,0,0,0 };
    F32                     mJoystickFlycamLastDelta[7] { 0,0,0,0,0,0,0 };

// FIRE-14344 - Add button preview and allow for more than 6 axes
public:
    U32 getNumOfJoystickAxes() const;
    U32 getNumOfJoystickButtons() const;
// </FS.Zi>
};

#endif
