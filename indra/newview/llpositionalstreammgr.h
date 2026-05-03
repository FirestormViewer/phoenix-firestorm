/**
 * @file llpositionalstreammgr.h
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

#ifndef LL_POSITIONAL_STREAM_MGR_H
#define LL_POSITIONAL_STREAM_MGR_H

#include "lluuid.h"
#include "stdtypes.h"
#include "v3math.h"

#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class LLPositionalStream;
class LLPositionalStreamStereo;
class LLPositionalStreamMulti;

class LLPositionalStreamMgr
{
public:
    static LLPositionalStreamMgr& instance();

    // Per-frame: poll FMOD async opens, refresh positions of bound prims,
    // drop bindings whose object disappeared.
    void update();

    // Called from the message handlers that decode ObjectProperties /
    // ObjectPropertiesFamily. Caches the description, and (re)evaluates
    // whether this prim should be bound to a stream.
    void onObjectPropertiesReceived(const LLUUID& id, const std::string& description);

    // Apply a global default rolloff to all currently-active streams.
    // Per-prim tags that explicitly set min/max are NOT overridden.
    void applyDefaultRolloff(F32 default_min, F32 default_max);

    // M7: Apply the global master volume (0..1) to every active stream.
    // Driven by Stream3DVolumeMaster signal; new bindings also pick up the
    // current setting at bind time.
    void applyMasterVolume(F32 volume);

    // M8: Tear down every prim binding (mono + stereo) but leave debug streams
    // alone. Used when Stream3DDescriptionScan is toggled off.
    void shutdownPrimBindings();

    // M8: Tear down everything — prim bindings AND debug streams. Used when
    // the master Stream3DEnabled kill switch is toggled off.
    void shutdownAll();

    // r7: Reset every prim's last-polled stamp so the next scan tick treats
    // them all as un-polled and (re)issues ObjectPropertiesFamily requests.
    // Used when Stream3DEnabled is flipped back on, so we don't make the user
    // wait up to Stream3DPollInterval seconds before tagged prims rebind.
    void forceRescan();

    // Debug toggle stream (driven by Stream3DDebugPlay). Independent of
    // the prim binding map.
    void startDebug(const std::string& url, const LLVector3& world_pos);
    void stopDebug();

    // Stereo debug stream (driven by Stream3DDebugStereoPlay, M5-a spike).
    // Pulls PCM from a source HTTP stream and feeds two OPENUSER 3D mono
    // sounds. In M5-a both sides receive the same downmixed signal.
    void startDebugStereo(const std::string& url,
                          const LLVector3& l_pos,
                          const LLVector3& r_pos);
    void stopDebugStereo();

    // Mono tag parsed from a prim Description ([3dstream:{...}]; legacy
    // [ayastream:{...}] also accepted).
    struct TagData
    {
        std::string url;
        std::optional<F32> min;
        std::optional<F32> max;
    };

    // Returns parsed [3dstream:{...}{...}] tag, or nullopt if none /
    // malformed / missing URL. (Legacy [ayastream:...] is also accepted.)
    static std::optional<TagData> parseTag(const std::string& description);

    // r8: distributed-description stereo (1 source → N speakers).
    // Channel assignment of one speaker prim. r10 will extend this enum to
    // include FL/FR/C/LFE/SL/SR for 5.1 venue placement (see spec §9.1).
    enum class DistChannel { L, R, M };

    // r8: parsed [3dstream-stereo:{url:...}{range:...}{ch:...}{volume:...}] tag.
    // A single prim's description may declare:
    //   - source only (root):         {url}      [+ {range}]
    //   - speaker only (root/child):  {ch}       [+ {range} + {volume}]
    //   - source + self-speaker:      {url}{ch}  [+ {range} + {volume}]
    // The same {range} field, when present, fills both range_default (source
    // role) and range_speaker (speaker role) of the same prim — the spec
    // §4.3 treats it as a single shared field rather than two separate keys.
    struct DistStereoTagData
    {
        // Source declaration fields (set only when {url:...} is present).
        std::optional<std::string> url;
        std::optional<F32> range_default;

        // Speaker declaration fields (set only when {ch:...} is present).
        std::optional<DistChannel> ch;
        std::optional<F32> range_speaker;
        std::optional<F32> volume; // 0.0 .. 1.0
    };

    // r8: parse-time error categories. The error string carries the offending
    // input value (e.g. "X" for {ch:X}, "1.5" for {volume:1.5}) so the F4
    // throttled notifier can echo it back to the user.
    //
    // Note: `Ok` (not `None`) for the no-error sentinel — X11 headers define
    // `None` as a macro (0L), and llpositionalstreammgr.h is reached through
    // PCH paths that include X11/Xlib.h.
    enum class DistParseError
    {
        Ok,
        BadCh,        // {ch:...} value not L/R/M (case-insensitive)
        BadRange,     // {range:N} not > 0 or unparseable
        BadVolume,    // {volume:N} not in [0.0, 1.0] or unparseable
        EmptyUrl,     // {url:} empty
    };

    struct DistParseResult
    {
        // data has value only when the tag has at least one of {url} / {ch}
        // and all present fields parse cleanly. nullopt with error==Ok
        // means "no recognizable [3dstream-stereo:...] tag at all"; nullopt
        // with error!=Ok means "tag present but a field is malformed".
        std::optional<DistStereoTagData> data;
        DistParseError error = DistParseError::Ok;
        std::string bad_value;
    };

    // r8: parse the [3dstream-stereo:...] body for the distributed format.
    //
    // Returns:
    //   - tag absent              → {nullopt, Ok, ""}
    //   - tag present, all valid  → {data,    Ok, ""}
    //   - tag present, field bad  → {nullopt, <error>, bad_value}
    //
    // The tag is recognized when {url} or {ch} is present. The legacy
    // {l:N}{r:N} format (r5–r7) is no longer supported in r8.
    static DistParseResult parseDistributedStereoTag(const std::string& description);

private:
    LLPositionalStreamMgr();
    ~LLPositionalStreamMgr();
    LLPositionalStreamMgr(const LLPositionalStreamMgr&) = delete;
    LLPositionalStreamMgr& operator=(const LLPositionalStreamMgr&) = delete;

    struct Binding
    {
        std::string url;
        // Values explicitly set by the prim's tag — nullopt means "fall
        // back to the global default" so global slider changes still apply.
        std::optional<F32> tag_min;
        std::optional<F32> tag_max;
        F32 applied_min = 1.f;
        F32 applied_max = 20.f;
        // M7: reconnect bookkeeping. reconnect_attempts counts consecutive
        // failed retries; reset to 0 on successful playback. next_retry_time
        // is the monotonic-seconds timestamp at which the next retry should
        // fire (0 = no retry pending).
        S32 reconnect_attempts = 0;
        F64 next_retry_time = 0.0;
        // M8: set true once the stream has reached Playing for the first time
        // since this Binding was created. Suppresses duplicate "now playing"
        // toasts on every reconnect-success transition (only the *initial*
        // bind notifies; rebinds inherit a fresh false because new Binding).
        bool notified_played = false;
        std::unique_ptr<LLPositionalStream> stream;
    };

    // r8 F2-a: one entry per speaker prim that participates in a distributed
    // binding. `range` is the per-speaker resolved value (slot {range} → root
    // {range} → settings.xml fallback) — F3 reads it directly when calling
    // FMOD set3DMinMaxDistance, so the precedence resolution lives only in
    // evaluateLinkset, not in the streaming layer.
    struct SpeakerSlot
    {
        LLUUID prim_id;
        DistChannel ch = DistChannel::M;
        F32 range = 20.f;
        F32 volume = 1.f;
    };

    // r8 F2-a: aggregated linkset state for one distributed-stereo source.
    // Keyed by root_id in mDistributedBindings.
    struct DistributedStereoBinding
    {
        LLUUID root_id;
        std::string url;
        F32 range_default = 20.f;
        std::vector<SpeakerSlot> speakers;
        // Count of speakers truncated by the per-binding cap. Surfaced in
        // F4 throttled notification; F2-a only logs.
        S32 dropped_speakers = 0;
        // r8 F3-3: live FMOD-backed multi-speaker stream. nullptr while the
        // linkset is still incomplete (priority-poll pending) or when the
        // last (re)evaluation deferred a restart. evaluateLinkset compares
        // the new (url, speakers) tuple against the old; identical tuples
        // keep the existing stream so unrelated tag edits in the linkset
        // don't audibly bounce the audio.
        std::unique_ptr<LLPositionalStreamMulti> stream;
        // r8 F7: reconnect bookkeeping mirrors the mono Binding loop so a
        // socket-level outage rebuilds the stream instead of leaving the
        // linkset silent.
        S32 reconnect_attempts = 0;
        F64 next_retry_time = 0.0;
        bool notified_played = false;
    };

    // r8 F4: throttled error notification. Keyed by (prim_id, kind) so the
    // user gets one toast per failure mode per 30 seconds even if the parse /
    // start path is hit on every poll tick. spec §4.9.
    enum class DistErrorKind
    {
        BadCh,
        BadRange,
        BadVolume,
        EmptyUrl,
        NoSpeakers,
        SpeakerOverLimit,
        StreamStartFailed,
    };

    // detail carries the raw bad value (e.g. "X" for {ch:X}, "1.5" for
    // {volume:1.5}) or an over-limit count, depending on kind. Empty for
    // kinds that don't have a useful payload (NoSpeakers).
    void notifyDistributedError(const LLUUID& prim_id, DistErrorKind kind,
                                const std::string& detail);

    void evaluateBinding(const LLUUID& id);
    void evaluateMonoBinding(const LLUUID& id, const TagData& tag);

    // r8 F2-a: (re)build the distributed-stereo binding rooted at root_id by
    // walking the linkset and harvesting whatever speaker descriptions are
    // already in mDescriptionCache. r8 F2-b: any participating prim whose
    // description is not yet cached is enqueued onto mPriorityPollQueue so
    // the binding completes within a few poll ticks rather than waiting for
    // round-robin discovery.
    // root_id is taken by value: evaluateLinkset() may erase entries from
    // mPrimToRoot whose `.second` is the very reference a caller would pass
    // (e.g. evaluateAndDispatch hands us `pr_it->second`). Copying defuses
    // that use-after-free without auditing every call site.
    void evaluateLinkset(LLUUID root_id);
    void teardownDistributedBinding(const LLUUID& root_id);

    // r8 F2-b: push id onto mPriorityPollQueue if not already queued.
    // Linear scan dedup is fine — the queue is bounded by ~16 speakers per
    // pending linkset and drains every poll tick.
    void enqueuePriorityPoll(const LLUUID& id);

    // r8 F2-c: scan mPrimToRoot looking for prims whose getRootEdit() no
    // longer matches the registered root (link / unlink / death). Both the
    // stale and the current root are re-evaluated, which transparently
    // moves the speaker slot between linksets or tears down a binding when
    // a participating prim is gone. Cheap: O(mPrimToRoot.size()) per tick,
    // bounded by max_speakers × active bindings.
    void detectLinksetStructureChanges();

    // M3b: walk in-range prims and re-poll RequestObjectPropertiesFamily for
    // any whose Description we haven't seen recently. Throttled so we never
    // burst more than a handful of requests per second at the sim. r8 F2-b:
    // mPriorityPollQueue is drained first (within the same per-tick budget)
    // so distributed-stereo linksets complete promptly.
    void pollObjectPropertiesFamily(F64 now_seconds);

    struct CacheEntry
    {
        std::string description;
        // Monotonic seconds (LLTimer::getElapsedSeconds) of the most recent
        // request OR reply for this prim. 0 means "never seen". Used by the
        // poll loop to throttle re-requests to Stream3DPollInterval.
        F64 last_polled = 0.0;
    };

    std::map<LLUUID, CacheEntry> mDescriptionCache;
    std::map<LLUUID, Binding> mBindings;
    // r8 F2-a: distributed-stereo bindings, keyed by root prim id.
    std::map<LLUUID, DistributedStereoBinding> mDistributedBindings;
    // r8 F2-a: reverse index for O(log N) child→root resolution from
    // onObjectPropertiesReceived. Populated/cleared by evaluateLinkset and
    // teardownDistributedBinding so it stays in sync with mDistributedBindings.
    std::map<LLUUID, LLUUID> mPrimToRoot;
    std::unique_ptr<LLPositionalStream> mDebugStream;
    std::unique_ptr<LLPositionalStreamStereo> mDebugStereoStream;

    // M3b: throttle for the per-frame poll scan.
    F64 mLastPollScanTime = 0.0;
    // M3b: round-robin cursor into gObjectList so per-pass budget doesn't
    // starve prims past the first slice when num_objects > one pass can cover.
    S32 mPollCursor = 0;
    // r8 F2-b: prims that evaluateLinkset wants polled ahead of the
    // round-robin scan because they belong to a linkset where a source
    // declaration was just observed. Drained at the head of every
    // pollObjectPropertiesFamily tick, sharing the same per-tick budget.
    std::deque<LLUUID> mPriorityPollQueue;
    // M8: edge-trigger for the "max concurrent reached" toast. Set true the
    // first time we refuse a binding due to the cap; reset to false whenever
    // total binding count drops back below the cap, so the user sees the
    // notification once per "newly full" event rather than every poll cycle.
    bool mCapNotified = false;

    // r8 F4: last-notified timestamp per (prim, kind). 30s suppression.
    // The map can in principle accumulate entries indefinitely (one per prim
    // that ever produced an error) but each entry is small (~32 B) and the
    // population in practice tracks the user's tagged prim count, so a prune
    // pass is not yet required. Cleared by shutdownAll().
    std::map<std::pair<LLUUID, DistErrorKind>, F64> mErrorThrottle;
};

#endif // LL_POSITIONAL_STREAM_MGR_H
