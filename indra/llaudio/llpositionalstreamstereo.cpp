/**
 * @file llpositionalstreamstereo.cpp
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

#include "linden_common.h"

#include "llpositionalstreamstereo.h"

#include "llaudioengine.h"
#include "llaudioengine_fmodstudio.h"
#include "llstring.h"

#include "fmodstudio/fmod.hpp"
#include "fmodstudio/fmod_errors.h"

#include <cstring>

namespace
{
    bool checkFmod(FMOD_RESULT result, const char* what)
    {
        if (result == FMOD_OK)
        {
            return false;
        }
        LL_WARNS("AYAStream") << what << " error: " << FMOD_ErrorString(result) << LL_ENDL;
        return true;
    }

    // round up to next power of two >= v (used for ring sizing).
    size_t nextPow2(size_t v)
    {
        size_t p = 1;
        while (p < v) p <<= 1;
        return p;
    }
}

// ---------------------------------------------------------------------------
// LLFloatRing
// ---------------------------------------------------------------------------

LLFloatRing::LLFloatRing()
:   mWriteIdx(0),
    mReadIdx(0)
{
}

void LLFloatRing::reset(size_t capacity)
{
    // capacity is in F32 samples; we keep one slot empty to disambiguate
    // full vs empty so request capacity+1.
    mBuf.assign(capacity + 1, 0.f);
    mWriteIdx.store(0, std::memory_order_relaxed);
    mReadIdx.store(0, std::memory_order_relaxed);
}

void LLFloatRing::clear()
{
    mWriteIdx.store(0, std::memory_order_relaxed);
    mReadIdx.store(0, std::memory_order_relaxed);
}

size_t LLFloatRing::readAvailable() const
{
    if (mBuf.empty()) return 0;
    size_t w = mWriteIdx.load(std::memory_order_acquire);
    size_t r = mReadIdx.load(std::memory_order_acquire);
    return (w >= r) ? (w - r) : (mBuf.size() - r + w);
}

size_t LLFloatRing::writeAvailable() const
{
    if (mBuf.empty()) return 0;
    // total slots minus one (reserved) minus what's already filled.
    return mBuf.size() - 1 - readAvailable();
}

size_t LLFloatRing::write(const F32* src, size_t n)
{
    if (mBuf.empty()) return 0;
    size_t w = mWriteIdx.load(std::memory_order_relaxed);
    size_t r = mReadIdx.load(std::memory_order_acquire);
    size_t free_slots = (mBuf.size() - 1 - ((w >= r) ? (w - r) : (mBuf.size() - r + w)));
    size_t to_write = (n < free_slots) ? n : free_slots;

    size_t first_chunk = std::min(to_write, mBuf.size() - w);
    std::memcpy(mBuf.data() + w, src, first_chunk * sizeof(F32));
    if (to_write > first_chunk)
    {
        std::memcpy(mBuf.data(), src + first_chunk, (to_write - first_chunk) * sizeof(F32));
    }
    mWriteIdx.store((w + to_write) % mBuf.size(), std::memory_order_release);
    return to_write;
}

size_t LLFloatRing::read(F32* dst, size_t n)
{
    if (mBuf.empty()) return 0;
    size_t r = mReadIdx.load(std::memory_order_relaxed);
    size_t w = mWriteIdx.load(std::memory_order_acquire);
    size_t available = (w >= r) ? (w - r) : (mBuf.size() - r + w);
    size_t to_read = (n < available) ? n : available;

    size_t first_chunk = std::min(to_read, mBuf.size() - r);
    std::memcpy(dst, mBuf.data() + r, first_chunk * sizeof(F32));
    if (to_read > first_chunk)
    {
        std::memcpy(dst + first_chunk, mBuf.data(), (to_read - first_chunk) * sizeof(F32));
    }
    mReadIdx.store((r + to_read) % mBuf.size(), std::memory_order_release);
    return to_read;
}

// ---------------------------------------------------------------------------
// LLPositionalStreamStereo
// ---------------------------------------------------------------------------

LLPositionalStreamStereo::LLPositionalStreamStereo()
:   mSourceSound(nullptr),
    mUserSoundL(nullptr),
    mUserSoundR(nullptr),
    mChannelL(nullptr),
    mChannelR(nullptr),
    mSampleRate(0),
    mSourceChannels(0),
    mSourceBytesPerSample(0),
    mSourceIsFloat(false),
    mPositionL(0.f, 0.f, 0.f),
    mPositionR(0.f, 0.f, 0.f),
    mVolume(1.f),
    mRolloffMin(1.f),
    mRolloffMax(20.f),
    mState(State::Idle)
{
}

LLPositionalStreamStereo::~LLPositionalStreamStereo()
{
    if (gAudiop)
    {
        stop();
    }
}

FMOD::System* LLPositionalStreamStereo::getFmodSystem() const
{
    if (!gAudiop) return nullptr;
    LLAudioEngine_FMODSTUDIO* engine = dynamic_cast<LLAudioEngine_FMODSTUDIO*>(gAudiop);
    return engine ? engine->getSystem() : nullptr;
}

bool LLPositionalStreamStereo::start(const std::string& url,
                                     const LLVector3& l_pos,
                                     const LLVector3& r_pos)
{
    stop();

    std::string clean_url(url);
    LLStringUtil::trim(clean_url);
    if (clean_url.empty())
    {
        LL_WARNS("AYAStream") << "Refusing to start stereo stream with empty URL" << LL_ENDL;
        return false;
    }

    FMOD::System* system = getFmodSystem();
    if (!system)
    {
        LL_WARNS("AYAStream") << "FMOD Studio system unavailable" << LL_ENDL;
        return false;
    }

    mUrl = clean_url;
    mPositionL = l_pos;
    mPositionR = r_pos;

    // Source: 2D, non-blocking, no implicit playback. We will drain it via
    // Sound::readData() rather than playSound().
    const FMOD_MODE source_mode = FMOD_2D
                                | FMOD_NONBLOCKING
                                | FMOD_IGNORETAGS;

    if (checkFmod(system->createStream(clean_url.c_str(), source_mode, nullptr, &mSourceSound),
                  "createStream(source)"))
    {
        mSourceSound = nullptr;
        mUrl.clear();
        return false;
    }

    mState = State::Opening;
    LL_INFOS("AYAStream") << "Opening stereo source '" << clean_url
                          << "' L=" << l_pos << " R=" << r_pos << LL_ENDL;
    return true;
}

void LLPositionalStreamStereo::stop()
{
    releaseAll();
    mUrl.clear();
    mState = State::Idle;
}

void LLPositionalStreamStereo::releaseAll()
{
    if (mChannelL)
    {
        checkFmod(mChannelL->stop(), "Channel::stop(L)");
        mChannelL = nullptr;
    }
    if (mChannelR)
    {
        checkFmod(mChannelR->stop(), "Channel::stop(R)");
        mChannelR = nullptr;
    }
    if (mUserSoundL)
    {
        checkFmod(mUserSoundL->release(), "Sound::release(userL)");
        mUserSoundL = nullptr;
    }
    if (mUserSoundR)
    {
        checkFmod(mUserSoundR->release(), "Sound::release(userR)");
        mUserSoundR = nullptr;
    }
    if (mSourceSound)
    {
        checkFmod(mSourceSound->release(), "Sound::release(source)");
        mSourceSound = nullptr;
    }
    mRingL.clear();
    mRingR.clear();
    mSampleRate = 0;
    mSourceChannels = 0;
    mSourceBytesPerSample = 0;
    mSourceIsFloat = false;
}

void LLPositionalStreamStereo::setPositions(const LLVector3& l_pos, const LLVector3& r_pos)
{
    mPositionL = l_pos;
    mPositionR = r_pos;
    if (mChannelL) applyChannelAttributes(mChannelL, l_pos);
    if (mChannelR) applyChannelAttributes(mChannelR, r_pos);
}

void LLPositionalStreamStereo::applyChannelAttributes(FMOD::Channel* channel, const LLVector3& pos)
{
    FMOD_VECTOR fpos = { pos.mV[0], pos.mV[1], pos.mV[2] };
    FMOD_VECTOR fvel = { 0.f, 0.f, 0.f };
    checkFmod(channel->set3DAttributes(&fpos, &fvel), "Channel::set3DAttributes");
}

void LLPositionalStreamStereo::setVolume(F32 volume)
{
    mVolume = volume;
    if (mChannelL) checkFmod(mChannelL->setVolume(volume), "Channel::setVolume(L)");
    if (mChannelR) checkFmod(mChannelR->setVolume(volume), "Channel::setVolume(R)");
}

void LLPositionalStreamStereo::setRolloffDistances(F32 min_distance, F32 max_distance)
{
    min_distance = llmax(min_distance, 0.1f);
    max_distance = llmax(max_distance, min_distance + 0.1f);
    mRolloffMin = min_distance;
    mRolloffMax = max_distance;
    if (mChannelL)
        checkFmod(mChannelL->set3DMinMaxDistance(mRolloffMin, mRolloffMax),
                  "Channel::set3DMinMaxDistance(L)");
    if (mChannelR)
        checkFmod(mChannelR->set3DMinMaxDistance(mRolloffMin, mRolloffMax),
                  "Channel::set3DMinMaxDistance(R)");
}

// static
FMOD_RESULT F_CALL
LLPositionalStreamStereo::pcmReadCallbackL(FMOD_SOUND* sound, void* data, U32 datalen)
{
    void* ud = nullptr;
    reinterpret_cast<FMOD::Sound*>(sound)->getUserData(&ud);
    auto* self = static_cast<LLPositionalStreamStereo*>(ud);
    F32* out = static_cast<F32*>(data);
    size_t n = datalen / sizeof(F32);
    size_t got = self ? self->mRingL.read(out, n) : 0;
    if (got < n)
    {
        std::memset(out + got, 0, (n - got) * sizeof(F32));
    }
    return FMOD_OK;
}

// static
FMOD_RESULT F_CALL
LLPositionalStreamStereo::pcmReadCallbackR(FMOD_SOUND* sound, void* data, U32 datalen)
{
    void* ud = nullptr;
    reinterpret_cast<FMOD::Sound*>(sound)->getUserData(&ud);
    auto* self = static_cast<LLPositionalStreamStereo*>(ud);
    F32* out = static_cast<F32*>(data);
    size_t n = datalen / sizeof(F32);
    size_t got = self ? self->mRingR.read(out, n) : 0;
    if (got < n)
    {
        std::memset(out + got, 0, (n - got) * sizeof(F32));
    }
    return FMOD_OK;
}

bool LLPositionalStreamStereo::createUserSounds()
{
    FMOD::System* system = getFmodSystem();
    if (!system) return false;

    auto build = [&](FMOD_SOUND_PCMREAD_CALLBACK cb, FMOD::Sound** out_sound) -> bool
    {
        FMOD_CREATESOUNDEXINFO ex = {};
        ex.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
        ex.numchannels = 1;
        ex.defaultfrequency = mSampleRate;
        ex.format = FMOD_SOUND_FORMAT_PCMFLOAT;
        // Length in BYTES; FMOD loops back when reaching it but our callback
        // keeps producing data so playback never genuinely ends.
        ex.length = static_cast<U32>(mSampleRate) * sizeof(F32) * 5;
        ex.decodebuffersize = 4096;
        ex.pcmreadcallback = cb;

        const FMOD_MODE mode = FMOD_OPENUSER
                             | FMOD_CREATESTREAM
                             | FMOD_LOOP_NORMAL
                             | FMOD_3D
                             | FMOD_3D_LINEARSQUAREROLLOFF;

        if (checkFmod(system->createSound(nullptr, mode, &ex, out_sound), "createSound(OPENUSER)"))
        {
            *out_sound = nullptr;
            return false;
        }
        checkFmod((*out_sound)->setUserData(this), "Sound::setUserData");
        return true;
    };

    if (!build(&pcmReadCallbackL, &mUserSoundL)) return false;
    if (!build(&pcmReadCallbackR, &mUserSoundR)) return false;
    return true;
}

bool LLPositionalStreamStereo::startUserChannels()
{
    FMOD::System* system = getFmodSystem();
    if (!system || !mUserSoundL || !mUserSoundR) return false;

    if (checkFmod(system->playSound(mUserSoundL, nullptr, true /*paused*/, &mChannelL),
                  "playSound(L)"))
    {
        mChannelL = nullptr;
        return false;
    }
    if (checkFmod(system->playSound(mUserSoundR, nullptr, true /*paused*/, &mChannelR),
                  "playSound(R)"))
    {
        mChannelR = nullptr;
        return false;
    }

    applyChannelAttributes(mChannelL, mPositionL);
    applyChannelAttributes(mChannelR, mPositionR);
    checkFmod(mChannelL->set3DMinMaxDistance(mRolloffMin, mRolloffMax),
              "Channel::set3DMinMaxDistance(L)");
    checkFmod(mChannelR->set3DMinMaxDistance(mRolloffMin, mRolloffMax),
              "Channel::set3DMinMaxDistance(R)");
    checkFmod(mChannelL->setVolume(mVolume), "Channel::setVolume(L)");
    checkFmod(mChannelR->setVolume(mVolume), "Channel::setVolume(R)");

    // Sample-accurate sync between L and R. Without this, the two channels
    // start on different mixer ticks (~5–20 ms skew), which on shared L/R
    // content causes a fixed image offset (Haas) and comb filtering.
    // Read the parent (mixer) DSP clock and schedule both channels to begin
    // at the same future sample index.
    unsigned long long parent_now = 0;
    checkFmod(mChannelL->getDSPClock(nullptr, &parent_now), "Channel::getDSPClock(L)");
    const unsigned long long lead = static_cast<unsigned long long>(mSampleRate) / 50; // ~20ms
    const unsigned long long start_at = parent_now + lead;
    checkFmod(mChannelL->setDelay(start_at, 0, false), "Channel::setDelay(L)");
    checkFmod(mChannelR->setDelay(start_at, 0, false), "Channel::setDelay(R)");

    checkFmod(mChannelL->setPaused(false), "Channel::setPaused(L)");
    checkFmod(mChannelR->setPaused(false), "Channel::setPaused(R)");
    return true;
}

