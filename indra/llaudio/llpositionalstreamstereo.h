/**
 * @file llpositionalstreamstereo.h
 * @brief Stereo-aware HTTP audio stream split across two 3D positions (AYAstorm M5).
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

#ifndef LL_POSITIONAL_STREAM_STEREO_H
#define LL_POSITIONAL_STREAM_STEREO_H

#include "stdtypes.h"
#include "v3math.h"

#include "fmodstudio/fmod_common.h"

#include <atomic>
#include <condition_variable>
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

// Single-producer / single-consumer ring buffer for F32 PCM samples.
// Producer: main-thread update() draining FMOD source via Sound::readData().
// Consumer: FMOD mixer thread invoking the OPENUSER pcmreadcallback.
class LLFloatRing
{
public:
    LLFloatRing();
    void reset(size_t capacity);
    void clear();
    size_t capacity() const { return mBuf.size(); }
    size_t readAvailable() const;
    size_t writeAvailable() const;
    size_t write(const F32* src, size_t n);
    size_t read(F32* dst, size_t n);

private:
    std::vector<F32> mBuf;
    std::atomic<size_t> mWriteIdx;
    std::atomic<size_t> mReadIdx;
};

// One HTTP stream split (or duplicated, in M5-a) across two 3D point sources.
// Architecture:
//   source FMOD::Sound (createStream, NEVER played) → polled with readData()
//     ↓ deinterleave / mix to mono per side
//   LLFloatRing × 2 (one per side)
//     ↓ pcmreadcallback drains
//   OPENUSER 3D mono FMOD::Sound × 2 → FMOD::Channel × 2 (positioned independently)
class LLPositionalStreamStereo
{
public:
    LLPositionalStreamStereo();
    ~LLPositionalStreamStereo();
    LLPositionalStreamStereo(const LLPositionalStreamStereo&) = delete;
    LLPositionalStreamStereo& operator=(const LLPositionalStreamStereo&) = delete;

    // Start opening url. Both sides start at l_pos / r_pos and update once
    // the source becomes ready and enough PCM has been buffered.
    bool start(const std::string& url, const LLVector3& l_pos, const LLVector3& r_pos);

    void stop();

    bool isOpen() const { return mSourceSound != nullptr; }
    bool isPlaying() const { return mChannelL != nullptr || mChannelR != nullptr; }
    // True after FMOD reports an unrecoverable error during open or playback.
    // The manager uses this to drive auto-reconnect (M7).
    bool isFailed() const { return mState == State::Failed; }

    void setPositions(const LLVector3& l_pos, const LLVector3& r_pos);
    void setVolume(F32 volume);
    void setRolloffDistances(F32 min_distance, F32 max_distance);

    // Per-frame: drives source readData, transitions opening→buffering→playing.
    void update();

private:
    enum class State { Idle, Opening, Buffering, Playing, Failed };

    // FMOD pcmreadcallback thunks (one per side). Both call back into the
    // owning instance via the Sound's user data pointer.
    static FMOD_RESULT F_CALL pcmReadCallbackL(FMOD_SOUND* sound, void* data, U32 datalen);
    static FMOD_RESULT F_CALL pcmReadCallbackR(FMOD_SOUND* sound, void* data, U32 datalen);

    FMOD::System* getFmodSystem() const;

    void releaseAll();
    bool createUserSounds();
    bool startUserChannels();
    void pumpSource();
    void applyChannelAttributes(FMOD::Channel* channel, const LLVector3& pos);

    // r7 M1: decode thread skeleton. M1 spins up a thread on State::Playing
    // and tears it down in stop(); the loop itself is empty here. M2 will
    // move pumpSource() into it. Spec: doc/spec_stream3d_decode_thread.md §4.
    void startDecodeThread();
    void stopDecodeThread();
    void decodeThreadMain();

    FMOD::Sound* mSourceSound;
    FMOD::Sound* mUserSoundL;
    FMOD::Sound* mUserSoundR;
    FMOD::Channel* mChannelL;
    FMOD::Channel* mChannelR;

    int mSampleRate;
    int mSourceChannels;       // 1 or 2
    int mSourceBytesPerSample; // 2 for PCM16, 4 for PCMFLOAT
    bool mSourceIsFloat;       // true: PCMFLOAT; false: PCM16

    LLFloatRing mRingL;
    LLFloatRing mRingR;

    LLVector3 mPositionL;
    LLVector3 mPositionR;
    F32 mVolume;
    F32 mRolloffMin;
    F32 mRolloffMax;
    std::string mUrl;

    State mState;

    // Scratch buffer reused by pumpSource() to avoid per-frame allocs.
    std::vector<U8> mReadScratch;

    // r7 M1: decode thread plumbing. mDecodeStop is set by stop()/dtor; the
    // condvar wakes the loop from its idle wait. mDecodeThread is non-joinable
    // until startDecodeThread() runs. Invariant: stop() must request-stop and
    // join *before* releaseAll() touches FMOD resources (M3 hardens this).
    std::thread mDecodeThread;
    std::atomic<bool> mDecodeStop;
    std::mutex mDecodeMutex;
    std::condition_variable mDecodeCv;

    // Frames of PCM to accumulate in each ring before unpausing playback.
    static constexpr size_t kPrebufferFrames = 4096;
    static constexpr size_t kRingFrames      = 1 << 15;  // ~0.74 s at 44.1 kHz
};

#endif // LL_POSITIONAL_STREAM_STEREO_H
