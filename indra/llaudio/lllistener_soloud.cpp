/**
 * @file lllistener_soloud.cpp
 * @brief Implementation of LISTENER class abstracting the audio
 * support as a SoLoud implementation
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2026, Firestorm Viewer Project
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
#include "llaudioengine_soloud.h"

#include "lllistener_soloud.h"

LLListener_SoLoud::LLListener_SoLoud()
    : mDopplerFactor(1.0f),
      mRolloffFactor(1.0f)
{
    init();
}

LLListener_SoLoud::~LLListener_SoLoud()
{
}

void LLListener_SoLoud::commitDeferredChanges()
{
    SoLoud::Soloud* soloud = LLAudioEngine_SoLoud::getSoloud();
    if (!soloud)
    {
        return;
    }

    soloud->set3dListenerParameters(
        mPosition.mV[0], mPosition.mV[1], mPosition.mV[2],
        mListenAt.mV[0], mListenAt.mV[1], mListenAt.mV[2],
        mListenUp.mV[0], mListenUp.mV[1], mListenUp.mV[2],
        mVelocity.mV[0], mVelocity.mV[1], mVelocity.mV[2]);

    soloud->update3dAudio();
}

void LLListener_SoLoud::setDopplerFactor(F32 factor)
{
    mDopplerFactor = factor;
}

F32 LLListener_SoLoud::getDopplerFactor()
{
    return mDopplerFactor;
}

void LLListener_SoLoud::setRolloffFactor(F32 factor)
{
    mRolloffFactor = factor;
}

F32 LLListener_SoLoud::getRolloffFactor()
{
    return mRolloffFactor;
}
