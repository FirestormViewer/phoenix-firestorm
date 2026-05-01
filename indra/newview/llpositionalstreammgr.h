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

#include <map>
#include <memory>
#include <optional>
#include <string>

class LLPositionalStream;
class LLPositionalStreamStereo;

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
    // Driven by AYAStreamVolumeMaster signal; new bindings also pick up the
    // current setting at bind time.
    void applyMasterVolume(F32 volume);

    // M8: Tear down every prim binding (mono + stereo) but leave debug streams
    // alone. Used when AYAStreamDescriptionScan is toggled off.
    void shutdownPrimBindings();

    // M8: Tear down everything — prim bindings AND debug streams. Used when
    // the master AYAStreamEnabled kill switch is toggled off.
    void shutdownAll();

    // Debug toggle stream (driven by AYAStreamDebugPlay). Independent of
    // the prim binding map.
    void startDebug(const std::string& url, const LLVector3& world_pos);
    void stopDebug();

    // Stereo debug stream (driven by AYAStreamDebugStereoPlay, M5-a spike).
    // Pulls PCM from a source HTTP stream and feeds two OPENUSER 3D mono
    // sounds. In M5-a both sides receive the same downmixed signal.
    void startDebugStereo(const std::string& url,
                          const LLVector3& l_pos,
                          const LLVector3& r_pos);
    void stopDebugStereo();

    // Mono tag parsed from a prim Description ([ayastream:{...}]).
    struct TagData
    {
        std::string url;
        std::optional<F32> min;
        std::optional<F32> max;
    };

    // Stereo tag parsed from a prim Description ([ayastream-stereo:{...}]).
    // L / R are linkset link numbers (1 = root, 2 = first child, ...).
    struct StereoTagData
    {
        std::string url;
        std::optional<F32> min;
        std::optional<F32> max;
        S32 l_link = 1;
        S32 r_link = 2;
    };

    // Returns parsed [ayastream:{...},{...}] tag, or nullopt if none /
    // malformed / missing URL.
    static std::optional<TagData> parseTag(const std::string& description);

    // Returns parsed [ayastream-stereo:{...}] tag, or nullopt if none /
    // malformed / missing URL / L == R.
    static std::optional<StereoTagData> parseStereoTag(const std::string& description);

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

    struct StereoBinding
    {
        std::string url;
        std::optional<F32> tag_min;
        std::optional<F32> tag_max;
        F32 applied_min = 1.f;
        F32 applied_max = 20.f;
        S32 l_link = 1;
        S32 r_link = 2;
        // Resolved at bind time by walking the linkset; refreshed on each
        // re-evaluation in case the linkset changes.
        LLUUID l_prim;
        LLUUID r_prim;
        S32 reconnect_attempts = 0;
        F64 next_retry_time = 0.0;
        bool notified_played = false;
        std::unique_ptr<LLPositionalStreamStereo> stream;
    };

    void evaluateBinding(const LLUUID& id);
    void evaluateMonoBinding(const LLUUID& id, const TagData& tag);
    void evaluateStereoBinding(const LLUUID& id, const StereoTagData& tag);

    // M3b: walk in-range prims and re-poll RequestObjectPropertiesFamily for
    // any whose Description we haven't seen recently. Throttled so we never
    // burst more than a handful of requests per second at the sim.
    void pollObjectPropertiesFamily(F64 now_seconds);

    struct CacheEntry
    {
        std::string description;
        // Monotonic seconds (LLTimer::getElapsedSeconds) of the most recent
        // request OR reply for this prim. 0 means "never seen". Used by the
        // poll loop to throttle re-requests to AYAStreamPollInterval.
        F64 last_polled = 0.0;
    };

    std::map<LLUUID, CacheEntry> mDescriptionCache;
    std::map<LLUUID, Binding> mBindings;
    std::map<LLUUID, StereoBinding> mStereoBindings;
    std::unique_ptr<LLPositionalStream> mDebugStream;
    std::unique_ptr<LLPositionalStreamStereo> mDebugStereoStream;

    // M3b: throttle for the per-frame poll scan.
    F64 mLastPollScanTime = 0.0;
    // M3b: round-robin cursor into gObjectList so per-pass budget doesn't
    // starve prims past the first slice when num_objects > one pass can cover.
    S32 mPollCursor = 0;
    // M8: edge-trigger for the "max concurrent reached" toast. Set true the
    // first time we refuse a binding due to the cap; reset to false whenever
    // total binding count drops back below the cap, so the user sees the
    // notification once per "newly full" event rather than every poll cycle.
    bool mCapNotified = false;
};

#endif // LL_POSITIONAL_STREAM_MGR_H
