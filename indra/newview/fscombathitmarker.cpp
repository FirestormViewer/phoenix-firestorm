/**
 * @file fscombathitmarker.cpp
 * @brief Hitmarker and hit report for outgoing Combat 2.0 damage.
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

#include "fscombathitmarker.h"

#include "llagent.h"
#include "llagentcamera.h"
#include "llaudioengine.h"
#include "llavatarnamecache.h"
#include "llcriticaldamp.h"
#include "llframetimer.h"
#include "llrender.h"
#include "llsdjson.h"
#include "llui.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"
#include "pipeline.h"

#include <boost/json.hpp>

// Timing envelope (matches the user-tuned LSL original)
static const F64 BATCH_WINDOW   = 0.3;   // damage aggregation window
static const F64 FLASH_DURATION = 0.75;  // marker flash
static const F64 TEXT_HOLD      = 1.75;  // text fully opaque (flash + hold)
static const F64 TEXT_FADE      = 4.0;   // fade to zero after hold

// --- pending batch state --------------------------------------------------
static bool     sBatching = false;
static F64      sBatchStart = 0.0;
static F32      sBatchDamage = 0.f;
static S32      sBatchHits = 0;
static S32      sLastType = 0;
static LLUUID   sLastTarget;
static bool     sKillSeen = false;
static LLUUID   sKillTarget;

// --- active display state -------------------------------------------------
static F64          sFlashStart = -1000.0;
static bool         sFlashIsKill = false;
static F64          sTextStart = -1000.0;
static std::string  sText;

// Built-in damage type symbol defaults (user-curated set); type values and
// names per secondlife/lsl-definitions plus the community-adopted custom
// damage types 100-106 documented on the SL wiki's Combat 2.0 Damage Types
// page. Overridable via the FSHitMarkerEmojiMap setting (JSON, keyed by
// type number as a string), edited through the symbols editor floater.
static const FSCombatHitMarker::DamageTypeInfo DAMAGE_TYPES[] = {
    { -1, "Impact",      "\xF0\x9F\x92\xA5" },
    {  0, "Generic",     "\xF0\x9F\x92\xA5" },
    {  1, "Acid",        "\xF0\x9F\xA7\xAA" },
    {  2, "Bludgeoning", "\xF0\x9F\x94\xA8" },
    {  3, "Cold",        "\xE2\x9D\x84\xEF\xB8\x8F" },
    {  4, "Electric",    "\xE2\x9A\xA1\xEF\xB8\x8F" },
    {  5, "Fire",        "\xF0\x9F\x94\xA5" },
    {  6, "Force",       "\xE2\x98\x84\xEF\xB8\x8F" },
    {  7, "Necrotic",    "\xF0\x9F\x92\x80" },
    {  8, "Piercing",    "\xF0\x9F\x97\xA1" },
    {  9, "Poison",      "\xE2\x98\xA3\xEF\xB8\x8F" },
    { 10, "Psychic",     "\xF0\x9F\x94\xAE" },
    { 11, "Radiant",     "\xE2\x9C\xA8" },
    { 12, "Slashing",    "\xF0\x9F\x94\xAA" },
    { 13, "Sonic",       "\xF0\x9F\x94\x8A" },
    { 14, "Emotional",   "\xF0\x9F\x8E\xAD" },
    { 100, "Medical",     "\xF0\x9F\x92\x89" },
    { 101, "Repair",      "\xF0\x9F\x94\xA7" },
    { 102, "Explosive",   "\xF0\x9F\x92\xA3" },
    { 103, "Crushing",    "\xF0\x9F\xA4\x9C\xF0\x9F\xA4\x9B" },
    { 104, "Anti-Armor",  "\xF0\x9F\x92\xA5\xF0\x9F\x9B\xA1\xEF\xB8\x8F" },
    { 105, "Suffocation", "\xF0\x9F\x8C\xAC\xEF\xB8\x8F" },
    { 106, "Redeploy",    "\xF0\x9F\x92\xAA" },
};

// Damage type -> emoji, honoring the FSHitMarkerEmojiMap JSON override.
static std::string emoji_for_type(S32 type)
{
    static std::string last_raw = "\x01"; // force first parse
    static LLSD overrides;

    static LLCachedControl<std::string> map_setting(gSavedSettings, "FSHitMarkerEmojiMap", "");
    if (map_setting() != last_raw)
    {
        last_raw = map_setting();
        overrides = LLSD();
        if (!last_raw.empty())
        {
            boost::system::error_code ec;
            boost::json::value jv = boost::json::parse(last_raw, ec);
            if (!ec && jv.is_object())
            {
                overrides = LlsdFromJson(jv);
            }
        }
    }

    const std::string key = llformat("%d", type);
    if (overrides.has(key))
    {
        return overrides[key].asString();
    }
    for (const auto& entry : DAMAGE_TYPES)
    {
        if (entry.mType == type)
        {
            return entry.mDefaultEmoji;
        }
    }
    return llformat("type %d", type);
}

// static
S32 FSCombatHitMarker::getDamageTypeCount()
{
    return (S32)(sizeof(DAMAGE_TYPES) / sizeof(DAMAGE_TYPES[0]));
}

// static
const FSCombatHitMarker::DamageTypeInfo& FSCombatHitMarker::getDamageTypeInfo(S32 index)
{
    return DAMAGE_TYPES[llclamp(index, 0, getDamageTypeCount() - 1)];
}

// static
std::string FSCombatHitMarker::getEmojiForType(S32 type)
{
    return emoji_for_type(type);
}

// Read the override map setting as LLSD (empty map when unset/invalid).
static LLSD read_emoji_overrides()
{
    const std::string raw = gSavedSettings.getString("FSHitMarkerEmojiMap");
    if (!raw.empty())
    {
        boost::system::error_code ec;
        boost::json::value jv = boost::json::parse(raw, ec);
        if (!ec && jv.is_object())
        {
            return LlsdFromJson(jv);
        }
    }
    return LLSD::emptyMap();
}

// static
void FSCombatHitMarker::setEmojiOverride(S32 type, const std::string& symbol)
{
    LLSD overrides = read_emoji_overrides();
    const std::string key = llformat("%d", type);

    // Setting a symbol back to its built-in default clears the override.
    std::string default_symbol;
    for (const auto& entry : DAMAGE_TYPES)
    {
        if (entry.mType == type)
        {
            default_symbol = entry.mDefaultEmoji;
            break;
        }
    }

    if (symbol.empty() || symbol == default_symbol)
    {
        overrides.erase(key);
    }
    else
    {
        overrides[key] = symbol;
    }
    // Clearing the last override empties the setting entirely.
    std::string serialized;
    if (overrides.size() > 0)
    {
        serialized = boost::json::serialize(LlsdToJson(overrides));
    }
    gSavedSettings.setString("FSHitMarkerEmojiMap", serialized);
}

// static
void FSCombatHitMarker::clearEmojiOverrides()
{
    gSavedSettings.setString("FSHitMarkerEmojiMap", "");
}

static void hitmarker_name_resolved(const LLUUID& id, const LLAvatarName& av_name)
{
    // Names are read from the cache when the batch finalizes; nothing to do.
}

static std::string hitmarker_name(const LLUUID& id)
{
    if (id.isNull())
    {
        return "(unknown)";
    }
    LLAvatarName av_name;
    if (!LLAvatarNameCache::get(id, &av_name))
    {
        LLAvatarNameCache::get(id, boost::bind(&hitmarker_name_resolved, _1, _2));
        return "(resolving)";
    }
    std::string name = av_name.getDisplayName();
    static LLCachedControl<bool> short_names(gSavedSettings, "FSHitMarkerShortNames", true);
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

static void play_sound_setting(const char* setting_name)
{
    static LLCachedControl<bool> sounds_on(gSavedSettings, "FSHitMarkerSounds", true);
    if (!sounds_on || !gAudiop)
    {
        return;
    }
    LLUUID sound_id(gSavedSettings.getString(setting_name));
    if (sound_id.notNull())
    {
        gAudiop->triggerSound(sound_id, gAgent.getID(), 1.0f, LLAudioEngine::AUDIO_TYPE_UI);
    }
}

// Build the display text and arm the flash from the accumulated batch.
static void finalize_batch()
{
    sBatching = false;

    static LLCachedControl<bool> show_text(gSavedSettings, "FSHitMarkerText", true);
    static LLCachedControl<bool> show_name(gSavedSettings, "FSHitMarkerShowName", true);
    static LLCachedControl<bool> show_emoji(gSavedSettings, "FSHitMarkerShowEmoji", true);

    sFlashStart = LLFrameTimer::getTotalSeconds();
    sFlashIsKill = sKillSeen;

    if (show_text)
    {
        std::string text;
        if (sKillSeen)
        {
            text = "Killed";
            if (show_name)
            {
                text += " " + hitmarker_name(sKillTarget);
            }
        }
        else
        {
            if (show_name)
            {
                text = hitmarker_name(sLastTarget);
            }
            if (show_emoji)
            {
                if (!text.empty()) text += " ";
                text += emoji_for_type(sLastType);
            }
            if (!text.empty()) text += " ";
            text += llformat("(%.0fdmg", sBatchDamage);
            if (sBatchHits > 1)
            {
                text += llformat(", %d hits", sBatchHits);
            }
            text += ")";
        }
        sText = text;
        sTextStart = sFlashStart;
    }

    // Reset batch accumulators
    sBatchDamage = 0.f;
    sBatchHits = 0;
    sKillSeen = false;
    sKillTarget.setNull();
}

// static
void FSCombatHitMarker::handleBridgeEvent(const std::string& json)
{
    static LLCachedControl<bool> enabled(gSavedSettings, "FSHitMarkerEnabled", false);
    if (!enabled)
    {
        return;
    }
    boost::system::error_code ec;
    boost::json::value jv = boost::json::parse(json, ec);
    if (!ec && jv.is_object())
    {
        handleCombatEvent(LlsdFromJson(jv));
    }
}

// static
void FSCombatHitMarker::handleCombatEvent(const LLSD& event)
{
    static LLCachedControl<bool> enabled(gSavedSettings, "FSHitMarkerEnabled", false);
    if (!enabled)
    {
        return;
    }

    // Only the agent's own outgoing damage. The bridge already filters by
    // owner, but the kill feed's all-deaths stream comes through here too.
    if (event["owner"].asUUID() != gAgent.getID())
    {
        return;
    }

    const LLUUID target = event["target"].asUUID();
    if (target.isNull() || target == gAgent.getID())
    {
        return; // self-damage is not a "hit"
    }

    const std::string type = event["event"].asString();
    const F64 now = LLFrameTimer::getTotalSeconds();

    if (type == "DEATH")
    {
        sKillSeen = true;
        sKillTarget = target;
        play_sound_setting("FSHitMarkerKillSound");
        // Kills display immediately; no reason to wait out the batch window.
        finalize_batch();
    }
    else if (type == "DAMAGE")
    {
        sBatchDamage += (F32)event["damage"].asReal();
        ++sBatchHits;
        sLastTarget = target;
        sLastType = (S32)event["type"].asInteger();
        if (!sBatching)
        {
            sBatching = true;
            sBatchStart = now;
            // Sound feedback is instant; only the text waits for the batch.
            play_sound_setting("FSHitMarkerHitSound");
        }
    }
}

// static
void FSCombatHitMarker::draw()
{
    const F64 now = LLFrameTimer::getTotalSeconds();

    // Finalize a pending batch once its aggregation window closes.
    if (sBatching && now - sBatchStart >= BATCH_WINDOW)
    {
        finalize_batch();
    }

    static LLCachedControl<F32> scale_setting(gSavedSettings, "FSHitMarkerScale", 1.0f);
    const F32 scale = llclamp((F32)scale_setting, 0.5f, 3.f);
    const S32 view_width = gViewerWindow->getWorldViewRectScaled().getWidth();
    const S32 view_height = gViewerWindow->getWorldViewRectScaled().getHeight();
    const S32 center_x = view_width / 2;
    const S32 center_y = view_height / 2;

    // Hitmarker flash
    const F64 flash_t = now - sFlashStart;
    if (flash_t >= 0.0 && flash_t < FLASH_DURATION)
    {
        LLUIImagePtr marker = LLUI::getUIImage("FSHitMarker");
        if (marker)
        {
            const F32 alpha = 1.f - (F32)(flash_t / FLASH_DURATION);
            LLColor4 color = sFlashIsKill ? LLColor4(1.f, 0.f, 0.f, alpha) : LLColor4(1.f, 1.f, 1.f, alpha);
            const S32 w = (S32)(marker->getWidth() * scale);
            const S32 h = (S32)(marker->getHeight() * scale);
            marker->draw(center_x - w / 2, center_y - h / 2, w, h, color);
        }
    }

    // Fading hit report text (independent scale and weight)
    const F64 text_t = now - sTextStart;
    if (!sText.empty() && text_t >= 0.0 && text_t < TEXT_HOLD + TEXT_FADE)
    {
        static LLCachedControl<F32> text_scale_setting(gSavedSettings, "FSHitMarkerTextScale", 1.0f);
        static LLCachedControl<bool> text_bold(gSavedSettings, "FSHitMarkerTextBold", true);
        const F32 text_scale = llclamp((F32)text_scale_setting, 0.5f, 3.f);

        F32 alpha = 1.f;
        if (text_t > TEXT_HOLD)
        {
            alpha = 1.f - (F32)((text_t - TEXT_HOLD) / TEXT_FADE);
        }
        const LLFontGL* font = LLFontGL::getFontSansSerif();
        const U8 style = text_bold ? LLFontGL::BOLD : LLFontGL::NORMAL;
        LLColor4 color = sFlashIsKill ? LLColor4(1.f, 0.2f, 0.2f, alpha) : LLColor4(1.f, 1.f, 1.f, alpha);

        gGL.pushMatrix();
        gGL.translatef((F32)center_x, (F32)center_y - 32.f * scale, 0.f);
        gGL.scalef(text_scale, text_scale, 1.f);
        font->renderUTF8(sText, 0, 0.f, 0.f, color,
                         LLFontGL::HCENTER, LLFontGL::TOP, style, LLFontGL::DROP_SHADOW_SOFT);
        gGL.popMatrix();
    }
}

static LLColor4 sCrosshairTint = LLColor4::white;

// static
void FSCombatHitMarker::setCrosshairTint(const LLColor4& color)
{
    sCrosshairTint = color;
}

// Squared distance between segment [p1,q1] and segment [p2,q2], with the
// parametric closest points returned in s (along p1->q1) and t (along p2->q2).
// Standard clamped solution (Ericson, Real-Time Collision Detection). Used to
// test the crosshair ray against an avatar's capsule axis cheaply.
static F32 closestSegmentSegmentSq(const LLVector3& p1, const LLVector3& q1,
                                   const LLVector3& p2, const LLVector3& q2,
                                   F32& s, F32& t)
{
    const F32 EPS = 1e-6f;
    LLVector3 d1 = q1 - p1; // direction and length of segment 1
    LLVector3 d2 = q2 - p2; // direction and length of segment 2
    LLVector3 r  = p1 - p2;
    F32 a = d1 * d1; // squared length of segment 1 (dot)
    F32 e = d2 * d2; // squared length of segment 2 (dot)
    F32 f = d2 * r;

    if (a <= EPS && e <= EPS)
    {
        s = t = 0.f;
        return r * r;
    }
    if (a <= EPS)
    {
        s = 0.f;
        t = llclamp(f / e, 0.f, 1.f);
    }
    else
    {
        F32 c = d1 * r;
        if (e <= EPS)
        {
            t = 0.f;
            s = llclamp(-c / a, 0.f, 1.f);
        }
        else
        {
            F32 b = d1 * d2;
            F32 denom = a * e - b * b;
            s = (denom > EPS) ? llclamp((b * f - c * e) / denom, 0.f, 1.f) : 0.f;
            t = (b * s + f) / e;
            if (t < 0.f)
            {
                t = 0.f;
                s = llclamp(-c / a, 0.f, 1.f);
            }
            else if (t > 1.f)
            {
                t = 1.f;
                s = llclamp((b - c) / a, 0.f, 1.f);
            }
        }
    }
    LLVector3 c1 = p1 + d1 * s;
    LLVector3 c2 = p2 + d2 * t;
    LLVector3 diff = c1 - c2;
    return diff * diff;
}

// static
LLVector3 FSCombatHitMarker::getOTSConvergenceTarget(const LLVector3& cam_origin,
                                                     const LLVector3& cam_at)
{
    const F32 CONVERGE_RANGE = 512.f; // meters; open-sky fallback distance

    LLVector3 target = cam_origin + cam_at * CONVERGE_RANGE;

    LLVector4a ray_start, ray_end, hit;
    ray_start.load3(cam_origin.mV);
    ray_end.load3(target.mV);

    // World-geometry depth. pick_rigged stays false: rigged avatar meshes are
    // not the physical hitbox and per-triangle picking them every frame is the
    // cost we are avoiding. The cast returns terrain/prims; avatars are handled
    // by the cheap capsule pass below. Step past our own body/attachments, which
    // the shoulder ray can graze on its way out, so self never wins. With nothing
    // solid under the crosshair the far point stands.
    for (S32 i = 0; i < 4; ++i)
    {
        S32 face_hit = -1;
        LLViewerObject* obj = gPipeline.lineSegmentIntersectInWorld(
            ray_start, ray_end,
            false /*pick_transparent*/, false /*pick_rigged*/,
            false /*pick_unselectable*/, false /*pick_reflection_probe*/,
            &face_hit, nullptr, nullptr, &hit);
        if (!obj)
        {
            break;
        }
        const bool is_self = isAgentAvatarValid() &&
            (obj == gAgentAvatarp || obj->getAvatar() == gAgentAvatarp);
        if (is_self)
        {
            LLVector3 past(hit.getF32ptr());
            past += cam_at * 0.10f; // nudge just past the surface we grazed
            ray_start.load3(past.mV);
            continue;
        }
        target.set(hit.getF32ptr());
        break;
    }

    // Convergence depth measured along the camera ray (target is colinear with
    // cam_at, so the dot is its distance).
    F32 world_dist = (target - cam_origin) * cam_at;
    if (world_dist < 0.f)
    {
        world_dist = CONVERGE_RANGE;
    }

    // Lean guard: a convergence point right on top of the camera makes the
    // eye->target angle blow up (~atan(shoulder_offset / dist)), which IS the
    // shoulder-dependent swing. Clamp the *world* distance only; genuine avatar
    // hits below are trusted and exempt, so close targets still converge dead on.
    static LLCachedControl<F32> min_dist(gSavedSettings, "FSOTSConvergeMinDistance", 2.0f);
    F32 best_dist = llmax(world_dist, (F32)min_dist);

    // Cheap avatar convergence: approximate each nearby avatar as a vertical
    // capsule and take the nearest one the crosshair ray pierces. Restores "aim
    // converges on the player, not the wall behind them" without the rigged-mesh
    // cost, and a capsule is steadier than mesh edges (no frame-to-frame flicker
    // between a pauldron and the gap behind it). Taken as min() against the world
    // depth, so a wall in front of the target still wins (you cannot shoot through it).
    static LLCachedControl<bool> av_converge(gSavedSettings, "FSOTSAvatarConverge", true);
    if (av_converge)
    {
        static LLCachedControl<F32> av_radius(gSavedSettings, "FSOTSAvatarConvergeRadius", 0.5f);
        const F32 radius = llmax(0.1f, (F32)av_radius);
        const F32 HALF_HEIGHT = 1.0f; // capsule half-extent about the render position
        const LLVector3 cam_far = cam_origin + cam_at * CONVERGE_RANGE;

        for (LLCharacter* character : LLCharacter::sInstances)
        {
            LLVOAvatar* avatar = (LLVOAvatar*)character;
            if (!avatar || avatar->isDead() || avatar->isControlAvatar() || avatar->isSelf())
            {
                continue;
            }

            const LLVector3 center = avatar->getRenderPosition();
            const LLVector3 to_av = center - cam_origin;
            if (to_av * cam_at < 0.f)
            {
                continue; // behind the camera
            }
            if (to_av * to_av > CONVERGE_RANGE * CONVERGE_RANGE)
            {
                continue; // out of range
            }

            LLVector3 p0 = center; p0.mV[VZ] -= HALF_HEIGHT;
            LLVector3 p1 = center; p1.mV[VZ] += HALF_HEIGHT;

            F32 s_ray = 0.f, t_seg = 0.f;
            const F32 d2 = closestSegmentSegmentSq(cam_origin, cam_far, p0, p1, s_ray, t_seg);
            if (d2 <= radius * radius)
            {
                const F32 av_dist = s_ray * CONVERGE_RANGE; // closest-approach depth
                if (av_dist > 0.1f && av_dist < best_dist)
                {
                    best_dist = av_dist; // trusted target; exempt from the min clamp
                }
            }
        }
    }

    // Smooth the convergence DEPTH (a scalar along the ray), not the 3D point, so
    // the target stays exactly under the crosshair while its depth eases across
    // discontinuities instead of snapping, and rough-geometry jitter stops
    // shaking the aim. Advanced once per frame: both callers (send_agent_update
    // and the true-aim dot) invoke this every frame and must get the same value,
    // so we key the lerp on the frame counter and reuse the result within a frame.
    static LLCachedControl<F32> smooth_hl(gSavedSettings, "FSOTSConvergeSmoothingHalfLife", 0.06f);
    if ((F32)smooth_hl > 0.f)
    {
        static U32 last_frame = 0;
        static F32 smoothed_dist = CONVERGE_RANGE;
        const U32 frame = LLFrameTimer::getFrameCount();
        if (frame != last_frame)
        {
            last_frame = frame;
            const F32 amt = LLSmoothInterpolation::getInterpolant((F32)smooth_hl);
            smoothed_dist = lerp(smoothed_dist, best_dist, amt);
        }
        best_dist = smoothed_dist;
    }

    return cam_origin + cam_at * best_dist;
}

