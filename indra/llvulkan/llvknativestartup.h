#ifndef LLVKNATIVESTARTUP_H
#define LLVKNATIVESTARTUP_H

#include <optional>
#include <string>

std::optional<int> llvkNativeStartup(const std::wstring& commandLine, const std::string& profileName,
    const std::string& shortVersion);

#endif