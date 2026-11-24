/**
 * @file fscameralinks.h
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

#ifndef FS_CAMERALINKS_H
#define FS_CAMERALINKS_H

#include "lluuid.h"
#include "v3math.h"

#include <boost/signals2.hpp>

#include <functional>
#include <string>
#include <vector>

class LLSD;

// Camera links. A link of the form
//   secondlife:///app/camera/<Region>/<x>/<y>/<z>?focus=<fx>,<fy>,<fz>&fov=<degrees>&roll=<degrees>
// holds a camera position and focus point in region coordinates. Clicking it moves the clicker's
// camera there like an alt-click focus would (Esc or walking brings the camera back).
// The fov and roll parts are optional. Including tp in the querystrings also marks where the avatar
// stood when taking the link. This position is used in case a user must be teleported to view the link.
//
// A link can also follow an object or avatar:
//   ...&target=<uuid>&toff=<x>,<y>,<z>&tfocus=<x>,<y>,<z>[&turn=1]
// The camera then keeps its place relative to the target and follows it as it moves.
namespace FSCameraLinks
{
    struct View
    {
        std::string mRegion;    // region name, unescaped (while sim-provided regions are safe, usermade links can contain garbage for region names)
        LLVector3   mPos;       // camera position, region coordinates
        LLVector3   mFocus;     // point the camera looks at, region coordinates
        F32         mFovDeg = 0.f;  // vertical field of view in degrees, 0 to leave it as it is
        F32         mRollDeg = 0.f; // camera roll in degrees, about the view direction
        LLVector3   mAvatarPos;             // where the avatar stood, region coordinates, to teleport to
        bool        mHasAvatarPos = false;

        LLUUID      mTarget;                // object or avatar followed, null for none
        LLVector3   mTargetOffset;          // camera from the target
        LLVector3   mTargetFocusOffset;     // focus point from the target
        bool        mTargetTurns = false;   // the offsets are in the target's frame and turn with it

        bool hasTarget() const { return mTarget.notNull(); }
    };

    // Parse the path tokens (region, x, y, z) and query (focus, fov, roll, target...)
    // False when anything is missing or coords are invalid.
    bool parse(const LLSD& tokens, const LLSD& query_map, View& view);

    // Parse a whole secondlife:///app/camera/... link
    bool parseLink(const std::string& link, View& view);

    // The link for a view
    std::string buildLink(const View& view);

    bool buildCheckedLink(const View& view, std::string& link);

    // The current camera as a view, with its field of view and roll.
    // False if the view cannot be generated.
    bool getCurrentView(View& view);

    bool getCurrentFollowView(View& view, bool turn_with_target, std::string* target_name = nullptr);

    // Follow a given link
    void activate(const View& view);

    // Copy a link to the current camera view to the clipboard (menu item)
    void copyCurrentViewLink();

    // Whether the camera is already following the target of this link
    bool isFollowingLink(const std::string& link);

    // Unlocks a camera from a tour or a target.
    void releaseCamera();

    // The cpcampos chat command's text: "<x, y, z>|<fx, fy, fz>|roll",
    struct CamPos
    {
        LLVector3   pos;
        LLVector3   focus;
        bool        has_focus = false;
        F32         roll = 0.f;
        bool        has_roll = false;
    };
    // A whole string as a finite number, surrounding spaces allowed
    bool parseNumber(std::string text, F32& value);

    bool parseCamPos(const std::string& text, CamPos& out);
    std::string formatCamPos(const LLVector3& pos, const LLVector3& focus, F32 roll);

    // Action names for what a camera does while stopped
    enum class TourMove
    {
        NONE,
        ORBIT_LEFT,
        ORBIT_RIGHT,
        PAN_LEFT,
        PAN_RIGHT,
        PUSH_IN,
        PULL_OUT,
        RISE,
        SINK,
    };
    std::string tourMoveName(TourMove move);
    bool parseTourMove(const std::string& name, TourMove& move);
    F32 defaultTourMoveAmount(TourMove move);

    // Movement method names for a tour
    enum class TourEase
    {
        SMOOTH,
        LINEAR,
        EASE_IN,
        EASE_OUT,
    };
    std::string tourEaseName(TourEase ease);
    bool parseTourEase(const std::string& name, TourEase& ease);

    struct TourStop
    {
        View        mView;
        F32         mTravel = 4.f;      // seconds flying to it from the stop before
        F32         mHold = 5.f;        // seconds there
        TourMove    mMove = TourMove::NONE;
        F32         mAmount = 0.f;      // of the move: degrees, percent or metres
        TourEase    mEase = TourEase::SMOOTH;
    };

    // Repeat modes for tours
    enum class TourRepeat
    {
        ONCE,
        LOOP,
        BOUNCE,
    };
    std::string tourRepeatName(TourRepeat repeat);
    bool parseTourRepeat(const std::string& name, TourRepeat& repeat);

    // Limits of tour stops and options, shared by playback and the bookmarks floater
    constexpr F32 TOUR_MAX_TRAVEL = 60.f;       // seconds
    constexpr F32 TOUR_MAX_HOLD = 600.f;        // seconds
    constexpr F32 TOUR_MAX_AMOUNT = 1000.f;     // degrees, percent or metres
    constexpr F32 TOUR_MIN_SPEED = 0.1f;
    constexpr F32 TOUR_MAX_SPEED = 10.f;

    struct TourOptions
    {
        TourRepeat  mRepeat = TourRepeat::ONCE;
        bool        mCurved = false;    // a smooth curve through the stops instead of straight lines
        F32         mSpeed = 1.f;       // playback speed (multiplies all stops and motions)
        bool        mHideUI = false;
    };

    // Camera tour helpers
    bool startTour(const std::vector<TourStop>& stops, size_t first, const TourOptions& options);
    void stopTour();
    bool isTourPlaying();
    S32 getTourIndex();
    S32 getTourSkipped();
    boost::signals2::connection setTourChangedCallback(const std::function<void()>& cb);
}

#endif // FS_CAMERALINKS_H
