/**
 * @file llaudioengine_soloud.cpp
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

#include "linden_common.h"
#include "lldir.h"
#include "llmd5.h"

#include "llaudioengine_soloud.h"
#include "lllistener_soloud.h"
#include "llwindgen.h"

#include <array>
#include <atomic>

// Declarations only; the implementation lives in SoLoud's miniaudio backend
#include "miniaudio.h"

namespace SoLoud
{
    // Output device override supplied by the revision 2 autobuild package
    extern ma_device_id gSoloudRequestedDeviceId;
    extern bool gSoloudUseRequestedDeviceId;
}

// Enumeration context and UUID -> miniaudio device id mapping for the
// Firestorm output device selector
static ma_context sMaContext;
static bool sMaContextInited = false;
static std::map<LLUUID, ma_device_id> sMaDeviceIds;

static bool ensure_ma_context()
{
    if (!sMaContextInited)
    {
        if (ma_context_init(NULL, 0, NULL, &sMaContext) != MA_SUCCESS)
        {
            LL_WARNS() << "miniaudio context init failed; no device enumeration" << LL_ENDL;
            return false;
        }
        sMaContextInited = true;
    }
    return true;
}

// Sounds are audible to roughly this distance; SoLoud requires an explicit
// max distance for its attenuation models (there is no unclamped mode).
static const F32 SOLOUD_MAX_AUDIBLE_DISTANCE = 512.f;

SoLoud::Soloud* LLAudioEngine_SoLoud::sSoloud = nullptr;
bool LLAudioEngine_SoLoud::sBackendReady = false;

struct LLSoLoudWindState
{
    LLWindGen<F32> generator;
    std::atomic<F32> frequency{100.f};
    std::atomic<F32> gain{0.f};
    std::atomic<F32> pan{0.5f};
};

// ------------ wind ------------

// Streams LLWindGen output into the mixer. SoLoud has no insert-DSP hook
// like FMOD, so wind is a regular (protected, non-3D) voice whose instance
// pulls samples from the generator on the audio thread.
class LLSoLoudWindInstance : public SoLoud::AudioSourceInstance
{
public:
    LLSoLoudWindInstance(LLSoLoudWindState& state)
        : mState(state)
    {
    }

    virtual unsigned int getAudio(float *aBuffer, unsigned int aSamplesToRead, unsigned int aBufferSize)
    {
        mState.generator.mTargetFreq = mState.frequency.load(std::memory_order_relaxed);
        mState.generator.mTargetGain = mState.gain.load(std::memory_order_relaxed);
        mState.generator.mTargetPanGainR = mState.pan.load(std::memory_order_relaxed);
        for (unsigned int offset = 0; offset < aSamplesToRead;)
        {
            const unsigned int frames = llmin(aSamplesToRead - offset, 512u);
            mState.generator.windGenerate(mScratch.data(), frames);
            for (unsigned int frame = 0; frame < frames; ++frame)
            {
                aBuffer[offset + frame] = mScratch[frame * 2];
                aBuffer[aBufferSize + offset + frame] = mScratch[frame * 2 + 1];
            }
            offset += frames;
        }
        return aSamplesToRead;
    }

    virtual bool hasEnded()
    {
        return false;
    }

private:
    LLSoLoudWindState& mState;
    std::array<float, 1024> mScratch;
};

class LLSoLoudWindSource : public SoLoud::AudioSource
{
public:
    LLSoLoudWindSource()
    {
        mChannels = 2;
        mBaseSamplerate = (float)mState.generator.getInputSamplingRate();
        setSingleInstance(true);
    }

    virtual ~LLSoLoudWindSource()
    {
        stop();
    }

    virtual SoLoud::AudioSourceInstance* createInstance()
    {
        return new LLSoLoudWindInstance(mState);
    }

    void setTargets(F32 freq, F32 gain, F32 pan_gain_r)
    {
        mState.frequency.store(freq, std::memory_order_relaxed);
        mState.gain.store(gain, std::memory_order_relaxed);
        mState.pan.store(pan_gain_r, std::memory_order_relaxed);
    }

private:
    LLSoLoudWindState mState;
};

// ------------ engine ------------

LLAudioEngine_SoLoud::LLAudioEngine_SoLoud()
    : mWindSource(nullptr),
      mWindHandle(0)
{
}

LLAudioEngine_SoLoud::~LLAudioEngine_SoLoud()
{
}

bool LLAudioEngine_SoLoud::startBackend(unsigned int backend, unsigned int channels)
{
    sBackendReady = false;
    SoLoud::result res = sSoloud->init(SoLoud::Soloud::CLIP_ROUNDOFF,
                                       backend,
                                       SoLoud::Soloud::AUTO,
                                       SoLoud::Soloud::AUTO,
                                       channels);
    if (res != SoLoud::SO_NO_ERROR)
    {
        LL_WARNS() << "LLAudioEngine_SoLoud backend init failed: " << res << LL_ENDL;
        return false;
    }

    // Enough active voices for all engine channels plus wind and headroom
    sSoloud->setMaxActiveVoiceCount(64);

    if (sSoloud->getBackendChannels() == 8)
    {
        sSoloud->setSpeakerPosition(4, 2.f, 0.f, -1.f);
        sSoloud->setSpeakerPosition(5, -2.f, 0.f, -1.f);
        sSoloud->setSpeakerPosition(6, 2.f, 0.f, 0.f);
        sSoloud->setSpeakerPosition(7, -2.f, 0.f, 0.f);
    }
    sSoloud->setGlobalVolume(llmax(mInternalGain, 0.f));
    sBackendReady = true;
    mListenerp->commitDeferredChanges();

    return true;
}

bool LLAudioEngine_SoLoud::init(void* userdata, const std::string &app_title)
{
    LLAudioEngine::init(userdata, app_title);

    sSoloud = new SoLoud::Soloud();

    std::unique_lock<std::timed_mutex> lock(gAudioDeviceMutex, std::chrono::seconds(1));
    if (lock.owns_lock())
    {
        SoLoud::gSoloudUseRequestedDeviceId = false;
    }
    if (!lock.owns_lock() || !startBackend())
    {
        delete sSoloud;
        sSoloud = nullptr;
        delete mListenerp;
        mListenerp = nullptr;
        return false;
    }

    LL_INFOS() << "LLAudioEngine_SoLoud::init() SoLoud initialized: "
               << getDriverName(true) << LL_ENDL;

    return true;
}

// <FS:Ansariel> Output device selection
//virtual
LLAudioEngine_SoLoud::output_device_map_t LLAudioEngine_SoLoud::getDevices()
{
    output_device_map_t device_map;

    std::unique_lock<std::timed_mutex> lock(gAudioDeviceMutex, std::chrono::seconds(1));
    if (!lock.owns_lock())
    {
        return mKnownDevices;
    }
    if (!ensure_ma_context())
    {
        return device_map;
    }

    ma_device_info* playback_infos = nullptr;
    ma_uint32 playback_count = 0;
    if (ma_context_get_devices(&sMaContext, &playback_infos, &playback_count, NULL, NULL) != MA_SUCCESS)
    {
        LL_WARNS() << "miniaudio device enumeration failed" << LL_ENDL;
        return device_map;
    }

    sMaDeviceIds.clear();
    for (ma_uint32 i = 0; i < playback_count; ++i)
    {
        LLUUID device_uuid;
        LLMD5 md5;
        md5.update(std::string(ma_get_backend_name(sMaContext.backend)));
        const ma_device_id& device_id = playback_infos[i].id;
        switch (sMaContext.backend)
        {
        case ma_backend_wasapi:
            md5.update(reinterpret_cast<const uint8_t*>(device_id.wasapi),
                       wcslen(device_id.wasapi) * sizeof(wchar_t));
            break;
        case ma_backend_dsound:
            md5.update(device_id.dsound, sizeof(device_id.dsound));
            break;
        case ma_backend_winmm:
            md5.update(reinterpret_cast<const uint8_t*>(&device_id.winmm), sizeof(device_id.winmm));
            break;
        default:
            LL_WARNS() << "Unsupported miniaudio device identity backend" << LL_ENDL;
            continue;
        }
        md5.finalize();
        md5.raw_digest(device_uuid.mData);

        device_map.insert(std::make_pair(device_uuid, std::string(playback_infos[i].name)));
        sMaDeviceIds[device_uuid] = playback_infos[i].id;

        LL_DEBUGS("AppInit") << "LLAudioEngine_SoLoud::getDevices(): name=\"" << playback_infos[i].name
                            << "\" - uuid: " << device_uuid << LL_ENDL;
    }

    return device_map;
}

//virtual
void LLAudioEngine_SoLoud::setDevice(const LLUUID& device_uuid)
{
    if (!sSoloud)
    {
        return;
    }

    mRequestedDeviceUUID = device_uuid;
    if (sBackendReady && device_uuid == mSelectedDeviceUUID)
    {
        mDeviceChangePending = false;
        return;
    }

    ma_device_id requested_id{};
    if (!device_uuid.isNull())
    {
        getDevices();
        std::map<LLUUID, ma_device_id>::const_iterator found = sMaDeviceIds.find(device_uuid);
        if (found == sMaDeviceIds.end())
        {
            LL_WARNS() << "Requested output device " << device_uuid
                       << " not found; retaining the current output device" << LL_ENDL;
            mDeviceChangePending = false;
            return;
        }
        requested_id = found->second;
    }

    // Share the device access mutex with the voice subsystem (FIRE-36022)
    try
    {
        std::unique_lock<std::timed_mutex> lock(gAudioDeviceMutex, std::chrono::seconds(1));
        if (!lock.owns_lock())
        {
            LL_WARNS() << "Could not access the audio device mutex; device change deferred" << LL_ENDL;
            mDeviceChangePending = true;
            mDeviceRetryTimer.reset();
            return;
        }
        const ma_device_id previous_id = SoLoud::gSoloudRequestedDeviceId;
        const bool previous_override = SoLoud::gSoloudUseRequestedDeviceId;
        SoLoud::gSoloudRequestedDeviceId = requested_id;
        SoLoud::gSoloudUseRequestedDeviceId = !device_uuid.isNull();
        mDeviceChangePending = false;
        if (!reinitBackend())
        {
            LL_WARNS() << "Requested device failed; restoring previous output device" << LL_ENDL;
            SoLoud::gSoloudRequestedDeviceId = previous_id;
            SoLoud::gSoloudUseRequestedDeviceId = previous_override;
            if (!reinitBackend())
            {
                LL_WARNS() << "Previous device could not be restored; audio unavailable" << LL_ENDL;
            }
            return;
        }
        mSelectedDeviceUUID = device_uuid;
    }
    catch (const std::exception& e)
    {
        LL_WARNS() << "Exception during audio device change: " << e.what() << LL_ENDL;
        return;
    }

    LL_INFOS() << "LLAudioEngine_SoLoud::setDevice() now on: " << getDriverName(true) << LL_ENDL;
}
// </FS:Ansariel>

bool LLAudioEngine_SoLoud::reinitBackend(unsigned int backend, unsigned int channels)
{
    cleanupWind();

    sBackendReady = false;
    for (U32 i = 0; i < LL_MAX_AUDIO_CHANNELS; ++i)
    {
        if (mChannels[i])
        {
            ((LLAudioChannelSoLoud*)mChannels[i])->resetVoiceHandle();
        }
    }

    sSoloud->deinit();
    for (LLAudioBuffer* buffer : mBuffers)
    {
        if (buffer)
        {
            auto& wav = static_cast<LLAudioBufferSoLoud*>(buffer)->getWav();
            wav.mSoloud = nullptr;
            wav.mAudioSourceID = 0;
        }
    }
    delete sSoloud;
    sSoloud = nullptr;
    sSoloud = new SoLoud::Soloud();

    if (!startBackend(backend, channels))
    {
        return false;
    }

    if (mEnableWind)
    {
        initWind();
    }
    return true;
}

std::string LLAudioEngine_SoLoud::getDriverName(bool verbose)
{
    std::ostringstream version;
    version << "SoLoud";

    if (verbose && sSoloud)
    {
        version << ", version " << SOLOUD_VERSION
                << ", backend " << ll_safe_string(sSoloud->getBackendString())
                << " @ " << sSoloud->getBackendSamplerate() << " Hz, "
                << sSoloud->getBackendChannels() << " channels, "
                << sSoloud->getBackendBufferSize() << " frame buffer";
    }

    return version.str();
}

void LLAudioEngine_SoLoud::allocateListener()
{
    mListenerp = (LLListener *) new LLListener_SoLoud();
    if (!mListenerp)
    {
        LL_WARNS() << "LLAudioEngine_SoLoud::allocateListener() Listener creation failed" << LL_ENDL;
    }
}

void LLAudioEngine_SoLoud::shutdown()
{
    LLAudioEngine::shutdown();

    std::lock_guard<std::timed_mutex> lock(gAudioDeviceMutex);
    sBackendReady = false;
    mDeviceChangePending = false;
    if (sSoloud)
    {
        sSoloud->deinit();
        delete sSoloud;
        sSoloud = nullptr;
    }

    LL_INFOS() << "LLAudioEngine_SoLoud::shutdown() SoLoud shut down" << LL_ENDL;

    if (sMaContextInited)
    {
        ma_context_uninit(&sMaContext);
        sMaContextInited = false;
        sMaDeviceIds.clear();
    }

    delete mListenerp;
    mListenerp = nullptr;
}

void LLAudioEngine_SoLoud::idle()
{
    if (mDeviceChangePending && mDeviceRetryTimer.getElapsedTimeF32() > 1.f)
    {
        setDevice(mRequestedDeviceUUID);
    }
    if (mDeviceListTimer.getElapsedTimeF32() > 5.f)
    {
        mDeviceListTimer.reset();
        const auto devices = getDevices();
        if (devices != mKnownDevices)
        {
            mKnownDevices = devices;
            OnOutputDeviceListChanged(devices);
        }
    }
    if (!sBackendReady)
    {
        updateInternetStream();
        return;
    }
    LLAudioEngine::idle();

    // Diagnostic heartbeat: reconcile what the engine thinks it is doing
    // with what is audible (mute/channel toggles reportedly ineffective)
    static LLFrameTimer state_log_timer;
    if (sSoloud && state_log_timer.getElapsedTimeF32() > 5.f)
    {
        state_log_timer.reset();
        LL_DEBUGS("AudioDebug") << "SoLoud state: muted=" << mMuted
                               << " globalVol=" << sSoloud->getGlobalVolume()
                               << " activeVoices=" << sSoloud->getActiveVoiceCount()
                               << " voiceCount=" << sSoloud->getVoiceCount()
                               << " sfxGain=" << mSecondaryGain[AUDIO_TYPE_SFX]
                               << " uiGain=" << mSecondaryGain[AUDIO_TYPE_UI]
                               << " ambientGain=" << mSecondaryGain[AUDIO_TYPE_AMBIENT]
                               << " wind=" << (mWindHandle != 0)
                               << LL_ENDL;
    }
}

LLAudioBuffer *LLAudioEngine_SoLoud::createBuffer()
{
    return new LLAudioBufferSoLoud();
}

LLAudioChannel *LLAudioEngine_SoLoud::createChannel()
{
    return new LLAudioChannelSoLoud();
}

void LLAudioEngine_SoLoud::setInternalGain(F32 gain)
{
    if (sBackendReady)
    {
        LL_DEBUGS("AudioDebug") << "SoLoud global volume -> " << gain
                                << " (muted=" << mMuted << "), active voices: "
                                << sSoloud->getActiveVoiceCount() << LL_ENDL;
        sSoloud->setGlobalVolume(gain);
    }
}

bool LLAudioEngine_SoLoud::initWind()
{
    if (!sBackendReady)
    {
        return false;
    }

    if (!mWindSource)
    {
        mWindSource = new LLSoLoudWindSource();
        mWindHandle = sSoloud->play(*mWindSource, 1.0f);
        sSoloud->setProtectVoice(mWindHandle, true);
    }

    return true;
}

void LLAudioEngine_SoLoud::cleanupWind()
{
    if (sSoloud && mWindHandle)
    {
        sSoloud->stop(mWindHandle);
    }
    mWindHandle = 0;

    delete mWindSource;
    mWindSource = nullptr;
}

void LLAudioEngine_SoLoud::updateWind(LLVector3 wind_vec, F32 camera_altitude)
{
    if (!mEnableWind || !mWindSource)
    {
        return;
    }

    if (mWindUpdateTimer.checkExpirationAndReset(LL_WIND_UPDATE_INTERVAL))
    {
        // wind comes in as Linden coordinate (+X = forward, +Y = left, +Z = up)
        // need to convert this to the conventional orientation DS3D and OpenAL use
        // where +X = right, +Y = up, +Z = backwards
        wind_vec.setVec(-wind_vec.mV[1], wind_vec.mV[2], -wind_vec.mV[0]);

        F64 pitch = 1.0 + mapWindVecToPitch(wind_vec);
        F64 center_freq = 80.0 * pow(pitch, 2.5 * (mapWindVecToGain(wind_vec) + 1.0));

        mWindSource->setTargets((F32)center_freq,
                                (F32)mapWindVecToGain(wind_vec) * mMaxWindGain,
                                (F32)mapWindVecToPan(wind_vec));
    }
}

// ------------ channel ------------

// Stop with a short fade instead of truncating mid-sample, which pops
static void stop_voice_declicked(SoLoud::Soloud* soloud, SoLoud::handle voice_handle)
{
    const double DECLICK_FADE_SEC = 0.02;
    soloud->fadeVolume(voice_handle, 0.f, DECLICK_FADE_SEC);
    soloud->scheduleStop(voice_handle, DECLICK_FADE_SEC);
}

LLAudioChannelSoLoud::LLAudioChannelSoLoud()
    : mHandle(0),
      mLastLoopCount(0),
      mLastGain(-1.f),
      mLastLoop(false)
{
}

LLAudioChannelSoLoud::~LLAudioChannelSoLoud()
{
    cleanup();
}

void LLAudioChannelSoLoud::cleanup()
{
    SoLoud::Soloud* soloud = LLAudioEngine_SoLoud::getSoloud();
    if (soloud && mHandle)
    {
        stop_voice_declicked(soloud, mHandle);
    }
    mHandle = 0;
    mLastLoopCount = 0;
    mLastGain = -1.f;

    mCurrentBufferp = nullptr;
}

// Every SoLoud call takes the global audio mutex and the mixer runs on a
// ~10ms period, so per-frame calls must be limited to actual state changes
void LLAudioChannelSoLoud::applyVoiceParams()
{
    SoLoud::Soloud* soloud = LLAudioEngine_SoLoud::getSoloud();
    if (!soloud || !mHandle || !mCurrentSourcep)
    {
        return;
    }

    F32 gain = mCurrentSourcep->getGain() * getSecondaryGain();
    if (gain != mLastGain)
    {
        soloud->setVolume(mHandle, gain);
        mLastGain = gain;
    }

    bool loop = mCurrentSourcep->isLoop();
    if (loop != mLastLoop)
    {
        soloud->setLooping(mHandle, loop);
        mLastLoop = loop;
    }
}

// Create the voice PAUSED and fully configure it before it can reach the
// mixer: any parameter applied after an unpaused create races the audio
// thread, and the first granules mix with defaults (worst: unattenuated
// full-volume starts of distant sounds - an audible thump per sound start)
void LLAudioChannelSoLoud::startVoicePaused()
{
    SoLoud::Soloud* soloud = LLAudioEngine_SoLoud::getSoloud();
    LLAudioBufferSoLoud *bufferp = (LLAudioBufferSoLoud *)mCurrentBufferp;
    if (!soloud || !bufferp || !mCurrentSourcep)
    {
        return;
    }

    F32 gain = mCurrentSourcep->getGain() * getSecondaryGain();

    if (mCurrentSourcep->isForcedPriority())
    {
        // UI / preview sounds play flat, no spatialization
        mHandle = soloud->play(bufferp->getWav(), gain, 0.f, true /*paused*/);
    }
    else
    {
        LLVector3 pos;
        pos.setVec(mCurrentSourcep->getPositionGlobal());

        const LLVector3& velocity = mCurrentSourcep->getVelocity();
        mHandle = soloud->play3d(bufferp->getWav(),
                                 pos.mV[0], pos.mV[1], pos.mV[2],
                     velocity.mV[0], velocity.mV[1], velocity.mV[2],
                                 gain, true /*paused*/);
        soloud->set3dSourceDopplerFactor(mHandle, gAudiop->getDopplerFactor());
        F32 rolloff = gAudiop->getRolloffFactor();
        if (rolloff != 1.f)
        {
            soloud->set3dSourceAttenuation(mHandle, SoLoud::AudioSource::INVERSE_DISTANCE, rolloff);
        }
    }

    mLastGain = gain;
    mLastLoop = mCurrentSourcep->isLoop();
    soloud->setLooping(mHandle, mLastLoop);
    // Keep inaudible voices ticking so loop timing and completion stay correct
    soloud->setInaudibleBehavior(mHandle, true, false);
    if (!mCurrentSourcep->isForcedPriority())
    {
        soloud->update3dAudio();
    }
    mLastLoopCount = 0;
}

