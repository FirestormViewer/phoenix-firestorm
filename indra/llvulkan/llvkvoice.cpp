#include "llvkvoice.h"
#include "llwebrtc.h"
#include <cmath>
#include <mutex>
#include <thread>

struct LLVKVoice::Impl final : llwebrtc::LLWebRTCDevicesObserver, llwebrtc::LLWebRTCLogCallback
{
    std::thread::id thread = std::this_thread::get_id();
    std::string input, output;
    llwebrtc::LLWebRTCDeviceInterface* device = nullptr;
    std::mutex mutex;
    State state;
    AudioConfig audioConfig;
    bool tuning = false, stopped = false;

    bool onThread(std::string& error) const
    {
        error.clear();
        if (std::this_thread::get_id() == thread) return true;
        error = "Native voice requires its creating thread";
        return false;
    }
    bool start(std::string& error)
    {
        if (!onThread(error)) return false;
        if (stopped) { error = "Native voice service has stopped"; return false; }
        if (device) return true;
        if (llwebrtc::getDeviceInterface()) { error = "Voice device engine already has an owner"; return false; }
        llwebrtc::init(this);
        device = llwebrtc::getDeviceInterface();
        if (!device) { llwebrtc::terminate(); error = "Native voice device engine initialization failed"; return false; }
        device->setDevicesObserver(this);
        device->setMute(true);
        device->setVoiceEnabled(false);
        device->setAudioConfig(audioConfig);
        device->setCaptureDevice(input);
        device->setRenderDevice(output);
        device->refreshDevices();
        return true;
    }
    void OnDevicesChanged(const llwebrtc::LLWebRTCVoiceDeviceList& outputs,const llwebrtc::LLWebRTCVoiceDeviceList& inputs) override
    {
        State replacement;
        for (const auto& entry : inputs) replacement.inputs.push_back({entry.mDisplayName,entry.mID});
        for (const auto& entry : outputs) replacement.outputs.push_back({entry.mDisplayName,entry.mID});
        std::lock_guard lock(mutex);
        replacement.generation = state.generation+1;
        state = std::move(replacement);
    }
    void LogMessage(LogLevel,const std::string&) override {}
};

LLVKVoice::LLVKVoice(std::string input,std::string output) : mImpl(std::make_unique<Impl>())
{
    mImpl->input = std::move(input);
    mImpl->output = std::move(output);
}

LLVKVoice::~LLVKVoice()
{
    std::string error;
    if (!stop(error)) std::terminate();
}

bool LLVKVoice::refresh(std::string& error)
{
    if (!mImpl->start(error)) return false;
    mImpl->device->refreshDevices();
    return true;
}

bool LLVKVoice::configure(const AudioConfig& config,std::string& error)
{
    if (!mImpl->onThread(error)) return false;
    if (mImpl->stopped) { error = "Native voice service has stopped"; return false; }
    if (config.mNoiseSuppressionLevel < AudioConfig::NOISE_SUPPRESSION_LEVEL_NONE ||
        config.mNoiseSuppressionLevel > AudioConfig::NOISE_SUPPRESSION_LEVEL_VERY_HIGH)
    { error = "Invalid native voice noise suppression level"; return false; }
    const auto& current = mImpl->audioConfig;
    if (current.mAGC == config.mAGC && current.mEchoCancellation == config.mEchoCancellation &&
        current.mNoiseSuppressionLevel == config.mNoiseSuppressionLevel) return true;
    if (mImpl->device) mImpl->device->setAudioConfig(config);
    mImpl->audioConfig = config;
    return true;
}

std::optional<LLVKVoice::State> LLVKVoice::state(std::string& error)
{
    if (!mImpl->start(error)) return std::nullopt;
    State result;
    { std::lock_guard lock(mImpl->mutex); result = mImpl->state; }
    result.tuning = mImpl->tuning;
    result.audioConfig = mImpl->audioConfig;
    if (result.tuning)
    {
        const auto level = mImpl->device->getTuningAudioLevel();
        result.energy = std::isfinite(level) ? 0.8f-0.01f*level : 0.f;
    }
    return result;
}

bool LLVKVoice::select(bool input,const std::string& device,std::string& error)
{
    if (!mImpl->start(error)) return false;
    if (input) mImpl->device->setCaptureDevice(device); else mImpl->device->setRenderDevice(device);
    return true;
}

bool LLVKVoice::tune(bool enabled,float gain,std::string& error)
{
    if (!mImpl->onThread(error)) return false;
    if (mImpl->stopped) { error = "Native voice service has stopped"; return false; }
    if (!std::isfinite(gain) || gain < 0.f || gain > 2.f)
    { error = "Invalid native voice tuning gain"; return false; }
    if (!enabled && !mImpl->device) return true;
    if (!mImpl->start(error)) return false;
    if (mImpl->tuning != enabled)
    {
        mImpl->device->setVoiceEnabled(enabled);
        mImpl->device->setTuningMode(enabled);
        mImpl->tuning = enabled;
    }
    if (enabled) mImpl->device->setTuningMicGain(gain);
    return true;
}

bool LLVKVoice::stop(std::string& error)
{
    if (!mImpl->onThread(error)) return false;
    if (mImpl->device)
    {
        mImpl->device->unsetDevicesObserver(mImpl.get());
        mImpl->device->setTuningMode(false);
        mImpl->device->setVoiceEnabled(false);
        llwebrtc::terminate();
        mImpl->device = nullptr;
    }
    mImpl->tuning = false;
    mImpl->stopped = true;
    { std::lock_guard lock(mImpl->mutex); mImpl->state = {}; }
    return true;
}