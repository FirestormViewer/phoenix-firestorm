#pragma once

#include "llvkloginprotocol.h"
#include "llvkchatprotocol.h"
#include <memory>
#include <string>

class LLVKRegionCircuit final
{
public:
    enum class Status { Idle, Connecting, Connected, Closing, Closed, Failed };
    LLVKRegionCircuit();
    ~LLVKRegionCircuit();
    bool start(const LLVKLoginProtocol::Bootstrap& bootstrap,std::string& error);
    Status pump(std::string& error);
    Status close(std::string& error);
    void cancel();
    bool sendLocal(const std::string& text,std::uint8_t type,std::int32_t channel,std::string& error);
    bool sendInstant(const LLVKChatProtocol::Message& message,std::string& error);
    std::vector<LLVKChatProtocol::Message> takeMessages();
    const std::string& regionName() const;
private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};