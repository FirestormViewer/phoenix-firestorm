/**
 * @file fscameralinks.cpp
 * @brief Camera links: secondlife:///app/camera/ SLurls that move the camera to a saved view
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2026, The Phoenix Firestorm Project, Inc.
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
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "fscameralinks.h"

#include "llagent.h"
#include "llagentcamera.h"
#include "llavatarname.h"
#include "llavatarnamecache.h"
#include "llcallbacklist.h"
#include "llclipboard.h"
#include "llcommandhandler.h"
#include "llcontrolavatar.h"
#include "llcriticaldamp.h"
#include "llnotificationsutil.h"
#include "llregionhandle.h"
#include "llselectmgr.h"
#include "llslurl.h"
#include "lltimer.h"
#include "lluri.h"
#include "llviewercamera.h"
#include "llviewerjoystick.h"
#include "llviewermenu.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"
#include "llworld.h"
#include "llworldmap.h"
#include "llworldmapmessage.h"
#include "rlvactions.h"
#include "rlvcommon.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace
{
    // Region width is checked once the view is applied in its region. Opensim supports more than 256m
    constexpr F32 MAX_REGION_WIDTH = 8192.f;
    constexpr size_t MAX_REGION_NAME_LENGTH = 64;
    // Links made from a camera across a region border are still accepted
    constexpr F32 COORD_MARGIN = 256.f;
    constexpr F32 MIN_Z = -100.f;
    constexpr F32 MAX_Z = 10000.f;
    // The focus point only sets a direction and an orbit distance, it can't be far from the camera
    constexpr F32 MAX_FOCUS_DISTANCE = 1024.f;
    constexpr F32 MIN_FOV_DEG = 5.f;
    constexpr F32 MAX_FOV_DEG = 175.f;
    constexpr F32 MAX_ROLL_INPUT_DEG = 360.f;
    constexpr F32 MAX_TARGET_OFFSET = 1024.f;
    // Orbit distance used when the current focus point makes no sense (mouselook, flycam)
    constexpr F64 DEFAULT_FOCUS_DISTANCE = 10.0;
    // Time before a missing target aborts the view
    constexpr F32 PENDING_TIMEOUT = 30.f;
    // Time in the region before the waiting view is applied
    constexpr F32 ARRIVAL_SETTLE = 3.0f;
    constexpr F32 POLL_INTERVAL = 0.25f;
    // Smoothing of the camera to a moving target to attempt to reduce motion sickness
    constexpr F32 FOLLOW_SMOOTHING = 0.06f;
    constexpr F32 FOLLOW_USER_MOVE_SQUARED = 0.0001f;  // a camera move by the user over a centimetre while following

    FSCameraLinks::View sPendingView;
    bool sPendingActive = false;
    bool sPendingPollerRunning = false;
    bool sPendingSawTeleport = false;
    bool sPendingSettling = false;
    LLTimer sPendingTimer;
    LLTimer sPendingSettleTimer;

    bool sFovApplied = false;
    bool sFovPollerRunning = false;
    F32 sFovBefore = 0.f;
    F32 sFovSet = 0.f;

    // "x,y,z"
    bool parse_vector(const std::string& text, LLVector3& value)
    {
        std::vector<std::string> parts;
        LLStringUtil::getTokens(text, parts, ",");
        if (parts.size() != 3)
        {
            return false;
        }
        for (S32 i = 0; i < 3; ++i)
        {
            if (!FSCameraLinks::parseNumber(parts[i], value.mV[i]))
            {
                return false;
            }
        }
        return true;
    }

    bool in_range(F32 value, F32 min_value, F32 max_value)
    {
        return value >= min_value && value <= max_value;
    }

    bool valid_xy(F32 value, F32 width)
    {
        return in_range(value, -COORD_MARGIN, width + COORD_MARGIN);
    }

    // An angle in degrees brought into -180..180
    F32 wrap_degrees(F32 degrees)
    {
        degrees = fmodf(degrees, 360.f);
        if (degrees > 180.f)
        {
            degrees -= 360.f;
        }
        else if (degrees < -180.f)
        {
            degrees += 360.f;
        }
        return degrees;
    }

    bool same_region(const LLViewerRegion* region, const std::string& name)
    {
        return region && LLStringUtil::compareInsensitive(region->getName(), name) == 0;
    }

    // The agent's region or a neighbour. This is necessary for events, they often span across region borders to have higher capacity.
    // A link across must move the camera without offering a TP, to support this use case.
    LLViewerRegion* find_known_region(const std::string& name)
    {
        LLViewerRegion* agent_region = gAgent.getRegion();
        if (same_region(agent_region, name))
        {
            return agent_region;
        }
        for (LLViewerRegion* region : LLWorld::getInstance()->getRegionList())
        {
            if (region && region->isAlive() && same_region(region, name))
            {
                return region;
            }
        }
        return nullptr;
    }

    // A view farther from the avatar than the draw distance is impossible and requires a teleport
    bool view_in_reach(const FSCameraLinks::View& view, LLViewerRegion* region)
    {
        if (region == gAgent.getRegion())
        {
            return true;
        }
        const F64 distance = (region->getPosGlobalFromRegion(view.mPos) - gAgent.getPositionGlobal()).magVec();
        return distance <= (F64)llmax(gAgentCamera.mDrawDistance, 64.f);
    }

    // Any RLVa camera restriction means an object is steering or limiting the camera; a link must not
    // override that.
    // RLVa restrictions that block camera links: @setcam, @setcam_eyeoffset, @setcam_unlock.
    // FOV is clamped as-is, and distances are checked per view at a later time.
    bool rlv_locks_camera()
    {
        return RlvActions::isRlvEnabled() && (RlvActions::isCameraPresetLocked() || RlvActions::hasBehaviour(RLV_BHVR_SETCAM_UNLOCK));
    }

    // Whether a camera is within RLVa restriction limits
    bool rlv_allows_camera_at(const LLVector3d& pos_global)
    {
        if (!RlvActions::isRlvEnabled() || !RlvActions::isCameraDistanceClamped())
        {
            return true;
        }
        if (!isAgentAvatarValid())
        {
            return false;
        }
        const LLVector3d head = gAgent.getPosGlobalFromAgent(gAgentAvatarp->mHeadp ? gAgentAvatarp->mHeadp->getWorldPosition() : gAgent.getPositionAgent());
        const F32 distance = (F32)(pos_global - head).magVec();
        F32 min_distance = 0.f;
        F32 max_distance = 0.f;
        if (RlvActions::getCameraAvatarDistanceLimits(min_distance, max_distance) && (distance < min_distance || distance > max_distance))
        {
            return false;
        }
        if (RlvActions::getCameraOriginDistanceLimits(min_distance, max_distance) && (distance < min_distance || distance > max_distance))
        {
            return false;
        }
        return true;
    }

    bool rlv_restricts_teleport()
    {
        return RlvActions::isRlvEnabled() && !RlvActions::canTeleportToLocation();
    }

    // Restore FOV after a link was used
    bool poll_fov_restore()
    {
        if (!sFovApplied)
        {
            sFovPollerRunning = false;
            return true;
        }
        if (!gAgentCamera.getFocusOnAvatar())
        {
            return false;
        }
        LLViewerCamera* camera = LLViewerCamera::getInstance();
        if (std::fabs(camera->getDefaultFOV() - sFovSet) < 0.001f)
        {
            camera->setDefaultFOV(sFovBefore);
        }
        sFovApplied = false;
        sFovPollerRunning = false;
        return true;
    }

    void apply_fov(F32 fov_rad)
    {
        LLViewerCamera* camera = LLViewerCamera::getInstance();
        if (!sFovApplied)
        {
            sFovBefore = camera->getDefaultFOV();
        }
        // Not saved to CameraAngle: the link's angle is temporary and must not outlive the use of a link
        camera->setDefaultFOV(fov_rad);
        sFovSet = camera->getDefaultFOV();
        sFovApplied = true;
        if (!sFovPollerRunning)
        {
            sFovPollerRunning = true;
            doPeriodically(poll_fov_restore, POLL_INTERVAL);
        }
    }

    // Takes the camera from flycam and mouselook, unlock it like an alt-click
    void take_camera()
    {
        if (LLViewerJoystick::getInstance()->getOverrideCamera())
        {
            handle_toggle_flycam();
            LLViewerJoystick::getInstance()->setCameraNeedsUpdate(true);
        }
        if (gAgentCamera.cameraMouselook())
        {
            gAgentCamera.changeCameraToDefault();
        }
        gAgentCamera.unlockView();
    }

    // Moves the camera to the desired location
    void place_camera(const LLVector3d& pos, const LLVector3d& focus, F32 roll_rad, F32 fov_rad)
    {
        gAgentCamera.setCameraPosAndFocusGlobal(pos, focus, LLUUID::null);
        gAgentCamera.stopCameraAnimation();
        gAgentCamera.setRollAngle(roll_rad);
        if (fov_rad > 0.f)
        {
            apply_fov(fov_rad);
        }
    }

    // Current camera info to create a link from
    void current_camera(LLVector3d& pos, LLVector3d& focus)
    {
        LLViewerCamera* camera = LLViewerCamera::getInstance();
        pos = gAgent.getPosGlobalFromAgent(camera->getOrigin());
        F64 distance = DEFAULT_FOCUS_DISTANCE;
        if (!gAgentCamera.cameraMouselook() && !LLViewerJoystick::getInstance()->getOverrideCamera())
        {
            const LLVector3d target = gAgentCamera.getFocusTargetGlobal();
            if (!target.isExactlyZero())
            {
                distance = llclamp((target - pos).magVec(), 1.0, 256.0);
            }
        }
        focus = pos + LLVector3d(camera->getAtAxis()) * distance;
    }

    // Current camera view, false if it cannot be told (due to RLVa)
    bool current_view(FSCameraLinks::View& view, LLViewerRegion*& region)
    {
        // A link names the region, which RLVa may be hiding
        if (RlvActions::isRlvEnabled() && !RlvActions::canShowLocation())
        {
            LLNotificationsUtil::add("FSCameraLinkRlvLocation");
            return false;
        }

        LLVector3d camera_global;
        LLVector3d focus_global;
        current_camera(camera_global, focus_global);

        // Allow neighboring regions
        region = LLWorld::getInstance()->getRegionFromPosGlobal(camera_global);
        if (!region)
        {
            region = gAgent.getRegion();
        }
        if (!region)
        {
            LLNotificationsUtil::add("FSCameraLinkNoRegion");
            return false;
        }

        view = FSCameraLinks::View();
        view.mRegion = region->getName();
        view.mPos = region->getPosRegionFromGlobal(camera_global);
        view.mFocus = region->getPosRegionFromGlobal(focus_global);
        view.mFovDeg = LLViewerCamera::getInstance()->getDefaultFOV() * RAD_TO_DEG;
        view.mRollDeg = wrap_degrees(gAgentCamera.getRollAngle() * RAD_TO_DEG);

        // Position the avatar after a TP, if able.
        if (gAgent.getRegion() == region)
        {
            view.mAvatarPos = region->getPosRegionFromGlobal(gAgent.getPositionGlobal());
            const F32 width = region->getWidth();
            view.mHasAvatarPos = in_range(view.mAvatarPos.mV[VX], 0.f, width) && in_range(view.mAvatarPos.mV[VY], 0.f, width) &&
                                 in_range(view.mAvatarPos.mV[VZ], MIN_Z, MAX_Z);
        }
        return true;
    }

    ///////////////////////////////////////////////////////////////////////////
    // Targets

    LLViewerObject* find_target(const LLUUID& id)
    {
        if (id.isNull())
        {
            return nullptr;
        }
        LLViewerObject* object = gObjectList.findObject(id);
        return (object && !object->isDead() && object->getRegion()) ? object : nullptr;
    }

    // Where the camera should be relative to the target
    void target_pose(const LLViewerObject* target, const FSCameraLinks::View& view, LLVector3d& pos, LLVector3d& focus)
    {
        const LLVector3d base = target->getPositionGlobal();
        LLVector3 offset = view.mTargetOffset;
        LLVector3 focus_offset = view.mTargetFocusOffset;
        if (view.mTargetTurns)
        {
            const LLQuaternion rotation = target->getRenderRotation();
            offset = offset * rotation;
            focus_offset = focus_offset * rotation;
        }
        pos = base + LLVector3d(offset);
        focus = base + LLVector3d(focus_offset);
    }

    LLViewerObject* follow_root(LLViewerObject* object)
    {
        if (!object || object->isDead())
        {
            return nullptr;
        }
        if (object->isAvatar())
        {
            LLVOAvatar* avatar = object->asAvatar();
            if (avatar && avatar->isControlAvatar())
            {
                LLControlAvatar* control = static_cast<LLControlAvatar*>(avatar);
                return control->mRootVolp;
            }
            return object;
        }
        LLViewerObject* root = object->getRootEdit();
        if (root && root->isAttachment())
        {
            return root->getAvatar();
        }
        return root;
    }

    std::string target_name(LLViewerObject* target)
    {
        if (target->isAvatar())
        {
            // RLVa may be hiding names
            const bool hide = RlvActions::isRlvEnabled() && !RlvActions::canShowName(RlvActions::SNC_DEFAULT, target->getID());
            LLAvatarName av_name;
            if (LLAvatarNameCache::get(target->getID(), &av_name))
            {
                return hide ? RlvStrings::getAnonym(av_name) : av_name.getCompleteName();
            }
            const std::string name = target->asAvatar()->getFullname();
            return hide ? RlvStrings::getAnonym(name) : name;
        }
        if (LLSelectNode* node = LLSelectMgr::getInstance()->getSelection()->findNode(target))
        {
            if (node->mValid && !node->mName.empty())
            {
                return node->mName;
            }
        }
        return std::string();
    }

    // Following a link's target: the camera keeps its place relative to it
    FSCameraLinks::View sFollowView;
    std::string sFollowLink;
    bool sFollowActive = false;
    bool sFollowStarted = false;
    U32 sFollowGeneration = 0;
    LLVector3d sFollowPos;
    LLVector3d sFollowFocus;

    void stop_follow()
    {
        if (sFollowActive)
        {
            sFollowActive = false;
            ++sFollowGeneration;
        }
    }

    bool follow_idle(U32 generation)
    {
        if (!sFollowActive || generation != sFollowGeneration)
        {
            return true;
        }
        // Something took the camera back from the bookmark (mouselook, walking with the setting on, ESC, etc.)
        if (sFollowStarted && (gAgentCamera.getFocusOnAvatar() || gAgentCamera.cameraMouselook()))
        {
            stop_follow();
            return true;
        }
        if (!isAgentAvatarValid() || gAgent.getTeleportState() != LLAgent::TELEPORT_NONE)
        {
            stop_follow();
            return true;
        }
        if (rlv_locks_camera())
        {
            LLNotificationsUtil::add("FSCameraLinkRlvCamera");
            stop_follow();
            return true;
        }
        LLViewerObject* target = find_target(sFollowView.mTarget);
        if (!target)
        {
            // gone (left, derendered, out of range): camera stops where it was
            LLNotificationsUtil::add("FSCameraLinkTargetLost");
            stop_follow();
            return true;
        }

        if (sFollowStarted)
        {
            // The camera was zoomed, orbited or panned since the last frame: keep that, relative to the target
            const LLVector3d user_focus = gAgentCamera.getFocusTargetGlobal();
            const LLVector3d user_pos = user_focus + gAgentCamera.getCameraFocusOffsetTarget();
            LLVector3 moved(user_pos - sFollowPos);
            LLVector3 focus_moved(user_focus - sFollowFocus);
            if (moved.magVecSquared() > FOLLOW_USER_MOVE_SQUARED || focus_moved.magVecSquared() > FOLLOW_USER_MOVE_SQUARED)
            {
                if (sFollowView.mTargetTurns)
                {
                    const LLQuaternion inverse = ~target->getRenderRotation();
                    moved = moved * inverse;
                    focus_moved = focus_moved * inverse;
                }
                sFollowView.mTargetOffset += moved;
                sFollowView.mTargetFocusOffset += focus_moved;
                sFollowPos = user_pos;
                sFollowFocus = user_focus;
            }
        }

        LLVector3d pos;
        LLVector3d focus;
        target_pose(target, sFollowView, pos, focus);
        if (!rlv_allows_camera_at(pos))
        {
            // the target took the camera past the RLVa distance limits: stop tracking
            LLNotificationsUtil::add("FSCameraLinkRlvDistance");
            stop_follow();
            return true;
        }
        if (sFollowStarted)
        {
            const F64 u = LLSmoothInterpolation::getInterpolant(FOLLOW_SMOOTHING);
            pos = sFollowPos + (pos - sFollowPos) * u;
            focus = sFollowFocus + (focus - sFollowFocus) * u;
        }
        sFollowPos = pos;
        sFollowFocus = focus;
        sFollowStarted = true;
        gAgentCamera.setCameraPosAndFocusGlobal(pos, focus, LLUUID::null);
        gAgentCamera.stopCameraAnimation();
        return false;
    }

    void start_follow(const FSCameraLinks::View& view, LLViewerObject* target)
    {
        LLVector3d pos;
        LLVector3d focus;
        target_pose(target, view, pos, focus);
        if (!rlv_allows_camera_at(pos))
        {
            LLNotificationsUtil::add("FSCameraLinkRlvDistance");
            return;
        }
        stop_follow();
        take_camera();
        place_camera(pos, focus, view.mRollDeg * DEG_TO_RAD, view.mFovDeg > 0.f ? view.mFovDeg * DEG_TO_RAD : 0.f);

        sFollowView = view;
        sFollowLink = FSCameraLinks::buildLink(view);
        sFollowActive = true;
        sFollowStarted = false;
        sFollowPos = pos;
        sFollowFocus = focus;
        const U32 generation = ++sFollowGeneration;
        doOnIdleRepeating([generation]() { return follow_idle(generation); });
    }

    void apply_view(const FSCameraLinks::View& view, LLViewerRegion* region)
    {
        // If the target isn't present, we just drop the camera where the bookmark starts and can't start a follow.
        if (view.hasTarget())
        {
            if (LLViewerObject* target = find_target(view.mTarget))
            {
                start_follow(view, target);
                return;
            }
        }

        const F32 width = region->getWidth();
        if (!valid_xy(view.mPos.mV[VX], width) || !valid_xy(view.mPos.mV[VY], width))
        {
            LLNotificationsUtil::add("FSCameraLinkInvalid");
            return;
        }
        if (view.hasTarget())
        {
            LLNotificationsUtil::add("FSCameraLinkTargetGone");
        }

        const LLVector3d pos = region->getPosGlobalFromRegion(view.mPos);
        if (!rlv_allows_camera_at(pos))
        {
            LLNotificationsUtil::add("FSCameraLinkRlvDistance");
            return;
        }
        stop_follow();
        take_camera();
        place_camera(pos, region->getPosGlobalFromRegion(view.mFocus),
                     view.mRollDeg * DEG_TO_RAD, view.mFovDeg > 0.f ? view.mFovDeg * DEG_TO_RAD : 0.f);
    }

    // Waits for the agent to arrive in the pending view's region after the teleport, give up if something seemingly went wrong (as is common in SL!)
    bool poll_pending_view()
    {
        if (!sPendingActive)
        {
            sPendingPollerRunning = false;
            return true;
        }
        if (sPendingTimer.getElapsedTimeF32() > PENDING_TIMEOUT)
        {
            LLSD args;
            args["REGION"] = sPendingView.mRegion;
            LLNotificationsUtil::add("FSCameraLinkGaveUp", args);
            sPendingActive = false;
            sPendingPollerRunning = false;
            return true;
        }

        const bool teleporting = gAgent.getTeleportState() != LLAgent::TELEPORT_NONE;
        if (teleporting)
        {
            sPendingSawTeleport = true;
        }

        LLViewerRegion* region = gAgent.getRegion();
        if (teleporting || !isAgentAvatarValid() || !same_region(region, sPendingView.mRegion))
        {
            sPendingSettling = false;
            if (sPendingSawTeleport && !teleporting && region && !same_region(region, sPendingView.mRegion))
            {
                // The teleport failed or moved the avatar somewhere unexpected
                sPendingActive = false;
                sPendingPollerRunning = false;
                return true;
            }
            return false;
        }

        if (!sPendingSettling)
        {
            sPendingSettling = true;
            sPendingSettleTimer.reset();
            return false;
        }
        if (sPendingSettleTimer.getElapsedTimeF32() < ARRIVAL_SETTLE)
        {
            return false;
        }

        sPendingActive = false;
        sPendingPollerRunning = false;
        // Restrictions may have changed after teleporting, make sure we can still take the camera.
        if (rlv_locks_camera())
        {
            LLNotificationsUtil::add("FSCameraLinkRlvCamera");
            return true;
        }
        apply_view(sPendingView, region);
        return true;
    }

    void on_region_handle(U64 region_handle)
    {
        if (!sPendingActive)
        {
            return;
        }
        if (rlv_restricts_teleport())
        {
            sPendingActive = false;
            LLNotificationsUtil::add("FSCameraLinkRlvTeleport");
            return;
        }

        // Teleport to where the avatar stood when the link was made, or else to the camera's x and y at ground level
        const LLVector3& spot = sPendingView.mHasAvatarPos ? sPendingView.mAvatarPos : sPendingView.mPos;
        const F32 landing_z = sPendingView.mHasAvatarPos ? llmax(sPendingView.mAvatarPos.mV[VZ], 0.f) : 0.f;
        // The map reply that brought the handle also recorded the region's size (OpenSim var regions are larger than 256m)
        F32 width_x = REGION_WIDTH_METERS;
        F32 width_y = REGION_WIDTH_METERS;
        if (const LLSimInfo* info = LLWorldMap::getInstance()->simInfoFromHandle(region_handle))
        {
            width_x = llmax((F32)info->getSizeX(), REGION_WIDTH_METERS);
            width_y = llmax((F32)info->getSizeY(), REGION_WIDTH_METERS);
        }
        LLVector3 landing(llclamp(spot.mV[VX], 1.f, width_x - 1.f), llclamp(spot.mV[VY], 1.f, width_y - 1.f), landing_z);
        const LLVector3d pos_global = from_region_handle(region_handle) + LLVector3d(landing);

        sPendingTimer.reset();
        sPendingSawTeleport = false;
        sPendingSettling = false;
        gAgent.teleportViaLocation(pos_global);

        if (!sPendingPollerRunning)
        {
            sPendingPollerRunning = true;
            doPeriodically(poll_pending_view, POLL_INTERVAL);
        }
    }

    void request_teleport(const FSCameraLinks::View& view)
    {
        if (rlv_restricts_teleport())
        {
            LLNotificationsUtil::add("FSCameraLinkRlvTeleport");
            return;
        }

        // A newer view replaces one still waiting, along with its progress
        sPendingView = view;
        sPendingActive = true;
        sPendingSawTeleport = false;
        sPendingSettling = false;
        sPendingTimer.reset();

        // Same path as secondlife:///app/teleport/ links
        LLWorldMapMessage::getInstance()->sendNamedRegionRequest(view.mRegion,
            [](U64 region_handle, const std::string&, const LLUUID&, bool) { on_region_handle(region_handle); },
            std::string(),
            true);

        // Give up if we never got a handle
        if (!sPendingPollerRunning)
        {
            sPendingPollerRunning = true;
            doPeriodically(poll_pending_view, POLL_INTERVAL);
        }
    }

    void on_teleport_confirmed(const LLSD& notification, const LLSD& response)
    {
        if (LLNotificationsUtil::getSelectedOption(notification, response) != 0)
        {
            return;
        }
        FSCameraLinks::View view;
        if (FSCameraLinks::parseLink(notification["payload"]["link"].asString(), view))
        {
            request_teleport(view);
        }
    }

    class FSCameraLinkHandler : public LLCommandHandler
    {
    public:
        // Moving the camera, and the tp offer, must come from a click.
        // Else it is a vulnerability that lets scripts steer the cam and tp
        FSCameraLinkHandler() : LLCommandHandler("camera", UNTRUSTED_CLICK_ONLY) {}

        bool handle(const LLSD& tokens, const LLSD& query_map, const std::string& grid, LLMediaCtrl* web) override
        {
            FSCameraLinks::View view;
            if (!FSCameraLinks::parse(tokens, query_map, view))
            {
                LLNotificationsUtil::add("FSCameraLinkInvalid");
                return true;
            }
            FSCameraLinks::activate(view);
            return true;
        }
    };
    FSCameraLinkHandler gFSCameraLinkHandler;

    // "<x, y, z>", the cpcampos cmd form of a vector
    bool parse_bracket_vector(std::string text, LLVector3& out)
    {
        LLStringUtil::trim(text);
        if (text.size() < 5 || text.front() != '<' || text.back() != '>')
        {
            return false;
        }
        text = text.substr(1, text.size() - 2);
        LLStringUtil::replaceString(text, ",", " ");
        return LLVector3::parseVector3(text, &out);
    }
}

namespace FSCameraLinks
{
    bool parse(const LLSD& tokens, const LLSD& query_map, View& view)
    {
        if (!tokens.isArray() || tokens.size() < 4)
        {
            return false;
        }

        // Region names may contain escaped spaces. Links can be user-crafted and contain junk, gate against that.
        std::string region = LLURI::unescape(tokens[0].asString());
        LLStringUtil::trim(region);
        if (region.empty() || region.size() > MAX_REGION_NAME_LENGTH ||
            region.find_first_of("<>|/\\[]") != std::string::npos ||
            std::any_of(region.begin(), region.end(), [](char ch) { return (U8)ch < 0x20 || ch == 0x7f; }))
        {
            return false;
        }

        LLVector3 pos;
        for (S32 i = 0; i < 3; ++i)
        {
            if (!FSCameraLinks::parseNumber(tokens[i + 1].asString(), pos.mV[i]))
            {
                return false;
            }
        }
        if (!valid_xy(pos.mV[VX], MAX_REGION_WIDTH) || !valid_xy(pos.mV[VY], MAX_REGION_WIDTH) || !in_range(pos.mV[VZ], MIN_Z, MAX_Z))
        {
            return false;
        }

        LLVector3 focus;
        if (!query_map.has("focus") || !parse_vector(query_map["focus"].asString(), focus))
        {
            return false;
        }
        const F32 focus_distance = (focus - pos).magVec();
        if (focus_distance < 0.01f || focus_distance > MAX_FOCUS_DISTANCE)
        {
            return false;
        }

        // INFO: If some day we want to extand the querystrings to support more camera parameters, simply wire them down below.
        //       As well as in buildlinks()
        F32 fov = 0.f;
        if (query_map.has("fov"))
        {
            if (!FSCameraLinks::parseNumber(query_map["fov"].asString(), fov) || !in_range(fov, MIN_FOV_DEG, MAX_FOV_DEG))
            {
                return false;
            }
        }

        F32 roll = 0.f;
        if (query_map.has("roll"))
        {
            if (!FSCameraLinks::parseNumber(query_map["roll"].asString(), roll) || !in_range(roll, -MAX_ROLL_INPUT_DEG, MAX_ROLL_INPUT_DEG))
            {
                return false;
            }
            roll = wrap_degrees(roll);
        }

        // Where the avatar stood, to teleport to
        LLVector3 avatar_pos;
        const bool has_avatar_pos = query_map.has("tp");
        if (has_avatar_pos)
        {
            if (!parse_vector(query_map["tp"].asString(), avatar_pos) ||
                !valid_xy(avatar_pos.mV[VX], MAX_REGION_WIDTH) || !valid_xy(avatar_pos.mV[VY], MAX_REGION_WIDTH) ||
                !in_range(avatar_pos.mV[VZ], MIN_Z, MAX_Z))
            {
                return false;
            }
        }

        // Following a target
        LLUUID target;
        LLVector3 target_offset;
        LLVector3 target_focus_offset;
        bool target_turns = false;
        if (query_map.has("target"))
        {
            if (!target.set(query_map["target"].asString(), false) || target.isNull() ||
                !query_map.has("toff") || !parse_vector(query_map["toff"].asString(), target_offset) ||
                !query_map.has("tfocus") || !parse_vector(query_map["tfocus"].asString(), target_focus_offset))
            {
                return false;
            }
            const F32 target_focus_distance = (target_focus_offset - target_offset).magVec();
            if (target_offset.magVec() > MAX_TARGET_OFFSET || target_focus_offset.magVec() > MAX_TARGET_OFFSET ||
                target_focus_distance < 0.01f || target_focus_distance > MAX_FOCUS_DISTANCE)
            {
                return false;
            }
            target_turns = query_map.has("turn") && query_map["turn"].asString() == "1";
        }

        view.mRegion = region;
        view.mPos = pos;
        view.mFocus = focus;
        view.mFovDeg = fov;
        view.mRollDeg = roll;
        view.mAvatarPos = avatar_pos;
        view.mHasAvatarPos = has_avatar_pos;
        view.mTarget = target;
        view.mTargetOffset = target_offset;
        view.mTargetFocusOffset = target_focus_offset;
        view.mTargetTurns = target_turns;
        return true;
    }

    bool parseLink(const std::string& link, View& view)
    {
        LLSLURL slurl(link);
        if (slurl.getType() != LLSLURL::APP || slurl.getAppCmd() != "camera")
        {
            return false;
        }
        return parse(slurl.getAppPath(), slurl.getAppQueryMap(), view);
    }

    std::string buildLink(const View& view)
    {
        std::string link = "secondlife:///app/camera/" + LLURI::escape(view.mRegion);
        link += llformat("/%.2f/%.2f/%.2f?focus=%.2f,%.2f,%.2f",
                         view.mPos.mV[VX], view.mPos.mV[VY], view.mPos.mV[VZ],
                         view.mFocus.mV[VX], view.mFocus.mV[VY], view.mFocus.mV[VZ]);
        if (view.mFovDeg > 0.f)
        {
            link += llformat("&fov=%.1f", view.mFovDeg);
        }
        if (std::fabs(view.mRollDeg) >= 0.05f)
        {
            link += llformat("&roll=%.1f", view.mRollDeg);
        }
        if (view.mHasAvatarPos)
        {
            link += llformat("&tp=%.1f,%.1f,%.1f", view.mAvatarPos.mV[VX], view.mAvatarPos.mV[VY], view.mAvatarPos.mV[VZ]);
        }
        if (view.hasTarget())
        {
            link += "&target=" + view.mTarget.asString();
            link += llformat("&toff=%.2f,%.2f,%.2f&tfocus=%.2f,%.2f,%.2f",
                             view.mTargetOffset.mV[VX], view.mTargetOffset.mV[VY], view.mTargetOffset.mV[VZ],
                             view.mTargetFocusOffset.mV[VX], view.mTargetFocusOffset.mV[VY], view.mTargetFocusOffset.mV[VZ]);
            if (view.mTargetTurns)
            {
                link += "&turn=1";
            }
        }
        return link;
    }

    bool getCurrentView(View& view)
    {
        LLViewerRegion* region = nullptr;
        return current_view(view, region);
    }

    bool getCurrentFollowView(View& view, bool turn_with_target, std::string* name)
    {
        // Check what the camera is currently alt-clicked on, or what is selected.
        LLViewerObject* target = nullptr;
        if (sFollowActive)
        {
            target = find_target(sFollowView.mTarget);
        }
        if (!target && !gAgentCamera.getFocusOnAvatar())
        {
            target = follow_root(gAgentCamera.getFocusObject().get());
        }
        if (!target)
        {
            target = follow_root(LLSelectMgr::getInstance()->getSelection()->getFirstRootObject(true));
        }
        if (!target && gAgentCamera.getFocusOnAvatar() && isAgentAvatarValid())
        {
            target = gAgentAvatarp.get();
        }
        if (!target || !target->getRegion())
        {
            LLNotificationsUtil::add("FSCameraLinkNoTarget");
            return false;
        }
        LLViewerRegion* region = nullptr;
        if (!current_view(view, region))
        {
            return false;
        }
        const LLVector3d base = target->getPositionGlobal();
        LLVector3 offset(region->getPosGlobalFromRegion(view.mPos) - base);
        LLVector3 focus_offset(region->getPosGlobalFromRegion(view.mFocus) - base);
        if (offset.magVec() > MAX_TARGET_OFFSET || focus_offset.magVec() > MAX_TARGET_OFFSET)
        {
            LLNotificationsUtil::add("FSCameraLinkTargetTooFar");
            return false;
        }
        if (turn_with_target)
        {
            // ... in the target's own frame
            const LLQuaternion inverse = ~target->getRenderRotation();
            offset = offset * inverse;
            focus_offset = focus_offset * inverse;
        }
        view.mTarget = target->getID();
        view.mTargetOffset = offset;
        view.mTargetFocusOffset = focus_offset;
        view.mTargetTurns = turn_with_target;
        if (name)
        {
            *name = target_name(target);
        }
        return true;
    }

    void activate(const View& view)
    {
        LLViewerRegion* region = gAgent.getRegion();
        if (!region || !isAgentAvatarValid())
        {
            return;
        }
        if (rlv_locks_camera())
        {
            LLNotificationsUtil::add("FSCameraLinkRlvCamera");
            return;
        }
        // a link clicked during a tour will alt the tour
        stopTour();

        // Targets follow where the target is, rather than a set point
        if (view.hasTarget())
        {
            if (LLViewerObject* target = find_target(view.mTarget))
            {
                sPendingActive = false;
                start_follow(view, target);
                return;
            }
        }

        // No TP needed for camera bookmarks across a reachable sim border
        LLViewerRegion* known = find_known_region(view.mRegion);
        if (known && view_in_reach(view, known))
        {
            // A view clicked here replaces any view still waiting for a teleport
            sPendingActive = false;
            apply_view(view, known);
            return;
        }

        if (rlv_restricts_teleport())
        {
            LLNotificationsUtil::add("FSCameraLinkRlvTeleport");
            return;
        }

        LLSD args;
        args["REGION"] = view.mRegion;
        LLSD payload;
        payload["link"] = buildLink(view);
        LLNotificationsUtil::add("FSCameraLinkTeleport", args, payload, on_teleport_confirmed);
    }

    bool buildCheckedLink(const View& view, std::string& link)
    {
        link = buildLink(view);
        View check;
        if (!parseLink(link, check))
        {
            LLNotificationsUtil::add("FSCameraLinkInvalid");
            return false;
        }
        return true;
    }

    void copyCurrentViewLink()
    {
        View view;
        std::string link;
        if (!getCurrentView(view) || !buildCheckedLink(view, link))
        {
            return;
        }
        const LLWString wide_link = utf8str_to_wstring(link);
        LLClipboard::instance().copyToClipboard(wide_link, 0, (S32)wide_link.size());

        LLSD args;
        args["REGION"] = view.mRegion;
        LLNotificationsUtil::add("FSCameraLinkCopied", args);
    }

    bool parseNumber(std::string text, F32& value)
    {
        LLStringUtil::trim(text);
        if (text.empty())
        {
            return false;
        }
        char* end = nullptr;
        const double parsed = strtod(text.c_str(), &end);
        if (!end || *end != '\0' || !std::isfinite(parsed))
        {
            return false;
        }
        value = (F32)parsed;
        return std::isfinite(value);
    }

    bool parseCamPos(const std::string& text, CamPos& out)
    {
        const size_t first_pipe = text.find('|');
        if (!parse_bracket_vector(first_pipe == std::string::npos ? text : text.substr(0, first_pipe), out.pos))
        {
            return false;
        }
        if (first_pipe == std::string::npos)
        {
            return true;
        }

        const size_t second_pipe = text.find('|', first_pipe + 1);
        const std::string focus_segment = (second_pipe == std::string::npos)
            ? text.substr(first_pipe + 1)
            : text.substr(first_pipe + 1, second_pipe - first_pipe - 1);
        if (!parse_bracket_vector(focus_segment, out.focus))
        {
            return false;
        }
        out.has_focus = true;

        if (second_pipe != std::string::npos)
        {
            std::string roll_segment = text.substr(second_pipe + 1);
            LLStringUtil::trim(roll_segment);
            if (sscanf(roll_segment.c_str(), "%f", &out.roll) != 1)
            {
                return false;
            }
            out.has_roll = true;
        }
        return true;
    }

    std::string formatCamPos(const LLVector3& pos, const LLVector3& focus, F32 roll)
    {
        std::string result = llformat("<%.3f, %.3f, %.3f>", pos.mV[VX], pos.mV[VY], pos.mV[VZ]);
        result += llformat("|<%.3f, %.3f, %.3f>", focus.mV[VX], focus.mV[VY], focus.mV[VZ]);
        result += llformat("|%.3f", roll);
        return result;
    }
}

///////////////////////////////////////////////////////////////////////////////
// Camera tours

namespace FSCameraTour
{
    struct TourPose
    {
        LLVector3d  mPos;
        LLVector3d  mFocus;
        F32         mFovRad = 0.f;
        F32         mRollRad = 0.f;
    };

    std::vector<FSCameraLinks::TourStop> sTourStops;
    std::vector<S32> sTourOrder;
    FSCameraLinks::TourOptions sTourOptions;
    bool sTourActive = false;
    bool sTourTravelling = false;
    bool sTourCameraTaken = false;
    bool sTourHidUI = false;
    U32 sTourGeneration = 0;
    S32 sTourPlace = -1;
    S32 sTourIndex = -1;
    S32 sTourSkipped = 0;
    TourPose sTourFrom;
    TourPose sTourTo;
    TourPose sTourLast;
    TourPose sTourBefore;
    TourPose sTourAfter;
    bool sTourHasBefore = false;
    LLTimer sTourTimer;
    boost::signals2::signal<void()> sTourChanged;

    F32 smoothstep(F32 u)
    {
        u = llclamp(u, 0.f, 1.f);
        return u * u * (3.f - 2.f * u);
    }

    F32 tour_ease(FSCameraLinks::TourEase ease, F32 u)
    {
        u = llclamp(u, 0.f, 1.f);
        switch (ease)
        {
            case FSCameraLinks::TourEase::LINEAR:   return u;
            case FSCameraLinks::TourEase::EASE_IN:  return u * u;
            case FSCameraLinks::TourEase::EASE_OUT: return 1.f - (1.f - u) * (1.f - u);
            default:                                return smoothstep(u);
        }
    }

    // v turned about the vertical by angle radians
    LLVector3d rotate_z(const LLVector3d& v, F64 angle)
    {
        const F64 c = cos(angle);
        const F64 s = sin(angle);
        return LLVector3d(v.mdV[VX] * c - v.mdV[VY] * s, v.mdV[VX] * s + v.mdV[VY] * c, v.mdV[VZ]);
    }

    // A point on the Catmull-Rom curve through p1 (t = 0) and p2 (t = 1), shaped by p0 and p3. (https://en.wikipedia.org/wiki/Catmull%E2%80%93Rom_spline)
    LLVector3d catmull_rom(const LLVector3d& p0, const LLVector3d& p1, const LLVector3d& p2, const LLVector3d& p3, F64 t)
    {
        const F64 t2 = t * t;
        const F64 t3 = t2 * t;
        return (p1 * 2.0 + (p2 - p0) * t + (p0 * 2.0 - p1 * 5.0 + p2 * 4.0 - p3) * t2 + (p1 * 3.0 - p0 - p2 * 3.0 + p3) * t3) * 0.5;
    }

    // The turn from one angle to another the short way round, in radians
    F32 shortest_turn(F32 from, F32 to)
    {
        F32 delta = fmodf(to - from, F_TWO_PI);
        if (delta > F_PI)
        {
            delta -= F_TWO_PI;
        }
        else if (delta < -F_PI)
        {
            delta += F_TWO_PI;
        }
        return delta;
    }

    // The camera part way (t from 0 to 1)
    TourPose hold_pose(const TourPose& base, const FSCameraLinks::TourStop& stop, F32 t)
    {
        using FSCameraLinks::TourMove;
        const F64 s = smoothstep(t);
        const F64 amount = stop.mAmount;
        TourPose pose = base;
        switch (stop.mMove)
        {
            case TourMove::ORBIT_LEFT:  // the camera circles to its left around the focus point
            case TourMove::ORBIT_RIGHT:
            {
                const F64 angle = amount * DEG_TO_RAD * s * (stop.mMove == TourMove::ORBIT_LEFT ? -1.0 : 1.0);
                pose.mPos = base.mFocus + rotate_z(base.mPos - base.mFocus, angle);
                break;
            }
            case TourMove::PAN_LEFT:    // the camera turns in place
            case TourMove::PAN_RIGHT:
            {
                const F64 angle = amount * DEG_TO_RAD * s * (stop.mMove == TourMove::PAN_LEFT ? 1.0 : -1.0);
                pose.mFocus = base.mPos + rotate_z(base.mFocus - base.mPos, angle);
                break;
            }
            case TourMove::PUSH_IN:
                pose.mPos = base.mFocus + (base.mPos - base.mFocus) * (1.0 - llclamp(amount, 0.0, 95.0) / 100.0 * s);
                break;
            case TourMove::PULL_OUT:
                pose.mPos = base.mFocus + (base.mPos - base.mFocus) * (1.0 + llclamp(amount, 0.0, (F64)FSCameraLinks::TOUR_MAX_AMOUNT) / 100.0 * s);
                break;
            case TourMove::RISE:
            case TourMove::SINK:
            {
                const F64 dz = amount * s * (stop.mMove == TourMove::RISE ? 1.0 : -1.0);
                pose.mPos.mdV[VZ] += dz;
                pose.mFocus.mdV[VZ] += dz;
                break;
            }
            default:
                break;
        }
        return pose;
    }

    // The view in global coordinates
    bool resolve_view(const FSCameraLinks::View& view, TourPose& pose)
    {
        pose.mFovRad = view.mFovDeg > 0.f ? view.mFovDeg * DEG_TO_RAD : 0.f;
        pose.mRollRad = view.mRollDeg * DEG_TO_RAD;
        if (const LLViewerObject* target = find_target(view.mTarget))
        {
            target_pose(target, view, pose.mPos, pose.mFocus);
            return rlv_allows_camera_at(pose.mPos);
        }
        LLViewerRegion* region = find_known_region(view.mRegion);
        if (!region || !view_in_reach(view, region))
        {
            return false;
        }
        const F32 width = region->getWidth();
        if (!valid_xy(view.mPos.mV[VX], width) || !valid_xy(view.mPos.mV[VY], width))
        {
            return false;
        }
        pose.mPos = region->getPosGlobalFromRegion(view.mPos);
        pose.mFocus = region->getPosGlobalFromRegion(view.mFocus);
        // out of the RLVa camera distance limits counts as out of reach
        return rlv_allows_camera_at(pose.mPos);
    }

    TourPose current_pose()
    {
        TourPose pose;
        current_camera(pose.mPos, pose.mFocus);
        pose.mFovRad = LLViewerCamera::getInstance()->getDefaultFOV();
        pose.mRollRad = gAgentCamera.getRollAngle();
        return pose;
    }

    void set_tour_camera(const TourPose& pose)
    {
        if (!sTourCameraTaken)
        {
            take_camera();
            sTourCameraTaken = true;
        }
        sTourLast = pose;
        place_camera(pose.mPos, pose.mFocus, pose.mRollRad, pose.mFovRad);
    }

    void end_tour()
    {
        if (!sTourActive)
        {
            return;
        }
        sTourActive = false;
        sTourTravelling = false;
        sTourStops.clear();
        sTourOrder.clear();
        sTourIndex = -1;
        sTourPlace = -1;
        ++sTourGeneration;
        if (sTourHidUI)
        {
            sTourHidUI = false;
            gViewerWindow->setUIVisibility(true);
        }
        sTourChanged();
    }

    S32 next_place(S32 place)
    {
        const S32 count = (S32)sTourOrder.size();
        if (place + 1 < count)
        {
            return place + 1;
        }
        return sTourOptions.mRepeat == FSCameraLinks::TourRepeat::ONCE ? -1 : 0;
    }

    // Starts travelling to the first stop in reach
    bool begin_leg(S32 place)
    {
        const S32 count = (S32)sTourOrder.size();
        for (S32 step = 0; step < count && place >= 0; ++step, place = next_place(place))
        {
            const S32 index = sTourOrder[place];
            TourPose pose;
            if (!resolve_view(sTourStops[index].mView, pose))
            {
                continue;
            }
            // from where the tour left the camera
            sTourHasBefore = sTourCameraTaken;
            sTourBefore = sTourFrom;
            sTourFrom = sTourCameraTaken ? sTourLast : current_pose();
            if (sTourFrom.mFovRad <= 0.f)
            {
                sTourFrom.mFovRad = current_pose().mFovRad;
            }
            sTourTo = pose;
            if (sTourTo.mFovRad <= 0.f)
            {
                sTourTo.mFovRad = sTourFrom.mFovRad;
            }

            // the curve's shape past this stop: the next stop in reach, or straight on at the end
            sTourAfter = sTourTo;
            sTourAfter.mPos = sTourTo.mPos + (sTourTo.mPos - sTourFrom.mPos);
            sTourAfter.mFocus = sTourTo.mFocus + (sTourTo.mFocus - sTourFrom.mFocus);
            for (S32 next = next_place(place), look = 0; next >= 0 && look < count; next = next_place(next), ++look)
            {
                TourPose after;
                if (resolve_view(sTourStops[sTourOrder[next]].mView, after))
                {
                    sTourAfter = after;
                    break;
                }
            }
            if (!sTourHasBefore)
            {
                sTourBefore = sTourFrom;
                sTourBefore.mPos = sTourFrom.mPos - (sTourTo.mPos - sTourFrom.mPos);
                sTourBefore.mFocus = sTourFrom.mFocus - (sTourTo.mFocus - sTourFrom.mFocus);
            }

            sTourPlace = place;
            sTourIndex = index;
            sTourTravelling = true;
            sTourTimer.reset();
            sTourChanged();
            return true;
        }
        return false;
    }

    bool tour_idle(U32 generation)
    {
        if (!sTourActive || generation != sTourGeneration)
        {
            return true;
        }
        // Something took the camera back (ML, ESC, Walking, etc.)
        if (sTourCameraTaken && (gAgentCamera.getFocusOnAvatar() || gAgentCamera.cameraMouselook()))
        {
            end_tour();
            return true;
        }
        if (!isAgentAvatarValid() || gAgent.getTeleportState() != LLAgent::TELEPORT_NONE)
        {
            end_tour();
            return true;
        }
        if (rlv_locks_camera())
        {
            LLNotificationsUtil::add("FSCameraLinkRlvCamera");
            end_tour();
            return true;
        }
        if (sTourIndex < 0 || sTourIndex >= (S32)sTourStops.size())
        {
            end_tour();
            return true;
        }

        const F32 elapsed = sTourTimer.getElapsedTimeF32();
        const FSCameraLinks::TourStop& stop = sTourStops[sTourIndex];
        if (stop.mView.hasTarget())
        {
            if (const LLViewerObject* target = find_target(stop.mView.mTarget))
            {
                target_pose(target, stop.mView, sTourTo.mPos, sTourTo.mFocus);
            }
        }
        const TourPose to = sTourTo;
        if (sTourTravelling)
        {
            const F32 u = stop.mTravel > 0.f ? llclamp(elapsed / stop.mTravel, 0.f, 1.f) : 1.f;
            const F32 s = tour_ease(stop.mEase, u);
            TourPose pose;
            if (sTourOptions.mCurved)
            {
                pose.mPos = catmull_rom(sTourBefore.mPos, sTourFrom.mPos, to.mPos, sTourAfter.mPos, s);
                pose.mFocus = catmull_rom(sTourBefore.mFocus, sTourFrom.mFocus, to.mFocus, sTourAfter.mFocus, s);
            }
            else
            {
                pose.mPos = sTourFrom.mPos + (to.mPos - sTourFrom.mPos) * (F64)s;
                pose.mFocus = sTourFrom.mFocus + (to.mFocus - sTourFrom.mFocus) * (F64)s;
            }
            pose.mFovRad = sTourFrom.mFovRad + (to.mFovRad - sTourFrom.mFovRad) * s;
            pose.mRollRad = sTourFrom.mRollRad + shortest_turn(sTourFrom.mRollRad, to.mRollRad) * s;
            set_tour_camera(pose);
            if (u >= 1.f)
            {
                sTourTravelling = false;
                sTourTimer.reset();
            }
            return false;
        }

        const bool moves = stop.mMove != FSCameraLinks::TourMove::NONE && stop.mAmount > 0.f;
        if (elapsed < stop.mHold)
        {
            if (moves)
            {
                set_tour_camera(hold_pose(to, stop, elapsed / stop.mHold));
            }
            else if (stop.mView.hasTarget())
            {
                set_tour_camera(to);    // keeps up with the target
            }
            return false;
        }
        if (moves && stop.mHold > 0.f)
        {
            set_tour_camera(hold_pose(to, stop, 1.f)); // end of a move
        }
        const S32 next = next_place(sTourPlace);
        if (next < 0 || !begin_leg(next))
        {
            // End of a tour stays in place until camera is taken back.
            end_tour();
            return true;
        }
        return false;
    }
}

namespace FSCameraLinks
{
    using namespace FSCameraTour;

    std::string tourMoveName(TourMove move)
    {
        switch (move)
        {
            case TourMove::ORBIT_LEFT:  return "orbit_left";
            case TourMove::ORBIT_RIGHT: return "orbit_right";
            case TourMove::PAN_LEFT:    return "pan_left";
            case TourMove::PAN_RIGHT:   return "pan_right";
            case TourMove::PUSH_IN:     return "push_in";
            case TourMove::PULL_OUT:    return "pull_out";
            case TourMove::RISE:        return "rise";
            case TourMove::SINK:        return "sink";
            default:                    return "none";
        }
    }

    bool parseTourMove(const std::string& name, TourMove& move)
    {
        static const TourMove ALL[] = { TourMove::NONE, TourMove::ORBIT_LEFT, TourMove::ORBIT_RIGHT, TourMove::PAN_LEFT,
                                        TourMove::PAN_RIGHT, TourMove::PUSH_IN, TourMove::PULL_OUT, TourMove::RISE, TourMove::SINK };
        for (TourMove candidate : ALL)
        {
            if (tourMoveName(candidate) == name)
            {
                move = candidate;
                return true;
            }
        }
        return false;
    }

    F32 defaultTourMoveAmount(TourMove move)
    {
        switch (move)
        {
            case TourMove::ORBIT_LEFT:
            case TourMove::ORBIT_RIGHT:
            case TourMove::PAN_LEFT:
            case TourMove::PAN_RIGHT:   return 30.f;
            case TourMove::PUSH_IN:
            case TourMove::PULL_OUT:    return 25.f;
            case TourMove::RISE:
            case TourMove::SINK:        return 2.f;
            default:                    return 0.f;
        }
    }

    std::string tourEaseName(TourEase ease)
    {
        switch (ease)
        {
            case TourEase::LINEAR:      return "linear";
            case TourEase::EASE_IN:     return "ease_in";
            case TourEase::EASE_OUT:    return "ease_out";
            default:                    return "smooth";
        }
    }

    bool parseTourEase(const std::string& name, TourEase& ease)
    {
        static const TourEase ALL[] = { TourEase::SMOOTH, TourEase::LINEAR, TourEase::EASE_IN, TourEase::EASE_OUT };
        for (TourEase candidate : ALL)
        {
            if (tourEaseName(candidate) == name)
            {
                ease = candidate;
                return true;
            }
        }
        return false;
    }

    std::string tourRepeatName(TourRepeat repeat)
    {
        switch (repeat)
        {
            case TourRepeat::LOOP:      return "loop";
            case TourRepeat::BOUNCE:    return "bounce";
            default:                    return "once";
        }
    }

    bool parseTourRepeat(const std::string& name, TourRepeat& repeat)
    {
        static const TourRepeat ALL[] = { TourRepeat::ONCE, TourRepeat::LOOP, TourRepeat::BOUNCE };
        for (TourRepeat candidate : ALL)
        {
            if (tourRepeatName(candidate) == name)
            {
                repeat = candidate;
                return true;
            }
        }
        return false;
    }

    bool startTour(const std::vector<TourStop>& stops, size_t first, const TourOptions& options)
    {
        stopTour();
        if (stops.empty() || !gAgent.getRegion() || !isAgentAvatarValid())
        {
            return false;
        }
        if (rlv_locks_camera())
        {
            LLNotificationsUtil::add("FSCameraLinkRlvCamera");
            return false;
        }

        sTourOptions = options;
        sTourOptions.mSpeed = llclamp(options.mSpeed, FSCameraLinks::TOUR_MIN_SPEED, FSCameraLinks::TOUR_MAX_SPEED);
        sTourStops = stops;
        for (TourStop& stop : sTourStops)
        {
            stop.mTravel = llclamp(stop.mTravel, 0.f, FSCameraLinks::TOUR_MAX_TRAVEL) / sTourOptions.mSpeed;
            stop.mHold = llclamp(stop.mHold, 0.f, FSCameraLinks::TOUR_MAX_HOLD) / sTourOptions.mSpeed;
            stop.mAmount = llclamp(stop.mAmount, 0.f, FSCameraLinks::TOUR_MAX_AMOUNT);
        }
        sTourSkipped = 0;
        for (const TourStop& stop : stops)
        {
            TourPose pose;
            if (!resolve_view(stop.mView, pose))
            {
                ++sTourSkipped;
            }
        }
        if (sTourSkipped == (S32)stops.size())
        {
            sTourStops.clear();
            LLNotificationsUtil::add("FSCameraTourNoneInReach");
            return false;
        }

        // Play order
        const S32 count = (S32)sTourStops.size();
        sTourOrder.clear();
        for (S32 i = 0; i < count; ++i)
        {
            sTourOrder.push_back(i);
        }
        if (sTourOptions.mRepeat == TourRepeat::BOUNCE)
        {
            for (S32 i = count - 2; i > 0; --i)
            {
                sTourOrder.push_back(i);
            }
        }

        // A view waiting for a teleport
        sPendingActive = false;
        stop_follow();
        sTourActive = true;
        sTourCameraTaken = false;
        sTourHasBefore = false;
        const U32 generation = ++sTourGeneration;
        const S32 start = (S32)llmin(first, stops.size() - 1);
        if (!begin_leg(start) && !(start > 0 && begin_leg(0)))
        {
            end_tour();
            return false;
        }
        if (sTourOptions.mHideUI && gViewerWindow->getUIVisibility())
        {
            LLNotificationsUtil::add("FSCameraTourInterfaceHidden");
            gViewerWindow->setUIVisibility(false);
            sTourHidUI = true;
        }
        doOnIdleRepeating([generation]() { return tour_idle(generation); });
        return true;
    }

    void stopTour()
    {
        end_tour();
    }

    bool isFollowingLink(const std::string& link)
    {
        return sFollowActive && sFollowLink == link;
    }

    void releaseCamera()
    {
        end_tour();
        stop_follow();
    }

    bool isTourPlaying()
    {
        return sTourActive;
    }

    S32 getTourIndex()
    {
        return sTourActive ? sTourIndex : -1;
    }

    S32 getTourSkipped()
    {
        return sTourSkipped;
    }

    boost::signals2::connection setTourChangedCallback(const std::function<void()>& cb)
    {
        return sTourChanged.connect(cb);
    }
}
