/**
 * @file fsfloaterkillfeed.h
 * @brief Kill feed: Combat 2.0 DEATH events shown as a text-only screen
 *        overlay, fed by the LSL bridge combat log listener. This floater
 *        is the settings panel (enable, text size, position, lines, hold
 *        time); the overlay itself renders from LLViewerWindow::draw.
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

#ifndef FS_FLOATERKILLFEED_H
#define FS_FLOATERKILLFEED_H

#include "llfloater.h"

#include <deque>

class LLAvatarName;

class FSFloaterKillFeed : public LLFloater
{
public:
    FSFloaterKillFeed(const LLSD& key);
    virtual ~FSFloaterKillFeed() = default;

    bool postBuild() override;

    // Chat pipeline hook for CHAT_TYPE_OWNER messages. Returns true when
    // the message was a kill feed relay line (consumed and suppressed).
    // This is the standalone-relay path (doc/killfeed_relay.lsl) for users
    // who run with the LSL bridge disabled.
    static bool handleChatMessage(const std::string& mesg, const LLUUID& from_id, const LLUUID& owner_id);

    // DEATH event JSON forwarded by the Firestorm LSL bridge
    // (<bridgeKillFeed> tag); called from FSLSLBridge::lslToViewer.
    static void handleBridgeEvent(const std::string& json);

    // Text-only kill feed overlay, rendered every frame from
    // LLViewerWindow::draw when FSKillFeedEnabled is set. Position, scale,
    // line count and hold time come from saved settings.
    static void drawOverlay();

    // Async avatar name lookup completion (names appear on the next frame).
    static void onAvatarNameResolved(const LLUUID& id, const LLAvatarName& av_name);

    struct KillEntry
    {
        F64         mReceivedTime;  // LLFrameTimer::getTotalSeconds() at receipt
        LLUUID      mVictim;        // DEATH "target"
        LLUUID      mKiller;        // DEATH "owner" (falls back to "source")
        std::string mWeapon;        // relay-resolved "weapon_name", may be empty
        S32         mType;          // bridge-cached damage type, -1000 unknown
        F32         mDamage;        // killing blow damage, <0 unknown
        F32         mDistance;      // meters between source_pos/target_pos, <0 unknown
    };

private:
    typedef std::deque<KillEntry> entries_t;

    static entries_t sEntries;

    static void addEntry(const LLSD& event);
};

#endif // FS_FLOATERKILLFEED_H
