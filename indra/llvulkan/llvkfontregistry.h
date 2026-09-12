#ifndef LLVKFONTREGISTRY_H
#define LLVKFONTREGISTRY_H

#include "llvkfont.h"
#include <filesystem>

class LLVKFontRegistry final
{
public:
    struct Request
    {
        std::string name;
        std::string size;
        std::uint8_t style = 0;
        bool tabularNumbers = false;
        auto operator<=>(const Request&) const = default;
    };
    struct Configuration
    {
        std::string platform;
        std::vector<std::filesystem::path> searchDirectories;
        std::vector<std::string> ultimateFallbacks;
        float horizontalDpi = 96.f;
        float verticalDpi = 96.f;
        float sizeAdjustment = 0.f;
        bool monochromeEmoji = false;
    };
    static Request normalize(Request request);
    static std::unique_ptr<LLVKFontRegistry> create(std::span<const std::string> documents,
                                                   Configuration configuration, std::string& error);
    std::shared_ptr<LLVKFont> resolve(const Request& request, std::string& error);
    std::string diagnostics() const;
    ~LLVKFontRegistry();
private:
    struct Impl;
    explicit LLVKFontRegistry(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> mImpl;
};

#endif