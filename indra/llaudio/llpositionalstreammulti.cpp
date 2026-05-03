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
            // Sum-to-mono with -6 dB for stereo source (spec §4.6). Mono
            // source is duplicated into both tracks at write time so this
            // collapses to the original sample.
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
    for (const auto& sr : mSpeakerRuntime)
    {
        if (sr.channel) return true;
    }
    return false;
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
    mReadFailStreak = 0;
    mLastReadFailLogTime = 0.0;
    mFailReason.store(FailReason::Ok, std::memory_order_relaxed);
    mFailDetail.clear();

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

void LLPositionalStreamMulti::setFailed(FailReason reason, std::string detail)
{
    mFailDetail = std::move(detail);
    // Order matters: detail + reason must be observable before the Failed
    // state publishes. mState's release pairs with isFailed()'s acquire.
    mFailReason.store(reason, std::memory_order_release);
    mState.store(State::Failed, std::memory_order_release);
}

void LLPositionalStreamMulti::releaseAll()
{
    llassert(!mDecodeThread.joinable());

    // Channels must be stopped before their backing OPENUSER sounds are
    // released; FMOD will warn (and may briefly stutter) otherwise.
    for (auto& sr : mSpeakerRuntime)
    {
        if (sr.channel)
        {
            checkFmod(sr.channel->stop(), "Channel::stop");
            sr.channel = nullptr;
        }
    }
    for (auto& sr : mSpeakerRuntime)
    {
        if (sr.user_sound)
        {
            checkFmod(sr.user_sound->release(), "Sound::release(user)");
            sr.user_sound = nullptr;
        }
    }
    // SpeakerCallback structs are referenced by the FMOD pcmreadcallback,
    // which won't fire again after the sound is released — safe to drop.
    mSpeakerRuntime.clear();

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
    mDownmix = LLMultichannelDownmix();
    // Don't reset mFailReason / mFailDetail here: releaseAll() runs as part
    // of the transition into Failed and the manager reads these immediately
    // afterwards. They're cleared on the next start() instead.
}

void LLPositionalStreamMulti::setSpeakerPosition(size_t idx, const LLVector3& pos)
{
    if (idx >= mSpeakers.size()) return;
    mSpeakers[idx].position = pos;
    if (idx < mSpeakerRuntime.size() && mSpeakerRuntime[idx].channel)
    {
        applyChannelAttributes(mSpeakerRuntime[idx].channel, pos, mSpeakers[idx].range);
    }
}

void LLPositionalStreamMulti::setVolume(F32 volume)
{
    mVolume = volume;
    for (size_t i = 0; i < mSpeakerRuntime.size() && i < mSpeakers.size(); ++i)
    {
        if (mSpeakerRuntime[i].channel)
        {
            checkFmod(mSpeakerRuntime[i].channel->setVolume(mVolume * mSpeakers[i].volume),
                      "Channel::setVolume");
        }
    }
}

void LLPositionalStreamMulti::applyChannelAttributes(FMOD::Channel* channel,
                                                     const LLVector3& pos,
                                                     F32 range)
{
    FMOD_VECTOR fpos = { pos.mV[0], pos.mV[1], pos.mV[2] };
    FMOD_VECTOR fvel = { 0.f, 0.f, 0.f };
    checkFmod(channel->set3DAttributes(&fpos, &fvel), "Channel::set3DAttributes");
    // Spec §4.3 fixes the inner radius to 1.0 m (no per-speaker {min} field
    // exists in r8; min was dropped from the tag format intentionally).
    checkFmod(channel->set3DMinMaxDistance(1.f, std::max(range, 1.1f)),
              "Channel::set3DMinMaxDistance");
}