void LLAudioChannelSoLoud::play()
{
    SoLoud::Soloud* soloud = LLAudioEngine_SoLoud::getSoloud();
    if (!soloud)
    {
        return;
    }

    if (isPlaying())
    {
        return;
    }

    if (!mCurrentBufferp || !mCurrentSourcep)
    {
        LL_WARNS() << "Playing without a buffer or source, aborting" << LL_ENDL;
        return;
    }

    startVoicePaused();
    if (mHandle)
    {
        soloud->setPause(mHandle, false);
    }
    mCurrentSourcep->setPlayedOnce(true);
}

void LLAudioChannelSoLoud::playSynced(LLAudioChannel *channelp)
{
    SoLoud::Soloud* soloud = LLAudioEngine_SoLoud::getSoloud();
    if (soloud && channelp)
    {
        LLAudioChannelSoLoud *masterchannelp = (LLAudioChannelSoLoud*)channelp;
        if (masterchannelp->mHandle && soloud->isValidVoiceHandle(masterchannelp->mHandle))
        {
            // Stream position is monotonic across loops; reduce it to the
            // position within the master's current loop before seeking.
            double master_offset = soloud->getStreamPosition(masterchannelp->mHandle);
            LLAudioBufferSoLoud *master_bufferp = (LLAudioBufferSoLoud *)masterchannelp->mCurrentBufferp;
            if (master_bufferp)
            {
                double master_length = master_bufferp->getWav().getLength();
                if (master_length > 0.0)
                {
                    master_offset = fmod(master_offset, master_length);
                }
            }
            startVoicePaused();
            if (mHandle)
            {
                // Seek and arm the fade-in while still paused: the seek
                // lands mid-waveform past the PCM head fade, so an
                // unramped start (or any mixing before the seek) thumps
                float target_volume = mLastGain;
                soloud->setVolume(mHandle, 0.f);
                soloud->seek(mHandle, master_offset);
                soloud->fadeVolume(mHandle, target_volume, 0.02);
                soloud->setPause(mHandle, false);
            }
            if (mCurrentSourcep)
            {
                mCurrentSourcep->setPlayedOnce(true);
            }
            return;
        }
    }
    play();
}

