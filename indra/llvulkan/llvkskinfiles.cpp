#include "llvkskinfiles.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace
{
    std::filesystem::path utf8Path(const std::string& value)
    {
        return std::filesystem::path(std::u8string(value.begin(),value.end()));
    }
    bool relative(const std::string& value)
    {
        return value.find("..") == std::string::npos && value.find(':') == std::string::npos &&
            value.find('\0') == std::string::npos && !utf8Path(value).has_root_path() &&
            (value.empty() || (value.front() != '/' && value.front() != '\\'));
    }
    bool component(const std::string& value)
    {
        return relative(value) && value.find_first_of("/\\") == std::string::npos;
    }
    bool sameDirectory(const std::filesystem::path& left, const std::filesystem::path& right)
    {
        auto first = left.generic_wstring();
        auto second = right.generic_wstring();
        const auto lower = [](wchar_t value) { return value >= L'A' && value <= L'Z' ? value+(L'a'-L'A') : value; };
        std::transform(first.begin(),first.end(),first.begin(),lower);
        std::transform(second.begin(),second.end(),second.begin(),lower);
        return first == second;
    }
}

LLVKSkinFiles::LLVKSkinFiles(Configuration configuration, Exists exists)
    : mConfiguration(std::move(configuration)), mExists(std::move(exists))
{
    if (!component(mConfiguration.skin) || !component(mConfiguration.theme) || !component(mConfiguration.language))
        throw std::invalid_argument("Invalid native skin, theme or language component");
    if (!mExists) mExists = [](const auto& path)
    {
        std::error_code error;
        return std::filesystem::exists(path,error) && !error;
    };
    const auto add = [&](std::filesystem::path path)
    {
        if (std::find(mDirectories.begin(),mDirectories.end(),path) == mDirectories.end())
            mDirectories.push_back(std::move(path));
    };
    if (!sameDirectory(mConfiguration.executableDirectory,mConfiguration.workingDirectory))
    {
        add(mConfiguration.executableDirectory / "skins");
        add(mConfiguration.executableDirectory / "skins" / "default");
    }
    add(mConfiguration.skinBaseDirectory / "default");
    const auto selected = mConfiguration.skinBaseDirectory / utf8Path(mConfiguration.skin);
    add(selected);
    if (!mConfiguration.theme.empty()) add(selected / "themes" / utf8Path(mConfiguration.theme));
    add(mConfiguration.userAppDirectory / "skins" / "default");
    add(mConfiguration.userAppDirectory / "skins" / utf8Path(mConfiguration.skin));
}

void LLVKSkinFiles::invalidate()
{
    mExistence.clear();
    mLanguages.clear();
}

std::optional<std::vector<std::filesystem::path>> LLVKSkinFiles::find(const std::string& subdirectory,
    const std::string& filename, Policy policy, std::string& error) const
{
    error.clear();
    if (!relative(subdirectory) || !relative(filename) || filename.empty() ||
        (policy != Policy::Current && policy != Policy::All))
    { error = "Invalid native skin lookup path or policy"; return std::nullopt; }
    auto language = mLanguages.find(subdirectory);
    if (language == mLanguages.end())
    {
        std::string fallback;
        if (!subdirectory.empty() && subdirectory != "textures")
        {
            const auto base = mConfiguration.skinBaseDirectory / "default" / utf8Path(subdirectory);
            if (mExists(base / "en")) fallback = "en";
            else if (mExists(base / "en-us")) fallback = "en-us";
        }
        language = mLanguages.emplace(subdirectory,std::move(fallback)).first;
    }
    std::vector<std::string> languages{language->second};
    if (!language->second.empty() && mConfiguration.language != language->second) languages.push_back(mConfiguration.language);
    std::vector<std::filesystem::path> result;
    std::map<std::string,std::filesystem::path> selected;
    for (const auto& directory : mDirectories)
        for (const auto& current : languages)
        {
            auto path = directory;
            if (!subdirectory.empty()) path /= utf8Path(subdirectory);
            if (!current.empty()) path /= utf8Path(current);
            path /= utf8Path(filename);
            auto exists = mExistence.find(path);
            if (exists == mExistence.end()) exists = mExistence.emplace(path,mExists(path)).first;
            if (!exists->second) continue;
            if (policy == Policy::All) result.push_back(path);
            else selected.insert_or_assign(current,path);
        }
    if (policy == Policy::Current)
        for (const auto& current : languages)
        {
            const auto found = selected.find(current);
            if (found != selected.end()) result.push_back(found->second);
        }
    return result;
}

std::optional<std::vector<std::string>> LLVKSkinFiles::read(const std::string& subdirectory,
    const std::string& filename, Policy policy, std::string& error) const
{
    const auto paths = find(subdirectory,filename,policy,error);
    if (!paths) return std::nullopt;
    if (paths->empty()) { error = "Native skin file not found: " + filename; return std::nullopt; }
    std::vector<std::string> documents;
    std::size_t total = 0;
    for (const auto& path : *paths)
    {
        std::ifstream stream(path,std::ios::binary|std::ios::ate);
        if (!stream) { error = "Native skin file could not be opened: " + filename; return std::nullopt; }
        const auto size = stream.tellg();
        if (size < 0 || size > 4*1024*1024 || static_cast<std::size_t>(size) > 64*1024*1024-total)
        { error = "Native skin file exceeds declaration byte budget"; return std::nullopt; }
        std::string data(static_cast<std::size_t>(size),'\0');
        stream.seekg(0);
        if (!stream.read(data.data(),static_cast<std::streamsize>(data.size())))
        { error = "Native skin file read failed: " + filename; return std::nullopt; }
        total += data.size();
        documents.push_back(std::move(data));
    }
    return documents;
}