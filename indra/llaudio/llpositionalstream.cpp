/**
 * @file llpositionalstream.cpp
 * @brief 3D-positioned HTTP audio stream playback (AYAstorm).
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

#include "llpositionalstream.h"

#include "llaudioengine.h"
#include "llaudioengine_fmodstudio.h"
#include "llfasttimer.h"
#include "llstring.h"
#include "lltimer.h"

#include "fmodstudio/fmod.hpp"
#include "fmodstudio/fmod_errors.h"

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
}

LLPositionalStream::LLPositionalStream()
:   mSound(nullptr),
    mChannel(nullptr),
    mPosition(0.f, 0.f, 0.f),
    mVolume(1.f),
    mRolloffMin(1.f),
    mRolloffMax(20.f),
    mState(State::Idle),
    mStarvingSince(0.0)
{
}

LLPositionalStream::~LLPositionalStream()
{
    // Best-effort cleanup; if the audio engine has already shut down we
    // simply abandon the FMOD handles since FMOD itself owns the memory.
    if (gAudiop)
    {
        stop();
    }
}

FMOD::System* LLPositionalStream::getFmodSystem() const
{
    if (!gAudiop)
    {
        return nullptr;
    }
    LLAudioEngine_FMODSTUDIO* engine = dynamic_cast<LLAudioEngine_FMODSTUDIO*>(gAudiop);
    return engine ? engine->getSystem() : nullptr;
}

bool LLPositionalStream::start(const std::string& url, const LLVector3& world_pos)
{
    stop();

    std::string clean_url(url);
    LLStringUtil::trim(clean_url);

    if (clean_url.empty())
    {
        LL_WARNS("Stream3D") << "Refusing to start positional stream with empty URL" << LL_ENDL;
        return false;
    }

    FMOD::System* system = getFmodSystem();
    if (!system)
    {
        LL_WARNS("Stream3D") << "FMOD Studio system unavailable" << LL_ENDL;
        return false;
    }

    mUrl = clean_url;
    mPosition = world_pos;

    const FMOD_MODE mode = FMOD_3D
                         | FMOD_3D_LINEARSQUAREROLLOFF
                         | FMOD_NONBLOCKING
                         | FMOD_IGNORETAGS;

    if (checkFmod(system->createStream(clean_url.c_str(), mode, nullptr, &mSound), "createStream"))
    {
        mSound = nullptr;
        // Keep mUrl so the manager can drive a retry from the same address.
        mState = State::Failed;
        return false;
    }

    mState = State::Opening;
    LL_INFOS("Stream3D") << "Opening positional stream '" << clean_url
                          << "' at " << world_pos << LL_ENDL;
    return true;
}

void LLPositionalStream::stop()
{
    if (mChannel)
    {
        checkFmod(mChannel->stop(), "Channel::stop");
        mChannel = nullptr;
    }
    releaseSound();
    mUrl.clear();
    mState = State::Idle;
    mStarvingSince = 0.0;
}

void LLPositionalStream::releaseSound()
{
    if (mSound)
    {
        checkFmod(mSound->release(), "Sound::release");
        mSound = nullptr;
    }
}

void LLPositionalStream::setPosition(const LLVector3& world_pos)
{
    mPosition = world_pos;
    if (mChannel)
    {
        FMOD_VECTOR pos = { world_pos.mV[0], world_pos.mV[1], world_pos.mV[2] };
        FMOD_VECTOR vel = { 0.f, 0.f, 0.f };
        checkFmod(mChannel->set3DAttributes(&pos, &vel), "Channel::set3DAttributes");
    }
}

void LLPositionalStream::setVolume(F32 volume)
{
    mVolume = volume;
    if (mChannel)
    {
        checkFmod(mChannel->setVolume(volume), "Channel::setVolume");
    }
}

void LLPositionalStream::setRolloffDistances(F32 min_distance, F32 max_distance)
{
    // Guard against zero/inverted values that would make FMOD reject the call.
    min_distance = llmax(min_distance, 0.1f);
    max_distance = llmax(max_distance, min_distance + 0.1f);

    mRolloffMin = min_distance;
    mRolloffMax = max_distance;
    if (mChannel)
    {
        checkFmod(mChannel->set3DMinMaxDistance(mRolloffMin, mRolloffMax),
                  "Channel::set3DMinMaxDistance");
    }
}

static LLTrace::BlockTimerStatHandle FTM_STREAM3D_MONO_UPDATE("Stream3D Mono Update");

void LLPositionalStream::update()
{
    LL_RECORD_BLOCK_TIME(FTM_STREAM3D_MONO_UPDATE);

    // Already-playing path: detect mid-stream drops so the manager can retry.
    // FMOD doesn't stop the channel just because the network died — it will
    // happily play silence. Three signals tell us the stream is actually dead:
    //   1. isPlaying() RPC fails / returns false (channel stolen, stopped)
    //   2. getOpenState() returns ERROR (FMOD itself gave up)
    //   3. starving flag stays true continuously for kStarvedTimeout seconds
    // Signal 3 is debounced because a brief packet hiccup is normal.
    if (mChannel)
    {
        static constexpr F64 kStarvedTimeout = 3.0;
        const F64 now = LLTimer::getElapsedSeconds();

        bool failed = false;
        const char* reason = "";

        bool still_playing = false;
        FMOD_RESULT pr = mChannel->isPlaying(&still_playing);
        if (pr != FMOD_OK || !still_playing)
        {
            failed = true;
            reason = "channel stopped";
        }
        else if (mSound)
        {
            FMOD_OPENSTATE st = FMOD_OPENSTATE_PLAYING;
            bool starving = false;
            FMOD_RESULT or_ = mSound->getOpenState(&st, nullptr, &starving, nullptr);
            if (or_ != FMOD_OK || st == FMOD_OPENSTATE_ERROR)
            {
                failed = true;
                reason = "openstate error";
            }
            else if (starving)
            {
                if (mStarvingSince == 0.0)
                {
                    mStarvingSince = now;
                    LL_INFOS("Stream3D") << "Stream starving (will fail in "
                                          << kStarvedTimeout << "s if it continues): "
                                          << mUrl << LL_ENDL;
                }
                else if ((now - mStarvingSince) > kStarvedTimeout)
                {
                    failed = true;
                    reason = "starved";
                }
            }
            else
            {
                mStarvingSince = 0.0;
            }
        }

        if (failed)
        {
            LL_WARNS("Stream3D") << "Stream dropped mid-playback (" << reason
                                  << "): " << mUrl << LL_ENDL;
            mChannel = nullptr;
            releaseSound();
            mState = State::Failed;
            mStarvingSince = 0.0;
        }
        return;
    }

    if (!mSound)
    {
        return;
    }

    FMOD::System* system = getFmodSystem();
    if (!system)
    {
        return;
    }

    FMOD_OPENSTATE state = FMOD_OPENSTATE_LOADING;
    FMOD_RESULT open_result = mSound->getOpenState(&state, nullptr, nullptr, nullptr);
    if (open_result != FMOD_OK || state == FMOD_OPENSTATE_ERROR)
    {
        LL_WARNS("Stream3D") << "Stream open errored: " << mUrl
                              << " (" << FMOD_ErrorString(open_result) << ")" << LL_ENDL;
        releaseSound();
        // Keep mUrl so the manager's reconnect loop can re-open the same source.
        mState = State::Failed;
        return;
    }
    if (state != FMOD_OPENSTATE_READY && state != FMOD_OPENSTATE_PLAYING)
    {
        // Still connecting / buffering; check again next tick.
        return;
    }

    FMOD::Channel* channel = nullptr;
    if (checkFmod(system->playSound(mSound, nullptr, true /*paused*/, &channel),
                  "System::playSound"))
    {
        return;
    }
    mChannel = channel;

    FMOD_VECTOR pos = { mPosition.mV[0], mPosition.mV[1], mPosition.mV[2] };
    FMOD_VECTOR vel = { 0.f, 0.f, 0.f };
    checkFmod(mChannel->set3DAttributes(&pos, &vel), "Channel::set3DAttributes");
    checkFmod(mChannel->set3DMinMaxDistance(mRolloffMin, mRolloffMax),
              "Channel::set3DMinMaxDistance");
    checkFmod(mChannel->setVolume(mVolume), "Channel::setVolume");
    checkFmod(mChannel->setPaused(false), "Channel::setPaused");

    mState = State::Playing;
    LL_INFOS("Stream3D") << "Positional stream playing: " << mUrl << LL_ENDL;
}
