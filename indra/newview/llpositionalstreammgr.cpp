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

#include "llfasttimer.h"
#include "llpositionalstream.h"
#include "llpositionalstreammulti.h"
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
#include <set>

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
LLPositionalStreamMgr::DistParseResult
LLPositionalStreamMgr::parseDistributedStereoTag(const std::string& description)
{
    // r5 で導入した [3dstream-stereo:...] と alias [ayastream-stereo:...] を
    // 同じ本体としてサポート (r8 で旧 {l:N}{r:N} 書式は廃止し、新書式のみ受ける)。
    static const std::string kPrefix    = "[3dstream-stereo:";
    static const std::string kPrefixOld = "[ayastream-stereo:";

    DistParseResult result;

    size_t content_start = 0, end = 0;
    if (!findTagBody(description, kPrefix, content_start, end) &&
        !findTagBody(description, kPrefixOld, content_start, end))
    {
        return result; // no tag at all
    }

    // Track which keys appeared (regardless of value validity) so a
    // {ch:X} with an invalid value still surfaces BadCh rather than
    // silently being treated as "no tag here".
    bool seen_ch_key  = false;
    bool seen_url_key = false;

    DistStereoTagData data;
    F32 range_value = 0.f;
    bool range_valid = false;
    F32 volume_value = 0.f;
    bool volume_valid = false;

    auto setError = [&](DistParseError e, const std::string& v)
    {
        // First error wins — keeps the surfaced message stable when several
        // fields are bad (LSL editing typically fixes one at a time anyway).
        if (result.error == DistParseError::Ok)
        {
            result.error = e;
            result.bad_value = v;
        }
    };

    forEachKeyValue(description, content_start, end,
        [&](const std::string& key, const std::string& val)
        {
            if (key == "url")
            {
                seen_url_key = true;
                if (val.empty())
                {
                    setError(DistParseError::EmptyUrl, val);
                }
                else
                {
                    data.url = val;
                }
            }
            else if (key == "ch")
            {
                seen_ch_key = true;
                std::string v = toLowerAscii(val);
                if      (v == "l") data.ch = DistChannel::L;
                else if (v == "r") data.ch = DistChannel::R;
                else if (v == "m") data.ch = DistChannel::M;
                else               setError(DistParseError::BadCh, val);
            }
            else if (key == "range")
            {
                F32 f;
                if (tryParseFloat(val, f) && f > 0.f)
                {
                    range_value = f;
                    range_valid = true;
                }
                else
                {
                    setError(DistParseError::BadRange, val);
                }
            }
            else if (key == "volume")
            {
                F32 f;
                if (tryParseFloat(val, f) && f >= 0.f && f <= 1.f)
                {
                    volume_value = f;
                    volume_valid = true;
                }
                else
                {
                    setError(DistParseError::BadVolume, val);
                }
            }
            // Unknown keys (incl. removed-in-r8 {l}/{r}/{min}/{max}) are
            // silently ignored — the spec is permissive about extra fields.
        });

    // The tag is recognized when at least one of {url} or {ch} appeared.
    // A bracket body with neither is treated as "no tag here".
    const bool tag_recognized = seen_ch_key || seen_url_key;
    if (!tag_recognized)
    {
        return result;
    }

    if (result.error != DistParseError::Ok)
    {
        return result; // data stays nullopt; caller can throttle/notify
    }

    // Fan {range} and {volume} into the role-specific slots. {range} on a
    // prim that is both source and speaker fills both range_default and
    // range_speaker from the same value (spec §4.3).
    if (range_valid)
    {
        if (data.url.has_value()) data.range_default = range_value;
        if (data.ch.has_value())  data.range_speaker = range_value;
    }
    if (volume_valid && data.ch.has_value())
    {
        data.volume = volume_value;
    }

    result.data = data;
    return result;
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

void LLPositionalStreamMgr::notifyDistributedError(const LLUUID& prim_id,
                                                   DistErrorKind kind,
                                                   const std::string& detail)
{
    const F64 now = LLTimer::getElapsedSeconds();
    auto key = std::make_pair(prim_id, kind);
    auto it = mErrorThrottle.find(key);
    if (it != mErrorThrottle.end() && (now - it->second) < 30.0)
    {
        // spec §4.9: suppression logged but not surfaced to chat.
        LL_DEBUGS("Stream3D") << "[3dstream-stereo] notification suppressed: kind="
                               << static_cast<int>(kind) << " prim=" << prim_id
                               << " (next allowed in "
                               << (30.0 - (now - it->second)) << "s)" << LL_ENDL;
        return;
    }
    mErrorThrottle[key] = now;

    // spec §4.9 message templates. detail is interpolated where it carries
    // useful information (raw bad value, over-limit count, URL). Each kind
    // ends in a worked example so the user can copy-paste a fix.
    const std::string id_short = prim_id.asString().substr(0, 8);
    std::string msg;
    switch (kind)
    {
    case DistErrorKind::BadCh:
        msg = "タグ書式エラー (prim " + id_short + "): ch の値は L/R/M のいずれかである必要があります";
        if (!detail.empty()) msg += " (got '" + detail + "')";
        msg += "。例: [3dstream-stereo:{ch:L}{range:30}]";
        break;
    case DistErrorKind::BadRange:
        msg = "タグ書式エラー (prim " + id_short + "): range は正の数で指定してください";
        if (!detail.empty()) msg += " (got '" + detail + "')";
        msg += "。例: [3dstream-stereo:{ch:L}{range:30}]";
        break;
    case DistErrorKind::BadVolume:
        msg = "タグ書式エラー (prim " + id_short + "): volume は 0.0〜1.0 の範囲で指定してください";
        if (!detail.empty()) msg += " (got '" + detail + "')";
        msg += "。例: [3dstream-stereo:{ch:L}{volume:0.8}]";
        break;
    case DistErrorKind::EmptyUrl:
        msg = "タグ書式エラー (prim " + id_short + "): url が空です";
        msg += "。例: [3dstream-stereo:{url:http://example/stream.mp3}{range:30}]";
        break;
    case DistErrorKind::NoSpeakers:
        msg = "構造エラー (root " + id_short + "): 音源宣言 (url) が root にあるがスピーカー (ch) が見つかりません";
        msg += "。各スピーカープリムに [3dstream-stereo:{ch:L|R|M}] を記載してください";
        break;
    case DistErrorKind::SpeakerOverLimit:
        msg = "構造エラー (root " + id_short + "): スピーカー数が上限を超えています";
        if (!detail.empty()) msg += " (" + detail + ")";
        msg += "。Stream3DStereoMaxSpeakers の上限まで採用しました";
        break;
    case DistErrorKind::StreamStartFailed:
        msg = "再生エラー (root " + id_short + "): ストリームを開始できませんでした";
        if (!detail.empty()) msg += " (url='" + detail + "')";
        msg += "。URL とネットワーク接続を確認してください";
        break;
    }
    notifyStream3D(msg);
}

void LLPositionalStreamMgr::evaluateBinding(const LLUUID& id)
{
    auto desc_it = mDescriptionCache.find(id);
    if (desc_it == mDescriptionCache.end())
    {
        return;
    }

    const std::string& desc = desc_it->second.description;
    auto mono_tag = parseTag(desc);

    if (mono_tag)
    {
        evaluateMonoBinding(id, *mono_tag);
        // r8 F2-a: a prim that just became a mono source must drop out of any
        // distributed-stereo linkset it had been participating in. Re-evaluate
        // the previously-known root so the speaker slot is recomputed without
        // this prim. (mPrimToRoot lookup is the only side effect here; the
        // mono path itself is unchanged.)
        auto pr_it = mPrimToRoot.find(id);
        if (pr_it != mPrimToRoot.end())
        {
            evaluateLinkset(pr_it->second);
        }
        return;
    }

    // No mono tag — drop any leftover mono binding before considering the
    // distributed-stereo path.
    auto bind_it = mBindings.find(id);
    if (bind_it != mBindings.end())
    {
        LL_INFOS("Stream3D") << "Removing positional binding for " << id
                              << " (tag gone)" << LL_ENDL;
        mBindings.erase(bind_it);
    }

    // r8 F2-a: distributed-stereo dispatch. Either {url} or {ch} triggers a
    // linkset-level (re)evaluation rooted at this prim's getRootEdit().
    auto dist = parseDistributedStereoTag(desc);

    if (dist.error != DistParseError::Ok)
    {
        LL_INFOS("Stream3D") << "[3dstream-stereo] parse error on " << id
                              << " (kind=" << static_cast<int>(dist.error)
                              << ", value='" << dist.bad_value << "')" << LL_ENDL;

        DistErrorKind k = DistErrorKind::BadCh;
        switch (dist.error)
        {
        case DistParseError::BadCh:     k = DistErrorKind::BadCh;     break;
        case DistParseError::BadRange:  k = DistErrorKind::BadRange;  break;
        case DistParseError::BadVolume: k = DistErrorKind::BadVolume; break;
        case DistParseError::EmptyUrl:  k = DistErrorKind::EmptyUrl;  break;
        case DistParseError::Ok:        break; // unreachable
        }
        notifyDistributedError(id, k, dist.bad_value);

        // Field is malformed → treat the slot as missing and re-evaluate the
        // owning linkset, if we knew about one.
        auto pr_it = mPrimToRoot.find(id);
        if (pr_it != mPrimToRoot.end())
        {
            evaluateLinkset(pr_it->second);
        }
        return;
    }

    if (dist.data)
    {
        LLViewerObject* obj = gObjectList.findObject(id);
        if (!obj || obj->isDead())
        {
            return;
        }
        LLViewerObject* root = obj->getRootEdit();
        LLUUID root_id = root ? root->getID() : id;
        evaluateLinkset(root_id);
        return;
    }

    // No tag of any kind. If this prim used to participate in a linkset
    // binding, that binding needs to be recomputed without it.
    auto pr_it = mPrimToRoot.find(id);
    if (pr_it != mPrimToRoot.end())
    {
        evaluateLinkset(pr_it->second);
    }
}

void LLPositionalStreamMgr::evaluateLinkset(LLUUID root_id)
{
    LLViewerObject* root = gObjectList.findObject(root_id);
    if (!root || root->isDead() || root->isAvatar())
    {
        teardownDistributedBinding(root_id);
        return;
    }

    auto root_desc_it = mDescriptionCache.find(root_id);
    if (root_desc_it == mDescriptionCache.end()
        || root_desc_it->second.description.empty())
    {
        // Root description not yet known — keep any existing binding pending
        // and ask the poll loop to fetch the root proactively.
        enqueuePriorityPoll(root_id);
        return;
    }

    auto root_parse = parseDistributedStereoTag(root_desc_it->second.description);
    if (!root_parse.data || !root_parse.data->url.has_value())
    {
        // r8 F2-a constraint: source declaration must live on the root prim.
        // A linkset without a root-level {url} cannot form a binding.
        teardownDistributedBinding(root_id);
        return;
    }

    const auto& root_data = *root_parse.data;
    const std::string url = *root_data.url;
    const F32 fallback_range = gSavedSettings.getF32("Stream3DRolloffMax");
    const F32 range_default = root_data.range_default.value_or(fallback_range);

    std::vector<SpeakerSlot> speakers;
    auto collectSpeaker = [&](const LLUUID& prim_id, const DistStereoTagData& d)
    {
        if (!d.ch.has_value()) return;
        SpeakerSlot s;
        s.prim_id = prim_id;
        s.ch = *d.ch;
        s.range = d.range_speaker.value_or(range_default);
        s.volume = d.volume.value_or(1.f);
        speakers.push_back(s);
    };

    // Root may be both source and speaker.
    collectSpeaker(root_id, root_data);

    for (const auto& child : root->getChildren())
    {
        if (!child || child->isDead()) continue;
        const LLUUID& child_id = child->getID();
        auto cdesc_it = mDescriptionCache.find(child_id);
        if (cdesc_it == mDescriptionCache.end()
            || cdesc_it->second.description.empty())
        {
            // r8 F2-b: ask the poll loop to fetch this child ahead of the
            // round-robin scan so the binding completes promptly.
            enqueuePriorityPoll(child_id);
            continue;
        }
        auto cparse = parseDistributedStereoTag(cdesc_it->second.description);
        if (!cparse.data) continue;
        collectSpeaker(child_id, *cparse.data);
    }

    if (speakers.empty())
    {
        LL_INFOS("Stream3D") << "[3dstream-stereo] structural error: root "
                              << root_id << " has no speakers" << LL_ENDL;
        notifyDistributedError(root_id, DistErrorKind::NoSpeakers, "");
        teardownDistributedBinding(root_id);
        return;
    }

    // r8 F5: per-binding speaker cap from settings.xml. Reads on every
    // evaluation so a runtime change (debug menu / login.xml override)
    // takes effect on the next poll cycle. Clamped to ≥ 1 so a hostile /
    // mistyped 0 setting doesn't silently disable distributed-stereo.
    const S32 kMaxSpeakers = std::max(1, gSavedSettings.getS32("Stream3DStereoMaxSpeakers"));
    S32 dropped = 0;
    if (static_cast<S32>(speakers.size()) > kMaxSpeakers)
    {
        dropped = static_cast<S32>(speakers.size()) - kMaxSpeakers;
        const S32 total = static_cast<S32>(speakers.size());
        speakers.resize(kMaxSpeakers);
        LL_INFOS("Stream3D") << "[3dstream-stereo] too many speakers on root "
                              << root_id << " — truncated to " << kMaxSpeakers
                              << " (dropped " << dropped << ")" << LL_ENDL;
        notifyDistributedError(root_id, DistErrorKind::SpeakerOverLimit,
                               llformat("declared=%d, used=%d", total, kMaxSpeakers));
    }

    // r8 F3-3: detect "no audible change" so we can keep the running stream
    // when an unrelated tag in the linkset is re-polled. Comparison covers
    // url + element-wise speaker tuple (prim, ch, range, volume); position
    // changes are propagated every tick by the update loop and never trigger
    // a restart.
    auto old_it = mDistributedBindings.find(root_id);
    const bool was_present = (old_it != mDistributedBindings.end());
    bool fingerprint_match = false;
    if (was_present)
    {
        const auto& old_b = old_it->second;
        if (old_b.url == url
            && old_b.speakers.size() == speakers.size()
            && old_b.stream)
        {
            fingerprint_match = true;
            for (size_t i = 0; i < speakers.size(); ++i)
            {
                const auto& a = old_b.speakers[i];
                const auto& c = speakers[i];
                if (a.prim_id != c.prim_id || a.ch != c.ch
                    || a.range != c.range || a.volume != c.volume)
                {
                    fingerprint_match = false;
                    break;
                }
            }
        }
    }

    if (fingerprint_match)
    {
        // Refresh metadata cheaply (range_default may have moved if the root
        // {range:} was re-typed to the same numeric value, etc.) but leave
        // the live stream untouched.
        old_it->second.range_default = range_default;
        old_it->second.dropped_speakers = dropped;
        return;
    }

    // Structural change (or first build). Drop the previous mPrimToRoot
    // entries so the new speaker set is the only one indexed.
    if (was_present)
    {
        for (const auto& s : old_it->second.speakers)
        {
            auto pr_it = mPrimToRoot.find(s.prim_id);
            if (pr_it != mPrimToRoot.end() && pr_it->second == root_id)
            {
                mPrimToRoot.erase(pr_it);
            }
        }
    }

    auto& binding = mDistributedBindings[root_id];
    binding.root_id = root_id;
    binding.url = url;
    binding.range_default = range_default;
    binding.speakers = std::move(speakers);
    binding.dropped_speakers = dropped;

    for (const auto& s : binding.speakers)
    {
        mPrimToRoot[s.prim_id] = root_id;
    }

    // (Re)build the FMOD stream. unique_ptr::reset() invokes the existing
    // stream's destructor, which joins the decode thread before releasing
    // FMOD resources (r7 M3 invariant carried from Stereo into Multi).
    binding.stream.reset();
    // Fresh stream object: any in-flight retry budget belongs to the old
    // stream. Keep notified_played sticky so unrelated tag edits don't spam
    // "now playing" again. (F7)
    binding.reconnect_attempts = 0;
    binding.next_retry_time = 0.0;
    auto stream = std::make_unique<LLPositionalStreamMulti>();
    stream->setVolume(gSavedSettings.getF32("Stream3DVolumeMaster"));

    std::vector<LLPositionalStreamMulti::SpeakerConfig> configs;
    configs.reserve(binding.speakers.size());
    for (const auto& s : binding.speakers)
    {
        LLPositionalStreamMulti::SpeakerConfig c;
        switch (s.ch)
        {
        case DistChannel::L: c.ch = LLPositionalStreamMulti::Channel::L; break;
        case DistChannel::R: c.ch = LLPositionalStreamMulti::Channel::R; break;
        case DistChannel::M: c.ch = LLPositionalStreamMulti::Channel::M; break;
        }
        c.range = s.range;
        c.volume = s.volume;
        // Initial position: best effort. Speaker prims that arrived in the
        // poll cache before their viewer object did get a zero vector here;
        // the next update tick replaces it with the real getPositionGlobal().
        if (LLViewerObject* obj = gObjectList.findObject(s.prim_id))
        {
            if (!obj->isDead())
            {
                c.position = toFloatVec(obj->getPositionGlobal());
            }
        }
        configs.push_back(c);
    }

    if (!stream->start(url, configs))
    {
        LL_WARNS("Stream3D") << "[3dstream-stereo] stream start failed root="
                              << root_id << " url=" << url << LL_ENDL;
        notifyDistributedError(root_id, DistErrorKind::StreamStartFailed, url);
        // Keep the binding metadata so a subsequent re-evaluation (e.g.
        // network recovers) can retry without re-walking the linkset; the
        // missing stream is the signal that we owe a retry.
    }
    else
    {
        binding.stream = std::move(stream);
    }

    LL_INFOS("Stream3D") << "[3dstream-stereo] binding "
                          << (was_present ? "rebuilt" : "constructed")
                          << " root=" << root_id
                          << " url=" << url
                          << " speakers=" << binding.speakers.size()
                          << " (dropped=" << dropped << ")"
                          << " stream=" << (binding.stream ? "started" : "deferred")
                          << LL_ENDL;
}

void LLPositionalStreamMgr::enqueuePriorityPoll(const LLUUID& id)
{
    // O(N) dedup; queue is small in practice (≤ ~16 per pending linkset).
    for (const auto& q : mPriorityPollQueue)
    {
        if (q == id) return;
    }
    mPriorityPollQueue.push_back(id);
}

void LLPositionalStreamMgr::detectLinksetStructureChanges()
{
    if (mPrimToRoot.empty()) return;

    // Collect deltas first, then re-evaluate — calling evaluateLinkset while
    // iterating mPrimToRoot would mutate it under us.
    std::set<LLUUID> roots_to_reeval;
    for (const auto& [prim_id, registered_root] : mPrimToRoot)
    {
        LLViewerObject* obj = gObjectList.findObject(prim_id);
        if (!obj || obj->isDead())
        {
            // Prim gone (e.g. derez). Re-evaluating the registered root will
            // drop this slot and either rebuild or tear down the binding.
            roots_to_reeval.insert(registered_root);
            continue;
        }
        LLViewerObject* current_root_obj = obj->getRootEdit();
        const LLUUID current_root = current_root_obj
            ? current_root_obj->getID() : prim_id;
        if (current_root != registered_root)
        {
            // link / unlink moved this prim across linksets.
            roots_to_reeval.insert(registered_root);
            roots_to_reeval.insert(current_root);
        }
    }

    for (const auto& r : roots_to_reeval)
    {
        LL_INFOS("Stream3D") << "[3dstream-stereo] linkset structure change "
                              << "— re-evaluating root " << r << LL_ENDL;
        evaluateLinkset(r);
    }
}

void LLPositionalStreamMgr::teardownDistributedBinding(const LLUUID& root_id)
{
    auto it = mDistributedBindings.find(root_id);
    if (it == mDistributedBindings.end()) return;

    LL_INFOS("Stream3D") << "[3dstream-stereo] tearing down binding root="
                          << root_id
                          << " speakers=" << it->second.speakers.size()
                          << LL_ENDL;

    for (const auto& s : it->second.speakers)
    {
        auto pr_it = mPrimToRoot.find(s.prim_id);
        if (pr_it != mPrimToRoot.end() && pr_it->second == root_id)
        {
            mPrimToRoot.erase(pr_it);
        }
    }
    mDistributedBindings.erase(it);
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
    if (cap > 0 && static_cast<S32>(mBindings.size()) >= cap)
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

static LLTrace::BlockTimerStatHandle FTM_STREAM3D_MGR_UPDATE("Stream3D Mgr Update");

void LLPositionalStreamMgr::update()
{
    LL_RECORD_BLOCK_TIME(FTM_STREAM3D_MGR_UPDATE);

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
        const S32 total = static_cast<S32>(mBindings.size());
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

    // r8 F3-3: distributed-stereo bindings. F7 mirrors the mono Bindings loop:
    // socket-level outages flip stream → State::Failed, this loop runs the
    // retry budget (Stream3DReconnectAttempts × 5s) and either reconnects or
    // drops the binding. F4 still owns parse-time error notifications inside
    // evaluateLinkset.
    constexpr F64 kRetryDelayDist = 5.0;
    const S32 max_attempts_dist = gSavedSettings.getS32("Stream3DReconnectAttempts");
    const F64 now_dist = LLTimer::getElapsedSeconds();

    std::vector<LLUUID> dead_roots;
    for (auto& [root_id, b] : mDistributedBindings)
    {
        LLViewerObject* root = gObjectList.findObject(root_id);
        if (!root || root->isDead())
        {
            dead_roots.push_back(root_id);
            continue;
        }
        if (!b.stream)
        {
            // Stream couldn't be built yet (deferred above). Re-evaluating
            // here is not appropriate — that's the poll loop's job once the
            // descriptions / network resolve. Leave it for now.
            continue;
        }

        if (b.stream->isFailed())
        {
            if (max_attempts_dist <= 0)
            {
                LL_WARNS("Stream3D") << "[3dstream-stereo] stream for root "
                                      << root_id
                                      << " failed; auto-reconnect disabled, dropping binding"
                                      << LL_ENDL;
                dead_roots.push_back(root_id);
                continue;
            }
            if (b.reconnect_attempts >= max_attempts_dist)
            {
                LL_WARNS("Stream3D") << "[3dstream-stereo] stream for root "
                                      << root_id << " exhausted "
                                      << max_attempts_dist
                                      << " reconnect attempts; dropping binding"
                                      << LL_ENDL;
                notifyStream3D("stream gave up after "
                                + llformat("%d", max_attempts_dist)
                                + " reconnect attempts: " + b.url);
                dead_roots.push_back(root_id);
                continue;
            }
            if (b.next_retry_time == 0.0)
            {
                b.next_retry_time = now_dist + kRetryDelayDist;
                LL_INFOS("Stream3D") << "[3dstream-stereo] stream for root "
                                      << root_id << " failed; scheduling reconnect "
                                      << (b.reconnect_attempts + 1) << "/"
                                      << max_attempts_dist
                                      << " in " << kRetryDelayDist << "s" << LL_ENDL;
            }
            else if (now_dist >= b.next_retry_time)
            {
                ++b.reconnect_attempts;
                b.next_retry_time = 0.0;
                std::vector<LLPositionalStreamMulti::SpeakerConfig> configs;
                configs.reserve(b.speakers.size());
                for (const auto& s : b.speakers)
                {
                    LLPositionalStreamMulti::SpeakerConfig c;
                    switch (s.ch)
                    {
                    case DistChannel::L:
                        c.ch = LLPositionalStreamMulti::Channel::L;
                        break;
                    case DistChannel::R:
                        c.ch = LLPositionalStreamMulti::Channel::R;
                        break;
                    case DistChannel::M:
                        c.ch = LLPositionalStreamMulti::Channel::M;
                        break;
                    }
                    c.range = s.range;
                    c.volume = s.volume;
                    if (LLViewerObject* sp = gObjectList.findObject(s.prim_id))
                    {
                        if (!sp->isDead())
                        {
                            c.position = toFloatVec(sp->getPositionGlobal());
                        }
                    }
                    configs.push_back(c);
                }
                b.stream->setVolume(gSavedSettings.getF32("Stream3DVolumeMaster"));
                LL_INFOS("Stream3D") << "[3dstream-stereo] reconnect attempt "
                                      << b.reconnect_attempts << "/" << max_attempts_dist
                                      << " for root " << root_id
                                      << " url=" << b.url << LL_ENDL;
                b.stream->start(b.url, configs);
            }
            // Skip position pushes & update() while Failed.
            continue;
        }

        // Successful playback resets the retry counter so a later, independent
        // outage gets its own full budget rather than inheriting old strikes.
        if (b.reconnect_attempts > 0 && b.stream->isPlaying())
        {
            LL_INFOS("Stream3D") << "[3dstream-stereo] reconnect succeeded for root "
                                  << root_id << " after "
                                  << b.reconnect_attempts << " attempt(s)" << LL_ENDL;
            b.reconnect_attempts = 0;
            b.next_retry_time = 0.0;
        }

        // Notify exactly once per binding when it first reaches Playing.
        // Reconnect successes don't re-fire because the binding (and the
        // flag) survives across stream->start() retries.
        if (!b.notified_played && b.stream->isPlaying())
        {
            b.notified_played = true;
            notifyStream3D("now playing: " + b.url);
        }

        for (size_t i = 0; i < b.speakers.size(); ++i)
        {
            LLViewerObject* sp = gObjectList.findObject(b.speakers[i].prim_id);
            if (sp && !sp->isDead())
            {
                b.stream->setSpeakerPosition(i, toFloatVec(sp->getPositionGlobal()));
            }
        }
        b.stream->update();
    }
    for (const auto& r : dead_roots)
    {
        LL_INFOS("Stream3D") << "[3dstream-stereo] root " << r
                              << " gone; tearing down binding" << LL_ENDL;
        teardownDistributedBinding(r);
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
}

void LLPositionalStreamMgr::shutdownPrimBindings()
{
    if (!mBindings.empty())
    {
        LL_INFOS("Stream3D") << "Tearing down " << mBindings.size()
                              << " mono prim bindings" << LL_ENDL;
        // ~Binding's unique_ptr<> destructor invokes the stream's stop(),
        // which calls FMOD Channel::stop() synchronously. Output device
        // buffer drain (<50ms typical) is the only residual delay.
        mBindings.clear();
    }
    if (!mDistributedBindings.empty())
    {
        LL_INFOS("Stream3D") << "Tearing down " << mDistributedBindings.size()
                              << " distributed-stereo bindings" << LL_ENDL;
        mDistributedBindings.clear();
        mPrimToRoot.clear();
    }
    // r8 F2-b: any pending priority polls were tied to bindings that no
    // longer exist; drop them so we don't burn poll budget after teardown.
    mPriorityPollQueue.clear();
}

void LLPositionalStreamMgr::shutdownAll()
{
    shutdownPrimBindings();
    // r8 F4: drop throttle history so a fresh session starts with no stale
    // suppression. mErrorThrottle is not load-bearing across sessions.
    mErrorThrottle.clear();
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

void LLPositionalStreamMgr::forceRescan()
{
    // Two-step rescan:
    //
    // (1) Re-evaluate every cached prim *immediately* using the description
    //     we already hold. evaluateBinding() does a string parse + (re)bind
    //     against mDescriptionCache without any network roundtrip, so tagged
    //     prims rebind within this tick — no need to wait for the round-robin
    //     scan walk to circle back (which can take >1 minute in dense regions
    //     where ObjectPropertiesFamily is rate-limited to 10 req/s).
    //
    // (2) Zero last_polled so the periodic poll loop will eventually re-issue
    //     ObjectPropertiesFamily for everything (in case any descriptions
    //     changed while Stream3DEnabled was off). This is the slow safety
    //     net; (1) is what users feel.
    //
    // mPollCursor is left alone — the round-robin walk picks up where it was.
    int rebind_candidates = 0;
    for (auto& kv : mDescriptionCache)
    {
        if (!kv.second.description.empty())
        {
            ++rebind_candidates;
            evaluateBinding(kv.first);
        }
        kv.second.last_polled = 0.0;
    }
    LL_INFOS("Stream3D") << "Forced rescan: " << rebind_candidates
                          << " cached descriptions re-evaluated, last_polled cleared on "
                          << mDescriptionCache.size() << " entries" << LL_ENDL;
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
    for (auto& [root_id, b] : mDistributedBindings)
    {
        if (b.stream)
        {
            b.stream->setVolume(volume);
        }
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

    // r8 F2-c: spot link/unlink/death once per scan tick. This is cheap and
    // independent of the request budget — it only reads gObjectList and may
    // call evaluateLinkset (which itself may enqueue priority polls picked
    // up by the drain below).
    detectLinksetStructureChanges();

    int budget = kBudgetPerScan;

    // r8 F2-b: drain the priority queue first. These are prims that
    // evaluateLinkset wants polled now (root or speaker that we don't yet
    // have description for). Distance / avatar filtering still applies; the
    // last_polled throttle still applies, since the priority hint and the
    // sim-friendly rate limit are independent concerns.
    int n_priority = 0;
    while (budget > 0 && !mPriorityPollQueue.empty())
    {
        const LLUUID id = mPriorityPollQueue.front();
        mPriorityPollQueue.pop_front();

        LLViewerObject* obj = gObjectList.findObject(id);
        if (!obj || obj->isDead()) continue;
        if (obj->isAvatar() || obj->isAttachment() || obj->isHUDAttachment()) continue;

        LLVector3d delta = obj->getPositionGlobal();
        delta -= agent_pos;
        if (static_cast<F32>(delta.magVecSquared()) > max_dist_sq) continue;

        auto& entry = mDescriptionCache[id];
        if (entry.last_polled > 0.0 && (now - entry.last_polled) < poll_interval) continue;

        LLSelectMgr::getInstance()->requestObjectPropertiesFamily(obj);
        entry.last_polled = now;
        --budget;
        ++n_priority;
    }

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
                           << " priority_sent=" << n_priority
                           << " priority_queue=" << mPriorityPollQueue.size()
                           << " examined=" << examined
                           << " cursor=" << mPollCursor
                           << "/" << num
                           << LL_ENDL;
}
