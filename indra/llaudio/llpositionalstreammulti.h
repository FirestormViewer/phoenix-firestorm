/**
 * @file llpositionalstreammulti.h
 * @brief Distributed-description stereo: 1 stream → N speakers (AYAstorm r8).
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

#ifndef LL_POSITIONAL_STREAM_MULTI_H
#define LL_POSITIONAL_STREAM_MULTI_H

#include "llmultichanneldownmix.h"
#include "stdtypes.h"
#include "v3math.h"

#include "fmodstudio/fmod_common.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace FMOD
{
    class Sound;
    class Channel;
    class System;
}

// r8: multi-tail SPSC ring buffer for the distributed-stereo decode thread.
//
// Storage layout: capacity_frames × n_tracks F32 samples, interleaved per
// frame (L0,R0,L1,R1,... for n_tracks=2). The writer is the decode thread
// (Sound::readData() → push frames). Readers are the per-speaker FMOD mixer
// callbacks; each reader keeps its own tail index and pulls a single track
// (or a sum-to-mono of two tracks) out of the shared frame stream.
//
// Multi-tail vs. one-ring-per-speaker: a per-speaker ring would force the
// decode thread to write the same frame N times and would let readers drift
// out of sync if any one of them blocked. Sharing the underlying frames and
// keeping per-reader tails costs O(N) tails but keeps L/R/M phase-coherent
// for free.
class LLMultiTailRing
{
public:
    enum class ReadMode
    {
        Track0,  // pick channel 0 directly (used for ch=L on stereo source)
        Track1,  // pick channel 1 directly (used for ch=R on stereo source)
        Mono,    // (track0 + track1) * 0.5 (used for ch=M on stereo source)
    };

    LLMultiTailRing();
    ~LLMultiTailRing();
    LLMultiTailRing(const LLMultiTailRing&) = delete;
    LLMultiTailRing& operator=(const LLMultiTailRing&) = delete;

    // Allocate buffer for capacity_frames frames × n_tracks tracks, with
    // n_readers independent tails. All tails reset to 0.
    void reset(size_t capacity_frames, size_t n_tracks, size_t n_readers);
    void clear();

    size_t capacityFrames() const { return mCapacityFrames; }
    size_t numTracks() const { return mNumTracks; }
    size_t numReaders() const { return mNumReaders; }

    // Writer-side: how many frames may be appended before catching up to the
    // slowest reader. Determined by the lagging reader (min over tails).
    size_t writeAvailable() const;

    // Reader-side: how many frames are queued for `reader_idx` to consume.
    size_t readAvailable(size_t reader_idx) const;

    // Append n_frames of n_tracks-interleaved F32 samples from src. Returns
    // frames actually written (capped by writeAvailable()).
    size_t writeFrames(const F32* src, size_t n_frames);

    // Pull n_frames worth of mono F32 samples for a single reader, applying
    // ReadMode to select / mix tracks. dst length must be ≥ n_frames. Returns
    // frames actually read.
    size_t readFrames(size_t reader_idx, F32* dst, size_t n_frames, ReadMode mode);

private:
    // One slot reserved (capacity_frames + 1) so full vs. empty stays
    // distinguishable per reader the same way LLFloatRing does.
    std::vector<F32> mBuf;            // size = (capacity_frames + 1) * n_tracks
    size_t mCapacityFrames;           // logical capacity in frames (excl. slot)
    size_t mNumTracks;
    size_t mNumReaders;
    std::atomic<size_t> mWriteFrame;  // frame index of next write
    std::unique_ptr<std::atomic<size_t>[]> mReadFrames; // n_readers tails
};

// r8: distributed-description stereo stream. One source HTTP stream feeds N
// per-speaker FMOD channels via a shared multi-tail ring.
//
// Architecture mirrors LLPositionalStreamStereo but generalises the L/R pair
// to a vector of speakers, each carrying its own channel role (L / R / M),
// rolloff range, volume and 3D position. The state machine, decode thread
// and source-pump path are intentionally the same shape as Stereo so r7 M3's
// shutdown invariants still apply unchanged.
//
// F3-1 scope: source open + decode thread that fills the multi-tail ring.
// No OPENUSER speaker sounds yet; speakers are stored but not instantiated.
// F3-2 will add the per-speaker FMOD wiring + sample-accurate setDelay() sync.
class LLPositionalStreamMulti
{
public:
    // Mirrors LLPositionalStreamMgr::ChannelKind; duplicated here so llaudio
    // does not depend on indra/newview.
    enum class Channel { L, R, M };

    struct SpeakerConfig
    {
        Channel ch = Channel::M;
        F32 range = 20.f;       // FMOD set3DMinMaxDistance() max
        F32 volume = 1.f;       // 0..1, multiplied with global mVolume
        LLVector3 position;
    };

    LLPositionalStreamMulti();
    ~LLPositionalStreamMulti();
    LLPositionalStreamMulti(const LLPositionalStreamMulti&) = delete;
    LLPositionalStreamMulti& operator=(const LLPositionalStreamMulti&) = delete;

    // Begin opening url. speakers describes every output point (≥ 1, ≤ cap).
    bool start(const std::string& url, const std::vector<SpeakerConfig>& speakers);
    void stop();

    bool isOpen() const { return mSourceSound != nullptr; }
    bool isPlaying() const;
    bool isFailed() const { return mState.load(std::memory_order_acquire) == State::Failed; }

    // Update one speaker's 3D position (caller is responsible for indexing
    // the same way it set up the speaker vector in start()).
    void setSpeakerPosition(size_t idx, const LLVector3& pos);

    // Global volume multiplier on top of per-speaker volume.
    void setVolume(F32 volume);

    // Per-frame: drives source state, transitions opening→buffering→playing.
    void update();

private:
    enum class State { Idle, Opening, Buffering, Playing, Failed };

    // Per-sound user-data passed to FMOD's pcmreadcallback. Heap-allocated
    // because FMOD stores the pointer; the speaker_idx lets one static thunk
    // service all N speakers without scanning a sound→speaker map.
    struct SpeakerCallback
    {
        LLPositionalStreamMulti* self = nullptr;
        size_t speaker_idx = 0;
    };

    struct SpeakerRuntime
    {
        FMOD::Sound* user_sound = nullptr;
        FMOD::Channel* channel = nullptr;
        std::unique_ptr<SpeakerCallback> cb;
    };

    static FMOD_RESULT F_CALL pcmReadCallback(FMOD_SOUND* sound, void* data, U32 datalen);

    FMOD::System* getFmodSystem() const;

    void releaseAll();
    bool createUserSounds();
    bool startUserChannels();
    void applyChannelAttributes(FMOD::Channel* channel, const LLVector3& pos, F32 range);
    size_t pumpSource();

    void startDecodeThread();
    void stopDecodeThread();
    void decodeThreadMain();

    FMOD::Sound* mSourceSound;
    int mSampleRate;
    int mSourceChannels;       // 1, 2, or (r9) 6
    int mSourceBytesPerSample; // 2 for PCM16, 4 for PCMFLOAT
    bool mSourceIsFloat;

    // r9: populated at format detection when mSourceChannels == 6. The ring
    // is still allocated with n_tracks = 2 — pumpSource() converts each 6ch
    // frame to interleaved L/R via mDownmix before writing.
    LLMultichannelDownmix mDownmix;

    // Ring is sized at Opening→Buffering with n_tracks = 2 across all source
    // channel counts. Mono sources duplicate the sample into both tracks at
    // write time so ch=L/R speakers each see the full mono signal; 6ch sources
    // are downmixed to L/R via mDownmix before writeFrames(). r10 may lift
    // n_tracks for true per-channel routing without touching the reader side.
    LLMultiTailRing mRing;

    std::vector<SpeakerConfig> mSpeakers;
    std::vector<SpeakerRuntime> mSpeakerRuntime;

    F32 mVolume;
    std::string mUrl;

    std::atomic<State> mState;

    std::vector<U8> mReadScratch;
    // r9: F32 scratch used by the 6ch path when the source decodes as PCM16.
    // Holds up to kChunkFrames × 6 floats; pre-sized at format detection so
    // the decode thread never allocates on the audio path.
    std::vector<F32> mDownmixInputScratch;

    std::thread mDecodeThread;
    std::atomic<bool> mDecodeStop;
    std::mutex mDecodeMutex;
    std::condition_variable mDecodeCv;

    // r8 F7: detect a dead source so the manager's reconnect loop can rebuild
    // us. Counted on the decode thread; State::Failed is observed by the
    // manager via isFailed() with acquire ordering.
    int mReadFailStreak = 0;
    F64 mLastReadFailLogTime = 0.0;

    // r8 F6 acceptance instrumentation: count frames the FMOD mixer callback
    // had to zero-fill because the ring drained (decode thread fell behind
    // or the source stalled). Bumped from the FMOD mixer thread (multiple
    // threads, one per speaker), drained from update() on the main thread.
    // Logged at kUnderrunLogPeriod cadence after a warmup window so
    // prebuffer-drain transients don't show up as "dropouts".
    std::atomic<U64> mUnderrunFrames{0};
    std::atomic<U64> mUnderrunCallbacks{0};
    F64 mPlayingStartTime = 0.0;
    F64 mLastUnderrunLogTime = 0.0;

    static constexpr size_t kPrebufferFrames = 4096;
    static constexpr size_t kRingFrames      = 1 << 15; // ~0.74 s at 44.1 kHz
    // r9: chunk granularity for the source → ring conversion loop. Sized so a
    // single chunk fits comfortably in L1 (1024 frames × 6 ch × 4 B = 24 KB).
    static constexpr size_t kPumpChunkFrames = 1024;
    // ~1s of pumpSource failures (200 Hz pump) before declaring the stream
    // dead. Generous so brief network hiccups don't trip a teardown.
    static constexpr int kMaxReadFailStreak  = 200;
    // r8 F6: skip the first second after Playing transition to discount
    // prebuffer warmup; emit the rolling counter every 10s thereafter.
    static constexpr F64 kUnderrunWarmupSec  = 1.0;
    static constexpr F64 kUnderrunLogPeriod  = 10.0;
};

#endif // LL_POSITIONAL_STREAM_MULTI_H
