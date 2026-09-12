#ifndef LLVKMEDIAFILTER_H
#define LLVKMEDIAFILTER_H

#include "llsd.h"
#include <cstdint>
#include <optional>
#include <string>

class LLVKMediaFilter final
{
public:
    enum class Action { Allow, Deny };
    bool load(const LLSD& rules, std::string& error);
    bool add(const std::string& url, Action action, std::string& error);
    bool remove(const std::string& domain);
    std::optional<Action> decide(const std::string& url) const;
    static std::string domain(std::string url);
    const LLSD& rules() const noexcept { return mRules; }
    std::uint64_t revision() const noexcept { return mRevision; }
private:
    LLSD mRules = LLSD::emptyArray();
    std::uint64_t mRevision = 0;
};

#endif