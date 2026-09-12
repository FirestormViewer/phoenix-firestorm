#include "llvktranslation.h"
#include "lluri.h"
#include <boost/json.hpp>
#include <boost/url.hpp>

std::optional<LLVKTranslation::Request> LLVKTranslation::verification(const std::string& service,const LLSD& key,std::string& error)
{
    error.clear();
    Request request;
    if (service=="google")
    {
        if (!key.isString() || key.asString().empty()) { error="A Google translation key is required"; return std::nullopt; }
        request.url="https://www.googleapis.com/language/translate/v2/languages?key="+LLURI::escape(key.asString())+"&target=en";
        request.headers["Accept"]="application/json";
    }
    else if (service=="azure" || service=="deepl")
    {
        if (!key.isMap() || key["id"].asString().empty()) { error="A translation service key is required"; return std::nullopt; }
        const auto endpoint=key[service=="azure" ? "endpoint" : "domain"].asString();
        const auto parsed=boost::urls::parse_uri(endpoint);
        if (!parsed || parsed->scheme()!="https" || parsed->host().empty() || parsed->has_userinfo() || parsed->has_query() || parsed->has_fragment())
        { error="Translation endpoint must be an HTTPS URL without credentials, query, or fragment"; return std::nullopt; }
        request.url=endpoint+(endpoint.back()=='/' ? "" : "/"); request.post=true;
        if (service=="azure")
        {
            request.url+="translate?api-version=3.0&to=en";
            request.headers["Content-Type"]="application/json";
            request.headers["Ocp-Apim-Subscription-Key"]=key["id"].asString();
            if (key.has("region")) request.headers["Ocp-Apim-Subscription-Region"]=key["region"].asString();
            request.body="[{\"intentionally_invalid_400\"}]";
        }
        else
        {
            request.url+="v2/translate";
            request.headers["Content-Type"]="application/x-www-form-urlencoded";
            request.headers["Authorization"]="DeepL-Auth-Key "+key["id"].asString();
            request.body="text=&target_lang=EN";
        }
    }
    else { error="Unknown native translation service"; return std::nullopt; }
    for (const auto& [name,value] : request.headers)
        if (value.size()>16384 || value.find_first_of("\r\n")!=std::string::npos || value.find('\0')!=std::string::npos)
        { error="Invalid translation request header value"; return std::nullopt; }
    if (request.url.size()>16384) { error="Translation request exceeds URL limit"; return std::nullopt; }
    return request;
}

bool LLVKTranslation::verified(const std::string& service,int status,std::string_view body)
{
    if (service=="google" || service=="deepl") return status==200;
    if (service!="azure" || status!=400 || body.empty() || body.size()>1024*1024) return false;
    boost::system::error_code error;
    boost::json::parse(body,error);
    return !error.failed();
}