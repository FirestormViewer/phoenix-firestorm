#include "llvkproxy.h"
#include "lluuid.h"
#if LL_WINDOWS
#include <windows.h>
#endif

bool LLVKProxy::saveCredentialFile(const std::filesystem::path& file,
    const std::optional<Credentials>& credentials, const CredentialWriter& writer, std::string& error)
{
    error.clear();
    if (!writer || !file.is_absolute()) { error="Invalid protected credential destination"; return false; }
    std::error_code status;
    std::filesystem::create_directories(file.parent_path(),status);
    if (status) { error="Cannot create protected credential directory"; return false; }
    if (std::filesystem::is_symlink(file,status) || std::filesystem::is_symlink(file.parent_path(),status))
    { error="Linked protected credential destinations are not allowed"; return false; }
    status.clear();
    const auto staging=file.parent_path()/(".native-credentials-"+LLUUID::generateNewID().asString());
    if (!std::filesystem::create_directory(staging,status)) { error="Cannot stage protected credentials"; return false; }
    struct Staging
    {
        std::filesystem::path directory;
        ~Staging()
        {
            std::error_code ignored;
            std::filesystem::remove(directory/"credentials.tmp",ignored);
            std::filesystem::remove(directory/"credentials",ignored);
            std::filesystem::remove(directory,ignored);
        }
    } cleanup{staging};
    const auto pending=staging/"credentials";
    const bool exists=std::filesystem::exists(file,status);
    if (status) { error="Cannot inspect protected credential store"; return false; }
    if (exists)
    {
        if (!std::filesystem::is_regular_file(file,status) || std::filesystem::file_size(file,status)>16*1024*1024 || status)
        { error="Invalid protected credential store"; return false; }
        if (!std::filesystem::copy_file(file,pending,std::filesystem::copy_options::none,status))
        { error="Cannot stage existing protected credentials"; return false; }
    }
    if (!writer(pending,credentials,error)) return false;
    if (!std::filesystem::is_regular_file(pending,status) || status)
    { error="Protected credential writer did not produce a verified file"; return false; }
#if LL_WINDOWS
    if (!MoveFileExW(pending.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
    { error="Cannot publish protected credentials"; return false; }
#else
    std::filesystem::rename(pending,file,status);
    if (status) { error="Cannot publish protected credentials"; return false; }
#endif
    return true;
}

std::optional<LLVKProxy::Endpoint> LLVKProxy::select(const std::map<std::string,LLSD>& settings,
    bool browser, std::string& error)
{
    error.clear();
    const auto value=[&](const char* name) -> LLSD
    {
        const auto found=settings.find(name);
        return found==settings.end() ? LLSD() : found->second;
    };
    const auto mode=browser ? (value("BrowserProxyEnabled").asBoolean() ? "Web" : "None") :
        value("HttpProxyType").asString();
    Endpoint endpoint;
    if (mode.empty() || mode=="None") return endpoint;
    if (mode=="Web" && value("BrowserProxyEnabled").asBoolean())
    {
        endpoint.type=Type::Http;
        endpoint.host=value("BrowserProxyAddress").asString();
        endpoint.port=value("BrowserProxyPort").asInteger();
    }
    else if (mode=="Socks" && value("Socks5ProxyEnabled").asBoolean())
    {
        endpoint.type=Type::Socks5;
        endpoint.host=value("Socks5ProxyHost").asString();
        endpoint.port=value("Socks5ProxyPort").asInteger();
        const auto authentication=value("Socks5AuthType").asString();
        if (authentication!="None" && authentication!="UserPass")
        { error="Invalid SOCKS5 authentication mode"; return {}; }
        endpoint.passwordAuthentication=authentication=="UserPass";
    }
    else { error="Selected HTTP proxy is disabled or invalid"; return {}; }
    if (endpoint.host.empty() || endpoint.host.size()>255 || endpoint.port<1 || endpoint.port>65535 ||
        endpoint.host.find_first_of("/@?#\\ \t\r\n")!=std::string::npos || endpoint.host.find('\0')!=std::string::npos)
    { error="Invalid proxy host or port"; return {}; }
    return endpoint;
}