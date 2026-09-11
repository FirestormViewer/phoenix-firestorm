#include "llvkaudio.h"
#include <thread>
#include <cmath>
#include <vector>
#if LL_OPENAL
#include <al.h>
#include <alc.h>
#include <alut.h>
#endif

struct LLVKAudio::Impl
{
    std::thread::id thread = std::this_thread::get_id();
    bool started = false, initialized = false;
    std::string driver = "Undefined";
    float uiGain = 1.f;
    bool uiMuted = false;
#if LL_OPENAL
    ALCcontext* context = nullptr;
    struct Sound { ALuint source=0, buffer=0; };
    std::vector<Sound> sounds;
#endif
};

LLVKAudio::LLVKAudio() : mImpl(std::make_unique<Impl>()) {}

LLVKAudio::~LLVKAudio()
{
    std::string error;
    if (mImpl->initialized && !stop(error)) std::terminate();
}

bool LLVKAudio::start(bool disabled,std::string& error)
{
    error.clear();
    if (std::this_thread::get_id() != mImpl->thread || mImpl->started)
    { error = "Native audio startup requires a fresh owner on its creating thread"; return false; }
    mImpl->started = true;
    if (disabled) return true;
#if LL_OPENAL
    if (alcGetCurrentContext())
    { error = "Native audio cannot take ownership of an existing OpenAL context"; return false; }
    if (!alutInit(nullptr,nullptr))
    { error = std::string("Native OpenAL initialization failed: ")+alutGetErrorString(alutGetError()); return false; }
    mImpl->initialized = true;
    mImpl->context=alcGetCurrentContext();
    const auto string = [](const ALchar* text) { return text ? std::string(text) : std::string(); };
    mImpl->driver = "OpenAL, version "+string(alGetString(AL_VERSION))+" / "+string(alGetString(AL_VENDOR))+" / "+string(alGetString(AL_RENDERER));
    if (auto* device = alcGetContextsDevice(alcGetCurrentContext()))
        mImpl->driver += ": "+string(alcGetString(device,ALC_DEFAULT_DEVICE_SPECIFIER));
    alListenerf(AL_GAIN,0.f);
    if (alGetError() != AL_NO_ERROR)
    {
        std::string ignored;
        stop(ignored);
        error = "Native OpenAL startup mute failed";
        return false;
    }
    return true;
#else
    error = "Native audio has no compiled provider";
    return false;
#endif
}

bool LLVKAudio::setVolume(const Volume& volume,std::string& error)
{
    error.clear();
    if (std::this_thread::get_id()!=mImpl->thread || !mImpl->started || !std::isfinite(volume.master) || volume.master<0.f)
    { error="Native audio volume requires its owner thread and finite nonnegative gain"; return false; }
    if (!mImpl->initialized) return true;
#if LL_OPENAL
    if (alcGetCurrentContext()!=mImpl->context)
    { error="Native audio volume lost its owning context"; return false; }
    const auto gain=volume.muted || volume.progressVisible || (!volume.windowActive && volume.muteWhenInactive) ? 0.f : volume.master;
    alListenerf(AL_GAIN,gain);
    if (alGetError()!=AL_NO_ERROR) { error="Native OpenAL volume update failed"; return false; }
#endif
    return true;
}

std::optional<float> LLVKAudio::listenerGain(std::string& error) const
{
    error.clear();
    if (std::this_thread::get_id()!=mImpl->thread || !mImpl->initialized)
    { error="Native audio gain query requires an active owner on its creating thread"; return std::nullopt; }
#if LL_OPENAL
    if (alcGetCurrentContext()!=mImpl->context)
    { error="Native audio gain query lost its owning context"; return std::nullopt; }
    float gain=0.f;
    alGetListenerf(AL_GAIN,&gain);
    if (alGetError()!=AL_NO_ERROR) { error="Native OpenAL gain query failed"; return std::nullopt; }
    return gain;
#else
    return std::nullopt;
#endif
}

