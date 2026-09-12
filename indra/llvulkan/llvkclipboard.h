#ifndef LLVKCLIPBOARD_H
#define LLVKCLIPBOARD_H

#include <memory>
#include <optional>
#include <string>
#include <string_view>

class LLVKClipboard
{
public:
    virtual ~LLVKClipboard() = default;
    virtual bool available(bool primary) const = 0;
    virtual std::optional<std::u32string> read(bool primary, std::string& error) = 0;
    virtual bool write(std::u32string_view text, bool primary, std::string& error) = 0;
    static std::shared_ptr<LLVKClipboard> forWindow(void* window, std::string& error);
    static std::optional<std::wstring> encodeWindows(std::u32string_view text, std::string& error);
    static std::optional<std::u32string> decodeWindows(std::wstring_view text, std::string& error);
};

#endif