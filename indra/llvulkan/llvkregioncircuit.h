#pragma once

#include "llvkloginprotocol.h"
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
private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};