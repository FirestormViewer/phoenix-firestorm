/**
 * @file llaudioengine_soloud.h
 * @brief Implementation of the audio engine using SoLoud
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

#ifndef LL_AUDIOENGINE_SOLOUD_H
#define LL_AUDIOENGINE_SOLOUD_H

#include "llaudioengine.h"
#include "lllistener_soloud.h"

// AL/al.h leaks an empty OPENAL macro that clobbers the OPENAL enumerator
// in SoLoud's BACKENDS enum when both headers end up in one translation unit
#ifdef OPENAL
#undef OPENAL
#endif

#include "soloud.h"
#include "soloud_wav.h"

class LLSoLoudWindSource;

class LLAudioEngine_SoLoud : public LLAudioEngine
{
public:
    LLAudioEngine_SoLoud();
    virtual ~LLAudioEngine_SoLoud();

    virtual bool init(void *user_data, const std::string &app_title);
    virtual std::string getDriverName(bool verbose);
    virtual LLStreamingAudioInterface* createDefaultStreamingAudioImpl() const { return nullptr; }
    virtual void allocateListener();

    virtual void shutdown();

    void setInternalGain(F32 gain);

    LLAudioBuffer* createBuffer();
    LLAudioChannel* createChannel();

    /*virtual*/ bool initWind();
    /*virtual*/ void cleanupWind();
    /*virtual*/ void updateWind(LLVector3 direction, F32 camera_altitude);

    virtual void idle();

    // <FS:Ansariel> Output device selection
    virtual output_device_map_t getDevices();
    virtual void setDevice(const LLUUID& device_uuid);
    // </FS:Ansariel>

    static SoLoud::Soloud* getSoloud() { return sBackendReady ? sSoloud : nullptr; }

private:
    friend struct LLSoLoudTestAccess;
    bool startBackend(unsigned int backend = SoLoud::Soloud::MINIAUDIO, unsigned int channels = 2);
    bool reinitBackend(unsigned int backend = SoLoud::Soloud::MINIAUDIO, unsigned int channels = 2);

    static SoLoud::Soloud* sSoloud;
    static bool sBackendReady;

    LLSoLoudWindSource* mWindSource;
    SoLoud::handle mWindHandle;
    LLUUID mSelectedDeviceUUID;
    LLUUID mRequestedDeviceUUID;
    bool mDeviceChangePending = false;
    LLFrameTimer mDeviceRetryTimer;
    LLFrameTimer mDeviceListTimer;
    output_device_map_t mKnownDevices;
};

class LLAudioChannelSoLoud : public LLAudioChannel
{
public:
    LLAudioChannelSoLoud();
    virtual ~LLAudioChannelSoLoud();

    // Voice handles do not survive a backend reinit (device switch); the
    // engine calls this on every channel to drop them safely
    void resetVoiceHandle() { cleanup(); }
protected:
    /*virtual*/ void play();
    /*virtual*/ void playSynced(LLAudioChannel *channelp);
    /*virtual*/ void cleanup();
    /*virtual*/ bool isPlaying();

    /*virtual*/ bool updateBuffer();
    /*virtual*/ void update3DPosition();
    /*virtual*/ void updateLoop();

    void applyVoiceParams();
    void startVoicePaused();

    SoLoud::handle mHandle;
    unsigned int mLastLoopCount;
    F32 mLastGain;
    bool mLastLoop;
};

class LLAudioBufferSoLoud : public LLAudioBuffer
{
public:
    LLAudioBufferSoLoud();
    virtual ~LLAudioBufferSoLoud();

    bool loadWAV(const std::string& filename);
    U32 getLength();

    friend class LLAudioChannelSoLoud;
    friend class LLAudioEngine_SoLoud;
protected:
    SoLoud::Wav& getWav() { return mWav; }

    SoLoud::Wav mWav;
};

#endif