bool LLAudioChannelSoLoud::isPlaying()
{
    SoLoud::Soloud* soloud = LLAudioEngine_SoLoud::getSoloud();
    if (soloud && mHandle)
    {
        return soloud->isValidVoiceHandle(mHandle);
    }
    return false;
}

bool LLAudioChannelSoLoud::updateBuffer()
{
    if (!mCurrentSourcep)
    {
        // This channel isn't associated with any source, nothing to update
        return false;
    }

    if (LLAudioChannel::updateBuffer())
    {
        // The source is switching to a different buffer; stop the old voice
        // so play() starts the new sound.
        SoLoud::Soloud* soloud = LLAudioEngine_SoLoud::getSoloud();
        if (soloud && mHandle)
        {
            stop_voice_declicked(soloud, mHandle);
        }
        mHandle = 0;
        mLastLoopCount = 0;
        mLastGain = -1.f;
    }

    applyVoiceParams();

    return true;
}

void LLAudioChannelSoLoud::updateLoop()
{
    SoLoud::Soloud* soloud = LLAudioEngine_SoLoud::getSoloud();
    if (!soloud || !mHandle)
    {
        return;
    }

    // SoLoud's stream position is monotonic and never wraps on loop, so the
    // OpenAL-style position heuristic can't work here; the engine's sync
    // master/slave and queued-sound logic depend on this flag, so use the
    // mixer's per-voice loop counter instead.
    unsigned int cur_loops = soloud->getLoopCount(mHandle);
    if (cur_loops != mLastLoopCount)
    {
        mLoopedThisFrame = true;
    }
    mLastLoopCount = cur_loops;
}

