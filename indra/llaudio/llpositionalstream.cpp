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
#include "llstring.h"

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
        LL_WARNS("AYAStream") << what << " error: " << FMOD_ErrorString(result) << LL_ENDL;
        return true;
    }
}

LLPositionalStream& LLPositionalStream::instance()
{
    static LLPositionalStream sInstance;
    return sInstance;
}

LLPositionalStream::LLPositionalStream()
:   mSound(nullptr),
    mChannel(nullptr),
    mPosition(0.f, 0.f, 0.f),
    mVolume(1.f),
    mRolloffMin(1.f),
    mRolloffMax(64.f)
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
        LL_WARNS("AYAStream") << "Refusing to start positional stream with empty URL" << LL_ENDL;
        return false;
    }

    FMOD::System* system = getFmodSystem();
    if (!system)
    {
        LL_WARNS("AYAStream") << "FMOD Studio system unavailable" << LL_ENDL;
        return false;
    }

    mUrl = clean_url;
    mPosition = world_pos;

    // Open as 2D to keep FMOD's stream parser happy for stereo SHOUTcast/Icecast;
    // 3D positioning is then applied per-channel after playSound.
    const FMOD_MODE mode = FMOD_2D
                         | FMOD_NONBLOCKING
                         | FMOD_IGNORETAGS;

    if (checkFmod(system->createStream(clean_url.c_str(), mode, nullptr, &mSound), "createStream"))
    {
        mSound = nullptr;
        mUrl.clear();
        return false;
    }

    LL_INFOS("AYAStream") << "Opening positional stream '" << clean_url
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

void LLPositionalStream::update()
{
    if (!mSound || mChannel)
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
        LL_WARNS("AYAStream") << "Stream open errored: " << mUrl
                              << " (" << FMOD_ErrorString(open_result) << ")" << LL_ENDL;
        releaseSound();
        mUrl.clear();
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

    // Promote the channel to 3D positional after the (2D) stream has been opened.
    checkFmod(mChannel->setMode(FMOD_3D | FMOD_3D_LINEARSQUAREROLLOFF),
              "Channel::setMode(3D)");

    FMOD_VECTOR pos = { mPosition.mV[0], mPosition.mV[1], mPosition.mV[2] };
    FMOD_VECTOR vel = { 0.f, 0.f, 0.f };
    checkFmod(mChannel->set3DAttributes(&pos, &vel), "Channel::set3DAttributes");
    checkFmod(mChannel->set3DMinMaxDistance(mRolloffMin, mRolloffMax),
              "Channel::set3DMinMaxDistance");
    checkFmod(mChannel->setVolume(mVolume), "Channel::setVolume");
    checkFmod(mChannel->setPaused(false), "Channel::setPaused");

    LL_INFOS("AYAStream") << "Positional stream playing: " << mUrl << LL_ENDL;
}
