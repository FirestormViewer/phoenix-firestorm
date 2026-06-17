/**
 * @file fsfloaterkillfeed.cpp
 * @brief Kill feed: Combat 2.0 DEATH events shown as a text-only screen
 *        overlay, fed by the LSL bridge combat log listener.
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

#include "llviewerprecompiledheaders.h"

#include "fsfloaterkillfeed.h"

#include "fscombathitmarker.h"
#include "llagent.h"
#include "llavatarnamecache.h"
#include "llfloaterreg.h"
#include "llrender.h"
#include "llsdjson.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "lggcontactsets.h"
#include "llgroupcolormap.h"
#include "llnetmap.h"
#include "llviewerobjectlist.h"
#include "llvoavatar.h"

#include <boost/json.hpp>

static const std::string KILLFEED_PREFIX = "SSKF|";
static const size_t MAX_ENTRIES = 200;

FSFloaterKillFeed::entries_t FSFloaterKillFeed::sEntries;

FSFloaterKillFeed::FSFloaterKillFeed(const LLSD& key)
:   LLFloater(key)
{
}

bool FSFloaterKillFeed::postBuild()
{
    return true;
}

// static
bool FSFloaterKillFeed::handleChatMessage(const std::string& mesg, const LLUUID& from_id, const LLUUID& owner_id)
{
    if (mesg.compare(0, KILLFEED_PREFIX.size(), KILLFEED_PREFIX) != 0)
    {
        return false;
    }

    // The relay is a worn attachment; owner-say only reaches its owner,
    // but be explicit anyway.
    if (owner_id != gAgent.getID())
    {
        return true;
    }

    boost::system::error_code ec;
    boost::json::value jv = boost::json::parse(mesg.substr(KILLFEED_PREFIX.size()), ec);
    if (!ec && jv.is_object())
    {
        addEntry(LlsdFromJson(jv));
    }
    return true;
}

// static
void FSFloaterKillFeed::handleBridgeEvent(const std::string& json)
{
    boost::system::error_code ec;
    boost::json::value jv = boost::json::parse(json, ec);
    if (!ec && jv.is_object())
    {
        addEntry(LlsdFromJson(jv));
    }
}

// static
void FSFloaterKillFeed::onAvatarNameResolved(const LLUUID& id, const LLAvatarName& av_name)
{
    // Names are looked up from the cache each frame by the overlay; nothing
    // to do here, the resolved name appears on the next draw.
}

// Apply the FSKillFeedShortNames setting: "Orion Starchild" becomes
// "Orion S.". Used for real names and the settings-panel example alike.
static std::string killfeed_maybe_shorten(std::string name)
{
    static LLCachedControl<bool> short_names(gSavedSettings, "FSKillFeedShortNames", true);
    if (short_names)
    {
        size_t last_space = name.find_last_of(' ');
        if (last_space != std::string::npos && last_space + 1 < name.size())
        {
            name = name.substr(0, name.find_first_of(' ') + 1) + name[last_space + 1] + ".";
        }
    }
    return name;
}

// Avatar display name (shortening setting applied), or a resolving
// placeholder. When allow_request is true a cache miss fires an async
// lookup; the per-frame overlay passes false since lookups are kicked
// once when the entry arrives.
static std::string killfeed_name(const LLUUID& id, bool allow_request = true)
{
    if (id.isNull())
    {
        return "(world)";
    }
    LLAvatarName av_name;
    if (LLAvatarNameCache::get(id, &av_name))
    {
        return killfeed_maybe_shorten(av_name.getDisplayName());
    }
    if (allow_request)
    {
        LLAvatarNameCache::get(id, boost::bind(&FSFloaterKillFeed::onAvatarNameResolved, _1, _2));
    }
    return "(resolving)";
}

// static
void FSFloaterKillFeed::addEntry(const LLSD& event)
{
    if (event["event"].asString() != "DEATH")
    {
        return;
    }

    KillEntry entry;
    entry.mVictim = event["target"].asUUID();
    entry.mKiller = event["owner"].asUUID();
    if (entry.mKiller.isNull())
    {
        entry.mKiller = event["source"].asUUID();
    }
    entry.mWeapon = event["weapon_name"].asString();
    // Damage type cached onto the DEATH by the bridge; -1 (Impact) is a
    // valid type, so the unknown sentinel sits outside the real range.
    entry.mType = event.has("type") ? (S32)event["type"].asInteger() : -1000;
    entry.mDamage = event.has("damage") ? (F32)event["damage"].asReal() : -1.f;

    entry.mDistance = -1.f;
    const LLSD& spos = event["source_pos"];
    const LLSD& tpos = event["target_pos"];
    if (spos.isArray() && spos.size() >= 3 && tpos.isArray() && tpos.size() >= 3)
    {
        LLVector3 sv((F32)spos[0].asReal(), (F32)spos[1].asReal(), (F32)spos[2].asReal());
        LLVector3 tv((F32)tpos[0].asReal(), (F32)tpos[1].asReal(), (F32)tpos[2].asReal());
        entry.mDistance = (tv - sv).length();
    }

    entry.mReceivedTime = LLFrameTimer::getTotalSeconds();

    // Warm the name cache now so the overlay (which never fires requests)
    // has names by the time it draws.
    killfeed_name(entry.mVictim);
    killfeed_name(entry.mKiller);

    sEntries.push_back(entry);
    while (sEntries.size() > MAX_ENTRIES)
    {
        sEntries.pop_front();
    }
}

struct KFSegment
{
    std::string text;
    LLColor4    color;
};

// Resolve a kill-feed name color. Your own name uses your chosen color when
// FSKillFeedColorOwnName is on (kill feed only); everyone else - and you when
// that toggle is off - follows map-mark > contact-set > group > default.
static LLColor4 killfeed_name_color(const LLUUID& id, const LLColor4& default_color)
{
    if (id.isNull())
    {
        return default_color;
    }

    if (id == gAgent.getID())
    {
        static LLCachedControl<bool> color_own(gSavedSettings, "FSKillFeedColorOwnName", false);
        if (color_own)
        {
            static LLCachedControl<LLColor4> own_color(gSavedSettings, "FSKillFeedOwnNameColor", LLColor4(1.f, 0.9f, 0.4f, 1.f));
            return own_color;
        }
    }

    LLColor4 c;
    if (LLNetMap::getAvatarMarkColor(id, c))
    {
        return c; // map mark
    }
    if (LGGContactSets::instance().hasFriendColorThatShouldShow(id, ContactSetType::MINIMAP, c))
    {
        return c; // contact set
    }
    LLViewerObject* obj = gObjectList.findObject(id);
    LLVOAvatar* av = obj ? obj->asAvatar() : nullptr;
    if (av && av->getActiveGroupID().notNull())
    {
        LLColor4 g = LLGroupColorMap::getInstance()->getGroupColor(av->getActiveGroupID());
        if (g.mV[VW] >= 0.01f)
        {
            return g; // group
        }
    }
    return default_color;
}

// "Killer killed Victim with [symbol] Weapon (100dmg, 50m)" split into colored
// segments so the killer and victim names can be tinted independently of the
// surrounding text (which uses default_color).
static std::vector<KFSegment> killfeed_segments(const FSFloaterKillFeed::KillEntry& entry, const LLColor4& default_color)
{
    std::vector<KFSegment> segs;
    segs.push_back({ killfeed_name(entry.mKiller, false), killfeed_name_color(entry.mKiller, default_color) });
    segs.push_back({ " killed ", default_color });
    segs.push_back({ killfeed_name(entry.mVictim, false), killfeed_name_color(entry.mVictim, default_color) });

    std::string tail;

    // Damage type symbol from the shared hit marker symbol table
    // (customizable in the symbols editor).
    static LLCachedControl<bool> show_emoji(gSavedSettings, "FSKillFeedShowEmoji", true);
    std::string symbol;
    if (show_emoji && entry.mType > -1000)
    {
        symbol = FSCombatHitMarker::getEmojiForType(entry.mType);
    }

    if (!entry.mWeapon.empty() || !symbol.empty())
    {
        tail += " with";
        if (!symbol.empty())
        {
            tail += " " + symbol;
        }
        if (!entry.mWeapon.empty())
        {
            tail += " " + entry.mWeapon;
        }
    }

    std::string stats;
    if (entry.mDamage >= 0.f)
    {
        stats = llformat("%.0fdmg", entry.mDamage);
    }
    if (entry.mDistance >= 0.f)
    {
        if (!stats.empty())
        {
            stats += ", ";
        }
        stats += llformat("%.0fm", entry.mDistance);
    }
    if (!stats.empty())
    {
        tail += " (" + stats + ")";
    }

    if (!tail.empty())
    {
        segs.push_back({ tail, default_color });
    }
    return segs;
}

// static
void FSFloaterKillFeed::drawOverlay()
{
    static LLCachedControl<bool> enabled(gSavedSettings, "FSKillFeedEnabled", false);

    // The settings panel shows a live example so position, font and color
    // can be customized without waiting for someone to die.
    const bool panel_open = LLFloaterReg::instanceVisible("fs_kill_feed");
    if (!enabled && !panel_open)
    {
        return;
    }

    static LLCachedControl<F32> text_scale(gSavedSettings, "FSKillFeedTextScale", 1.0f);
    static LLCachedControl<F32> screen_x(gSavedSettings, "FSKillFeedScreenX", 0.25f);
    static LLCachedControl<F32> screen_y(gSavedSettings, "FSKillFeedScreenY", 0.9f);
    static LLCachedControl<U32> max_lines(gSavedSettings, "FSKillFeedMaxLines", 5);
    static LLCachedControl<F32> hold_time(gSavedSettings, "FSKillFeedHoldTime", 20.f);
    static LLCachedControl<std::string> font_name(gSavedSettings, "FSKillFeedFontName", "SansSerif");
    static LLCachedControl<LLColor4> text_color(gSavedSettings, "FSKillFeedTextColor", LLColor4::white);
    static LLCachedControl<bool> bold_text(gSavedSettings, "FSKillFeedBoldText", false);

    const F64 now = LLFrameTimer::getTotalSeconds();
    const LLColor4 default_color = text_color;

    // Collect the visible lines (newest first) as colored segments.
    std::vector<std::vector<KFSegment>> seglines;
    if (enabled)
    {
        for (entries_t::const_reverse_iterator it = sEntries.rbegin();
             it != sEntries.rend() && seglines.size() < (size_t)max_lines;
             ++it)
        {
            if (now - it->mReceivedTime > (F64)hold_time)
            {
                break; // entries are time-ordered; everything older follows
            }
            seglines.push_back(killfeed_segments(*it, default_color));
        }
    }

    if (seglines.empty())
    {
        if (!panel_open)
        {
            return;
        }
        // Placeholder preview while the settings panel is open; runs
        // through the same name shortening and symbol option as real lines.
        static LLCachedControl<bool> example_emoji(gSavedSettings, "FSKillFeedShowEmoji", true);
        std::string example_symbol;
        if (example_emoji)
        {
            example_symbol = FSCombatHitMarker::getEmojiForType(5) + " "; // Fire
        }
        const std::string example_line = killfeed_maybe_shorten("Example Resident") + " killed " +
            killfeed_maybe_shorten("Guy Linden") + " with " + example_symbol + "Death (100dmg, 0m)";
        std::vector<KFSegment> example_segs(1, KFSegment{ example_line, default_color });
        seglines.assign((size_t)llmax((U32)1, (U32)max_lines), example_segs);
    }

    const LLFontGL* font = LLFontGL::getFontSansSerif();
    if (font_name() == "SansSerifSmall")
    {
        font = LLFontGL::getFontSansSerifSmall();
    }
    else if (font_name() == "SansSerifBig")
    {
        font = LLFontGL::getFontSansSerifBig();
    }
    else if (font_name() == "SansSerifHuge")
    {
        font = LLFontGL::getFontSansSerifHuge();
    }
    else if (font_name() == "Monospace")
    {
        font = LLFontGL::getFontMonospace();
    }

    const U8 font_style = bold_text ? LLFontGL::BOLD : LLFontGL::NORMAL;

    const S32 view_width = gViewerWindow->getWorldViewRectScaled().getWidth();
    const S32 view_height = gViewerWindow->getWorldViewRectScaled().getHeight();
    const F32 line_height = (F32)(font->getLineHeight() + 2);
    const F32 scale = llclamp((F32)text_scale, 0.5f, 3.f);

    gGL.pushMatrix();
    gGL.translatef((F32)view_width * llclamp((F32)screen_x, 0.f, 1.f),
                   (F32)view_height * llclamp((F32)screen_y, 0.f, 1.f), 0.f);
    gGL.scalef(scale, scale, 1.f);

    F32 y = 0.f;
    for (const std::vector<KFSegment>& segs : seglines)
    {
        F32 x = 0.f;
        for (const KFSegment& seg : segs)
        {
            font->renderUTF8(seg.text, 0, x, y, seg.color,
                             LLFontGL::LEFT, LLFontGL::TOP, font_style, LLFontGL::DROP_SHADOW_SOFT);
            x += font->getWidthF32(seg.text);
        }
        y -= line_height;
    }

    gGL.popMatrix();
}