void LLAudioChannelSoLoud::update3DPosition()
{
    SoLoud::Soloud* soloud = LLAudioEngine_SoLoud::getSoloud();
    if (!soloud || !mHandle || !mCurrentSourcep)
    {
        return;
    }

    if (!mCurrentSourcep->isForcedPriority())
    {
        LLVector3 pos;
        pos.setVec(mCurrentSourcep->getPositionGlobal());
        const LLVector3& velocity = mCurrentSourcep->getVelocity();
        soloud->set3dSourceParameters(mHandle,
                                      pos.mV[0], pos.mV[1], pos.mV[2],
                          velocity.mV[0], velocity.mV[1], velocity.mV[2]);
        soloud->set3dSourceDopplerFactor(mHandle, gAudiop->getDopplerFactor());
        soloud->set3dSourceAttenuation(mHandle, SoLoud::AudioSource::INVERSE_DISTANCE,
                         gAudiop->getRolloffFactor());
    }

    applyVoiceParams();
}

// ------------ buffer ------------

LLAudioBufferSoLoud::LLAudioBufferSoLoud()
{
}

LLAudioBufferSoLoud::~LLAudioBufferSoLoud()
{
    // ~Wav stops any voices still playing this buffer
}

bool LLAudioBufferSoLoud::loadWAV(const std::string& filename)
{
    SoLoud::result res = mWav.load(filename.c_str());
    if (res == SoLoud::SO_NO_ERROR)
    {
        // Voices inherit these at creation, which matters because play3d()
        // computes the voice's initial volume internally BEFORE any
        // per-voice setters can run: without source-level defaults the
        // first frames play UNATTENUATED (SoLoud's default is no
        // attenuation) and every distant sound start is an audible thump.
        mWav.set3dAttenuation(SoLoud::AudioSource::INVERSE_DISTANCE, 1.f);
        mWav.set3dMinMaxDistance(1.f, SOLOUD_MAX_AUDIBLE_DISTANCE);

        // Many sound assets start or end off a zero crossing; FMOD masked
        // the resulting step with its built-in play/stop volume ramps, which
        // SoLoud does not have, so the raw step is audible as a pop or thud.
        // Bake a ~1.5ms fade into the head and tail of the decoded PCM
        // (planar layout: channel c at mData + c * mSampleCount).
        const unsigned int FADE_SAMPLES = 64;
        if (mWav.mData && mWav.mSampleCount > FADE_SAMPLES * 4)
        {
            for (unsigned int ch = 0; ch < mWav.mChannels; ++ch)
            {
                float* chan_data = mWav.mData + ch * mWav.mSampleCount;
                for (unsigned int i = 0; i < FADE_SAMPLES; ++i)
                {
                    float fade_gain = (float)i / (float)FADE_SAMPLES;
                    chan_data[i] *= fade_gain;
                    chan_data[mWav.mSampleCount - 1 - i] *= fade_gain;
                }
            }
        }
    }
    else
    {
        if (gDirUtilp->fileExists(filename))
        {
            LL_WARNS() << "LLAudioBufferSoLoud::loadWAV() Error loading "
                       << filename << " (" << res << ")" << LL_ENDL;
        }
        else
        {
            // It's common for the file to not actually exist.
            LL_DEBUGS() << "LLAudioBufferSoLoud::loadWAV() Error loading "
                        << filename << " (" << res << ")" << LL_ENDL;
        }
        return false;
    }

    return true;
}

U32 LLAudioBufferSoLoud::getLength()
{
    return mWav.mSampleCount;
}
