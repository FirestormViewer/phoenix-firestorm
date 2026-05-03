/**
 * @file llpositionalstreammulti.cpp
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

#include "linden_common.h"

#include "llpositionalstreammulti.h"

#include "llaudioengine.h"
#include "llaudioengine_fmodstudio.h"
#include "llstring.h"
#include "lltimer.h"

#include "fmodstudio/fmod.hpp"
#include "fmodstudio/fmod_errors.h"

#include <algorithm>
#include <chrono>
#include <cstring>

#if LL_LINUX
#  include <pthread.h>
#endif

namespace
{
    bool checkFmod(FMOD_RESULT result, const char* what)
    {
        if (result == FMOD_OK)
        {
            return false;
        }
        LL_WARNS("Stream3D") << what << " error: " << FMOD_ErrorString(result) << LL_ENDL;
        return true;
    }

    size_t nextPow2(size_t v)
    {
        size_t p = 1;
        while (p < v) p <<= 1;
        return p;
    }
}

// ---------------------------------------------------------------------------
// LLMultiTailRing
// ---------------------------------------------------------------------------

LLMultiTailRing::LLMultiTailRing()
:   mCapacityFrames(0),
    mNumTracks(0),
    mNumReaders(0),
    mWriteFrame(0)
{
}

LLMultiTailRing::~LLMultiTailRing() = default;

void LLMultiTailRing::reset(size_t capacity_frames, size_t n_tracks, size_t n_readers)
{
    mCapacityFrames = capacity_frames;
    mNumTracks = n_tracks;
    mNumReaders = n_readers;
    // (capacity + 1) frames so write_frame == read_frame can mean empty and
    // (write_frame + 1) % size == read_frame can mean full (one slot reserved).
    mBuf.assign((capacity_frames + 1) * n_tracks, 0.f);
    mWriteFrame.store(0, std::memory_order_relaxed);
    mReadFrames.reset(new std::atomic<size_t>[n_readers]);
    for (size_t i = 0; i < n_readers; ++i)
    {
        mReadFrames[i].store(0, std::memory_order_relaxed);
    }
}

void LLMultiTailRing::clear()
{
    mWriteFrame.store(0, std::memory_order_relaxed);
    for (size_t i = 0; i < mNumReaders; ++i)
    {
        mReadFrames[i].store(0, std::memory_order_relaxed);
    }
}

size_t LLMultiTailRing::writeAvailable() const
{
    if (mCapacityFrames == 0 || mNumReaders == 0) return 0;
    const size_t total = mCapacityFrames + 1;
    const size_t w = mWriteFrame.load(std::memory_order_acquire);
    // Writer must stay one slot behind the slowest reader. Compute lag for
    // each reader and take the max; free = capacity - max_lag.
    size_t max_lag = 0;
    for (size_t i = 0; i < mNumReaders; ++i)
    {
        const size_t r = mReadFrames[i].load(std::memory_order_acquire);
        const size_t lag = (w >= r) ? (w - r) : (total - r + w);
        if (lag > max_lag) max_lag = lag;
    }
    return mCapacityFrames - max_lag;
}

size_t LLMultiTailRing::readAvailable(size_t reader_idx) const
{
    if (reader_idx >= mNumReaders || mCapacityFrames == 0) return 0;
    const size_t total = mCapacityFrames + 1;
    const size_t w = mWriteFrame.load(std::memory_order_acquire);
    const size_t r = mReadFrames[reader_idx].load(std::memory_order_acquire);
    return (w >= r) ? (w - r) : (total - r + w);
}

size_t LLMultiTailRing::writeFrames(const F32* src, size_t n_frames)
{
    if (mCapacityFrames == 0 || mNumTracks == 0 || mNumReaders == 0) return 0;
    const size_t free_frames = writeAvailable();
    const size_t to_write = std::min(n_frames, free_frames);
    if (to_write == 0) return 0;

    const size_t total = mCapacityFrames + 1;
    const size_t w = mWriteFrame.load(std::memory_order_relaxed);
    const size_t first_chunk = std::min(to_write, total - w);
    std::memcpy(mBuf.data() + w * mNumTracks,
                src,
                first_chunk * mNumTracks * sizeof(F32));
    if (to_write > first_chunk)
    {
        std::memcpy(mBuf.data(),
                    src + first_chunk * mNumTracks,
                    (to_write - first_chunk) * mNumTracks * sizeof(F32));
    }
    mWriteFrame.store((w + to_write) % total, std::memory_order_release);
    return to_write;
}

size_t LLMultiTailRing::readFrames(size_t reader_idx, F32* dst, size_t n_frames, ReadMode mode)
{
    if (reader_idx >= mNumReaders || mCapacityFrames == 0 || mNumTracks == 0) return 0;
    const size_t avail = readAvailable(reader_idx);
    const size_t to_read = std::min(n_frames, avail);
    if (to_read == 0) return 0;

    const size_t total = mCapacityFrames + 1;
    const size_t r = mReadFrames[reader_idx].load(std::memory_order_relaxed);
    // Source-track indices clamped to what the ring actually has — guards
    // against e.g. ReadMode::Track1 on a 1-track ring (mono source). On a
    // 1-track ring all three modes degrade to "track 0".
    const size_t track_l = 0;
    const size_t track_r = (mNumTracks >= 2) ? 1 : 0;

    auto sample_at = [&](size_t frame_idx, size_t track) -> F32
    {
        return mBuf[frame_idx * mNumTracks + track];
    };

    switch (mode)
    {
    case ReadMode::Track0:
        for (size_t i = 0; i < to_read; ++i)
        {
            const size_t f = (r + i) % total;
            dst[i] = sample_at(f, track_l);
        }
        break;
    case ReadMode::Track1:
        for (size_t i = 0; i < to_read; ++i)
        {
            const size_t f = (r + i) % total;
            dst[i] = sample_at(f, track_r);
        }
        break;
    case ReadMode::Mono:
        for (size_t i = 0; i < to_read; ++i)
        {
            const size_t f = (r + i) % total;
            // sum-to-mono with -6 dB for L+R correlated content (spec §4.6).
            // For mono sources track_l == track_r so this becomes a noop *0.5
            // which is acoustically wrong; the writer compensates by never
            // duplicating mono → both tracks (it leaves the second track at
            // zero) — so Mono on a mono ring gets the original sample / 2.
            // F3-2 will revisit this once the writer side is wired.
            dst[i] = (sample_at(f, track_l) + sample_at(f, track_r)) * 0.5f;
        }
        break;
    }

    mReadFrames[reader_idx].store((r + to_read) % total, std::memory_order_release);
    return to_read;
}

// ---------------------------------------------------------------------------
// LLPositionalStreamMulti
// ---------------------------------------------------------------------------

LLPositionalStreamMulti::LLPositionalStreamMulti()
:   mSourceSound(nullptr),
    mSampleRate(0),
    mSourceChannels(0),
    mSourceBytesPerSample(0),
    mSourceIsFloat(false),
    mVolume(1.f),
    mState(State::Idle),
    mDecodeStop(false)
{
}

LLPositionalStreamMulti::~LLPositionalStreamMulti()
{
    if (gAudiop)
    {
        stop();
    }
    else
    {
        // r7 M3 shutdown invariant carried into r8: the decode thread holds
        // mSourceSound; if FMOD is already gone we must still join before
        // ~std::thread() calls std::terminate(). releaseAll() is skipped
        // because the FMOD pointers are already invalid.
        stopDecodeThread();
    }
}

FMOD::System* LLPositionalStreamMulti::getFmodSystem() const
{
    if (!gAudiop) return nullptr;
    LLAudioEngine_FMODSTUDIO* engine = dynamic_cast<LLAudioEngine_FMODSTUDIO*>(gAudiop);
    return engine ? engine->getSystem() : nullptr;
}

bool LLPositionalStreamMulti::isPlaying() const
{
    // F3-1 has no per-speaker channels yet, so "playing" tracks the state
    // machine. F3-2 will switch this to "any speaker channel != nullptr".
    return mState.load(std::memory_order_acquire) == State::Playing;
}

bool LLPositionalStreamMulti::start(const std::string& url,
                                    const std::vector<SpeakerConfig>& speakers)
{
    stop();

    if (speakers.empty())
    {
        LL_WARNS("Stream3D") << "Refusing to start multi stream with zero speakers" << LL_ENDL;
        return false;
    }

    std::string clean_url(url);
    LLStringUtil::trim(clean_url);
    if (clean_url.empty())
    {
        LL_WARNS("Stream3D") << "Refusing to start multi stream with empty URL" << LL_ENDL;
        return false;
    }

    FMOD::System* system = getFmodSystem();
    if (!system)
    {
        LL_WARNS("Stream3D") << "FMOD Studio system unavailable" << LL_ENDL;
        return false;
    }

    mUrl = clean_url;
    mSpeakers = speakers;

    const FMOD_MODE source_mode = FMOD_2D
                                | FMOD_NONBLOCKING
                                | FMOD_IGNORETAGS;

    if (checkFmod(system->createStream(clean_url.c_str(), source_mode, nullptr, &mSourceSound),
                  "createStream(source)"))
    {
        mSourceSound = nullptr;
        mUrl.clear();
        mSpeakers.clear();
        return false;
    }

    mState = State::Opening;
    LL_INFOS("Stream3D") << "Opening multi source '" << clean_url
                          << "' with " << mSpeakers.size() << " speaker(s)" << LL_ENDL;
    return true;
}

void LLPositionalStreamMulti::stop()
{
    // r7 M3 invariant: join decode thread before releasing FMOD resources it
    // is reading from.
    stopDecodeThread();
    releaseAll();
    mUrl.clear();
    mSpeakers.clear();
    mState = State::Idle;
}

void LLPositionalStreamMulti::releaseAll()
{
    llassert(!mDecodeThread.joinable());

    // F3-1: only mSourceSound exists. F3-2 will release per-speaker channels
    // and OPENUSER sounds here, in the right order (channels before sounds).
    if (mSourceSound)
    {
        checkFmod(mSourceSound->release(), "Sound::release(source)");
        mSourceSound = nullptr;
    }
    mRing.clear();
    mSampleRate = 0;
    mSourceChannels = 0;
    mSourceBytesPerSample = 0;
    mSourceIsFloat = false;
}

void LLPositionalStreamMulti::setSpeakerPosition(size_t idx, const LLVector3& pos)
{
    if (idx >= mSpeakers.size()) return;
    mSpeakers[idx].position = pos;
    // F3-2: push to mChannels[idx]->set3DAttributes().
}

void LLPositionalStreamMulti::setVolume(F32 volume)
{
    mVolume = volume;
    // F3-2: push (mVolume * speaker.volume) to each channel.
}

size_t LLPositionalStreamMulti::pumpSource()
{
    if (!mSourceSound || mSampleRate <= 0 || mSourceChannels <= 0) return 0;

    constexpr size_t kMaxFramesPerPump = 8192;

    const size_t free_frames = mRing.writeAvailable();
    const size_t want_frames = std::min(free_frames, kMaxFramesPerPump);
    if (want_frames == 0) return 0;

    const size_t bytes_per_frame = static_cast<size_t>(mSourceBytesPerSample)
                                 * static_cast<size_t>(mSourceChannels);
    const size_t want_bytes = want_frames * bytes_per_frame;
    if (mReadScratch.size() < want_bytes)
    {
        mReadScratch.resize(want_bytes);
    }

    U32 read_bytes = 0;
    FMOD_RESULT rr = mSourceSound->readData(mReadScratch.data(),
                                            static_cast<U32>(want_bytes),
                                            &read_bytes);
    if (rr != FMOD_OK && rr != FMOD_ERR_FILE_EOF)
    {
        if (rr != FMOD_ERR_NOTREADY)
        {
            LL_WARNS("Stream3D") << "Sound::readData error: "
                                  << FMOD_ErrorString(rr) << LL_ENDL;
        }
        return 0;
    }
    if (read_bytes == 0) return 0;

    const size_t frames_read = read_bytes / bytes_per_frame;
    const size_t n_tracks = mRing.numTracks();

    // Convert source PCM → ring's track layout. Ring is allocated with
    // n_tracks = max(source_channels, 2) so:
    //   - mono source (1 ch) into 2-track ring: copy sample into track 0,
    //     leave track 1 zero. Mono ReadMode reads (s+0)*0.5 which halves
    //     amplitude vs. the source; this is acoustically wrong but never
    //     hits the wire in F3-1 (no readers connected yet) and F3-2 picks
    //     the cheaper fix of duplicating into both tracks at write time.
    //   - stereo source (2 ch) into 2-track ring: 1:1 copy.
    constexpr size_t kChunkFrames = 1024;
    std::vector<F32> chunk(kChunkFrames * n_tracks, 0.f);

    const U8* sp = mReadScratch.data();
    size_t remaining = frames_read;

    while (remaining > 0)
    {
        const size_t this_chunk = std::min(remaining, kChunkFrames);
        std::fill(chunk.begin(), chunk.begin() + this_chunk * n_tracks, 0.f);

        if (mSourceIsFloat)
        {
            const F32* f32 = reinterpret_cast<const F32*>(sp);
            for (size_t i = 0; i < this_chunk; ++i)
            {
                for (int c = 0; c < mSourceChannels && c < (int)n_tracks; ++c)
                {
                    chunk[i * n_tracks + c] = f32[i * mSourceChannels + c];
                }
            }
        }
        else
        {
            const S16* s16 = reinterpret_cast<const S16*>(sp);
            for (size_t i = 0; i < this_chunk; ++i)
            {
                for (int c = 0; c < mSourceChannels && c < (int)n_tracks; ++c)
                {
                    chunk[i * n_tracks + c] =
                        static_cast<F32>(s16[i * mSourceChannels + c]) * (1.f / 32768.f);
                }
            }
        }

        mRing.writeFrames(chunk.data(), this_chunk);

        sp += this_chunk * bytes_per_frame;
        remaining -= this_chunk;
    }

    return read_bytes;
}

void LLPositionalStreamMulti::update()
{
    const State st = mState.load(std::memory_order_acquire);
    if (st == State::Idle || st == State::Failed) return;
    if (!mSourceSound) return;

    if (st == State::Opening)
    {
        FMOD_OPENSTATE state = FMOD_OPENSTATE_LOADING;
        FMOD_RESULT rr = mSourceSound->getOpenState(&state, nullptr, nullptr, nullptr);
        if (rr != FMOD_OK || state == FMOD_OPENSTATE_ERROR)
        {
            LL_WARNS("Stream3D") << "Multi source open failed: " << mUrl
                                  << " (" << FMOD_ErrorString(rr) << ")" << LL_ENDL;
            releaseAll();
            mUrl.clear();
            mState = State::Failed;
            return;
        }
        if (state != FMOD_OPENSTATE_READY && state != FMOD_OPENSTATE_PLAYING)
        {
            return;
        }

        FMOD_SOUND_TYPE type;
        FMOD_SOUND_FORMAT fmt;
        int channels = 0;
        int bits = 0;
        if (checkFmod(mSourceSound->getFormat(&type, &fmt, &channels, &bits), "Sound::getFormat"))
        {
            releaseAll();
            mState = State::Failed;
            return;
        }
        float freq = 44100.f;
        int prio = 0;
        if (checkFmod(mSourceSound->getDefaults(&freq, &prio), "Sound::getDefaults"))
        {
            releaseAll();
            mState = State::Failed;
            return;
        }
        if (fmt == FMOD_SOUND_FORMAT_PCMFLOAT)
        {
            mSourceIsFloat = true;
            mSourceBytesPerSample = 4;
        }
        else if (fmt == FMOD_SOUND_FORMAT_PCM16)
        {
            mSourceIsFloat = false;
            mSourceBytesPerSample = 2;
        }
        else
        {
            LL_WARNS("Stream3D") << "Source format unsupported (got " << (int)fmt
                                  << "); aborting multi path" << LL_ENDL;
            releaseAll();
            mState = State::Failed;
            return;
        }

        mSampleRate = static_cast<int>(freq);
        mSourceChannels = channels;

        // r10 prep: parameterise N_track. Stereo path uses 2; a future 5.1
        // path would set this to 6 and add Track2..Track5 ReadModes. Mono
        // sources still allocate 2 tracks because ch=L/R speakers expect a
        // stereo-shaped ring upstream.
        const size_t n_tracks = std::max(2, mSourceChannels);
        const size_t cap = nextPow2(kRingFrames);
        mRing.reset(cap, n_tracks, mSpeakers.size());
        mState = State::Buffering;

        LL_INFOS("Stream3D") << "Multi source ready: " << mUrl
                              << " " << mSampleRate << " Hz x " << mSourceChannels
                              << " ch, fmt=" << (mSourceIsFloat ? "PCMFLOAT" : "PCM16")
                              << ", ring cap " << cap << " frames × " << n_tracks << " tracks"
                              << ", speakers=" << mSpeakers.size() << LL_ENDL;

        startDecodeThread();
    }

    if (st == State::Buffering)
    {
        // F3-1: there are no readers yet, so writeAvailable() never shrinks
        // and the ring fills up to capacity. Promote to Playing as soon as
        // we have the prebuffer's worth and let F3-2 actually wire OPENUSER
        // channels here. Until F3-2, "Playing" simply means "the decode
        // thread is keeping up"; nothing reaches the speakers.
        if (mRing.readAvailable(0) >= kPrebufferFrames)
        {
            mState = State::Playing;
            LL_INFOS("Stream3D") << "Multi path buffered (F3-1: no audio output yet): "
                                  << mUrl << LL_ENDL;
        }
    }
}

void LLPositionalStreamMulti::startDecodeThread()
{
    if (mDecodeThread.joinable()) return;
    mDecodeStop.store(false, std::memory_order_release);
    mDecodeThread = std::thread(&LLPositionalStreamMulti::decodeThreadMain, this);
    LL_INFOS("Stream3D") << "Multi decode thread started for " << mUrl << LL_ENDL;
}

void LLPositionalStreamMulti::stopDecodeThread()
{
    if (!mDecodeThread.joinable()) return;
    {
        std::lock_guard<std::mutex> lk(mDecodeMutex);
        mDecodeStop.store(true, std::memory_order_release);
    }
    mDecodeCv.notify_all();
    mDecodeThread.join();
    LL_INFOS("Stream3D") << "Multi decode thread joined for " << mUrl << LL_ENDL;
}

void LLPositionalStreamMulti::decodeThreadMain()
{
#if LL_LINUX
    pthread_setname_np(pthread_self(), "Stream3D-multi");
#endif

    static constexpr auto kPumpInterval = std::chrono::milliseconds(5);

    while (!mDecodeStop.load(std::memory_order_acquire))
    {
        pumpSource();

        std::unique_lock<std::mutex> lk(mDecodeMutex);
        mDecodeCv.wait_for(lk, kPumpInterval,
                           [this] { return mDecodeStop.load(std::memory_order_acquire); });
    }
}
