#ifndef LLVKAUDIO_H
#define LLVKAUDIO_H

#include <memory>
#include <optional>
#include <span>
#include <cstdint>
#include <string>

class LLVKAudio final
{
public:
    LLVKAudio();
    ~LLVKAudio();
    LLVKAudio(const LLVKAudio&) = delete;
    LLVKAudio& operator=(const LLVKAudio&) = delete;
    bool start(bool disabled, std::string& error);
    bool stop(std::string& error);
    struct Volume
    {
        float master = 1.f;
        bool muted = false, windowActive = true, muteWhenInactive = false, progressVisible = false;
    };
    bool setVolume(const Volume& volume, std::string& error);
    std::optional<float> listenerGain(std::string& error) const;
    bool playUiWav(std::span<const std::uint8_t> wav, std::string& error);
    bool setUiGain(float gain, bool muted, std::string& error);
    bool update(std::string& error);
    std::size_t activeUiSounds() const noexcept;
    bool active() const noexcept;
    std::string driverName() const;
private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};

#endif