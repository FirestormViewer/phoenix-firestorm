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