// static
FMOD_RESULT F_CALL
LLPositionalStreamMulti::pcmReadCallback(FMOD_SOUND* sound, void* data, U32 datalen)
{
    void* ud = nullptr;
    reinterpret_cast<FMOD::Sound*>(sound)->getUserData(&ud);
    auto* cb = static_cast<SpeakerCallback*>(ud);
    F32* out = static_cast<F32*>(data);
    const size_t n = datalen / sizeof(F32);

    size_t got = 0;
    if (cb && cb->self && cb->speaker_idx < cb->self->mSpeakers.size())
    {
        // Channel role → ring read mode. Stable for the lifetime of the
        // SpeakerCallback (tag changes go through stop()/start(), which
        // tears down the FMOD sound first), so reading mSpeakers from the
        // mixer thread is safe.
        LLMultiTailRing::ReadMode mode = LLMultiTailRing::ReadMode::Mono;
        switch (cb->self->mSpeakers[cb->speaker_idx].ch)
        {
        case Channel::L: mode = LLMultiTailRing::ReadMode::Track0; break;
        case Channel::R: mode = LLMultiTailRing::ReadMode::Track1; break;
        case Channel::M: mode = LLMultiTailRing::ReadMode::Mono;   break;
        }
        got = cb->self->mRing.readFrames(cb->speaker_idx, out, n, mode);
    }
    if (got < n)
    {
        std::memset(out + got, 0, (n - got) * sizeof(F32));
        // r8 F6 acceptance: count the underrun for the dropout metric. Done
        // here, not in readFrames, so a speaker whose tail reads short still
        // registers even if the ring isn't globally empty.
        if (cb && cb->self)
        {
            cb->self->mUnderrunFrames.fetch_add(static_cast<U64>(n - got),
                                                std::memory_order_relaxed);
            cb->self->mUnderrunCallbacks.fetch_add(1, std::memory_order_relaxed);
        }
    }
    return FMOD_OK;
}

bool LLPositionalStreamMulti::createUserSounds()
{
    FMOD::System* system = getFmodSystem();
    if (!system) return false;

    mSpeakerRuntime.clear();
    mSpeakerRuntime.resize(mSpeakers.size());

    for (size_t i = 0; i < mSpeakers.size(); ++i)
    {
        FMOD_CREATESOUNDEXINFO ex = {};
        ex.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
        ex.numchannels = 1;
        ex.defaultfrequency = mSampleRate;
        ex.format = FMOD_SOUND_FORMAT_PCMFLOAT;
        ex.length = static_cast<U32>(mSampleRate) * sizeof(F32) * 5;
        ex.decodebuffersize = 4096;
        ex.pcmreadcallback = &pcmReadCallback;

        const FMOD_MODE mode = FMOD_OPENUSER
                             | FMOD_CREATESTREAM
                             | FMOD_LOOP_NORMAL
                             | FMOD_3D
                             | FMOD_3D_LINEARSQUAREROLLOFF;

        FMOD::Sound* snd = nullptr;
        if (checkFmod(system->createSound(nullptr, mode, &ex, &snd), "createSound(OPENUSER)"))
        {
            return false;
        }

        // Heap-allocated so the FMOD callback's stored pointer survives
        // moves of the speaker runtime vector (which we don't do, but it's
        // cheaper than promising never to).
        auto cb = std::make_unique<SpeakerCallback>();
        cb->self = this;
        cb->speaker_idx = i;
        checkFmod(snd->setUserData(cb.get()), "Sound::setUserData");

        mSpeakerRuntime[i].user_sound = snd;
        mSpeakerRuntime[i].cb = std::move(cb);
    }
    return true;
}

