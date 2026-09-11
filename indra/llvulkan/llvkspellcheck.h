#ifndef LLVKSPELLCHECK_H
#define LLVKSPELLCHECK_H

#include "llsd.h"
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class Hunspell;

class LLVKSpellCheck final
{
public:
    LLVKSpellCheck(std::filesystem::path appDirectory, std::filesystem::path userDirectory);
    ~LLVKSpellCheck();
    bool refresh(std::string& error);
    bool activate(const std::string& primary, const std::vector<std::string>& secondary, std::string& error);
    bool check(const std::string& word) const;
    std::vector<std::string> suggestions(const std::string& word) const;
    bool active() const noexcept { return mHunspell!=nullptr; }
    const std::string& primary() const noexcept { return mPrimary; }
    const std::vector<std::string>& secondary() const noexcept { return mSecondary; }
    const LLSD& dictionaries() const noexcept { return mDictionaries; }
    const LLSD* dictionary(const std::string& language) const;
    bool canRemove(const std::string& language) const;
    bool remove(const std::string& language, std::string& error);
    bool importDictionary(const std::filesystem::path& path, std::string language, std::string& error);
    static std::optional<std::filesystem::path> resolveImportPath(const std::filesystem::path& path, std::string& error);
private:
    std::filesystem::path mAppDirectory, mUserDirectory;
    LLSD mDictionaries = LLSD::emptyArray();
    std::unique_ptr<Hunspell> mHunspell;
    std::string mPrimary;
    std::vector<std::string> mSecondary, mIgnored;
};

#endif