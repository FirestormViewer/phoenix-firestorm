#ifndef LLVKSTARTUP_H
#define LLVKSTARTUP_H

#include <optional>
#include <string>
#include "llvkproxy.h"

class LLControlGroup;
std::optional<int> llvkStartup(const std::wstring& commandLine, const std::string& profileName,
    const std::string& shortVersion, LLControlGroup& globalSettings,
    LLControlGroup& accountSettings, LLControlGroup& crashSettings, LLControlGroup& warningSettings,
    const LLVKProxy::CredentialFactory& proxyCredentials = {}, const std::function<void()>& clearSpamQueues = {},
    const std::string& executionMarkerName = {});

#endif