bool LLPositionalStreamMulti::startUserChannels()
{
    FMOD::System* system = getFmodSystem();
    if (!system) return false;

    for (size_t i = 0; i < mSpeakers.size(); ++i)
    {
        SpeakerRuntime& sr = mSpeakerRuntime[i];
        if (!sr.user_sound) return false;

        if (checkFmod(system->playSound(sr.user_sound, nullptr, true /*paused*/, &sr.channel),
                      "playSound(speaker)"))
        {
            sr.channel = nullptr;
            return false;
        }
        // r8 F9: pin priority to 0 (highest) immediately so the next iteration's
        // playSound() can't recycle this paused channel out from under us. With
        // 16 speakers, FMOD's free-channel pool is too small to hold them all,
        // and the default priority of a paused channel makes it the prime
        // recycle target — symptom is silent speakers whose pcmReadCallback
        // never fires, which then stalls the multi-tail ring writer.
        checkFmod(sr.channel->setPriority(0), "Channel::setPriority(speaker)");
        applyChannelAttributes(sr.channel, mSpeakers[i].position, mSpeakers[i].range);
        checkFmod(sr.channel->setVolume(mVolume * mSpeakers[i].volume),
                  "Channel::setVolume(speaker)");
    }

    // Sample-accurate sync across all N channels (spec §4.7). Same trick as
    // Stereo: read the mixer DSP clock from the first channel, schedule every
    // channel to begin at the same future sample index. ~20 ms lead is enough
    // for FMOD to commit the schedule before the first mixer tick consumes it.
    if (!mSpeakerRuntime.empty() && mSpeakerRuntime[0].channel)
    {
        unsigned long long parent_now = 0;
        checkFmod(mSpeakerRuntime[0].channel->getDSPClock(nullptr, &parent_now),
                  "Channel::getDSPClock");
        const unsigned long long lead = static_cast<unsigned long long>(mSampleRate) / 50;
        const unsigned long long start_at = parent_now + lead;
        for (auto& sr : mSpeakerRuntime)
        {
            if (sr.channel)
            {
                checkFmod(sr.channel->setDelay(start_at, 0, false), "Channel::setDelay");
            }
        }
    }

    for (auto& sr : mSpeakerRuntime)
    {
        if (sr.channel)
        {
            checkFmod(sr.channel->setPaused(false), "Channel::setPaused");
        }
    }
    return true;
}

