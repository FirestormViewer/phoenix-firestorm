/**
 * @file lllistener_soloud.h
 * @brief Description of LISTENER class abstracting the audio support
 * as a SoLoud implementation
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

#ifndef LL_LISTENER_SOLOUD_H
#define LL_LISTENER_SOLOUD_H

#include "lllistener.h"

class LLListener_SoLoud : public LLListener
{
public:
    LLListener_SoLoud();
    virtual ~LLListener_SoLoud();

    virtual void commitDeferredChanges();

    virtual void setDopplerFactor(F32 factor);
    virtual F32 getDopplerFactor();
    virtual void setRolloffFactor(F32 factor);
    virtual F32 getRolloffFactor();

protected:
    F32 mDopplerFactor;
    F32 mRolloffFactor;
};

#endif
