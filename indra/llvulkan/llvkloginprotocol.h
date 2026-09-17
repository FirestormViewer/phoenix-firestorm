#pragma once

#include "llsd.h"
#include "lluuid.h"
#include <cstdint>
#include <optional>
#include <string>

class LLVKLoginProtocol final
{
public:
    struct Bootstrap
    {
        LLUUID agentId;
        LLUUID sessionId;
        LLUUID secureSessionId;
        std::uint32_t circuitCode=0;
        std::string simulatorAddress;
        std::uint16_t simulatorPort=0;
        std::uint64_t regionHandle=0;
        std::string seedCapability;
    };
    static std::optional<std::string> encode(const LLSD& parameters,std::string& error);
    static std::optional<LLSD> credentials(std::string account,const std::string& password,
        const std::string& start,std::string& error);
    static bool boundedXml(const std::string& document);
    static std::optional<LLSD> decode(const std::string& response,std::string& error);
    static std::optional<Bootstrap> bootstrap(const LLSD& response,std::string& error);
};