// static
void FSCombatHitMarker::drawCrosshair(S32 view_width, S32 view_height)
{
    static LLCachedControl<F32> scale_setting(gSavedSettings, "FSHitMarkerScale", 1.0f);
    const F32 scale = llclamp((F32)scale_setting, 0.5f, 3.f);

    LLUIImagePtr dot = LLUI::getUIImage("FSCrosshairDot");
    LLUIImagePtr arrows = LLUI::getUIImage("FSCrosshairArrows");

    // Optional target-identification tint (contact set / group color of the
    // avatar under the crosshair, fed by the combat features pass).
    static LLCachedControl<bool> target_color(gSavedSettings, "FSCrosshairTargetColor", false);
    const LLColor4 color = target_color ? sCrosshairTint : LLColor4::white;

    // True-impact dot: the chevrons mark the camera ray, but a bullet
    // travels from the avatar (which is what the sim is told), so near
    // cover the two diverge in OTS. Cast the bullet's ray (eye origin,
    // camera direction) against world geometry and slide the dot to where
    // it actually lands. With nothing hit (open sky) the rays agree at
    // infinity and the dot rests at center, so there is always a dot to
    // aim with.
    F32 offset_x = 0.f;
    F32 offset_y = 0.f;
    static LLCachedControl<bool> true_aim(gSavedSettings, "FSOTSTrueAimDot", true);
    if (true_aim && gAgentCamera.cameraOTS() && isAgentAvatarValid() && gAgentAvatarp->mHeadp
        && gAgent.getTeleportState() == LLAgent::TELEPORT_NONE) // never raycast mid-teleport
    {
        const F32 TRUE_AIM_RANGE = 256.f; // meters; past this the parallax is subpixel
        LLViewerCamera* camera = LLViewerCamera::getInstance();
        const LLVector3 eye = gAgentAvatarp->mHeadp->getWorldPosition();
        LLVector4a ray_start, ray_end, hit;

        // Bullet direction mirrors what the sim is told. With the
        // fair-fire camera on, scripts see the eye aimed at the camera
        // ray's world target (converged); off, they see the real camera's
        // at-axis (parallel from the avatar).
        LLVector3 dir = camera->getAtAxis();
        LLVector3 aim_end = eye + dir * TRUE_AIM_RANGE; // parallel (true-fire) fallback end
        static LLCachedControl<bool> eye_camera(gSavedSettings, "FSOTSReportEyeCamera", true);
        if (eye_camera)
        {
            // Mirror the reported eye camera exactly (send_agent_update uses
            // the same helper): aim from the eye at whatever is under the
            // crosshair, avatars included.
            const LLVector3 target = getOTSConvergenceTarget(camera->getOrigin(), camera->getAtAxis());
            LLVector3 converged = target - eye;
            if (converged.normalize() > 0.f)
            {
                dir = converged;
                aim_end = target; // stop the cover test at the target, not past it
            }
        }

        // Blocked-shot warning: solid world geometry between the eye and the
        // aim point is cover the camera sees over (avatars never block, so
        // they can't be mistaken for cover). With a clear path no hit is
        // returned and the dot rests at center, which in convergence mode is
        // the target under the crosshair. Casting only as far as aim_end is
        // what keeps terrain *behind* the target from pulling the dot off it.
        ray_start.load3(eye.mV);
        ray_end.load3(aim_end.mV);
        if (gPipeline.lineSegmentIntersectWorldGeometry(ray_start, ray_end, &hit))
        {
            const LLRect& world_rect = gViewerWindow->getWorldViewRectScaled();
            LLCoordGL screen(world_rect.getCenterX(), world_rect.getCenterY());
            camera->projectPosAgentToScreen(LLVector3(hit.getF32ptr()), screen, true);
            offset_x = (F32)(screen.mX - world_rect.getCenterX());
            offset_y = (F32)(screen.mY - world_rect.getCenterY());
        }
    }

    // Smooth the dot's travel (raycasts against rough geometry jitter) and
    // snap to center inside a small deadzone so it doesn't shimmer at rest.
    static F32 smoothed_x = 0.f;
    static F32 smoothed_y = 0.f;
    const F32 interp = LLSmoothInterpolation::getInterpolant(0.05f);
    smoothed_x = lerp(smoothed_x, offset_x, interp);
    smoothed_y = lerp(smoothed_y, offset_y, interp);
    const F32 DEADZONE = 1.5f; // ui px
    if (fabsf(smoothed_x) < DEADZONE && fabsf(smoothed_y) < DEADZONE &&
        fabsf(offset_x) < DEADZONE && fabsf(offset_y) < DEADZONE)
    {
        smoothed_x = 0.f;
        smoothed_y = 0.f;
    }

    if (dot)
    {
        const S32 w = (S32)(dot->getWidth() * scale);
        const S32 h = (S32)(dot->getHeight() * scale);
        dot->draw((view_width - w) / 2 + (S32)smoothed_x, (view_height - h) / 2 + (S32)smoothed_y, w, h, color);
    }
    if (arrows)
    {
        const S32 w = (S32)(arrows->getWidth() * scale);
        const S32 h = (S32)(arrows->getHeight() * scale);
        arrows->draw((view_width - w) / 2, (view_height - h) / 2, w, h, color);
    }
}