void LLPositionalStreamStereo::pumpSource()
{
    if (!mSourceSound || mSampleRate <= 0 || mSourceChannels <= 0) return;

    // Try to top up both rings. We read enough source frames to fill whichever
    // ring has the least free space, capped at a reasonable per-frame amount
    // so we don't stall the main thread on a single update().
    constexpr size_t kMaxFramesPerPump = 8192;

    size_t free_l = mRingL.writeAvailable();
    size_t free_r = mRingR.writeAvailable();
    size_t want_frames = std::min({ free_l, free_r, kMaxFramesPerPump });
    if (want_frames == 0) return;

    const size_t bytes_per_frame = static_cast<size_t>(mSourceBytesPerSample)
                                 * static_cast<size_t>(mSourceChannels);
    size_t want_bytes = want_frames * bytes_per_frame;
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
        // FMOD returns FMOD_ERR_NOTREADY when no buffered data is currently
        // available; that's expected, just try again next frame.
        if (rr != FMOD_ERR_NOTREADY)
        {
            LL_WARNS("AYAStream") << "Sound::readData error: "
                                  << FMOD_ErrorString(rr) << LL_ENDL;
        }
        return;
    }
    if (read_bytes == 0) return;

    size_t frames_read = read_bytes / bytes_per_frame;

    // Convert (or pass-through) into F32 stack buffers in chunks. For mono
    // source we feed the same samples to both rings; for stereo we split the
    // interleaved L/R channels into separate rings (true M5-b deinterleave).
    constexpr size_t kChunk = 1024;
    F32 left[kChunk];
    F32 right[kChunk];

    const U8* sp = mReadScratch.data();
    size_t remaining = frames_read;

    while (remaining > 0)
    {
        size_t this_chunk = std::min(remaining, kChunk);

        if (mSourceChannels == 1)
        {
            // Mono source: same samples on both sides — there is no L/R to
            // separate. The 3D positions of the L and R channels still
            // produce different spatial cues from the listener's perspective.
            if (mSourceIsFloat)
            {
                std::memcpy(left, sp, this_chunk * sizeof(F32));
            }
            else
            {
                const S16* s16 = reinterpret_cast<const S16*>(sp);
                for (size_t i = 0; i < this_chunk; ++i)
                {
                    left[i] = static_cast<F32>(s16[i]) * (1.f / 32768.f);
                }
            }
            mRingL.write(left, this_chunk);
            mRingR.write(left, this_chunk);
        }
        else
        {
            // Stereo (or more): take channel 0 as L, channel 1 as R, ignore
            // any further channels. Source is FMOD-interleaved.
            if (mSourceIsFloat)
            {
                const F32* f32 = reinterpret_cast<const F32*>(sp);
                for (size_t i = 0; i < this_chunk; ++i)
                {
                    left[i]  = f32[i * mSourceChannels + 0];
                    right[i] = f32[i * mSourceChannels + 1];
                }
            }
            else
            {
                const S16* s16 = reinterpret_cast<const S16*>(sp);
                for (size_t i = 0; i < this_chunk; ++i)
                {
                    left[i]  = static_cast<F32>(s16[i * mSourceChannels + 0]) * (1.f / 32768.f);
                    right[i] = static_cast<F32>(s16[i * mSourceChannels + 1]) * (1.f / 32768.f);
                }
            }
            mRingL.write(left,  this_chunk);
            mRingR.write(right, this_chunk);
        }

        sp += this_chunk * bytes_per_frame;
        remaining -= this_chunk;
    }
}

