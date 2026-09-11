#ifndef LLVKJOYSTICK_H
#define LLVKJOYSTICK_H

#include "llsd.h"
#include <array>
#include <memory>
#include <string>
#include <vector>

class LLVKJoystick final
{
public:
    struct Device { std::string name; LLSD id; std::string persistedId; };
    struct State
    {
        std::array<float,8> axes{};
        std::array<bool,32> buttons{};
        std::size_t axisCount = 0, buttonCount = 0;
        bool connected = false;
    };
    LLVKJoystick();
    ~LLVKJoystick();
    bool start(void* window, std::string& error);
    bool enumerate(std::string& error);
    bool select(const LLSD& id, std::string& error);
    bool poll(std::string& error);
    void stop();
    const std::vector<Device>& devices() const noexcept;
    const State& state() const noexcept;
    LLSD selected() const;
private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};

#endif