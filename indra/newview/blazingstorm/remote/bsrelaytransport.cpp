/**
 * @file bsrelaytransport.cpp
 * @brief Azure Functions broker + Azure Web PubSub transport.
 */

#include "llviewerprecompiledheaders.h"

#include "blazingstorm/remote/bsrelaytransport.h"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#ifdef LL_WINDOWS
#include <wincrypt.h>
#endif

#include <algorithm>
#include <cctype>
#include <chrono>
#include <deque>
#include <functional>
#include <map>
#include <sstream>
#include <utility>

namespace
{
    namespace asio = boost::asio;
    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace websocket = beast::websocket;
    namespace ssl = asio::ssl;
    using tcp = asio::ip::tcp;

    struct ParsedUrl
    {
        std::string host;
        std::string hostHeader;
        std::string port;
        std::string target;
    };

    bool parseUrl(const std::string& url,
                  const std::string& scheme,
                  const std::string& default_port,
                  ParsedUrl& parsed,
                  std::string& error)
    {
        const std::string prefix = scheme + "://";
        if (url.compare(0, prefix.size(), prefix) != 0)
        {
            error = "URL must begin with " + prefix;
            return false;
        }

        const std::size_t authority_begin = prefix.size();
        std::size_t target_begin = url.find('/', authority_begin);
        const std::size_t query_begin = url.find('?', authority_begin);
        if (target_begin == std::string::npos
            || (query_begin != std::string::npos && query_begin < target_begin))
        {
            target_begin = query_begin;
        }

        const std::string authority = target_begin == std::string::npos
            ? url.substr(authority_begin)
            : url.substr(authority_begin, target_begin - authority_begin);

        if (authority.empty() || authority.find('@') != std::string::npos
            || authority.find('#') != std::string::npos
            || authority.find(' ') != std::string::npos)
        {
            error = "URL has an invalid host.";
            return false;
        }

        parsed.hostHeader = authority;
        parsed.port = default_port;

        if (authority.front() == '[')
        {
            const auto closing = authority.find(']');
            if (closing == std::string::npos || closing == 1)
            {
                error = "URL has an invalid IPv6 host.";
                return false;
            }
            parsed.host = authority.substr(1, closing - 1);
            if (closing + 1 < authority.size())
            {
                if (authority[closing + 1] != ':' || closing + 2 >= authority.size())
                {
                    error = "URL has an invalid port.";
                    return false;
                }
                parsed.port = authority.substr(closing + 2);
            }
        }
        else
        {
            const auto colon = authority.rfind(':');
            if (colon != std::string::npos)
            {
                if (colon == 0 || colon + 1 >= authority.size()
                    || authority.find(':') != colon)
                {
                    error = "URL has an invalid host or port.";
                    return false;
                }
                parsed.host = authority.substr(0, colon);
                parsed.port = authority.substr(colon + 1);
            }
            else
            {
                parsed.host = authority;
            }
        }

        if (parsed.host.empty())
        {
            error = "URL has an empty host.";
            return false;
        }

        for (const char ch : parsed.port)
        {
            if (ch < '0' || ch > '9')
            {
                error = "URL port must be numeric.";
                return false;
            }
        }

        if (target_begin == std::string::npos)
        {
            parsed.target = "/";
        }
        else if (url[target_begin] == '?')
        {
            parsed.target = "/" + url.substr(target_begin);
        }
        else
        {
            parsed.target = url.substr(target_begin);
        }

        if (parsed.target.empty() || parsed.target.find('#') != std::string::npos)
        {
            error = "URL has an invalid target.";
            return false;
        }
        return true;
    }

    std::string trimTrailingSlash(std::string value)
    {
        while (value.size() > 8 && !value.empty() && value.back() == '/')
        {
            value.pop_back();
        }
        return value;
    }

    std::string replaceSession(std::string path, const std::string& session_id)
    {
        static const std::string marker = "{session}";
        const auto pos = path.find(marker);
        if (pos != std::string::npos)
        {
            path.replace(pos, marker.size(), session_id);
        }
        return path;
    }

    std::string jsonEscape(const std::string& value)
    {
        std::string out;
        out.reserve(value.size() + 16);
        for (const unsigned char ch : value)
        {
            switch (ch)
            {
                case '\\': out += "\\\\"; break;
                case '"': out += "\\\""; break;
                case '\b': out += "\\b"; break;
                case '\f': out += "\\f"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (ch >= 0x20) out.push_back(static_cast<char>(ch));
                    break;
            }
        }
        return out;
    }

