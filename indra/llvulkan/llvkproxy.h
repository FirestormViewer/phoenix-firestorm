#ifndef LLVKPROXY_H
#define LLVKPROXY_H

#include "llsd.h"
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>

class LLVKProxy final
{
public:
    enum class Type { None, Http, Socks5 };
    struct Credentials
    {
        std::string username, password;
    };
    struct CredentialServices
    {
        std::function<std::optional<Credentials>(std::string&)> load;
        std::function<bool(const std::optional<Credentials>&,std::string&)> save;
    };
    using CredentialFactory = std::function<CredentialServices(const std::filesystem::path&)>;
    using CredentialWriter = std::function<bool(const std::filesystem::path&,
        const std::optional<Credentials>&,std::string&)>;
    static bool saveCredentialFile(const std::filesystem::path& file,
        const std::optional<Credentials>& credentials, const CredentialWriter& writer, std::string& error);
    struct Endpoint
    {
        Type type = Type::None;
        std::string host;
        int port = 0;
        bool passwordAuthentication = false;
    };
    static std::optional<Endpoint> select(const std::map<std::string,LLSD>& settings,
        bool browser, std::string& error);
};

#endif