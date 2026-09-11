#ifndef LLVKAUDIO_H
#define LLVKAUDIO_H

#include <memory>
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
    bool active() const noexcept;
    std::string driverName() const;
private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};

#endif