    bool jsonString(const std::string& json,
                    const std::string& key,
                    std::string& value)
    {
        const std::string marker = "\"" + key + "\"";
        auto pos = json.find(marker);
        if (pos == std::string::npos) return false;
        pos = json.find(':', pos + marker.size());
        if (pos == std::string::npos) return false;
        ++pos;
        while (pos < json.size()
               && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
        if (pos >= json.size() || json[pos] != '"') return false;
        ++pos;

        value.clear();
        while (pos < json.size())
        {
            const char ch = json[pos++];
            if (ch == '"') return true;
            if (ch != '\\')
            {
                value.push_back(ch);
                continue;
            }
            if (pos >= json.size()) return false;
            const char escaped = json[pos++];
            switch (escaped)
            {
                case '"': value.push_back('"'); break;
                case '\\': value.push_back('\\'); break;
                case '/': value.push_back('/'); break;
                case 'b': value.push_back('\b'); break;
                case 'f': value.push_back('\f'); break;
                case 'n': value.push_back('\n'); break;
                case 'r': value.push_back('\r'); break;
                case 't': value.push_back('\t'); break;
                default:
                    // Broker/Web PubSub fields used by this client are ASCII.
                    // Reject unsupported escape forms rather than interpreting
                    // security-sensitive tickets incorrectly.
                    return false;
            }
        }
        return false;
    }

    bool jsonBool(const std::string& json, const std::string& key, bool& value)
    {
        const std::string marker = "\"" + key + "\"";
        auto pos = json.find(marker);
        if (pos == std::string::npos) return false;
        pos = json.find(':', pos + marker.size());
        if (pos == std::string::npos) return false;
        ++pos;
        while (pos < json.size()
               && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
        if (json.compare(pos, 4, "true") == 0)
        {
            value = true;
            return true;
        }
        if (json.compare(pos, 5, "false") == 0)
        {
            value = false;
            return true;
        }
        return false;
    }

    bool jsonUnsigned(const std::string& json,
                      const std::string& key,
                      std::uint64_t& value)
    {
        const std::string marker = "\"" + key + "\"";
        auto pos = json.find(marker);
        if (pos == std::string::npos) return false;
        pos = json.find(':', pos + marker.size());
        if (pos == std::string::npos) return false;
        ++pos;
        while (pos < json.size()
               && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
        const auto begin = pos;
        while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') ++pos;
        if (pos == begin) return false;
        try
        {
            value = static_cast<std::uint64_t>(
                std::stoull(json.substr(begin, pos - begin)));
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool jsonObject(const std::string& json,
                    const std::string& key,
                    std::string& object)
    {
        const std::string marker = "\"" + key + "\"";
        auto pos = json.find(marker);
        if (pos == std::string::npos) return false;
        pos = json.find(':', pos + marker.size());
        if (pos == std::string::npos) return false;
        ++pos;
        while (pos < json.size()
               && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
        if (pos >= json.size() || json[pos] != '{') return false;

        const auto begin = pos;
        int depth = 0;
        bool in_string = false;
        bool escaped = false;
        for (; pos < json.size(); ++pos)
        {
            const char ch = json[pos];
            if (in_string)
            {
                if (escaped) escaped = false;
                else if (ch == '\\') escaped = true;
                else if (ch == '"') in_string = false;
                continue;
            }
            if (ch == '"')
            {
                in_string = true;
            }
            else if (ch == '{')
            {
                ++depth;
            }
            else if (ch == '}')
            {
                --depth;
                if (depth == 0)
                {
                    object = json.substr(begin, pos - begin + 1);
                    return true;
                }
            }
        }
        return false;
    }

    std::vector<std::string> jsonStringArray(
        const std::string& json,
        const std::string& key)
    {
        std::vector<std::string> values;
        const std::string marker = "\"" + key + "\"";
        auto pos = json.find(marker);
        if (pos == std::string::npos) return values;
        pos = json.find(':', pos + marker.size());
        if (pos == std::string::npos) return values;
        ++pos;
        while (pos < json.size()
               && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
        if (pos >= json.size() || json[pos] != '[') return values;
        ++pos;

        while (pos < json.size())
        {
            while (pos < json.size()
                   && (std::isspace(static_cast<unsigned char>(json[pos]))
                       || json[pos] == ',')) ++pos;
            if (pos >= json.size() || json[pos] == ']') break;
            if (json[pos] != '"') return {};
            ++pos;

            std::string value;
            while (pos < json.size())
            {
                const char ch = json[pos++];
                if (ch == '"') break;
                if (ch == '\\')
                {
                    if (pos >= json.size()) return {};
                    const char escaped = json[pos++];
                    if (escaped == '"' || escaped == '\\' || escaped == '/')
                        value.push_back(escaped);
                    else
                        return {};
                }
                else
                {
                    value.push_back(ch);
                }
            }
            values.push_back(std::move(value));
        }
        return values;
    }

    std::string firstJsonString(const std::string& json,
                                std::initializer_list<const char*> keys)
    {
        std::string value;
        for (const char* key : keys)
        {
            if (jsonString(json, key, value) && !value.empty()) return value;
        }
        return {};
    }

    std::vector<std::string> jsonStringOrArray(
        const std::string& json,
        const std::string& key)
    {
        auto values = jsonStringArray(json, key);
        if (!values.empty()) return values;

        std::string value;
        if (jsonString(json, key, value) && !value.empty())
        {
            values.push_back(std::move(value));
        }
        return values;
    }


    std::string base64UrlDecode(std::string value)
    {
        for (char& ch : value)
        {
            if (ch == '-') ch = '+';
            else if (ch == '_') ch = '/';
        }
        while ((value.size() % 4) != 0) value.push_back('=');

        auto decode = [](char ch) -> int
        {
            if (ch >= 'A' && ch <= 'Z') return ch - 'A';
            if (ch >= 'a' && ch <= 'z') return 26 + ch - 'a';
            if (ch >= '0' && ch <= '9') return 52 + ch - '0';
            if (ch == '+') return 62;
            if (ch == '/') return 63;
            return -1;
        };

        std::string output;
        std::uint32_t accumulator = 0;
        int bits = -8;
        for (const char ch : value)
        {
            if (ch == '=') break;
            const int decoded = decode(ch);
            if (decoded < 0) return {};
            accumulator = (accumulator << 6) | decoded;
            bits += 6;
            if (bits >= 0)
            {
                output.push_back(
                    static_cast<char>((accumulator >> bits) & 0xff));
                bits -= 8;
            }
        }
        return output;
    }

    std::string queryParameter(const std::string& url, const std::string& name)
    {
        const auto query = url.find('?');
        if (query == std::string::npos) return {};
        std::size_t pos = query + 1;
        while (pos < url.size())
        {
            const auto equal = url.find('=', pos);
            if (equal == std::string::npos) break;
            const auto end = url.find('&', equal + 1);
            if (url.substr(pos, equal - pos) == name)
            {
                return url.substr(equal + 1,
                    end == std::string::npos ? std::string::npos : end - equal - 1);
            }
            if (end == std::string::npos) break;
            pos = end + 1;
        }
        return {};
    }

    void inferGroupsFromAccessUrl(const std::string& client_url,
                                  std::string& receive_group,
                                  std::string& send_group,
                                  bool& receive_auto_joined)
    {
        receive_auto_joined = false;
        const std::string token = queryParameter(client_url, "access_token");
        if (token.empty()) return;

        const auto first_dot = token.find('.');
        const auto second_dot =
            first_dot == std::string::npos ? std::string::npos
                                           : token.find('.', first_dot + 1);
        if (first_dot == std::string::npos || second_dot == std::string::npos)
            return;

        const std::string payload =
            base64UrlDecode(token.substr(first_dot + 1, second_dot - first_dot - 1));
        if (payload.empty()) return;

        auto groups = jsonStringOrArray(payload, "webpubsub.group");
        if (groups.empty()) groups = jsonStringOrArray(payload, "groups");
        if (groups.empty()) groups = jsonStringOrArray(payload, "group");
        if (receive_group.empty() && !groups.empty())
        {
            receive_group = groups.front();
            receive_auto_joined = true;
        }
        else if (!receive_group.empty()
                 && std::find(groups.begin(), groups.end(), receive_group)
                    != groups.end())
        {
            receive_auto_joined = true;
        }

        auto roles = jsonStringOrArray(payload, "roles");
        if (roles.empty()) roles = jsonStringOrArray(payload, "role");

        static const std::string send_prefix = "webpubsub.sendToGroup.";
        static const std::string join_prefix = "webpubsub.joinLeaveGroup.";
        for (const auto& role : roles)
        {
            if (send_group.empty()
                && role.compare(0, send_prefix.size(), send_prefix) == 0)
            {
                send_group = role.substr(send_prefix.size());
            }
            if (receive_group.empty()
                && role.compare(0, join_prefix.size(), join_prefix) == 0)
            {
                receive_group = role.substr(join_prefix.size());
            }
        }
    }

#ifdef LL_WINDOWS
    bool addWindowsRootCertificates(ssl::context& context)
    {
        HCERTSTORE windows_store = CertOpenSystemStoreA(nullptr, "ROOT");
        if (!windows_store) return false;

        X509_STORE* openssl_store =
            SSL_CTX_get_cert_store(context.native_handle());
        bool added_any = false;
        PCCERT_CONTEXT certificate = nullptr;

        while ((certificate =
                    CertEnumCertificatesInStore(windows_store, certificate)) != nullptr)
        {
            const unsigned char* encoded = certificate->pbCertEncoded;
            X509* x509 = d2i_X509(
                nullptr, &encoded,
                static_cast<long>(certificate->cbCertEncoded));
            if (!x509)
            {
                ERR_clear_error();
                continue;
            }

            if (X509_STORE_add_cert(openssl_store, x509) == 1)
                added_any = true;
            else
                ERR_clear_error();
            X509_free(x509);
        }

        CertCloseStore(windows_store, 0);
        return added_any;
    }
#endif

    bool configureTlsContext(ssl::context& context, std::string& error)
    {
        boost::system::error_code trust_error;
        context.set_default_verify_paths(trust_error);
#ifdef LL_WINDOWS
        const bool windows_roots = addWindowsRootCertificates(context);
        if (trust_error && !windows_roots)
        {
            error = "Could not load TLS root certificates: "
                + trust_error.message();
            return false;
        }
#else
        if (trust_error)
        {
            error = "Could not load TLS root certificates: "
                + trust_error.message();
            return false;
        }
#endif
        context.set_verify_mode(ssl::verify_peer);
        return true;
    }

    struct HttpResult
    {
        unsigned status = 0;
        std::string body;
        std::string error;
    };

    HttpResult postJson(const std::string& base_url,
                        const std::string& path,
                        const std::string& body,
                        const std::vector<std::pair<std::string, std::string>>& headers)
    {
        HttpResult result;
        ParsedUrl base;
        std::string parse_error;
        if (!parseUrl(trimTrailingSlash(base_url), "https", "443", base, parse_error))
        {
            result.error = parse_error;
            return result;
        }

        std::string target = base.target;
        if (target == "/") target.clear();
        if (!path.empty() && path.front() != '/') target += '/';
        target += path;
        if (target.empty()) target = "/";

        asio::io_context io;
        ssl::context tls(ssl::context::tls_client);
        if (!configureTlsContext(tls, result.error)) return result;

        tcp::resolver resolver(io);
        beast::ssl_stream<beast::tcp_stream> stream(io, tls);

        if (!SSL_set_tlsext_host_name(
                stream.native_handle(), base.host.c_str()))
        {
            result.error = "TLS SNI setup failed.";
            return result;
        }
        stream.set_verify_callback(ssl::host_name_verification(base.host));

        boost::system::error_code error;
        auto endpoints = resolver.resolve(base.host, base.port, error);
        if (error)
        {
            result.error = "DNS lookup failed: " + error.message();
            return result;
        }

        beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(8));
        beast::get_lowest_layer(stream).connect(endpoints, error);
        if (error)
        {
            result.error = "Connection failed: " + error.message();
            return result;
        }

        stream.handshake(ssl::stream_base::client, error);
        if (error)
        {
            result.error = "TLS handshake failed: " + error.message();
            return result;
        }

        http::request<http::string_body> request{
            http::verb::post, target, 11};
        request.set(http::field::host, base.hostHeader);
        request.set(http::field::user_agent, "BlazingStormViewer/1");
        request.set(http::field::content_type, "application/json");
        request.set(http::field::accept, "application/json");
        for (const auto& header : headers)
        {
            if (!header.first.empty() && !header.second.empty())
                request.set(header.first, header.second);
        }
        request.body() = body;
        request.prepare_payload();

        http::write(stream, request, error);
        if (error)
        {
            result.error = "HTTP write failed: " + error.message();
            return result;
        }

        beast::flat_buffer buffer;
        http::response<http::string_body> response;
        http::read(stream, buffer, response, error);
        if (error)
        {
            result.error = "HTTP read failed: " + error.message();
            return result;
        }

        result.status = response.result_int();
        if (response.body().size() <= 256 * 1024)
            result.body = response.body();
        else
            result.error = "Broker response exceeded the viewer safety limit.";

        boost::system::error_code shutdown_error;
        stream.shutdown(shutdown_error);
        return result;
    }

    std::vector<std::string> createPaths(const std::string& override_path)
    {
        if (!override_path.empty()) return {override_path};
        return {
            "/api/session/create",
            "/api/sessions/create",
            "/api/create-session",
            "/api/createSession",
            "/api/relay/create-session",
            "/api/relay/session/create",
            "/api/session",
            "/api/sessions",
            "/api/relay/session",
            "/api/relay/create"
        };
    }

    std::vector<std::string> negotiatePaths(
        BlazingStorm::RelayBrokerRole role,
        const std::string& session_id,
        const std::string& override_path)
    {
        if (!override_path.empty())
            return {replaceSession(override_path, session_id)};

        const bool subject = role == BlazingStorm::RelayBrokerRole::Subject;
        const std::string role_name = subject ? "subject" : "controller";
        const std::string camel = subject ? "Subject" : "Controller";

        return {
            "/api/negotiate/" + role_name,
            "/api/negotiate-" + role_name,
            "/api/negotiate" + camel,
            "/api/" + role_name + "/negotiate",
            "/api/session/" + role_name + "/negotiate",
            "/api/session/negotiate",
            "/api/session/" + session_id + "/" + role_name + "/negotiate",
            "/api/session/" + session_id + "/" + role_name,
            "/api/sessions/" + session_id + "/" + role_name + "/negotiate",
            "/api/sessions/" + session_id + "/" + role_name,
            "/api/relay/negotiate/" + role_name,
            "/api/relay/session/" + session_id + "/" + role_name,
            "/api/relay/negotiate",
            "/api/negotiate"
        };
    }

    bool statusMeansNoRoute(unsigned status)
    {
        return status == 404 || status == 405;
    }

    std::string safeHttpFailure(unsigned status)
    {
        if (status == 400) return "Broker rejected the request as malformed.";
        if (status == 401) return "Broker authentication failed.";
        if (status == 403) return "Broker denied the relay request.";
        if (status == 408) return "Broker request timed out.";
        if (status == 429) return "Broker rate limit was reached.";
        if (status >= 500) return "Broker reported a server error.";
        return "Broker request failed with HTTP " + std::to_string(status) + ".";
    }

    bool extractCreated(const std::string& body,
                        BlazingStorm::RelayEvent& event)
    {
        event.sessionId = firstJsonString(
            body, {"sessionId", "sessionID", "session", "id"});
        event.subjectTicket = firstJsonString(
            body, {"subjectTicket", "subjectToken", "subjectKey"});
        event.controllerTicket = firstJsonString(
            body, {"controllerTicket", "controllerToken", "controllerKey"});

        std::string nested;
        if (event.subjectTicket.empty()
            && jsonObject(body, "subject", nested))
        {
            event.subjectTicket = firstJsonString(
                nested, {"ticket", "token", "key"});
        }
        if (event.controllerTicket.empty()
            && jsonObject(body, "controller", nested))
        {
            event.controllerTicket = firstJsonString(
                nested, {"ticket", "token", "key"});
        }

        if (event.subjectTicket.empty())
            event.subjectTicket = firstJsonString(body, {"subject"});
        if (event.controllerTicket.empty())
            event.controllerTicket = firstJsonString(body, {"controller"});

        return !event.sessionId.empty()
            && !event.subjectTicket.empty()
            && !event.controllerTicket.empty();
    }

    bool extractNegotiated(const std::string& body,
                           BlazingStorm::RelayBrokerRole role,
                           BlazingStorm::RelayEvent& event,
                           bool& receive_auto_joined)
    {
        event.clientUrl = firstJsonString(
            body, {"url", "clientUrl", "clientURL", "webSocketUrl",
                   "websocketUrl", "accessUrl", "accessURL"});
        event.receiveGroup = firstJsonString(
            body, {"receiveGroup", "subscribeGroup", "inboundGroup",
                   "joinGroup", "receive"});
        event.sendGroup = firstJsonString(
            body, {"sendGroup", "publishGroup", "outboundGroup", "send"});

        // The deployed relay is directional: Controller -> commands and
        // Subject -> events. Accept explicit group fields when returned.
        if (role == BlazingStorm::RelayBrokerRole::Subject)
        {
            if (event.receiveGroup.empty())
                event.receiveGroup =
                    firstJsonString(body, {"commandGroup", "commandsGroup"});
            if (event.sendGroup.empty())
                event.sendGroup =
                    firstJsonString(body, {"eventGroup", "eventsGroup"});
        }
        else
        {
            if (event.receiveGroup.empty())
                event.receiveGroup =
                    firstJsonString(body, {"eventGroup", "eventsGroup"});
            if (event.sendGroup.empty())
                event.sendGroup =
                    firstJsonString(body, {"commandGroup", "commandsGroup"});
        }

        std::string connection;
        if (event.clientUrl.empty()
            && jsonObject(body, "connection", connection))
        {
            event.clientUrl = firstJsonString(
                connection, {"url", "clientUrl", "webSocketUrl", "accessUrl"});
            if (event.receiveGroup.empty())
                event.receiveGroup = firstJsonString(
                    connection, {"receiveGroup", "subscribeGroup", "inboundGroup"});
            if (event.sendGroup.empty())
                event.sendGroup = firstJsonString(
                    connection, {"sendGroup", "publishGroup", "outboundGroup"});

            if (role == BlazingStorm::RelayBrokerRole::Subject)
            {
                if (event.receiveGroup.empty())
                    event.receiveGroup =
                        firstJsonString(connection, {"commandGroup", "commandsGroup"});
                if (event.sendGroup.empty())
                    event.sendGroup =
                        firstJsonString(connection, {"eventGroup", "eventsGroup"});
            }
            else
            {
                if (event.receiveGroup.empty())
                    event.receiveGroup =
                        firstJsonString(connection, {"eventGroup", "eventsGroup"});
                if (event.sendGroup.empty())
                    event.sendGroup =
                        firstJsonString(connection, {"commandGroup", "commandsGroup"});
            }
        }

        if (event.clientUrl.empty()) return false;

        inferGroupsFromAccessUrl(
            event.clientUrl,
            event.receiveGroup,
            event.sendGroup,
            receive_auto_joined);
        return true;
    }
}

namespace BlazingStorm
{
    class RelayTransport::PubSubImpl
    {
    public:
        using TlsWebSocket =
            websocket::stream<beast::ssl_stream<beast::tcp_stream>>;

        PubSubImpl(RelayTransport& owner,
                   std::uint64_t generation,
                   std::string client_url,
                   std::string receive_group,
                   std::string send_group)
        : mOwner(owner),
          mGeneration(generation),
          mClientUrl(std::move(client_url)),
          mReceiveGroup(std::move(receive_group)),
          mSendGroup(std::move(send_group)),
          mTls(ssl::context::tls_client),
          mResolver(mIo)
        {
        }

        ~PubSubImpl()
        {
            stop();
        }

        bool start(std::string& error)
        {
            if (!parseUrl(mClientUrl, "wss", "443", mEndpoint, error))
                return false;
            if (!configureTlsContext(mTls, error))
                return false;

            bool auto_joined = false;
            std::string ignored_send = mSendGroup;
            inferGroupsFromAccessUrl(
                mClientUrl, mReceiveGroup, ignored_send, auto_joined);
            if (mSendGroup.empty()) mSendGroup = ignored_send;
            mReceiveAutoJoined = auto_joined;

            try
            {
                mThread = std::thread([this]()
                {
                    asio::post(mIo, [this]() { beginResolve(); });
                    mIo.run();
                });
            }
            catch (const std::exception& e)
            {
                error = std::string("Could not start Web PubSub thread: ")
                    + e.what();
                return false;
            }
            return true;
        }

        void stop()
        {
            if (!mThread.joinable())
            {
                mOpen = false;
                return;
            }
            mStopping = true;
            mOpen = false;
            mIo.stop();
            mThread.join();
            mSocket.reset();
            mWrites.clear();
        }

        bool sendApplication(const std::string& payload)
        {
            if (!mOpen || mSendGroup.empty()
                || payload.empty()
                || payload.size() > RelayTransport::MAX_MESSAGE_BYTES)
            {
                return false;
            }

            const std::uint64_t ack = ++mNextAck;
            const std::string frame =
                "{\"type\":\"sendToGroup\",\"group\":\""
                + jsonEscape(mSendGroup)
                + "\",\"ackId\":" + std::to_string(ack)
                + ",\"noEcho\":true,\"dataType\":\"text\",\"data\":\""
                + jsonEscape(payload) + "\"}";

            asio::post(mIo, [this, frame]()
            {
                if (mStopping || !mSocket || !mSocket->is_open()) return;
                const bool idle = mWrites.empty();
                mWrites.push_back(frame);
                if (idle) beginWrite();
            });
            return true;
        }

        bool isOpen() const
        {
            return mOpen;
        }

    private:
        void push(RelayEvent event)
        {
            mOwner.pushEvent(mGeneration, std::move(event));
        }

        void fail(const std::string& detail,
                  const boost::system::error_code& error = {})
        {
            mOpen = false;
            if (!mStopping)
            {
                RelayEvent event;
                event.type = RelayEventType::Error;
                event.detail = error
                    ? detail + ": " + error.message()
                    : detail;
                push(std::move(event));
            }
            mIo.stop();
        }

        void beginResolve()
        {
            mResolver.async_resolve(
                mEndpoint.host,
                mEndpoint.port,
                [this](const boost::system::error_code& error,
                       tcp::resolver::results_type endpoints)
                {
                    if (error)
                    {
                        fail("Web PubSub DNS lookup failed", error);
                        return;
                    }

                    mSocket = std::make_unique<TlsWebSocket>(mIo, mTls);
                    beast::get_lowest_layer(*mSocket).expires_after(
                        std::chrono::seconds(15));
                    beast::get_lowest_layer(*mSocket).async_connect(
                        endpoints,
                        [this](const boost::system::error_code& connect_error,
                               const tcp::resolver::results_type::endpoint_type&)
                        {
                            if (connect_error)
                            {
                                fail("Web PubSub TCP connection failed",
                                     connect_error);
                                return;
                            }
                            beginTlsHandshake();
                        });
                });
        }

        void beginTlsHandshake()
        {
            if (!mSocket) return;

            if (!SSL_set_tlsext_host_name(
                    mSocket->next_layer().native_handle(),
                    mEndpoint.host.c_str()))
            {
                fail("Web PubSub TLS SNI setup failed");
                return;
            }

            mSocket->next_layer().set_verify_callback(
                ssl::host_name_verification(mEndpoint.host));
            mSocket->next_layer().async_handshake(
                ssl::stream_base::client,
                [this](const boost::system::error_code& error)
                {
                    if (error)
                    {
                        fail("Web PubSub TLS handshake failed", error);
                        return;
                    }
                    beginWebSocketHandshake();
                });
        }

        void beginWebSocketHandshake()
        {
            beast::get_lowest_layer(*mSocket).expires_never();

            websocket::stream_base::timeout timeout =
                websocket::stream_base::timeout::suggested(
                    beast::role_type::client);
            timeout.idle_timeout = std::chrono::seconds(40);
            timeout.keep_alive_pings = true;
            mSocket->set_option(timeout);

            mSocket->set_option(websocket::stream_base::decorator(
                [](websocket::request_type& request)
                {
                    request.set(
                        http::field::user_agent,
                        "BlazingStormViewer/1");
                    request.set(
                        "Sec-WebSocket-Protocol",
                        "json.webpubsub.azure.v1");
                }));

            mSocket->read_message_max(RelayTransport::MAX_MESSAGE_BYTES);
            mSocket->text(true);
            mSocket->async_handshake(
                mEndpoint.hostHeader,
                mEndpoint.target,
                [this](const boost::system::error_code& error)
                {
                    if (error)
                    {
                        fail("Web PubSub WebSocket handshake failed", error);
                        return;
                    }

                    mOpen = true;
                    mReceiveReady =
                        mReceiveGroup.empty() || mReceiveAutoJoined;
                    beginRead();

                    if (!mReceiveGroup.empty() && !mReceiveAutoJoined)
                    {
                        mJoinAck = ++mNextAck;
                        const std::string join =
                            "{\"type\":\"joinGroup\",\"group\":\""
                            + jsonEscape(mReceiveGroup)
                            + "\",\"ackId\":"
                            + std::to_string(mJoinAck) + "}";
                        mWrites.push_back(join);
                        beginWrite();
                    }
                });
        }

        void beginRead()
        {
            if (!mSocket || !mSocket->is_open() || mStopping) return;

            mSocket->async_read(
                mReadBuffer,
                [this](const boost::system::error_code& error, std::size_t)
                {
                    if (error)
                    {
                        mOpen = false;
                        if (!mStopping)
                        {
                            RelayEvent event;
                            if (error == websocket::error::closed)
                            {
                                event.type = RelayEventType::Closed;
                                event.detail = "Web PubSub connection closed.";
                                push(std::move(event));
                                mIo.stop();
                            }
                            else
                            {
                                fail("Web PubSub read failed", error);
                            }
                        }
                        return;
                    }

                    if (!mSocket->got_text())
                    {
                        fail("Web PubSub sent an unexpected binary frame.");
                        return;
                    }

                    std::string payload =
                        beast::buffers_to_string(mReadBuffer.data());
                    mReadBuffer.consume(mReadBuffer.size());

                    std::string type;
                    if (!jsonString(payload, "type", type))
                    {
                        fail("Web PubSub sent malformed JSON.");
                        return;
                    }

                    if (type == "ack")
                    {
                        std::uint64_t ack_id = 0;
                        bool success = false;
                        if (!jsonUnsigned(payload, "ackId", ack_id)
                            || !jsonBool(payload, "success", success))
                        {
                            fail("Web PubSub sent a malformed acknowledgement.");
                            return;
                        }

                        if (ack_id == mJoinAck)
                        {
                            if (!success)
                            {
                                fail("Web PubSub refused the receive-group join.");
                                return;
                            }
                            mJoinAck = 0;
                            mReceiveReady = true;
                            signalConnectedIfReady();
                        }
                        else if (!success)
                        {
                            fail("Web PubSub rejected an outbound relay message.");
                            return;
                        }
                    }
                    else if (type == "system")
                    {
                        std::string event_name;
                        if (jsonString(payload, "event", event_name)
                            && event_name == "connected")
                        {
                            mServiceConnected = true;
                            signalConnectedIfReady();
                        }
                    }
                    else if (type == "message")
                    {
                        std::string from;
                        std::string group;
                        std::string data_type;
                        std::string data;
                        const bool group_ok =
                            mReceiveGroup.empty()
                            || (jsonString(payload, "group", group)
                                && group == mReceiveGroup);

                        if (group_ok
                            && (!jsonString(payload, "from", from)
                                || from == "group")
                            && (!jsonString(payload, "dataType", data_type)
                                || data_type == "text")
                            && jsonString(payload, "data", data))
                        {
                            if (data.size() <= RelayTransport::MAX_MESSAGE_BYTES)
                            {
                                RelayEvent event;
                                event.type = RelayEventType::Message;
                                event.payload = std::move(data);
                                push(std::move(event));
                            }
                        }
                    }

                    beginRead();
                });
        }

        void signalConnectedIfReady()
        {
            if (mConnectedEventSent
                || !mServiceConnected
                || !mReceiveReady)
            {
                return;
            }

            mConnectedEventSent = true;
            RelayEvent event;
            event.type = RelayEventType::Connected;
            push(std::move(event));
        }

        void beginWrite()
        {
            if (!mSocket || !mSocket->is_open()
                || mWrites.empty() || mStopping)
            {
                return;
            }

            mSocket->text(true);
            mSocket->async_write(
                asio::buffer(mWrites.front()),
                [this](const boost::system::error_code& error, std::size_t)
                {
                    if (error)
                    {
                        fail("Web PubSub write failed", error);
                        return;
                    }
                    mWrites.pop_front();
                    if (!mWrites.empty()) beginWrite();
                });
        }

        RelayTransport& mOwner;
        std::uint64_t mGeneration = 0;
        std::string mClientUrl;
        std::string mReceiveGroup;
        std::string mSendGroup;
        bool mReceiveAutoJoined = false;

        asio::io_context mIo;
        ssl::context mTls;
        tcp::resolver mResolver;
        ParsedUrl mEndpoint;
        std::unique_ptr<TlsWebSocket> mSocket;
        beast::flat_buffer mReadBuffer;
        std::deque<std::string> mWrites;
        std::thread mThread;

        std::atomic<bool> mOpen{false};
        std::atomic<bool> mStopping{false};
        std::uint64_t mNextAck = 0;
        std::uint64_t mJoinAck = 0;
        bool mServiceConnected = false;
        bool mReceiveReady = false;
        bool mConnectedEventSent = false;
    };

    RelayTransport& RelayTransport::instance()
    {
        static RelayTransport transport;
        return transport;
    }

    RelayTransport::~RelayTransport()
    {
        disconnect();
    }

    void RelayTransport::joinBrokerThread()
    {
        if (mBrokerThread.joinable()) mBrokerThread.join();
    }

    bool RelayTransport::startBrokerTask(BrokerTask task)
    {
        joinBrokerThread();
        const std::uint64_t generation = mGeneration.load();
        try
        {
            mBrokerThread = std::thread(
                [task = std::move(task), generation]()
                {
                    task(generation);
                });
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    void RelayTransport::pushEvent(
        std::uint64_t generation,
        RelayEvent event)
    {
        if (generation != mGeneration.load()) return;
        std::lock_guard<std::mutex> lock(mEventMutex);
        mEvents.push_back(std::move(event));
    }

    bool RelayTransport::createSession(
        const std::string& broker_base_url,
        const std::string& relay_create_key,
        const std::string& controller_id,
        const std::string& controller_name,
        const std::string& nonce,
        const std::string& path_override)
    {
        if (broker_base_url.empty() || relay_create_key.empty()
            || controller_id.empty() || nonce.empty())
        {
            return false;
        }

        if (mPubSub) mPubSub->stop();
        mPubSub.reset();

        return startBrokerTask(
            [this, broker_base_url, relay_create_key, controller_id,
             controller_name, nonce, path_override](std::uint64_t generation)
            {
                const std::string body =
                    "{\"protocol\":1,\"controllerId\":\""
                    + jsonEscape(controller_id)
                    + "\",\"controllerName\":\""
                    + jsonEscape(controller_name)
                    + "\",\"nonce\":\""
                    + jsonEscape(nonce) + "\"}";

                const std::vector<std::pair<std::string, std::string>> headers = {
                    {"X-Relay-Create-Key", relay_create_key},
                    {"X-Blazing-Relay-Create-Key", relay_create_key},
                    {"Authorization", "Bearer " + relay_create_key}
                };

                for (const auto& path : createPaths(path_override))
                {
                    const HttpResult response =
                        postJson(broker_base_url, path, body, headers);

                    if (!response.error.empty())
                    {
                        RelayEvent event;
                        event.type = RelayEventType::Error;
                        event.detail = response.error;
                        pushEvent(generation, std::move(event));
                        return;
                    }

                    if (statusMeansNoRoute(response.status)
                        && path_override.empty())
                    {
                        continue;
                    }

                    if (response.status < 200 || response.status >= 300)
                    {
                        RelayEvent event;
                        event.type = RelayEventType::Error;
                        event.detail = safeHttpFailure(response.status)
                            + " Create-session route: " + path;
                        pushEvent(generation, std::move(event));
                        return;
                    }

                    RelayEvent event;
                    event.type = RelayEventType::SessionCreated;
                    event.matchedPath = path;
                    if (!extractCreated(response.body, event))
                    {
                        event.type = RelayEventType::Error;
                        event.detail =
                            "Broker create-session response did not contain "
                            "sessionId, subject ticket, and controller ticket.";
                    }
                    pushEvent(generation, std::move(event));
                    return;
                }

                RelayEvent event;
                event.type = RelayEventType::Error;
                event.detail =
                    "Could not find the relay create-session Function route. "
                    "Set BlazingStormRelayCreatePath if the deployed route is custom.";
                pushEvent(generation, std::move(event));
            });
    }

    bool RelayTransport::negotiate(
        const std::string& broker_base_url,
        RelayBrokerRole role,
        const std::string& session_id,
        const std::string& ticket,
        const std::string& path_override)
    {
        if (broker_base_url.empty() || session_id.empty() || ticket.empty())
            return false;

        return startBrokerTask(
            [this, broker_base_url, role, session_id, ticket,
             path_override](std::uint64_t generation)
            {
                const char* role_name =
                    role == RelayBrokerRole::Subject ? "subject" : "controller";
                const std::string body =
                    "{\"protocol\":1,\"role\":\""
                    + std::string(role_name)
                    + "\",\"sessionId\":\""
                    + jsonEscape(session_id)
                    + "\",\"session\":\""
                    + jsonEscape(session_id)
                    + "\",\"ticket\":\""
                    + jsonEscape(ticket)
                    + "\",\"token\":\""
                    + jsonEscape(ticket) + "\"}";

                const std::vector<std::pair<std::string, std::string>> headers = {
                    {"X-Relay-Ticket", ticket},
                    {"Authorization", "Bearer " + ticket}
                };

                for (const auto& path :
                     negotiatePaths(role, session_id, path_override))
                {
                    const HttpResult response =
                        postJson(broker_base_url, path, body, headers);

                    if (!response.error.empty())
                    {
                        RelayEvent event;
                        event.type = RelayEventType::Error;
                        event.detail = response.error;
                        pushEvent(generation, std::move(event));
                        return;
                    }

                    if (statusMeansNoRoute(response.status)
                        && path_override.empty())
                    {
                        continue;
                    }

                    if (response.status < 200 || response.status >= 300)
                    {
                        RelayEvent event;
                        event.type = RelayEventType::Error;
                        event.detail = safeHttpFailure(response.status)
                            + " Negotiate route: " + path;
                        pushEvent(generation, std::move(event));
                        return;
                    }

                    RelayEvent event;
                    event.type = RelayEventType::Negotiated;
                    event.sessionId = session_id;
                    event.matchedPath = path;
                    bool auto_joined = false;
                    if (!extractNegotiated(response.body, role, event, auto_joined))
                    {
                        event.type = RelayEventType::Error;
                        event.detail =
                            "Broker negotiate response did not contain a "
                            "Web PubSub client access URL.";
                    }
                    else if (event.sendGroup.empty())
                    {
                        event.type = RelayEventType::Error;
                        event.detail =
                            "Could not determine the Web PubSub outbound group "
                            "from the negotiate response or access token.";
                    }
                    pushEvent(generation, std::move(event));
                    return;
                }

                RelayEvent event;
                event.type = RelayEventType::Error;
                event.detail =
                    "Could not find the relay negotiate Function route. "
                    "Use the per-account negotiate path override if needed.";
                pushEvent(generation, std::move(event));
            });
    }

    bool RelayTransport::connectPubSub(
        const std::string& client_url,
        const std::string& receive_group,
        const std::string& send_group)
    {
        if (client_url.empty() || send_group.empty()) return false;

        if (mPubSub) mPubSub->stop();
        const auto generation = mGeneration.load();
        auto pubsub = std::make_unique<PubSubImpl>(
            *this, generation, client_url, receive_group, send_group);

        std::string error;
        if (!pubsub->start(error))
        {
            RelayEvent event;
            event.type = RelayEventType::Error;
            event.detail = error;
            pushEvent(generation, std::move(event));
            return false;
        }

        mPubSub = std::move(pubsub);
        return true;
    }

    bool RelayTransport::sendApplication(const std::string& payload)
    {
        return mPubSub && mPubSub->sendApplication(payload);
    }

    void RelayTransport::disconnect()
    {
        ++mGeneration;
        joinBrokerThread();
        if (mPubSub)
        {
            mPubSub->stop();
            mPubSub.reset();
        }

        std::lock_guard<std::mutex> lock(mEventMutex);
        mEvents.clear();
    }

    std::vector<RelayEvent> RelayTransport::takeEvents()
    {
        std::lock_guard<std::mutex> lock(mEventMutex);
        std::vector<RelayEvent> result;
        result.swap(mEvents);
        return result;
    }

    bool RelayTransport::isOpen() const
    {
        return mPubSub && mPubSub->isOpen();
    }
}
