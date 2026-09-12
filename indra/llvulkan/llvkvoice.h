#ifndef LLVKVOICE_H
#define LLVKVOICE_H

#include "llwebrtc.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class LLVKVoice final
{
public:
    using AudioConfig = llwebrtc::LLWebRTCDeviceInterface::AudioConfig;
    struct State
    {
        struct Device { std::string label, id; };
        std::vector<Device> inputs, outputs;
        std::uint64_t generation = 0;
        bool tuning = false;
        float energy = 0.f;
        AudioConfig audioConfig;
    };
    LLVKVoice(std::string input = "Default", std::string output = "Default");
    ~LLVKVoice();
    LLVKVoice(const LLVKVoice&) = delete;
    LLVKVoice& operator=(const LLVKVoice&) = delete;
    bool refresh(std::string& error);
    bool configure(const AudioConfig& config, std::string& error);
    std::optional<State> state(std::string& error);
    bool select(bool input, const std::string& device, std::string& error);
    bool tune(bool enabled, float gain, std::string& error);
    bool stop(std::string& error);
private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};

#endif