bool LLVKAudio::playUiWav(std::span<const std::uint8_t> wav,std::string& error)
{
    error.clear();
    if (std::this_thread::get_id()!=mImpl->thread || !mImpl->started)
    { error="Native UI sound requires its started owner thread"; return false; }
    if (!mImpl->initialized) return true;
    if (wav.empty() || wav.size()>16*1024*1024)
    { error="Native UI sound exceeds the decoded-file budget"; return false; }
#if LL_OPENAL
    if (!update(error)) return false;
    if (mImpl->sounds.size()>=32) { error="Native UI sound channel budget exhausted"; return false; }
    mImpl->sounds.reserve(mImpl->sounds.size()+1);
    alutGetError();
    const auto buffer=alutCreateBufferFromFileImage(wav.data(),static_cast<ALsizei>(wav.size()));
    if (!buffer) { error=std::string("Native UI WAV decode failed: ")+alutGetErrorString(alutGetError()); return false; }
    ALuint source=0;
    alGenSources(1,&source);
    alSourcei(source,AL_BUFFER,buffer);
    alSourcei(source,AL_SOURCE_RELATIVE,AL_TRUE);
    alSourcei(source,AL_LOOPING,AL_FALSE);
    alSourcef(source,AL_ROLLOFF_FACTOR,0.f);
    alSource3f(source,AL_POSITION,0.f,0.f,0.f);
    alSourcef(source,AL_GAIN,mImpl->uiMuted ? 0.f : mImpl->uiGain);
    alSourcePlay(source);
    if (alGetError()!=AL_NO_ERROR)
    {
        if (source) alDeleteSources(1,&source);
        alDeleteBuffers(1,&buffer);
        error="Native UI sound source initialization failed";
        return false;
    }
    mImpl->sounds.push_back({source,buffer});
#endif
    return true;
}

bool LLVKAudio::setUiGain(float gain,bool muted,std::string& error)
{
    error.clear();
    if (std::this_thread::get_id()!=mImpl->thread || !std::isfinite(gain) || gain<0.f)
    { error="Native UI sound gain requires finite nonnegative input on its owner thread"; return false; }
#if LL_OPENAL
    if (mImpl->initialized)
    {
        if (alcGetCurrentContext()!=mImpl->context) { error="Native UI sound lost its context"; return false; }
        for (const auto& sound : mImpl->sounds) alSourcef(sound.source,AL_GAIN,muted ? 0.f : gain);
        if (alGetError()!=AL_NO_ERROR) { error="Native UI sound gain update failed"; return false; }
    }
#endif
    mImpl->uiGain=gain; mImpl->uiMuted=muted;
    return true;
}

bool LLVKAudio::update(std::string& error)
{
    error.clear();
    if (std::this_thread::get_id()!=mImpl->thread) { error="Native audio update requires its owner thread"; return false; }
#if LL_OPENAL
    if (mImpl->initialized)
    {
        if (alcGetCurrentContext()!=mImpl->context) { error="Native audio update lost its context"; return false; }
        for (auto sound=mImpl->sounds.begin(); sound!=mImpl->sounds.end(); )
        {
            ALint state=0;
            alGetSourcei(sound->source,AL_SOURCE_STATE,&state);
            if (alGetError()!=AL_NO_ERROR) { error="Native sound playback state query failed"; return false; }
            if (state==AL_PLAYING || state==AL_PAUSED) { ++sound; continue; }
            alDeleteSources(1,&sound->source);
            alDeleteBuffers(1,&sound->buffer);
            if (alGetError()!=AL_NO_ERROR) { error="Native sound retirement failed"; return false; }
            sound=mImpl->sounds.erase(sound);
        }
    }
#endif
    return true;
}

std::size_t LLVKAudio::activeUiSounds() const noexcept
{
#if LL_OPENAL
    return mImpl->sounds.size();
#else
    return 0;
#endif
}

bool LLVKAudio::stop(std::string& error)
{
    error.clear();
    if (std::this_thread::get_id() != mImpl->thread)
    { error = "Native audio shutdown must run on its owning thread"; return false; }
    if (!mImpl->initialized) return true;
#if LL_OPENAL
    if (alcGetCurrentContext()!=mImpl->context)
    { error="Native audio shutdown lost its owning context"; return false; }
    for (const auto& sound : mImpl->sounds)
    {
        alSourceStop(sound.source);
        alDeleteSources(1,&sound.source);
        alDeleteBuffers(1,&sound.buffer);
    }
    mImpl->sounds.clear();
    if (alGetError()!=AL_NO_ERROR) { error="Native UI sound shutdown failed"; return false; }
    alutGetError();
    if (!alutExit())
    { error = std::string("Native OpenAL shutdown failed: ")+alutGetErrorString(alutGetError()); return false; }
    mImpl->context=nullptr;
#endif
    mImpl->initialized = false;
    mImpl->driver = "Undefined";
    return true;
}

bool LLVKAudio::active() const noexcept { return mImpl->initialized; }
std::string LLVKAudio::driverName() const { return mImpl->driver; }