size_t LLPositionalStreamMulti::pumpSource()
{
    if (!mSourceSound || mSampleRate <= 0 || mSourceChannels <= 0) return 0;
    // Once we've declared the stream dead, don't keep banging on a broken
    // source — the manager's reconnect loop will rebuild us.
    if (mState.load(std::memory_order_acquire) == State::Failed) return 0;

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
        // FMOD_ERR_NOTREADY just means "decoder hasn't fed us yet" — not a
        // fault. Real socket / EOF cascades return other codes; those count
        // toward kMaxReadFailStreak so the manager can rebuild this stream.
        if (rr != FMOD_ERR_NOTREADY)
        {
            ++mReadFailStreak;
            const F64 now = LLTimer::getElapsedSeconds();
            if (now - mLastReadFailLogTime >= 1.0)
            {
                LL_WARNS("Stream3D") << "Sound::readData error (" << mReadFailStreak
                                      << " consecutive): " << FMOD_ErrorString(rr) << LL_ENDL;
                mLastReadFailLogTime = now;
            }
            if (mReadFailStreak >= kMaxReadFailStreak)
            {
                LL_WARNS("Stream3D") << "Multi source readData failed " << mReadFailStreak
                                      << " times consecutively for " << mUrl
                                      << "; transitioning to Failed for reconnect" << LL_ENDL;
                setFailed(FailReason::Network, "readData streak");
            }
        }
        return 0;
    }
    if (read_bytes == 0) return 0;
    // Successful read: reset the failure streak so a later, independent
    // outage gets its own full budget rather than inheriting old strikes.
    mReadFailStreak = 0;

    const size_t frames_read = read_bytes / bytes_per_frame;
    const size_t n_tracks = mRing.numTracks();

    // Convert source PCM → ring's track layout. Ring is allocated with
    // n_tracks = max(source_channels, 2) so:
    //   - mono source (1 ch) into 2-track ring: copy the same sample into
    //     both tracks. ReadMode::Mono then returns (s+s)*0.5 = s, and ch=L/R
    //     speakers each see the full mono signal at their own 3D position.
    //   - stereo source (2 ch) into 2-track ring: 1:1 copy. ReadMode::Mono
    //     returns the spec-mandated -6 dB sum-to-mono.
    constexpr size_t kChunkFrames = kPumpChunkFrames;
    std::vector<F32> chunk(kChunkFrames * n_tracks, 0.f);

    const U8* sp = mReadScratch.data();
    size_t remaining = frames_read;

    while (remaining > 0)
    {
        const size_t this_chunk = std::min(remaining, kChunkFrames);
        std::fill(chunk.begin(), chunk.begin() + this_chunk * n_tracks, 0.f);

        if (mSourceChannels == 1)
        {
            // Mono source: duplicate into all ring tracks so every ReadMode
            // ends up with the original amplitude.
            if (mSourceIsFloat)
            {
                const F32* f32 = reinterpret_cast<const F32*>(sp);
                for (size_t i = 0; i < this_chunk; ++i)
                {
                    const F32 s = f32[i];
                    for (size_t t = 0; t < n_tracks; ++t)
                    {
                        chunk[i * n_tracks + t] = s;
                    }
                }
            }
            else
            {
                const S16* s16 = reinterpret_cast<const S16*>(sp);
                for (size_t i = 0; i < this_chunk; ++i)
                {
                    const F32 s = static_cast<F32>(s16[i]) * (1.f / 32768.f);
                    for (size_t t = 0; t < n_tracks; ++t)
                    {
                        chunk[i * n_tracks + t] = s;
                    }
                }
            }
        }
        else if (mSourceChannels == 6)
        {
            // r9: 6ch source → BS.775 downmix → 2-track ring. The downmix
            // expects F32 6ch interleaved; PCM16 input is widened into the
            // pre-sized mDownmixInputScratch first.
            const F32* in6 = nullptr;
            if (mSourceIsFloat)
            {
                in6 = reinterpret_cast<const F32*>(sp);
            }
            else
            {
                const S16* s16 = reinterpret_cast<const S16*>(sp);
                const size_t samples = this_chunk * 6;
                for (size_t i = 0; i < samples; ++i)
                {
                    mDownmixInputScratch[i] = static_cast<F32>(s16[i]) * (1.f / 32768.f);
                }
                in6 = mDownmixInputScratch.data();
            }
            // chunk is sized this_chunk * n_tracks (= 2), interleaved L,R.
            mDownmix.mix6chTo2chLR(in6, chunk.data(), this_chunk);
        }
        else if (mSourceIsFloat)
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
            setFailed(FailReason::Network, FMOD_ErrorString(rr));
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
            setFailed(FailReason::Network, "getFormat failed");
            return;
        }
        float freq = 44100.f;
        int prio = 0;
        if (checkFmod(mSourceSound->getDefaults(&freq, &prio), "Sound::getDefaults"))
        {
            releaseAll();
            setFailed(FailReason::Network, "getDefaults failed");
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
            setFailed(FailReason::FormatUnsupported,
                      llformat("sample-format=%d", (int)fmt));
            return;
        }

        mSampleRate = static_cast<int>(freq);
        mSourceChannels = channels;

        // r9 (§4.5): 6ch sources go through the BS.775 downmix to a 2-track
        // ring; 1ch / 2ch use the r8 path unchanged. Other channel counts
        // (3/4/5/7/8) and 6ch from unknown codecs are FormatUnsupported —
        // the manager's reconnect cascade reads failReason() and stops the
        // retry budget for these so the user sees one clear notification.
        if (mSourceChannels == 6)
        {
            mDownmix = LLMultichannelDownmix::forSourceFormat(type, mSourceChannels);
            if (!mDownmix.isSupported())
            {
                LL_WARNS("Stream3D") << "Multi source: 6ch from unsupported codec (FMOD type "
                                      << (int)type << ") — only Vorbis/Opus/FLAC accepted in r9: "
                                      << mUrl << LL_ENDL;
                releaseAll();
                setFailed(FailReason::FormatUnsupported,
                          llformat("channels=6 codec_type=%d", (int)type));
                return;
            }
            // Pre-size the F32 scratch used by the PCM16 widen step in
            // pumpSource(). PCMFLOAT sources never touch it, but the cost
            // is one allocation of 24 KB so we do it unconditionally.
            mDownmixInputScratch.assign(kPumpChunkFrames * 6, 0.f);
        }
        else if (mSourceChannels != 1 && mSourceChannels != 2)
        {
            LL_WARNS("Stream3D") << "Multi source: unsupported channel count "
                                  << mSourceChannels << " for " << mUrl
                                  << " (r9 accepts 1/2/6 only)" << LL_ENDL;
            releaseAll();
            setFailed(FailReason::FormatUnsupported,
                      llformat("channels=%d", mSourceChannels));
            return;
        }

        // r9: ring stays 2-track even for 6ch source (downmix to L/R happens
        // in pumpSource before writeFrames). Mono sources allocate 2 tracks
        // so ch=L/R speakers each see the full mono signal — pumpSource
        // duplicates the sample into both tracks at write time.
        const size_t n_tracks = 2;
        const size_t cap = nextPow2(kRingFrames);
        mRing.reset(cap, n_tracks, mSpeakers.size());
        mState = State::Buffering;

        LL_INFOS("Stream3D") << "Multi source ready: " << mUrl
                              << " " << mSampleRate << " Hz x " << mSourceChannels
                              << " ch, fmt=" << (mSourceIsFloat ? "PCMFLOAT" : "PCM16")
                              << (mSourceChannels == 6
                                  ? std::string(", downmix=") + mDownmix.layoutName()
                                  : std::string())
                              << ", ring cap " << cap << " frames × " << n_tracks << " tracks"
                              << ", speakers=" << mSpeakers.size() << LL_ENDL;

        startDecodeThread();
    }

    if (st == State::Buffering)
    {
        // Each speaker has its own reader tail; checking idx 0 alone is
        // enough because no reader has consumed yet (channels are still
        // unpaused below) so all tails read identical readAvailable values.
        if (mRing.readAvailable(0) >= kPrebufferFrames)
        {
            if (!createUserSounds() || !startUserChannels())
            {
                LL_WARNS("Stream3D") << "Failed to start OPENUSER channels for "
                                      << mUrl << LL_ENDL;
                // r7 M3 invariant: decode thread holds mSourceSound, must
                // join before releaseAll() reaches mSourceSound->release().
                stopDecodeThread();
                releaseAll();
                setFailed(FailReason::Network, "createUserSounds/startUserChannels");
                return;
            }
            mState = State::Playing;
            // r8 F6: anchor the warmup window for the dropout metric, and
            // zero the counters so any callbacks during Buffering→Playing
            // transition don't leak into the Playing measurement.
            mPlayingStartTime = LLTimer::getElapsedSeconds();
            mLastUnderrunLogTime = mPlayingStartTime;
            mUnderrunFrames.store(0, std::memory_order_relaxed);
            mUnderrunCallbacks.store(0, std::memory_order_relaxed);
            LL_INFOS("Stream3D") << "Multi path playing: " << mUrl
                                  << " (" << mSpeakers.size() << " speakers)" << LL_ENDL;
        }
    }

    if (st == State::Playing)
    {
        const F64 now = LLTimer::getElapsedSeconds();
        if (now - mPlayingStartTime >= kUnderrunWarmupSec &&
            now - mLastUnderrunLogTime >= kUnderrunLogPeriod)
        {
            const U64 frames = mUnderrunFrames.exchange(0, std::memory_order_relaxed);
            const U64 calls  = mUnderrunCallbacks.exchange(0, std::memory_order_relaxed);
            const F64 win    = now - mLastUnderrunLogTime;
            // Only log when there were actual underruns. Healthy streams
            // would otherwise spam an INFO line every 10 s. The counter
            // window itself still rolls forward so a single dropped frame
            // shows up as the integer it is, not as a fraction.
            if (frames > 0)
            {
                const F64 fps_per_spk = (mSpeakers.empty() || win <= 0.0)
                                      ? 0.0
                                      : static_cast<F64>(frames)
                                        / static_cast<F64>(mSpeakers.size())
                                        / win;
                LL_WARNS("Stream3D") << "Multi dropout (" << mUrl << "): "
                                      << frames << " zero-fill frames across "
                                      << calls << " callbacks over "
                                      << win << "s (" << fps_per_spk
                                      << " frames/spk/s, speakers="
                                      << mSpeakers.size() << ")" << LL_ENDL;
            }
            mLastUnderrunLogTime = now;
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
