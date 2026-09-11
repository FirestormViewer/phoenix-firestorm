#ifndef LLVKTRANSLATION_H
#define LLVKTRANSLATION_H

#include "llsd.h"
#include <map>
#include <optional>
#include <string>

struct LLVKTranslation final
{
    struct Request
    {
        std::string url, body;
        std::map<std::string,std::string> headers;
        bool post = false;
    };
    static std::optional<Request> verification(const std::string& service, const LLSD& key, std::string& error);
    static bool verified(const std::string& service, int status, std::string_view body);
};

#endif