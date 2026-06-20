/**
 * @file fscombathitmarker.h
 * @brief Hitmarker and hit report for outgoing Combat 2.0 damage: a marker
 *        flash at the crosshair (white on hit, red on kill), an optional
 *        fading hit report line, hit/kill sounds, and a custom composite
 *        crosshair (dot + arrows). Events arrive owner-filtered from the
 *        LSL bridge combat log listener.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
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
 * $/LicenseInfo$
 */

#ifndef FS_COMBATHITMARKER_H
#define FS_COMBATHITMARKER_H

#include "llsd.h"
#include "lluuid.h"
#include "v3math.h"
#include "v4color.h"

class FSCombatHitMarker
{
public:
    // Raw combat event JSON forwarded by the LSL bridge; parses and
    // routes through handleCombatEvent.
    static void handleBridgeEvent(const std::string& json);

    // A parsed combat log event (DAMAGE or DEATH). Ignores everything
    // that is not the agent's own outgoing damage to another avatar.
    static void handleCombatEvent(const LLSD& event);

    // Per-frame overlay: hitmarker flash and fading hit report text.
    // Renders in any camera mode. Called from LLViewerWindow::draw.
    static void draw();

    // The custom composite crosshair (dot + arrows). Called from
    // LLToolGun::draw in place of the stock crosshair when
    // FSCustomCrosshair is enabled.
    static void drawCrosshair(S32 view_width, S32 view_height);

    // Crosshair tint, set per-frame by the combat features target
    // identification (contact set / group color of the avatar under the
    // crosshair). White when nothing is identified. Only applied when
    // FSCrosshairTargetColor is enabled.
    static void setCrosshairTint(const LLColor4& color);

    // Play the hit/kill sound at the current FSHitMarkerSoundGain, bypassing the
    // master "Play sounds" toggle, so the floater's Test buttons can preview
    // levels even with sounds otherwise off.
    static void playHitSoundPreview();
    static void playKillSoundPreview();

    // OTS convergence target: the world point under the render
    // camera's crosshair that a bullet fired from the avatar's eye should aim
    // at. World geometry comes from a ray cast; nearby avatars are converged via
    // cheap vertical-capsule proxies (so aiming at a target lands on the target,
    // not the wall behind it, without the rigged-mesh pick cost), the shooter's
    // own body excluded. The depth is smoothed along the ray so it eases rather
    // than snaps. Shared by the reported eye camera (send_agent_update) and the
    // line-of-sight (LOS) dot so the two cannot drift apart; smoothing is advanced once per
    // frame so both callers receive the same value. Tunables: FSOTSAvatarConverge,
    // FSOTSAvatarConvergeRadius, FSOTSConvergeMinDistance, FSOTSConvergeSmoothingHalfLife.
    static LLVector3 getOTSConvergenceTarget(const LLVector3& cam_origin,
                                             const LLVector3& cam_at);

    // Draw the last OTS convergence solve as 3D debug lines (camera ray, bullet
    // ray, the converged point, and each nearby avatar capsule lit when detected
    // under the crosshair). Called from LLPipeline::renderDebug when
    // FSOTSConvergeDebugDraw is enabled (Develop > Rendering menu toggle).
    static void renderOTSConvergenceDebug();

    // --- Damage type symbol table (for the symbols editor) ---------------

    struct DamageTypeInfo
    {
        S32         mType;          // combat log damage type number
        const char* mName;          // display name
        const char* mDefaultEmoji;  // built-in symbol
    };
    static S32 getDamageTypeCount();
    static const DamageTypeInfo& getDamageTypeInfo(S32 index);

    // Effective symbol for a type (override if present, else default).
    static std::string getEmojiForType(S32 type);
    // Set (or clear with an empty string) the user override for a type;
    // persists into the FSHitMarkerEmojiMap setting.
    static void setEmojiOverride(S32 type, const std::string& symbol);
    static void clearEmojiOverrides();
};

#endif // FS_COMBATHITMARKER_H
