#include "linden_common.h"
#include "llvkloginprotocol.h"
#include "llxmlnode.h"
#include "llmd5.h"
#include <boost/asio/ip/address.hpp>
#include <boost/url.hpp>
#include <charconv>

namespace
{
    bool unsignedValue(const LLSD& value,std::uint32_t& result,bool signedBits=false)
    {
        if (signedBits && value.isInteger()) { result=static_cast<std::uint32_t>(value.asInteger()); return true; }
        if (!value.isInteger() && !value.isString()) return false;
        const auto text=value.asString();
        const auto parsed=std::from_chars(text.data(),text.data()+text.size(),result);
        return parsed.ec==std::errc{} && parsed.ptr==text.data()+text.size();
    }
}

bool LLVKLoginProtocol::boundedXml(const std::string& document)
{
    if (document.empty() || document.size()>8*1024*1024 || document.find('\0')!=document.npos) return false;
    struct Guard
    {
        XML_Parser parser=nullptr;
        unsigned depth=0,nodes=0;
        ~Guard() { if (parser) XML_ParserFree(parser); }
    } guard;
    guard.parser=XML_ParserCreate(nullptr);
    if (!guard.parser) return false;
    XML_SetUserData(guard.parser,&guard);
    XML_SetElementHandler(guard.parser,[](void* opaque,const XML_Char*,const XML_Char**)
    {
        auto& state=*static_cast<Guard*>(opaque);
        if (++state.depth>64 || ++state.nodes>200000) XML_StopParser(state.parser,XML_FALSE);
    },[](void* opaque,const XML_Char*) { --static_cast<Guard*>(opaque)->depth; });
    XML_SetStartDoctypeDeclHandler(guard.parser,[](void* opaque,const XML_Char*,const XML_Char*,const XML_Char*,int)
    { XML_StopParser(static_cast<Guard*>(opaque)->parser,XML_FALSE); });
    return XML_Parse(guard.parser,document.data(),static_cast<int>(document.size()),XML_TRUE)==XML_STATUS_OK;
}

std::optional<LLSD> LLVKLoginProtocol::credentials(std::string account,const std::string& password,
    const std::string& start,std::string& error)
{
    error.clear();
    LLStringUtil::trim(account);
    const auto separator=account.find_first_of(" ._");
    const auto first=account.substr(0,separator);
    auto last=separator==account.npos ? std::string("Resident") : account.substr(separator+1);
    LLStringUtil::trim(last);
    const auto invalid=[](const std::string& value)
    { return value.empty() || value.size()>64 || value.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-")!=value.npos; };
    if (invalid(first) || invalid(last) || password.empty() || password.size()>256 || password.find('\0')!=password.npos)
    { error="Enter a valid Second Life account name and password"; return {}; }
    if (start!="home" && start!="last")
    { error="Native login currently requires Home or Last Location"; return {}; }
    LLMD5 digest(reinterpret_cast<const unsigned char*>(password.c_str()));
    char hash[MD5HEX_STR_SIZE]{};
    digest.hex_digest(hash);
    LLSD parameters;
    parameters["first"]=first; parameters["last"]=last;
    parameters["passwd"]="$1$"+std::string(hash);
    parameters["start"]=start;
    return parameters;
}

std::optional<std::string> LLVKLoginProtocol::encode(const LLSD& parameters,std::string& error)
{
    error.clear();
    if (!parameters.isMap()) { error="Native login parameters must be a map"; return std::nullopt; }
    const auto value=parameters.asXMLRPCValue();
    if (value.size()>65536 || value.find('\0')!=value.npos)
    { error="Native login request exceeds its limit"; return std::nullopt; }
    return "<?xml version=\"1.0\"?><methodCall><methodName>login_to_simulator</methodName><params><param>"+
        value+"</param></params></methodCall>";
}

std::optional<LLSD> LLVKLoginProtocol::decode(const std::string& response,std::string& error)
{
    error.clear();
    if (!boundedXml(response))
    { error="Invalid native login response size or declaration"; return std::nullopt; }
    LLXMLNodePtr root;
    if (!LLXMLNode::parseBuffer(response.data(),response.size(),root,nullptr) || !root || !root->hasName("methodResponse"))
    { error="Invalid native XML-RPC response"; return std::nullopt; }
    const auto params=root->getFirstChild();
    const auto param=params ? params->getFirstChild() : LLXMLNodePtr();
    auto value=param ? param->getFirstChild() : LLXMLNodePtr();
    LLSD decoded;
    try
    {
    if (!params || !params->hasName("params") || params->getNextSibling() || !param || !param->hasName("param") ||
        param->getNextSibling() || !value || !value->hasName("value") || value->getNextSibling() ||
        !value->fromXMLRPCValue(decoded) || !decoded.isMap())
    { error="Invalid native login response structure"; return std::nullopt; }
    }
    catch (const std::invalid_argument&) { error="Invalid native login numeric value"; return {}; }
    catch (const std::out_of_range&) { error="Native login numeric value exceeds its limit"; return {}; }
    return decoded;
}

std::optional<LLVKLoginProtocol::Bootstrap> LLVKLoginProtocol::bootstrap(const LLSD& response,std::string& error)
{
    error.clear();
    Bootstrap result;
    std::uint32_t port=0,regionX=0,regionY=0;
    if (!response.isMap() || response["login"].asString()!="true" ||
        !result.agentId.set(response["agent_id"].asString(),false) || result.agentId.isNull() ||
        !result.sessionId.set(response["session_id"].asString(),false) || result.sessionId.isNull() ||
        !result.secureSessionId.set(response["secure_session_id"].asString(),false) || result.secureSessionId.isNull() ||
        !unsignedValue(response["circuit_code"],result.circuitCode,true) || !result.circuitCode ||
        !unsignedValue(response["sim_port"],port) || !port || port>65535 ||
        !unsignedValue(response["region_x"],regionX) || !unsignedValue(response["region_y"],regionY))
    { error="Native login authorization is missing valid session or region fields"; return std::nullopt; }
    result.simulatorAddress=response["sim_ip"].asString();
    boost::system::error_code addressError;
    const auto address=boost::asio::ip::make_address(result.simulatorAddress,addressError);
    if (addressError || !address.is_v4() || address.is_unspecified() || address.is_multicast())
    { error="Native login simulator address is invalid"; return std::nullopt; }
    result.seedCapability=response["seed_capability"].asString();
    const auto seed=boost::urls::parse_uri(result.seedCapability);
    if (result.seedCapability.size()>16384 || !seed || seed->scheme()!="https" || seed->host().empty() ||
        seed->has_userinfo() || seed->has_fragment())
    { error="Native login seed capability must be a valid HTTPS URL"; return std::nullopt; }
    result.simulatorPort=static_cast<std::uint16_t>(port);
    result.regionHandle=(std::uint64_t(regionX)<<32)|regionY;
    return result;
}