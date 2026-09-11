#ifndef LLVKSTARTUPSETTINGS_H
#define LLVKSTARTUPSETTINGS_H

#include "llsd.h"
#include <filesystem>
#include <map>
#include <optional>
#include <string_view>

class LLVKAutoReplaceSettings final
{
public:
    enum class AddResult { Added, DuplicateName, InvalidList };
    bool set(const LLSD& lists);
    const LLSD& lists() const noexcept { return mLists; }
    const LLSD* find(const std::string& name) const;
    AddResult add(const LLSD& list, bool replace = false);
    bool remove(const std::string& name);
    bool move(const std::string& name, bool up);
    bool setEntry(const std::string& name, const std::string& keyword, const std::string& replacement);
    bool removeEntry(const std::string& name, const std::string& keyword);
    std::string replaceWord(const std::string& word, bool enabled) const;
    static bool validList(const LLSD& list);
    bool loadFile(const std::filesystem::path& path, std::string& error);
    bool saveFile(const std::filesystem::path& path, std::string& error) const;
    static std::optional<LLSD> readListFile(const std::filesystem::path& path, std::string& error);
    static bool writeListFile(const std::filesystem::path& path, const LLSD& list, std::string& error);
private:
    LLSD mLists = LLSD::emptyArray();
};

class LLVKStartupSettings final
{
public:
    struct Entry
    {
        std::string type;
        LLSD defaultValue;
        LLSD definition;
        std::optional<LLSD> saved, transient;
        bool persistent = true;
        const LLSD& value() const { return transient ? *transient : saved ? *saved : defaultValue; }
        const LLSD& saveValue() const { return saved ? *saved : defaultValue; }
    };
    bool load(std::string_view xml, bool defaults, bool saved, std::string& error);
    bool loadFile(const std::filesystem::path& path, bool required, bool defaults, bool saved, std::string& error);
    bool set(const std::string& name, const LLSD& value, bool saved, std::string& error);
    bool saveChanges(const std::filesystem::path& path, const std::map<std::string,LLSD>& changes, std::string& error);
    const Entry* find(const std::string& name) const;
    std::map<std::string,LLSD> values() const;
    std::map<std::string,LLSD> defaults() const;
private:
    std::map<std::string,Entry> mEntries;
};

#endif