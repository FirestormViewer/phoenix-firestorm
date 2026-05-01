/**
 * @file llpositionalstreammgr.cpp
 * @brief Manager for prim-bound 3D positional audio streams (AYAstorm).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2026, Phoenix Firestorm Project, Inc.
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

#include "llpositionalstreammgr.h"

#include "llpositionalstream.h"
#include "llpositionalstreamstereo.h"

#include "llviewercontrol.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"

#include "llagent.h"
#include "llchat.h"
#include "llfloaterimnearbychat.h"
#include "llfloaterreg.h"
#include "llinstantmessage.h"
#include "llnotificationsutil.h"
#include "llselectmgr.h"

#include "llstring.h"
#include "lltimer.h"

#include <algorithm>
#include <cctype>

namespace
{
    LLVector3 toFloatVec(const LLVector3d& v)
    {
        LLVector3 out;
        out.setVec(v);
        return out;
    }

    bool tryParseFloat(const std::string& s, F32& out)
    {
        if (s.empty())
        {
            return false;
        }
        try
        {
            size_t consumed = 0;
            F32 val = std::stof(s, &consumed);
            if (consumed == 0)
            {
                return false;
            }
            out = val;
            return true;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }

    std::string toLowerAscii(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return s;
    }

    // Case-insensitive substring search (ASCII).
    size_t findCaseInsensitive(const std::string& haystack, const std::string& needle)
    {
        if (needle.empty() || haystack.size() < needle.size())
        {
            return std::string::npos;
        }
        const size_t end = haystack.size() - needle.size();
        for (size_t i = 0; i <= end; ++i)
        {
            bool match = true;
            for (size_t j = 0; j < needle.size(); ++j)
            {
                unsigned char a = static_cast<unsigned char>(haystack[i + j]);
                unsigned char b = static_cast<unsigned char>(needle[j]);
                if (std::tolower(a) != std::tolower(b))
                {
                    match = false;
                    break;
                }
            }
            if (match)
            {
                return i;
            }
        }
        return std::string::npos;
    }

    bool tryParseInt(const std::string& s, S32& out)
    {
        if (s.empty()) return false;
        try
        {
            size_t consumed = 0;
            S32 val = std::stoi(s, &consumed);
            if (consumed == 0) return false;
            out = val;
            return true;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }

    // Walks a body between [content_start, end) by directly scanning for
    // "{key:value}" blocks and invoking onPair(lowered_key, trimmed_val) for
    // each well-formed unit. Anything between blocks (comma, whitespace, or
    // nothing at all) is treated as a separator, so r5 unifies the previous
    // 3D Stream "comma required" and Parcel Hide "no separator" formats.
    template <typename F>
    void forEachKeyValue(const std::string& description,
                         size_t content_start, size_t end, F&& onPair)
    {
        size_t cursor = content_start;
        while (cursor < end)
        {
            size_t ob = description.find('{', cursor);
            if (ob == std::string::npos || ob >= end) break;
            size_t cb = description.find('}', ob + 1);
            if (cb == std::string::npos || cb > end) break;

            std::string inner = description.substr(ob + 1, cb - ob - 1);
            cursor = cb + 1;

            size_t colon = inner.find(':');
            if (colon == std::string::npos) continue;

            std::string key = inner.substr(0, colon);
            std::string val = inner.substr(colon + 1);
            LLStringUtil::trim(key);
            LLStringUtil::trim(val);
            key = toLowerAscii(key);

            onPair(key, val);
        }
    }

    // Locate the body of "[<prefix>...]" and return [content_start, end).
    // Returns false if prefix not found or no closing bracket.
    bool findTagBody(const std::string& description, const std::string& prefix,
                     size_t& out_content_start, size_t& out_end)
    {
        size_t begin = findCaseInsensitive(description, prefix);
        if (begin == std::string::npos) return false;
        size_t content_start = begin + prefix.size();
        size_t end = description.find(']', content_start);
        if (end == std::string::npos) return false;
        out_content_start = content_start;
        out_end = end;
        return true;
    }

    template <typename TagT>
    void resolveRolloffFromTag(const TagT& tag, F32& out_min, F32& out_max)
    {
        out_min = tag.min.value_or(gSavedSettings.getF32("Stream3DRolloffMin"));
        out_max = tag.max.value_or(gSavedSettings.getF32("Stream3DRolloffMax"));
    }

    // M8: emit a Firestorm toast that also auto-logs into Nearby Chat.
    // ChatSystemMessageTip is a built-in template (notifications.xml) that
    // marries `notifytip` (transient toast) with `log_to_chat="true"`.
    // Firestorm rewrote LLHandlerUtil::logToNearbyChat to deliver only into
    // FSFloaterNearbyChat, so AYAChatWindowStyle=2 (LL) users would otherwise
    // see only the toast. Forward the same line to LLFloaterIMNearbyChat in
    // that case so chat history matches the toast across both styles.
    void notifyStream3D(const std::string& message)
    {
        const std::string text = "3D Stream: " + message;

        LLSD args;
        args["MESSAGE"] = text;
        LLNotificationsUtil::add("ChatSystemMessageTip", args);

        if (ayastorm_is_ll_style())
        {
            if (LLFloaterIMNearbyChat* ll_chat =
                    LLFloaterReg::findTypedInstance<LLFloaterIMNearbyChat>("nearby_chat"))
            {
                LLChat chat_msg(text);
                chat_msg.mSourceType = CHAT_SOURCE_SYSTEM;
                chat_msg.mFromName = SYSTEM_FROM;
                chat_msg.mFromID = LLUUID::null;
                ll_chat->addMessage(chat_msg);
            }
        }
    }

    // Resolve link number to a child object. Link 1 is the root itself, link 2
    // is the first child, etc. Returns nullptr if no such link in this set.
    LLViewerObject* resolveLink(LLViewerObject* root, S32 link_num)
    {
        if (!root || link_num < 1) return nullptr;
        if (link_num == 1) return root;

        const auto& children = root->getChildren();
        S32 want_idx = link_num - 2; // link 2 == children[0]
        if (want_idx < 0 || want_idx >= static_cast<S32>(children.size())) return nullptr;
        auto it = children.begin();
        std::advance(it, want_idx);
        return it->get();
    }
}

LLPositionalStreamMgr& LLPositionalStreamMgr::instance()
{
    static LLPositionalStreamMgr sInstance;
    return sInstance;
}

LLPositionalStreamMgr::LLPositionalStreamMgr() = default;
LLPositionalStreamMgr::~LLPositionalStreamMgr() = default;

// static
std::optional<LLPositionalStreamMgr::TagData>
LLPositionalStreamMgr::parseTag(const std::string& description)
{
    // r5: new prefix is `[3dstream:`; the legacy `[ayastream:` is kept as a
    // permanent alias so already-placed prims keep working without re-edit.
    static const std::string kPrefix    = "[3dstream:";
    static const std::string kPrefixOld = "[ayastream:";

    size_t content_start = 0, end = 0;
    if (!findTagBody(description, kPrefix, content_start, end) &&
        !findTagBody(description, kPrefixOld, content_start, end)) return std::nullopt;

    TagData data;
    bool got_url = false;
    forEachKeyValue(description, content_start, end,
        [&](const std::string& key, const std::string& val)
        {
            if (key == "url")      { data.url = val; got_url = !val.empty(); }
            else if (key == "min") { F32 f; if (tryParseFloat(val, f)) data.min = f; }
            else if (key == "max") { F32 f; if (tryParseFloat(val, f)) data.max = f; }
        });

    if (!got_url) return std::nullopt;
    return data;
}

// static
std::optional<LLPositionalStreamMgr::StereoTagData>
LLPositionalStreamMgr::parseStereoTag(const std::string& description)
{
    // r5: see parseTag — same prefix-alias scheme for stereo.
    static const std::string kPrefix    = "[3dstream-stereo:";
    static const std::string kPrefixOld = "[ayastream-stereo:";

    size_t content_start = 0, end = 0;
    if (!findTagBody(description, kPrefix, content_start, end) &&
        !findTagBody(description, kPrefixOld, content_start, end)) return std::nullopt;

    StereoTagData data;
    bool got_url = false;
    forEachKeyValue(description, content_start, end,
        [&](const std::string& key, const std::string& val)
        {
            if (key == "url")      { data.url = val; got_url = !val.empty(); }
            else if (key == "min") { F32 f; if (tryParseFloat(val, f)) data.min = f; }
            else if (key == "max") { F32 f; if (tryParseFloat(val, f)) data.max = f; }
            else if (key == "l")   { S32 i; if (tryParseInt(val, i)) data.l_link = i; }
            else if (key == "r")   { S32 i; if (tryParseInt(val, i)) data.r_link = i; }
        });

    if (!got_url) return std::nullopt;
    if (data.l_link < 1 || data.r_link < 1) return std::nullopt;
    if (data.l_link == data.r_link) return std::nullopt;
    return data;
}

void LLPositionalStreamMgr::onObjectPropertiesReceived(const LLUUID& id,
                                                       const std::string& description)
{
    if (id.isNull())
    {
        return;
    }

    // M8: kill switch + scan toggle gating. Cache the description anyway when
    // only DescriptionScan is off — re-enabling shouldn't have to wait for a
    // fresh polling cycle to know what every prim said.
    if (!gSavedSettings.getBOOL("Stream3DEnabled"))
    {
        return;
    }
    if (!gSavedSettings.getBOOL("Stream3DDescriptionScan"))
    {
        return;
    }

    // Always re-evaluate: if the description is unchanged the diff inside
    // evaluateBinding is cheap, and re-arrivals after object recreate
    // (e.g., teleport out and back) need to re-bind even when the text matches.
    auto& entry = mDescriptionCache[id];
    entry.description = description;
    entry.last_polled = LLTimer::getElapsedSeconds();

    // M3b debug: only log replies that look like our tag, to keep the noise
    // floor sane in busy regions.
    if (description.find("3dstream") != std::string::npos ||
        description.find("ayastream") != std::string::npos)
    {
        LL_INFOS("Stream3D") << "reply: " << id
                              << " desc=\"" << description << "\"" << LL_ENDL;
    }

    evaluateBinding(id);
}

void LLPositionalStreamMgr::evaluateBinding(const LLUUID& id)
{
    auto desc_it = mDescriptionCache.find(id);
    if (desc_it == mDescriptionCache.end())
    {
        return;
    }

    // Stereo tag wins if both happen to be present (the prefixes don't
    // overlap textually, but a description could in theory contain both).
    const std::string& desc = desc_it->second.description;
    auto stereo_tag = parseStereoTag(desc);
    auto mono_tag = stereo_tag ? std::nullopt : parseTag(desc);

    auto bind_it = mBindings.find(id);
    auto sbind_it = mStereoBindings.find(id);

    // If a different mode now applies, drop the stale binding first so the
    // new path can recreate it cleanly.
    if (stereo_tag && bind_it != mBindings.end())
    {
        LL_INFOS("Stream3D") << "Replacing mono binding with stereo for " << id << LL_ENDL;
        mBindings.erase(bind_it);
        bind_it = mBindings.end();
    }
    if (mono_tag && sbind_it != mStereoBindings.end())
    {
        LL_INFOS("Stream3D") << "Replacing stereo binding with mono for " << id << LL_ENDL;
        mStereoBindings.erase(sbind_it);
        sbind_it = mStereoBindings.end();
    }

    if (stereo_tag)
    {
        evaluateStereoBinding(id, *stereo_tag);
        return;
    }
    if (mono_tag)
    {
        evaluateMonoBinding(id, *mono_tag);
        return;
    }

    // No tag of either kind: drop any bindings this prim still has.
    if (bind_it != mBindings.end())
    {
        LL_INFOS("Stream3D") << "Removing positional binding for " << id
                              << " (tag gone)" << LL_ENDL;
        mBindings.erase(bind_it);
    }
    if (sbind_it != mStereoBindings.end())
    {
        LL_INFOS("Stream3D") << "Removing stereo binding for " << id
                              << " (tag gone)" << LL_ENDL;
        mStereoBindings.erase(sbind_it);
    }
}

void LLPositionalStreamMgr::evaluateMonoBinding(const LLUUID& id, const TagData& tag)
{
    LLViewerObject* obj = gObjectList.findObject(id);
    if (!obj || obj->isDead())
    {
        return;
    }

    F32 want_min, want_max;
    resolveRolloffFromTag(tag, want_min, want_max);
    LLVector3 pos = toFloatVec(obj->getPositionGlobal());

    auto bind_it = mBindings.find(id);
    if (bind_it != mBindings.end())
    {
        Binding& b = bind_it->second;
        if (b.url == tag.url)
        {
            b.tag_min = tag.min;
            b.tag_max = tag.max;
            if (b.applied_min != want_min || b.applied_max != want_max)
            {
                b.stream->setRolloffDistances(want_min, want_max);
                b.applied_min = want_min;
                b.applied_max = want_max;
            }
            return;
        }
        LL_INFOS("Stream3D") << "Rebinding " << id << ": URL changed" << LL_ENDL;
        mBindings.erase(bind_it);
    }

    const S32 cap = gSavedSettings.getS32("Stream3DMaxConcurrent");
    if (cap > 0 && static_cast<S32>(mBindings.size() + mStereoBindings.size()) >= cap)
    {
        LL_WARNS("Stream3D") << "Max concurrent streams (" << cap
                              << ") reached; not binding " << id << LL_ENDL;
        if (!mCapNotified)
        {
            mCapNotified = true;
            notifyStream3D(llformat(
                "max concurrent streams (%d) reached; new tagged prims will be ignored until one is released.",
                cap));
        }
        return;
    }

    auto stream = std::make_unique<LLPositionalStream>();
    stream->setRolloffDistances(want_min, want_max);
    stream->setVolume(gSavedSettings.getF32("Stream3DVolumeMaster"));
    if (!stream->start(tag.url, pos))
    {
        return;
    }

    Binding b;
    b.url = tag.url;
    b.tag_min = tag.min;
    b.tag_max = tag.max;
    b.applied_min = want_min;
    b.applied_max = want_max;
    b.stream = std::move(stream);
    mBindings.emplace(id, std::move(b));

    LL_INFOS("Stream3D") << "Bound positional stream to " << id
                          << " url=" << tag.url << LL_ENDL;
}

void LLPositionalStreamMgr::evaluateStereoBinding(const LLUUID& id, const StereoTagData& tag)
{
    LLViewerObject* root = gObjectList.findObject(id);
    if (!root || root->isDead())
    {
        return;
    }

    LLViewerObject* l_obj = resolveLink(root, tag.l_link);
    LLViewerObject* r_obj = resolveLink(root, tag.r_link);
    if (!l_obj || !r_obj || l_obj->isDead() || r_obj->isDead())
    {
        // Linkset not fully resolvable yet; defer (will retry on next props
        // msg or when update() re-runs after children arrive).
        return;
    }

    F32 want_min, want_max;
    resolveRolloffFromTag(tag, want_min, want_max);
    LLVector3 l_pos = toFloatVec(l_obj->getPositionGlobal());
    LLVector3 r_pos = toFloatVec(r_obj->getPositionGlobal());

    auto sbind_it = mStereoBindings.find(id);
    if (sbind_it != mStereoBindings.end())
    {
        StereoBinding& b = sbind_it->second;
        const bool topology_same = (b.url == tag.url
                                    && b.l_link == tag.l_link
                                    && b.r_link == tag.r_link
                                    && b.l_prim == l_obj->getID()
                                    && b.r_prim == r_obj->getID());
        if (topology_same)
        {
            b.tag_min = tag.min;
            b.tag_max = tag.max;
            if (b.applied_min != want_min || b.applied_max != want_max)
            {
                b.stream->setRolloffDistances(want_min, want_max);
                b.applied_min = want_min;
                b.applied_max = want_max;
            }
            return;
        }
        LL_INFOS("Stream3D") << "Rebinding stereo " << id
                              << ": URL or linkset topology changed" << LL_ENDL;
        mStereoBindings.erase(sbind_it);
    }

    const S32 cap = gSavedSettings.getS32("Stream3DMaxConcurrent");
    if (cap > 0 && static_cast<S32>(mBindings.size() + mStereoBindings.size()) >= cap)
    {
        LL_WARNS("Stream3D") << "Max concurrent streams (" << cap
                              << ") reached; not binding stereo " << id << LL_ENDL;
        if (!mCapNotified)
        {
            mCapNotified = true;
            notifyStream3D(llformat(
                "max concurrent streams (%d) reached; new tagged prims will be ignored until one is released.",
                cap));
        }
        return;
    }

    auto stream = std::make_unique<LLPositionalStreamStereo>();
    stream->setRolloffDistances(want_min, want_max);
    stream->setVolume(gSavedSettings.getF32("Stream3DVolumeMaster"));
    if (!stream->start(tag.url, l_pos, r_pos))
    {
        return;
    }

    StereoBinding b;
    b.url = tag.url;
    b.tag_min = tag.min;
    b.tag_max = tag.max;
    b.applied_min = want_min;
    b.applied_max = want_max;
    b.l_link = tag.l_link;
    b.r_link = tag.r_link;
    b.l_prim = l_obj->getID();
    b.r_prim = r_obj->getID();
    b.stream = std::move(stream);
    mStereoBindings.emplace(id, std::move(b));

    LL_INFOS("Stream3D") << "Bound stereo stream to " << id
                          << " url=" << tag.url
                          << " L=link" << tag.l_link << "(" << l_obj->getID() << ")"
                          << " R=link" << tag.r_link << "(" << r_obj->getID() << ")"
                          << LL_ENDL;
}

void LLPositionalStreamMgr::update()
{
    // M8: master kill switch. Listener already tore down state when the
    // setting flipped to false, so just bail.
    if (!gSavedSettings.getBOOL("Stream3DEnabled"))
    {
        return;
    }

    // M8: re-arm the cap notification once we drop back under the cap, so a
    // future cap-hit triggers a fresh toast rather than being suppressed by
    // the stale flag.
    {
        const S32 cap = gSavedSettings.getS32("Stream3DMaxConcurrent");
        const S32 total = static_cast<S32>(mBindings.size() + mStereoBindings.size());
        if (mCapNotified && (cap <= 0 || total < cap))
        {
            mCapNotified = false;
        }
    }

    const F64 now = LLTimer::getElapsedSeconds();
    pollObjectPropertiesFamily(now);

    if (mDebugStream)
    {
        mDebugStream->update();
    }

    if (mDebugStereoStream)
    {
        mDebugStereoStream->update();
    }

    // M7: fixed 5s wait between reconnect attempts. Total worst-case downtime
    // before a binding is dropped == Stream3DReconnectAttempts * kRetryDelay.
    static constexpr F64 kRetryDelay = 5.0;
    const S32 max_attempts = gSavedSettings.getS32("Stream3DReconnectAttempts");

    for (auto it = mBindings.begin(); it != mBindings.end(); )
    {
        const LLUUID& id = it->first;
        Binding& b = it->second;

        LLViewerObject* obj = gObjectList.findObject(id);
        if (!obj || obj->isDead())
        {
            LL_INFOS("Stream3D") << "Object " << id
                                  << " gone; releasing positional stream" << LL_ENDL;
            it = mBindings.erase(it);
            continue;
        }

        if (b.stream->isFailed())
        {
            if (max_attempts <= 0)
            {
                LL_WARNS("Stream3D") << "Stream for " << id
                                      << " failed; auto-reconnect disabled, dropping binding"
                                      << LL_ENDL;
                it = mBindings.erase(it);
                continue;
            }
            if (b.reconnect_attempts >= max_attempts)
            {
                LL_WARNS("Stream3D") << "Stream for " << id << " exhausted "
                                      << max_attempts << " reconnect attempts; dropping binding"
                                      << LL_ENDL;
                notifyStream3D("stream gave up after "
                                + llformat("%d", max_attempts)
                                + " reconnect attempts: " + b.url);
                it = mBindings.erase(it);
                continue;
            }
            if (b.next_retry_time == 0.0)
            {
                b.next_retry_time = now + kRetryDelay;
                LL_INFOS("Stream3D") << "Stream for " << id
                                      << " failed; scheduling reconnect "
                                      << (b.reconnect_attempts + 1) << "/" << max_attempts
                                      << " in " << kRetryDelay << "s" << LL_ENDL;
            }
            else if (now >= b.next_retry_time)
            {
                ++b.reconnect_attempts;
                b.next_retry_time = 0.0;
                const LLVector3 pos = toFloatVec(obj->getPositionGlobal());
                b.stream->setRolloffDistances(b.applied_min, b.applied_max);
                b.stream->setVolume(gSavedSettings.getF32("Stream3DVolumeMaster"));
                LL_INFOS("Stream3D") << "Reconnect attempt " << b.reconnect_attempts
                                      << "/" << max_attempts << " for " << id
                                      << " url=" << b.url << LL_ENDL;
                b.stream->start(b.url, pos);
            }
            ++it;
            continue;
        }

        // Successful playback resets the retry counter so a later, independent
        // failure gets its own full budget rather than inheriting old strikes.
        if (b.reconnect_attempts > 0 && b.stream->isPlaying())
        {
            LL_INFOS("Stream3D") << "Reconnect succeeded for " << id
                                  << " after " << b.reconnect_attempts << " attempt(s)"
                                  << LL_ENDL;
            b.reconnect_attempts = 0;
            b.next_retry_time = 0.0;
        }

        // M8: notify exactly once per Binding, when it first reaches Playing.
        // Reconnect successes don't re-fire because Binding (and the flag)
        // survives across stream->start() retries.
        if (!b.notified_played && b.stream->isPlaying())
        {
            b.notified_played = true;
            notifyStream3D("now playing: " + b.url);
        }

        b.stream->setPosition(toFloatVec(obj->getPositionGlobal()));
        b.stream->update();
        ++it;
    }

    for (auto it = mStereoBindings.begin(); it != mStereoBindings.end(); )
    {
        const LLUUID& id = it->first;
        StereoBinding& b = it->second;

        LLViewerObject* l_obj = gObjectList.findObject(b.l_prim);
        LLViewerObject* r_obj = gObjectList.findObject(b.r_prim);
        if (!l_obj || !r_obj || l_obj->isDead() || r_obj->isDead())
        {
            LL_INFOS("Stream3D") << "Stereo pair (" << id
                                  << ") gone; releasing stereo stream" << LL_ENDL;
            it = mStereoBindings.erase(it);
            continue;
        }

        if (b.stream->isFailed())
        {
            if (max_attempts <= 0)
            {
                LL_WARNS("Stream3D") << "Stereo stream for " << id
                                      << " failed; auto-reconnect disabled, dropping binding"
                                      << LL_ENDL;
                it = mStereoBindings.erase(it);
                continue;
            }
            if (b.reconnect_attempts >= max_attempts)
            {
                LL_WARNS("Stream3D") << "Stereo stream for " << id << " exhausted "
                                      << max_attempts << " reconnect attempts; dropping binding"
                                      << LL_ENDL;
                notifyStream3D("stereo stream gave up after "
                                + llformat("%d", max_attempts)
                                + " reconnect attempts: " + b.url);
                it = mStereoBindings.erase(it);
                continue;
            }
            if (b.next_retry_time == 0.0)
            {
                b.next_retry_time = now + kRetryDelay;
                LL_INFOS("Stream3D") << "Stereo stream for " << id
                                      << " failed; scheduling reconnect "
                                      << (b.reconnect_attempts + 1) << "/" << max_attempts
                                      << " in " << kRetryDelay << "s" << LL_ENDL;
            }
            else if (now >= b.next_retry_time)
            {
                ++b.reconnect_attempts;
                b.next_retry_time = 0.0;
                const LLVector3 l_pos = toFloatVec(l_obj->getPositionGlobal());
                const LLVector3 r_pos = toFloatVec(r_obj->getPositionGlobal());
                b.stream->setRolloffDistances(b.applied_min, b.applied_max);
                b.stream->setVolume(gSavedSettings.getF32("Stream3DVolumeMaster"));
                LL_INFOS("Stream3D") << "Stereo reconnect attempt " << b.reconnect_attempts
                                      << "/" << max_attempts << " for " << id
                                      << " url=" << b.url << LL_ENDL;
                b.stream->start(b.url, l_pos, r_pos);
            }
            ++it;
            continue;
        }

        if (b.reconnect_attempts > 0 && b.stream->isPlaying())
        {
            LL_INFOS("Stream3D") << "Stereo reconnect succeeded for " << id
                                  << " after " << b.reconnect_attempts << " attempt(s)"
                                  << LL_ENDL;
            b.reconnect_attempts = 0;
            b.next_retry_time = 0.0;
        }

        if (!b.notified_played && b.stream->isPlaying())
        {
            b.notified_played = true;
            notifyStream3D("now playing (stereo): " + b.url);
        }

        b.stream->setPositions(toFloatVec(l_obj->getPositionGlobal()),
                               toFloatVec(r_obj->getPositionGlobal()));
        b.stream->update();
        ++it;
    }
}

void LLPositionalStreamMgr::applyDefaultRolloff(F32 default_min, F32 default_max)
{
    if (mDebugStream)
    {
        mDebugStream->setRolloffDistances(default_min, default_max);
    }

    if (mDebugStereoStream)
    {
        mDebugStereoStream->setRolloffDistances(default_min, default_max);
    }

    for (auto& [id, b] : mBindings)
    {
        F32 want_min = b.tag_min.value_or(default_min);
        F32 want_max = b.tag_max.value_or(default_max);
        if (b.applied_min != want_min || b.applied_max != want_max)
        {
            b.stream->setRolloffDistances(want_min, want_max);
            b.applied_min = want_min;
            b.applied_max = want_max;
        }
    }

    for (auto& [id, b] : mStereoBindings)
    {
        F32 want_min = b.tag_min.value_or(default_min);
        F32 want_max = b.tag_max.value_or(default_max);
        if (b.applied_min != want_min || b.applied_max != want_max)
        {
            b.stream->setRolloffDistances(want_min, want_max);
            b.applied_min = want_min;
            b.applied_max = want_max;
        }
    }
}

void LLPositionalStreamMgr::shutdownPrimBindings()
{
    if (mBindings.empty() && mStereoBindings.empty())
    {
        return;
    }
    LL_INFOS("Stream3D") << "Tearing down "
                          << mBindings.size() << " mono and "
                          << mStereoBindings.size() << " stereo prim bindings"
                          << LL_ENDL;
    // ~Binding / ~StereoBinding's unique_ptr<> destructors invoke each
    // stream's stop(), which calls FMOD Channel::stop() synchronously.
    // Output device buffer drain (<50ms typical) is the only residual delay.
    mBindings.clear();
    mStereoBindings.clear();
}

void LLPositionalStreamMgr::shutdownAll()
{
    shutdownPrimBindings();
    if (mDebugStream)
    {
        LL_INFOS("Stream3D") << "Tearing down debug mono stream" << LL_ENDL;
        mDebugStream->stop();
        mDebugStream.reset();
    }
    if (mDebugStereoStream)
    {
        LL_INFOS("Stream3D") << "Tearing down debug stereo stream" << LL_ENDL;
        mDebugStereoStream->stop();
        mDebugStereoStream.reset();
    }
}

void LLPositionalStreamMgr::applyMasterVolume(F32 volume)
{
    if (mDebugStream)
    {
        mDebugStream->setVolume(volume);
    }
    if (mDebugStereoStream)
    {
        mDebugStereoStream->setVolume(volume);
    }
    for (auto& [id, b] : mBindings)
    {
        b.stream->setVolume(volume);
    }
    for (auto& [id, b] : mStereoBindings)
    {
        b.stream->setVolume(volume);
    }
}

void LLPositionalStreamMgr::startDebug(const std::string& url, const LLVector3& world_pos)
{
    if (!mDebugStream)
    {
        mDebugStream = std::make_unique<LLPositionalStream>();
    }
    mDebugStream->setRolloffDistances(
        gSavedSettings.getF32("Stream3DRolloffMin"),
        gSavedSettings.getF32("Stream3DRolloffMax"));
    mDebugStream->start(url, world_pos);
}

void LLPositionalStreamMgr::stopDebug()
{
    if (mDebugStream)
    {
        mDebugStream->stop();
        mDebugStream.reset();
    }
}

void LLPositionalStreamMgr::startDebugStereo(const std::string& url,
                                             const LLVector3& l_pos,
                                             const LLVector3& r_pos)
{
    if (!mDebugStereoStream)
    {
        mDebugStereoStream = std::make_unique<LLPositionalStreamStereo>();
    }
    mDebugStereoStream->setRolloffDistances(
        gSavedSettings.getF32("Stream3DRolloffMin"),
        gSavedSettings.getF32("Stream3DRolloffMax"));
    mDebugStereoStream->start(url, l_pos, r_pos);
}

void LLPositionalStreamMgr::stopDebugStereo()
{
    if (mDebugStereoStream)
    {
        mDebugStereoStream->stop();
        mDebugStereoStream.reset();
    }
}

void LLPositionalStreamMgr::pollObjectPropertiesFamily(F64 now)
{
    // M8: Stream3DDescriptionScan also gates the active poll. (Enabled is
    // checked one level up in update(); reaching here implies Enabled=true.)
    if (!gSavedSettings.getBOOL("Stream3DDescriptionScan"))
    {
        return;
    }

    // Run the scan at ~2 Hz; with kBudgetPerScan = 5 that caps outgoing
    // requests at ~10 req/s sustained, regardless of frame rate.
    // 10 req/s × 30s poll_interval = ~300 prims per cycle. In denser regions
    // a given prim's effective re-poll interval will exceed Stream3DPollInterval.
    static constexpr F64 kScanInterval  = 0.5;
    static constexpr int kBudgetPerScan = 5;

    if (now - mLastPollScanTime < kScanInterval)
    {
        return;
    }
    mLastPollScanTime = now;

    const F32 poll_interval = gSavedSettings.getF32("Stream3DPollInterval");
    const F32 max_dist      = gSavedSettings.getF32("Stream3DMaxDistance");

    // Per-scan breakdown for support / tuning. Off by default; enable with
    //   --logdebug Stream3D
    LL_DEBUGS("Stream3D") << "poll diag: interval=" << poll_interval
                           << " max_dist=" << max_dist
                           << " num_objects=" << gObjectList.getNumObjects()
                           << LL_ENDL;

    if (poll_interval <= 0.f)
    {
        return;
    }

    if (max_dist <= 0.f)
    {
        return;
    }
    const F32 max_dist_sq = max_dist * max_dist;
    const LLVector3d agent_pos = gAgent.getPositionGlobal();

    int budget = kBudgetPerScan;
    const S32 num = gObjectList.getNumObjects();
    if (num == 0)
    {
        return;
    }

    int n_total = 0, n_filtered = 0, n_far = 0, n_recent = 0, n_sent = 0;
    int examined = 0;

    // Walk in round-robin order so dense regions don't starve prims past
    // the first slice each pass can reach before exhausting the budget.
    while (budget > 0 && examined < num)
    {
        if (mPollCursor >= num)
        {
            mPollCursor = 0;
        }
        const S32 i = mPollCursor++;
        ++examined;

        LLViewerObject* obj = gObjectList.getObject(i);
        if (!obj || obj->isDead())
        {
            continue;
        }
        ++n_total;

        // Avatars / attachments / HUDs don't carry [3dstream:...] tags.
        if (obj->isAvatar() || obj->isAttachment() || obj->isHUDAttachment())
        {
            ++n_filtered;
            continue;
        }

        LLVector3d delta = obj->getPositionGlobal();
        delta -= agent_pos;
        if (static_cast<F32>(delta.magVecSquared()) > max_dist_sq)
        {
            ++n_far;
            continue;
        }

        const LLUUID& id = obj->getID();
        // operator[] auto-creates an empty entry; harmless — last_polled stamps
        // it so we don't re-request before the interval elapses.
        auto& entry = mDescriptionCache[id];
        if (entry.last_polled > 0.0 && (now - entry.last_polled) < poll_interval)
        {
            ++n_recent;
            continue;
        }

        LLSelectMgr::getInstance()->requestObjectPropertiesFamily(obj);
        entry.last_polled = now;
        --budget;
        ++n_sent;
    }

    LL_DEBUGS("Stream3D") << "poll: scanned this pass total=" << n_total
                           << " filtered=" << n_filtered
                           << " far=" << n_far
                           << " recent=" << n_recent
                           << " sent=" << n_sent
                           << " examined=" << examined
                           << " cursor=" << mPollCursor
                           << "/" << num
                           << LL_ENDL;
}
