#ifndef LLVKSTARTUPSETTINGS_H
#define LLVKSTARTUPSETTINGS_H

#include "llsd.h"
#include <filesystem>
#include <map>
#include <optional>
#include <string_view>

class LLVKStartupSettings final
{
public:
    struct Entry
    {
        std::string type;
        LLSD defaultValue;
        std::optional<LLSD> saved, transient;
        bool persistent = true;
        const LLSD& value() const { return transient ? *transient : saved ? *saved : defaultValue; }
        const LLSD& saveValue() const { return saved ? *saved : defaultValue; }
    };
    bool load(std::string_view xml, bool defaults, bool saved, std::string& error);
    bool loadFile(const std::filesystem::path& path, bool required, bool defaults, bool saved, std::string& error);
    bool set(const std::string& name, const LLSD& value, bool saved, std::string& error);
    const Entry* find(const std::string& name) const;
    std::map<std::string,LLSD> values() const;
private:
    std::map<std::string,Entry> mEntries;
};

#endif