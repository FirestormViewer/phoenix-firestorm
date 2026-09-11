#include "llvkaudio.h"
#include <thread>
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

bool LLVKAudio::stop(std::string& error)
{
    error.clear();
    if (std::this_thread::get_id() != mImpl->thread)
    { error = "Native audio shutdown must run on its owning thread"; return false; }
    if (!mImpl->initialized) return true;
#if LL_OPENAL
    alutGetError();
    if (!alutExit())
    { error = std::string("Native OpenAL shutdown failed: ")+alutGetErrorString(alutGetError()); return false; }
#endif
    mImpl->initialized = false;
    mImpl->driver = "Undefined";
    return true;
}

bool LLVKAudio::active() const noexcept { return mImpl->initialized; }
std::string LLVKAudio::driverName() const { return mImpl->driver; }