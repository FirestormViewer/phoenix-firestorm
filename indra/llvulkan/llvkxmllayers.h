#ifndef LLVKXMLLAYERS_H
#define LLVKXMLLAYERS_H

#include <optional>
#include <span>
#include <string>
#include <string_view>

class LLVKXmlLayers final
{
public:
    static std::optional<std::string> merge(std::span<const std::string_view> layers, std::string& error);
};

#endif