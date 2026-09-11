#ifndef LLVKSKINFILES_H
#define LLVKSKINFILES_H

#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

class LLVKSkinFiles final
{
public:
    struct Configuration
    {
        std::filesystem::path executableDirectory, workingDirectory, skinBaseDirectory, userAppDirectory;
        std::string skin = "default";
        std::string theme;
        std::string language = "en";
    };
    using Exists = std::function<bool(const std::filesystem::path&)>;
    enum class Policy { Current, All };
    explicit LLVKSkinFiles(Configuration configuration, Exists exists = {});
    std::optional<std::vector<std::filesystem::path>> find(const std::string& subdirectory,
        const std::string& filename, Policy policy, std::string& error) const;
    std::optional<std::vector<std::string>> read(const std::string& subdirectory,
        const std::string& filename, Policy policy, std::string& error) const;
    void invalidate();
private:
    Configuration mConfiguration;
    Exists mExists;
    std::vector<std::filesystem::path> mDirectories;
    mutable std::map<std::filesystem::path,bool> mExistence;
    mutable std::map<std::string,std::string> mLanguages;
};

#endif