#include "llvkaudio.h"
#include <thread>
#include <cmath>
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
#if LL_OPENAL
    ALCcontext* context = nullptr;
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

bool LLVKAudio::stop(std::string& error)
{
    error.clear();
    if (std::this_thread::get_id() != mImpl->thread)
    { error = "Native audio shutdown must run on its owning thread"; return false; }
    if (!mImpl->initialized) return true;
#if LL_OPENAL
    if (alcGetCurrentContext()!=mImpl->context)
    { error="Native audio shutdown lost its owning context"; return false; }
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