void LLPositionalStreamStereo::update()
{
    if (mState == State::Idle || mState == State::Failed) return;
    if (!mSourceSound) return;

    if (mState == State::Opening)
    {
        FMOD_OPENSTATE state = FMOD_OPENSTATE_LOADING;
        FMOD_RESULT rr = mSourceSound->getOpenState(&state, nullptr, nullptr, nullptr);
        if (rr != FMOD_OK || state == FMOD_OPENSTATE_ERROR)
        {
            LL_WARNS("AYAStream") << "Stereo source open failed: " << mUrl
                                  << " (" << FMOD_ErrorString(rr) << ")" << LL_ENDL;
            releaseAll();
            mUrl.clear();
            mState = State::Failed;
            return;
        }
        if (state != FMOD_OPENSTATE_READY && state != FMOD_OPENSTATE_PLAYING)
        {
            return; // still buffering; check again next frame
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
            LL_WARNS("AYAStream") << "Source format unsupported (got " << (int)fmt
                                  << "); aborting stereo path" << LL_ENDL;
            releaseAll();
            mState = State::Failed;
            return;
        }

        mSampleRate = static_cast<int>(freq);
        mSourceChannels = channels;
        size_t cap = nextPow2(kRingFrames);
        mRingL.reset(cap);
        mRingR.reset(cap);
        mState = State::Buffering;

        LL_INFOS("AYAStream") << "Stereo source ready: " << mUrl
                              << " " << mSampleRate << " Hz x " << mSourceChannels
                              << " ch, fmt=" << (mSourceIsFloat ? "PCMFLOAT" : "PCM16")
                              << ", ring cap " << cap << " frames" << LL_ENDL;
    }

    pumpSource();

    if (mState == State::Buffering)
    {
        if (mRingL.readAvailable() >= kPrebufferFrames
            && mRingR.readAvailable() >= kPrebufferFrames)
        {
            if (!createUserSounds() || !startUserChannels())
            {
                LL_WARNS("AYAStream") << "Failed to start OPENUSER channels" << LL_ENDL;
                releaseAll();
                mState = State::Failed;
                return;
            }
            mState = State::Playing;
            LL_INFOS("AYAStream") << "Stereo path playing: " << mUrl << LL_ENDL;
        }
    }
}
