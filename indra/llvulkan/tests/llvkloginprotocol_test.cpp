#include "linden_common.h"
#include "llvkloginprotocol.h"
#include "llvkloginhttp.h"
#include "llvkregioncircuit.h"
#include "llvkchatprotocol.h"
#include "llvklogintransport.h"
#include "llmessagetemplateparser.h"
#include "lltemplatemessagebuilder.h"
#include "message.h"
#include "v3math.h"
#include "lltut.h"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/endian/conversion.hpp>
#include <openssl/x509v3.h>
#include "llsdserialize.h"
#include <chrono>
#include <fstream>
#include <filesystem>

namespace tut
{
    struct LoginTlsServer
    {
        boost::asio::io_context context;
        boost::asio::ssl::context tls{boost::asio::ssl::context::tls_server};
        boost::asio::ip::tcp::acceptor acceptor{context,{boost::asio::ip::address_v4::loopback(),0}};
        std::filesystem::path certificate=std::filesystem::temp_directory_path()/("native-login-ca-"+LLUUID::generateNewID().asString()+".pem");
        std::atomic<bool> stopping=false,polled=false,finished=false,searched=false;
        std::atomic<bool> groupStart=false,groupUnmute=false,groupInvite=false,groupAccepted=false;
        std::atomic<bool> groupModerated=false;
        std::atomic<bool> moderationDenied=false;
        std::thread worker;
        std::string url() const { return "https://127.0.0.1:"+std::to_string(acceptor.local_endpoint().port()); }
        explicit LoginTlsServer(LLSD login)
        {
            std::unique_ptr<EVP_PKEY_CTX,decltype(&EVP_PKEY_CTX_free)> keyContext(EVP_PKEY_CTX_new_id(EVP_PKEY_RSA,nullptr),EVP_PKEY_CTX_free);
            ensure("test key context",bool(keyContext));
            ensure("test key init",EVP_PKEY_keygen_init(keyContext.get())==1 && EVP_PKEY_CTX_set_rsa_keygen_bits(keyContext.get(),2048)==1);
            EVP_PKEY* rawKey=nullptr;
            ensure("test key generation",EVP_PKEY_keygen(keyContext.get(),&rawKey)==1);
            std::unique_ptr<EVP_PKEY,decltype(&EVP_PKEY_free)> key(rawKey,EVP_PKEY_free);
            std::unique_ptr<X509,decltype(&X509_free)> cert(X509_new(),X509_free);
            ensure("test certificate",bool(cert));
            X509_set_version(cert.get(),2);
            ASN1_INTEGER_set(X509_get_serialNumber(cert.get()),1);
            X509_gmtime_adj(X509_getm_notBefore(cert.get()),-60);
            X509_gmtime_adj(X509_getm_notAfter(cert.get()),3600);
            X509_set_pubkey(cert.get(),key.get());
            auto* subject=X509_get_subject_name(cert.get());
            X509_NAME_add_entry_by_txt(subject,"CN",MBSTRING_ASC,reinterpret_cast<const unsigned char*>("Native login test"),-1,-1,0);
            X509_set_issuer_name(cert.get(),subject);
            for (const auto& [identifier,value] : {std::pair{NID_basic_constraints,"critical,CA:TRUE"},std::pair{NID_subject_alt_name,"IP:127.0.0.1"}})
            {
                auto* extension=X509V3_EXT_conf_nid(nullptr,nullptr,identifier,const_cast<char*>(value));
                ensure("test certificate extension",extension!=nullptr);
                X509_add_ext(cert.get(),extension,-1); X509_EXTENSION_free(extension);
            }
            ensure("sign test certificate",X509_sign(cert.get(),key.get(),EVP_sha256())>0);
            ensure("install TLS certificate",SSL_CTX_use_certificate(tls.native_handle(),cert.get())==1 && SSL_CTX_use_PrivateKey(tls.native_handle(),key.get())==1);
            std::unique_ptr<BIO,decltype(&BIO_free)> encoded(BIO_new(BIO_s_mem()),BIO_free);
            ensure("encode public test certificate",PEM_write_bio_X509(encoded.get(),cert.get())==1);
            char* publicBytes=nullptr;
            const auto length=BIO_get_mem_data(encoded.get(),&publicBytes);
            std::ofstream output(certificate,std::ios::binary); output.write(publicBytes,length); output.close();
            ensure("public test CA written",output.good());
            acceptor.non_blocking(true);
            login["seed_capability"]=url()+"/seed";
            const auto address=url();
            worker=std::thread([this,login,address]
            {
                while (!stopping)
                {
                    boost::system::error_code problem;
                    boost::asio::ip::tcp::socket socket(context);
                    acceptor.accept(socket,problem);
                    if (problem) { std::this_thread::yield(); continue; }
                    const DWORD timeout=2000;
                    setsockopt(socket.native_handle(),SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<const char*>(&timeout),sizeof(timeout));
                    setsockopt(socket.native_handle(),SOL_SOCKET,SO_SNDTIMEO,reinterpret_cast<const char*>(&timeout),sizeof(timeout));
                    boost::asio::ssl::stream<boost::asio::ip::tcp::socket> stream(std::move(socket),tls);
                    stream.handshake(boost::asio::ssl::stream_base::server,problem);
                    if (problem) continue;
                    boost::beast::flat_buffer buffer;
                    boost::beast::http::request_parser<boost::beast::http::string_body> parser;
                    parser.body_limit(65536);
                    boost::beast::http::read(stream,buffer,parser,problem);
                    if (problem) continue;
                    const auto& request=parser.get();
                    std::string body;
                    bool noContent=false;
                    bool forbidden=false;
                    if (request.target()=="/login")
                        body="<methodResponse><params><param>"+login.asXMLRPCValue()+"</param></params></methodResponse>";
                    else
                    {
                        LLSD reply;
                        if (request.target()=="/seed")
                        {
                            reply["EventQueueGet"]=address+"/events";
                            reply["AvatarPickerSearch"]=address+"/avatars";
                            reply["ChatSessionRequest"]=address+"/chat";
                            reply["GetTexture"]="http://127.0.0.1:1/unused-texture";
                            reply["GetMesh"]="";
                        }
                        else if (request.target()=="/chat")
                        {
                            LLSD input;
                            std::istringstream incoming(request.body()); LLSDSerialize::fromXML(input,incoming);
                            if (input["method"].asString()=="mute update")
                            {
                                const bool validModeration=request.method()==boost::beast::http::verb::post &&
                                    input["session-id"].asUUID()==LLUUID("55555555-5555-5555-5555-555555555555") &&
                                    input["params"]["agent_id"].asUUID()==login["agent_id"].asUUID() &&
                                    input["params"]["mute_info"]["text"].isBoolean();
                                forbidden=validModeration && input["params"]["mute_info"]["text"].asBoolean();
                                if (forbidden) moderationDenied=true;
                                else
                                {
                                    groupModerated=validModeration; groupUnmute=validModeration;
                                    noContent=true;
                                }
                            }
                            else groupAccepted=request.method()==boost::beast::http::verb::post && input["method"].asString()=="accept invitation" &&
                                input["session-id"].asUUID()==LLUUID("55555555-5555-5555-5555-555555555555");
                            reply["agent_info"][login["agent_id"].asString()]["mutes"]["text"]=false;
                        }
                        else if (request.target().starts_with("/avatars/"))
                        {
                            searched=request.method()==boost::beast::http::verb::get &&
                                request.target().find("page_size=100")!=boost::beast::string_view::npos &&
                                request.target().find("names=Fixture")!=boost::beast::string_view::npos;
                            LLSD resident;
                            resident["id"]=LLUUID("44444444-4444-4444-4444-444444444444");
                            resident["display_name"]="Fixture Display"; resident["username"]="fixture.resident";
                            reply["agents"]=LLSD::emptyArray(); reply["agents"].append(resident);
                        }
                        else
                        {
                            LLSD input;
                            std::istringstream incoming(request.body()); LLSDSerialize::fromXML(input,incoming);
                            if (input["done"].asBoolean()) finished=true;
                            reply["id"]=1; reply["events"]=LLSD::emptyArray();
                            if (!input["done"].asBoolean() && !polled.exchange(true))
                            {
                                LLSD event,agent,group;
                                agent["AgentID"]=login["agent_id"];
                                event["message"]="AgentGroupDataUpdate";
                                event["body"]["AgentData"]=LLSD::emptyArray(); event["body"]["AgentData"].append(agent);
                                group["GroupID"]=LLUUID("55555555-5555-5555-5555-555555555555");
                                group["GroupName"]="Fixture Group";
                                LLSD::Binary powers(8); boost::endian::store_big_u64(powers.data(),(std::uint64_t(1)<<16)|(std::uint64_t(1)<<37));
                                group["GroupPowers"]=powers;
                                event["body"]["GroupData"]=LLSD::emptyArray(); event["body"]["GroupData"].append(group);
                                reply["events"].append(event);
                                LLSD instant,identity,block;
                                instant["message"]="ImprovedInstantMessage";
                                identity["AgentID"]=LLUUID("44444444-4444-4444-4444-444444444444");
                                block["ToAgentID"]=login["agent_id"].asUUID(); block["ID"]=identity["AgentID"].asUUID()^block["ToAgentID"].asUUID();
                                block["RegionID"]=LLUUID::null; block["FromGroup"]=false;
                                block["Offline"]=0; block["Dialog"]=0; block["Timestamp"]=LLSD::Binary(4,0);
                                block["FromAgentName"]="Event Resident"; block["Message"]="Event IM fixture";
                                block["BinaryBucket"]=LLSD::Binary{0}; block["Position"]=LLSD::emptyArray();
                                for (unsigned coordinate=0; coordinate<3; ++coordinate) block["Position"].append(0.);
                                instant["body"]["AgentData"]=LLSD::emptyArray(); instant["body"]["AgentData"].append(identity);
                                instant["body"]["MessageBlock"]=LLSD::emptyArray(); instant["body"]["MessageBlock"].append(block);
                                reply["events"].append(instant);
                            }
                            if (groupStart.exchange(false))
                            {
                                LLSD update;
                                update["message"]="ChatterBoxSessionAgentListUpdates";
                                update["body"]["session_id"]=LLUUID("55555555-5555-5555-5555-555555555555");
                                update["body"]["agent_updates"][login["agent_id"].asString()]["info"]["mutes"]["text"]=true;
                                reply["events"].append(update);
                                LLSD event;
                                event["message"]="ChatterBoxSessionStartReply";
                                event["body"]["temp_session_id"]=LLUUID("55555555-5555-5555-5555-555555555555");
                                event["body"]["session_id"]=event["body"]["temp_session_id"];
                                event["body"]["success"]=true;
                                event["body"]["agent_info"][login["agent_id"].asString()]["mutes"]["text"]=false;
                                event["body"]["agent_info"][login["agent_id"].asString()]["is_moderator"]=true;
                                reply["events"].append(event);
                            }
                            if (groupUnmute.exchange(false))
                            {
                                LLSD update;
                                update["message"]="ChatterBoxSessionAgentListUpdates";
                                update["body"]["session_id"]=LLUUID("55555555-5555-5555-5555-555555555555");
                                update["body"]["agent_updates"][login["agent_id"].asString()]["info"]["mutes"]["text"]=false;
                                reply["events"].append(update);
                            }
                            if (groupInvite.exchange(false))
                            {
                                LLSD event;
                                event["message"]="ChatterBoxInvitation";
                                auto& message=event["body"]["instantmessage"]["message_params"];
                                message["from_id"]=LLUUID("44444444-4444-4444-4444-444444444444");
                                message["id"]=LLUUID("55555555-5555-5555-5555-555555555555");
                                message["from_name"]="Fixture Resident"; message["message"]="Invitation fixture";
                                message["offline"]=0;
                                reply["events"].append(event);
                            }
                        }
                        std::ostringstream outgoing; LLSDSerialize::toXML(reply,outgoing); body=outgoing.str();
                    }
                    boost::beast::http::response<boost::beast::http::string_body> response{
                        forbidden ? boost::beast::http::status::forbidden : noContent ? boost::beast::http::status::no_content : boost::beast::http::status::ok,11};
                    if (noContent) body.clear();
                    response.body()=std::move(body); response.keep_alive(false); response.prepare_payload();
                    boost::beast::http::write(stream,response,problem);
                    stream.shutdown(problem);
                }
            });
        }
        ~LoginTlsServer()
        {
            stopping=true;
            if (worker.joinable()) worker.join();
            std::error_code ignored; std::filesystem::remove(certificate,ignored);
        }
    };
    struct loginprotocol_data {};
    typedef test_group<loginprotocol_data> loginprotocol_group;
    typedef loginprotocol_group::object loginprotocol_object;
    loginprotocol_group loginprotocol_tests("llvkloginprotocol");

    template<> template<> void loginprotocol_object::test<8>()
    {
        const LLUUID id("44444444-4444-4444-4444-444444444444");
        LLSD list=LLSD::emptyArray(),row;
        row["buddy_id"]=id; row["buddy_rights_given"]=1; row["buddy_rights_has"]=5; list.append(row);
        std::string error;
        const auto friends=LLVKChatProtocol::decodeFriends(list,error);
        ensure("friend initialization preserves permissions without inventing presence",friends && friends->size()==1 &&
            friends->front().id==id && friends->front().rightsGiven==1 && friends->front().rightsHeld==5 &&
            !friends->front().online && !friends->front().presenceReceived);
        list.append(row);
        ensure("duplicate friend identity rejected",!LLVKChatProtocol::decodeFriends(list,error));
        LLVKChatProtocol::Bytes payload{1}; payload.insert(payload.end(),id.mData,id.mData+16);
        const auto online=LLVKChatProtocol::decodePresence(payload,true,error),offline=LLVKChatProtocol::decodePresence(payload,false,error);
        ensure("presence transitions retain identity",online && offline && online->front().id==id && online->front().online && !offline->front().online);
        payload.pop_back();
        ensure("truncated presence rejected",!LLVKChatProtocol::decodePresence(payload,true,error));
    }

    template<> template<> void loginprotocol_object::test<7>()
    {
        LLSD body,agent,block;
        agent["AgentID"]=LLUUID("11111111-1111-1111-1111-111111111111");
        block["ToAgentID"]=LLUUID("22222222-2222-2222-2222-222222222222");
        block["ID"]=LLVKChatProtocol::directSession(agent["AgentID"].asUUID(),block["ToAgentID"].asUUID());
        block["RegionID"]=LLUUID::null; block["FromGroup"]=false; block["Offline"]=0; block["Dialog"]=0;
        block["FromAgentName"]="Fixture Resident"; block["Message"]="Structured IM fixture";
        block["BinaryBucket"]=LLSD::Binary{0};
        block["Timestamp"]=LLSD::Binary{0x12,0x34,0x56,0x78};
        block["Position"]=LLSD::emptyArray(); block["Position"].append(128.); block["Position"].append(129.); block["Position"].append(30.);
        body["AgentData"]=LLSD::emptyArray(); body["AgentData"].append(agent);
        body["MessageBlock"]=LLSD::emptyArray(); body["MessageBlock"].append(block);
        std::string error;
        const auto message=LLVKChatProtocol::decodeInstantEvent(body,error);
        ensure("structured IM retains identities and payload",message && message->sender==agent["AgentID"].asUUID() &&
            message->recipient==block["ToAgentID"].asUUID() && message->text=="Structured IM fixture" && message->timestamp==0x12345678 && message->position[1]==129.f);
        for (const auto field : {"ToAgentID","Dialog","Message","BinaryBucket","Timestamp","Position"})
        {
            auto malformed=body; malformed["MessageBlock"][0].erase(field);
            ensure("missing structured field rejected",!LLVKChatProtocol::decodeInstantEvent(malformed,error));
        }
        body["MessageBlock"][0]["Dialog"]=256;
        ensure("structured IM dialog cannot wrap",!LLVKChatProtocol::decodeInstantEvent(body,error));
    }

    template<> template<> void loginprotocol_object::test<6>()
    {
        const LLUUID agent("11111111-1111-1111-1111-111111111111"),groupId("55555555-5555-5555-5555-555555555555");
        LLSD body,identity,group;
        identity["AgentID"]=agent;
        body["AgentData"]=LLSD::emptyArray(); body["AgentData"].append(identity);
        group["GroupID"]=groupId; group["GroupName"]="Fixture Group";
        LLSD::Binary powers(8); boost::endian::store_big_u64(powers.data(),(std::uint64_t(1)<<16)|(std::uint64_t(1)<<37));
        group["GroupPowers"]=powers;
        body["GroupData"]=LLSD::emptyArray(); body["GroupData"].append(group);
        std::string error;
        const auto decoded=LLVKChatProtocol::decodeGroups(body,agent,error);
        ensure("group permission bits retain network order",decoded && decoded->size()==1 && decoded->front().id==groupId &&
            decoded->front().powers==((std::uint64_t(1)<<16)|(std::uint64_t(1)<<37)));
        ensure("wrong account membership rejected",!LLVKChatProtocol::decodeGroups(body,groupId,error));
        body["GroupData"][0]["GroupPowers"]=LLSD::Binary(7);
        ensure("truncated group powers rejected",!LLVKChatProtocol::decodeGroups(body,agent,error));
        body["GroupData"][0]=group; body["GroupData"].append(group);
        ensure("duplicate membership rejected",!LLVKChatProtocol::decodeGroups(body,agent,error));
    }

    template<> template<> void loginprotocol_object::test<5>()
    {
        std::ifstream file(LLVK_LOGIN_MESSAGE_TEMPLATE);
        ensure("chat reference template available",file.good());
        const std::string source{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
        LLTemplateTokenizer tokenizer(source); LLTemplateParser parser(tokenizer);
        std::vector<std::unique_ptr<LLMessageTemplate>> owned;
        LLTemplateMessageBuilder::message_template_name_map_t names;
        for (auto iterator=parser.getMessagesBegin(); iterator!=parser.getMessagesEnd(); ++iterator)
        { owned.emplace_back(*iterator); names[(*iterator)->mName]=*iterator; }
        const auto symbol=[](const char* value) { return LLMessageStringTable::getInstance()->getString(value); };
        const LLUUID agent("11111111-1111-1111-1111-111111111111"),session("22222222-2222-2222-2222-222222222222");
        const LLUUID recipient("33333333-3333-3333-3333-333333333333");
        const std::string text="Hello \xE2\x98\x83";
        std::string error;
        LLTemplateMessageBuilder builder(names);
        const auto payload=[&]()
        {
            std::array<std::uint8_t,4096> bytes{};
            const auto length=builder.buildMessage(bytes.data(),static_cast<U32>(bytes.size()),0);
            return LLVKChatProtocol::Bytes(bytes.begin()+10,bytes.begin()+length);
        };
        for (const std::uint8_t type : {0,1,2})
        {
            builder.newMessage(symbol("ChatFromViewer")); builder.nextBlock(symbol("AgentData"));
            builder.addUUID(symbol("AgentID"),agent); builder.addUUID(symbol("SessionID"),session);
            builder.nextBlock(symbol("ChatData")); builder.addString(symbol("Message"),text);
            builder.addU8(symbol("Type"),type); builder.addS32(symbol("Channel"),0);
            const auto encoded=LLVKChatProtocol::local(agent,session,text,type,0,error);
            ensure("local chat matches reference bytes",encoded && *encoded==payload());
        }
        builder.newMessage(symbol("ChatFromSimulator")); builder.nextBlock(symbol("ChatData"));
        builder.addString(symbol("FromName"),"Fixture Resident"); builder.addUUID(symbol("SourceID"),agent);
        builder.addUUID(symbol("OwnerID"),recipient); builder.addU8(symbol("SourceType"),1);
        builder.addU8(symbol("ChatType"),1); builder.addU8(symbol("Audible"),2);
        builder.addVector3(symbol("Position"),LLVector3(128,129,30)); builder.addString(symbol("Message"),text);
        const auto localPayload=payload();
        const auto local=LLVKChatProtocol::decodeLocal(localPayload,error);
        ensure("incoming local chat preserves reference fields",local && local->sender==agent && local->owner==recipient &&
            local->name=="Fixture Resident" && local->text==text && local->audible==2 && local->position[1]==129.f);
        for (std::size_t length=0; length<localPayload.size(); ++length)
            ensure("truncated local chat rejected",!LLVKChatProtocol::decodeLocal(std::span(localPayload).first(length),error));
        for (const std::uint8_t dialog : {0,15,17,18,41,42})
        {
            LLVKChatProtocol::Message message;
            message.recipient=recipient; message.name="Fixture Resident"; message.text=text; message.dialog=dialog;
            message.conversation=LLVKChatProtocol::directSession(agent,recipient);
            builder.newMessage(symbol("ImprovedInstantMessage")); builder.nextBlock(symbol("AgentData"));
            builder.addUUID(symbol("AgentID"),agent); builder.addUUID(symbol("SessionID"),session);
            builder.nextBlock(symbol("MessageBlock")); builder.addBOOL(symbol("FromGroup"),false);
            builder.addUUID(symbol("ToAgentID"),recipient); builder.addU32(symbol("ParentEstateID"),0);
            builder.addUUID(symbol("RegionID"),LLUUID::null); builder.addVector3(symbol("Position"),LLVector3::zero);
            builder.addU8(symbol("Offline"),0); builder.addU8(symbol("Dialog"),dialog);
            builder.addUUID(symbol("ID"),message.conversation); builder.addU32(symbol("Timestamp"),0);
            builder.addString(symbol("FromAgentName"),message.name); builder.addString(symbol("Message"),text);
            const std::uint8_t empty=0; builder.addBinaryData(symbol("BinaryBucket"),&empty,1);
            builder.nextBlock(symbol("EstateBlock")); builder.addU32(symbol("EstateID"),0);
            const auto reference=payload();
            const auto encoded=LLVKChatProtocol::instant(agent,session,message,error);
            ensure("IM matches reference bytes",encoded && *encoded==reference);
            const auto decoded=LLVKChatProtocol::decodeInstant(reference,error);
            ensure("incoming IM preserves reference fields",decoded && decoded->sender==agent && decoded->recipient==recipient &&
                decoded->conversation==message.conversation && decoded->text==text && decoded->dialog==dialog);
            ensure("truncated IM rejected",!LLVKChatProtocol::decodeInstant(std::span(reference).first(60),error));
        }
        ensure("self conversation identity matches reference",LLVKChatProtocol::directSession(agent,agent)==agent);
        ensure("invalid UTF-8 rejected",!LLVKChatProtocol::local(agent,session,std::string(1,char(0xff)),1,0,error));
        ensure("oversized chat rejected without truncation",!LLVKChatProtocol::local(agent,session,std::string(1024,'x'),1,0,error));
    }

    template<> template<> void loginprotocol_object::test<4>()
    {
        using Owner=LLVKSessionOwner;
        boost::asio::io_context context;
        boost::asio::ip::tcp::acceptor acceptor(context,{boost::asio::ip::address_v4::loopback(),0});
        acceptor.non_blocking(true);
        LLVKLoginTransport::Configuration configuration;
        configuration.http.allowLoopbackHttpForTests=true;
        configuration.endpoint="http://127.0.0.1:"+std::to_string(acceptor.local_endpoint().port())+"/login";
        auto transport=std::make_shared<LLVKLoginTransport>(configuration);
        Owner owner(transport);
        struct Cleanup { Owner& owner; ~Cleanup() { owner.shutdown(); } } cleanup{owner};
        std::string error;
        const auto credentials=LLVKLoginProtocol::credentials("Fixture.Resident","synthetic","last",error);
        ensure(error,credentials.has_value());
        const auto respond=[&](const LLSD& response)
        {
            boost::asio::ip::tcp::socket socket(context);
            bool accepted=false,replied=false;
            std::string request;
            const auto original=owner.snapshot();
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
            while (std::chrono::steady_clock::now()<deadline)
            {
                owner.pumpOne();
                if (owner.snapshot().state!=original.state) break;
                boost::system::error_code problem;
                if (!accepted)
                {
                    acceptor.accept(socket,problem);
                    if (!problem) { accepted=true; socket.non_blocking(true); }
                    else ensure("transport nonblocking accept",problem==boost::asio::error::would_block);
                }
                if (accepted && !replied)
                {
                    std::array<char,4096> buffer{};
                    const auto count=socket.read_some(boost::asio::buffer(buffer),problem);
                    if (!problem) request.append(buffer.data(),count);
                    else ensure("transport nonblocking request",problem==boost::asio::error::would_block);
                    if (request.ends_with("</methodCall>"))
                    {
                        ensure("transport uses reference XML-RPC method",request.find("<methodName>login_to_simulator</methodName>")!=request.npos);
                        ensure("plaintext password absent from wire",request.find("synthetic")==request.npos);
                        const auto body="<methodResponse><params><param>"+response.asXMLRPCValue()+"</param></params></methodResponse>";
                        const auto wire="HTTP/1.1 200 OK\r\nContent-Length: "+std::to_string(body.size())+"\r\nConnection: close\r\n\r\n"+body;
                        socket.non_blocking(false);
                        boost::asio::write(socket,boost::asio::buffer(wire),problem);
                        ensure("transport reply sent",!problem);
                        socket.close(); replied=true;
                    }
                }
            }
            ensure("transport response completed",replied && owner.snapshot().state!=original.state);
        };
        ensure("prepare native login",transport->prepare(*credentials,error));
        ensure("start native authentication",owner.beginLogin().ok());
        LLSD failure; failure["login"]="false"; failure["reason"]="key"; failure["message"]="private service text";
        respond(failure);
        ensure("server rejection is not authorization",owner.snapshot().state==Owner::State::PreLogin &&
            owner.snapshot().status.code==Owner::Code::AuthenticationFailed && !owner.snapshot().identity);
        ensure("prepare TOS response regression",transport->prepare(*credentials,error));
        ensure("start TOS response regression",owner.beginLogin().ok());
        LLSD tos; tos["login"]="false"; tos["reason"]="tos"; tos["message"]="Terms require the browser flow";
        respond(tos);
        ensure("TOS cannot be accepted as a generic text agreement",owner.snapshot().state==Owner::State::PreLogin && !owner.snapshot().agreement);
        ensure("retry can prepare credentials",transport->prepare(*credentials,error));
        ensure("retry can authenticate",owner.beginLogin().ok());
        LLSD agreement; agreement["login"]="false"; agreement["reason"]="critical"; agreement["message"]="Exact critical notice.";
        respond(agreement);
        const auto snapshot=owner.snapshot();
        ensure("server agreement requires explicit decision",snapshot.state==Owner::State::AwaitingAgreement &&
            snapshot.agreement && snapshot.agreement->text=="Exact critical notice.");
        ensure("decline cancels actual transport",owner.decideAgreement(snapshot.tag,*snapshot.agreement,false).code==Owner::Code::AgreementRejected);
        ensure("cancelled transport returns prelogin",owner.snapshot().state==Owner::State::PreLogin);
        ensure("new credentials after decline",transport->prepare(*credentials,error));
        ensure("new attempt after decline",owner.beginLogin().ok());
        LLSD challenge; challenge["login"]="false"; challenge["reason"]="mfa_challenge";
        challenge["message_id"]="LoginFailedAuthenticationMFARequired";
        respond(challenge);
        const auto challenged=owner.snapshot();
        ensure("actual MFA response reaches owner",challenged.state==Owner::State::AwaitingChallenge && challenged.challenge);
        ensure("MFA token submits next tagged request",owner.submitChallenge(challenged.tag,*challenged.challenge,"123456").ok());
        ensure("cancel pending HTTP",owner.cancel().code==Owner::Code::Cancelled);
        ensure("cancel never publishes identity",!owner.snapshot().identity);
    }

    template<> template<> void loginprotocol_object::test<3>()
    {
        std::ifstream file(LLVK_LOGIN_MESSAGE_TEMPLATE);
        ensure("message template available",file.good());
        const std::string source{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
        LLTemplateTokenizer tokenizer(source);
        LLTemplateParser parser(tokenizer);
        std::vector<std::unique_ptr<LLMessageTemplate>> templates;
        LLTemplateMessageBuilder::message_template_name_map_t names;
        for (auto iterator=parser.getMessagesBegin(); iterator!=parser.getMessagesEnd(); ++iterator)
        { templates.emplace_back(*iterator); names[(*iterator)->mName]=*iterator; }
        boost::asio::io_context context;
        boost::asio::ip::udp::socket server(context,{boost::asio::ip::address_v4::loopback(),0});
        server.non_blocking(true);
        LLVKLoginProtocol::Bootstrap bootstrap;
        bootstrap.agentId=LLUUID("11111111-1111-1111-1111-111111111111");
        bootstrap.sessionId=LLUUID("22222222-2222-2222-2222-222222222222");
        bootstrap.circuitCode=UINT32_MAX;
        bootstrap.regionHandle=(std::uint64_t(256000)<<32)|256256;
        bootstrap.simulatorAddress="127.0.0.1";
        bootstrap.simulatorPort=server.local_endpoint().port();
        LLVKRegionCircuit circuit;
        std::string error;
        ensure("start real UDP circuit",circuit.start(bootstrap,error));
        boost::asio::ip::udp::endpoint client;
        const auto receive=[&]()
        {
            std::vector<std::uint8_t> bytes(2048);
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
            for (;;)
            {
                boost::system::error_code problem;
                const auto count=server.receive_from(boost::asio::buffer(bytes),client,0,problem);
                if (!problem) { bytes.resize(count); return bytes; }
                ensure("bounded simulator receive",problem==boost::asio::error::would_block && std::chrono::steady_clock::now()<deadline);
                ensure(error,circuit.pump(error)!=LLVKRegionCircuit::Status::Failed);
            }
        };
        const auto first=receive();
        ensure("reference UseCircuitCode layout",first.size()==46 && first[6]==255 && first[7]==255 && first[8]==0 && first[9]==3);
        ensure_equals("wire circuit code",boost::endian::load_little_u32(first.data()+10),UINT32_MAX);
        ensure("wire session identity",std::equal(bootstrap.sessionId.mData,bootstrap.sessionId.mData+16,first.begin()+14));
        const auto acknowledged=boost::endian::load_big_u32(first.data()+1);
        std::uint32_t sequence=40;
        const auto send=[&](const char* message,std::uint32_t acknowledgement=0,bool longBucket=false)
        {
            const auto canonical=LLMessageStringTable::getInstance()->getString(message);
            auto* definition=names.at(canonical);
            LLTemplateMessageBuilder builder(names);
            builder.newMessage(canonical);
            for (auto* block : definition->mMemberBlocks)
            {
                builder.nextBlock(block->mName);
                for (auto* variable : block->mMemberVariables)
                {
                    const auto name=variable->getName();
                    switch (variable->getType())
                    {
                    case MVT_U8: builder.addU8(name,0); break;
                    case MVT_BOOL: builder.addBOOL(name,false); break;
                    case MVT_U32: builder.addU32(name,std::string_view(name)=="ID" ? acknowledgement : 0); break;
                    case MVT_S32: builder.addS32(name,0); break;
                    case MVT_U64: builder.addU64(name,std::string_view(name)=="RegionHandle" ? bootstrap.regionHandle : 0); break;
                    case MVT_F32: builder.addF32(name,0); break;
                    case MVT_LLUUID:
                        builder.addUUID(name,(std::string_view(message)=="OnlineNotification" || std::string_view(message)=="OfflineNotification") ?
                            LLUUID("44444444-4444-4444-4444-444444444444") : std::string_view(name)=="SessionID" ? bootstrap.sessionId : bootstrap.agentId);
                        break;
                    case MVT_LLVector3: builder.addVector3(name,LLVector3(128,128,30)); break;
                    case MVT_VARIABLE:
                        if (longBucket && std::string_view(name)=="BinaryBucket")
                        {
                            const std::array<std::uint8_t,600> bucket{};
                            builder.addBinaryData(name,bucket.data(),static_cast<S32>(bucket.size()));
                        }
                        else builder.addString(name,"");
                        break;
                    default: ensure("supported connection fixture field",false);
                    }
                }
            }
            std::array<std::uint8_t,4096> buffer{};
            buffer[0]=0x40;
            boost::endian::store_big_u32(buffer.data()+1,++sequence);
            auto length=builder.buildMessage(buffer.data(),static_cast<U32>(buffer.size()),0);
            auto* bytes=buffer.data();
            builder.compressMessage(bytes,length);
            if (longBucket)
            {
                std::vector<std::uint8_t> continued(bytes,bytes+length);
                const std::array<std::uint8_t,4> shortRuns{0,255,0,255};
                const auto run=std::search(continued.begin()+6,continued.end(),shortRuns.begin(),shortRuns.end());
                ensure("fixture contains two full zero runs",run!=continued.end());
                const auto offset=std::distance(continued.begin(),run);
                continued.erase(run,run+4);
                continued.insert(continued.begin()+offset,{0,0,254});
                server.send_to(boost::asio::buffer(continued),client);
            }
            else server.send_to(boost::asio::buffer(bytes,length),client);
        };
        send("RegionHandshake");
        ensure("handshake alone is not connected",circuit.pump(error)==LLVKRegionCircuit::Status::Connecting);
        send("PacketAck",acknowledged);
        ensure("circuit ack alone is not connected",circuit.pump(error)==LLVKRegionCircuit::Status::Connecting);
        send("AgentMovementComplete");
        ensure("matching simulator movement completes circuit",circuit.pump(error)==LLVKRegionCircuit::Status::Connected);
        ensure("connected local chat can send",circuit.sendLocal("Local fixture",1,0,error));
        LLVKChatProtocol::Message direct;
        direct.recipient=LLUUID("44444444-4444-4444-4444-444444444444"); direct.name="Fixture Resident";
        direct.text="IM fixture"; direct.conversation=LLVKChatProtocol::directSession(bootstrap.agentId,direct.recipient);
        ensure("connected IM can send",circuit.sendInstant(direct,error));
        send("ChatFromSimulator");
        send("ImprovedInstantMessage");
        send("OnlineNotification"); send("OfflineNotification");
        ensure("incoming chat pump",circuit.pump(error)==LLVKRegionCircuit::Status::Connected);
        const auto incoming=circuit.takeMessages();
        ensure("incoming local and IM reach native queue",incoming.size()==2 && incoming[0].kind==LLVKChatProtocol::Message::Kind::Local &&
            incoming[1].kind==LLVKChatProtocol::Message::Kind::Instant);
        ensure("IM receive diagnostics reflect actual decoded packets",circuit.messageDiagnostics().instantReceived &&
            circuit.messageDiagnostics().instantDecoded && !circuit.messageDiagnostics().instantRejected);
        const auto presence=circuit.takePresence();
        ensure("reference presence notifications reach native queue in order",presence.size()==2 &&
            presence[0].id==LLUUID("44444444-4444-4444-4444-444444444444") && presence[0].online && !presence[1].online && circuit.takePresence().empty());
        send("ImprovedInstantMessage",0,true);
        ensure("long zero-run IM remains connected",circuit.pump(error)==LLVKRegionCircuit::Status::Connected);
        const auto longIncoming=circuit.takeMessages();
        ensure("reference-compressed long IM bucket reaches consumer",longIncoming.size()==1 && longIncoming[0].bucket==LLVKChatProtocol::Bytes(600,0));
        ensure("message queue drains once",circuit.takeMessages().empty());
        ensure("logout waits for simulator",circuit.close(error)==LLVKRegionCircuit::Status::Closing);
        send("LogoutReply");
        ensure("matching logout completes",circuit.close(error)==LLVKRegionCircuit::Status::Closed);
        circuit.cancel();
        ensure("closed circuit cannot send local chat",!circuit.sendLocal("not sent",1,0,error));
        ensure("closed circuit cannot send IM",!circuit.sendInstant(direct,error));
        {
            std::array<std::uint8_t,2048> discarded{};
            boost::system::error_code problem;
            do { server.receive_from(boost::asio::buffer(discarded),client,0,problem); } while (!problem);
        }
        LLSD login;
        login["login"]="true"; login["agent_id"]=bootstrap.agentId.asString(); login["session_id"]=bootstrap.sessionId.asString();
        login["secure_session_id"]="33333333-3333-3333-3333-333333333333";
        login["circuit_code"]="4294967295"; login["sim_ip"]="127.0.0.1"; login["sim_port"]=bootstrap.simulatorPort;
        login["region_x"]=256000; login["region_y"]=256256;
        LLSD buddy;
        buddy["buddy_id"]=LLUUID("44444444-4444-4444-4444-444444444444"); buddy["buddy_rights_given"]=1; buddy["buddy_rights_has"]=5;
        login["buddy-list"]=LLSD::emptyArray(); login["buddy-list"].append(buddy);
        LoginTlsServer tls(login);
        {
            LLVKLoginHttp untrusted({});
            ensure("untrusted TLS request queues",untrusted.start(tls.url()+"/login","<test/>","text/xml",error));
            auto status=LLVKLoginHttp::Status::Pending;
            const auto trustDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
            while (status==LLVKLoginHttp::Status::Pending && std::chrono::steady_clock::now()<trustDeadline) status=untrusted.pump(error);
            ensure("untrusted certificate is rejected",status==LLVKLoginHttp::Status::Failed && untrusted.responseCode()==0);
        }
        LLVKLoginTransport::Configuration configuration;
        configuration.endpoint=tls.url()+"/login";
        configuration.http.certificateBundle=tls.certificate.string();
        std::vector<std::string> diagnosticStages;
        configuration.diagnostic=[&](const char* stage,long) { diagnosticStages.emplace_back(stage); };
        auto transport=std::make_shared<LLVKLoginTransport>(configuration);
        LLVKSessionOwner owner(transport);
        struct Cleanup { LLVKSessionOwner& owner; ~Cleanup() { owner.shutdown(); } } cleanup{owner};
        const auto credentials=LLVKLoginProtocol::credentials("Fixture.Resident","synthetic","last",error);
        ensure("TLS fixture credentials prepared",credentials && transport->prepare(*credentials,error));
        ensure("TLS authentication starts",owner.beginLogin().ok());
        bool connected=false,closing=false,keepalive=false,searchStarted=false,searchComplete=false;
        bool groupStarted=false,groupSent=false,groupLeft=false,moderationSeen=false,invitationSeen=false;
        bool denialRequested=false;
        unsigned eventInstantMessages=0;
        bool directSubmitted=false,directObserved=false;
        const LLUUID groupId("55555555-5555-5555-5555-555555555555");
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
        while (std::chrono::steady_clock::now()<deadline)
        {
            const auto snapshot=owner.snapshot();
            if (snapshot.state==LLVKSessionOwner::State::Disconnecting) owner.retryCleanup(snapshot.tag);
            else owner.pumpOne();
            if (owner.snapshot().state==LLVKSessionOwner::State::Stopped) break;
            if (owner.snapshot().state==LLVKSessionOwner::State::Connected && tls.polled)
            {
                connected=true;
                const auto chat=transport->chatContext(owner.snapshot().tag);
                ensure("connected identity available without secrets",chat && chat->agent==bootstrap.agentId);
                auto stale=owner.snapshot().tag; ++stale.generation;
                ensure("stale communication context rejected",!transport->chatContext(stale));
                ensure("stale message send rejected",!transport->sendLocal(stale,"not sent",1,error));
                if (!searchStarted)
                {
                    ensure("stale resident search rejected",!transport->searchResidents(stale,1,"Fixture",error));
                    ensure("resident search starts",transport->searchResidents(owner.snapshot().tag,1,"Fixture",error));
                    ensure("new search replaces pending request",transport->searchResidents(owner.snapshot().tag,2,"Fixture",error));
                    searchStarted=true;
                }
                if (auto result=transport->takeResidentSearch(owner.snapshot().tag))
                {
                    ensure("latest search publishes validated identity",result->query==2 && result->error.empty() && result->residents.size()==1 &&
                        result->residents[0].id==LLUUID("44444444-4444-4444-4444-444444444444") && result->residents[0].username=="fixture.resident");
                    ensure("search result consumed once",!transport->takeResidentSearch(owner.snapshot().tag));
                    searchComplete=true;
                }
                const auto memberships=transport->groups(owner.snapshot().tag);
                const auto friends=transport->friends(owner.snapshot().tag);
                if (!directSubmitted && friends.size()==1 && friends.front().presenceReceived)
                {
                    ensure("live friend state combines permissions and notifications",friends.front().rightsGiven==1 && friends.front().rightsHeld==5 && !friends.front().online);
                    ensure("stale tag cannot expose friends",transport->friends(stale).empty());
                    ensure("direct message uses authenticated friend data",transport->sendDirect(owner.snapshot().tag,friends.front().id,"Offline delivery fixture",false,false,error));
                    directSubmitted=true;
                }
                if (!memberships.empty() && !groupStarted)
                {
                    ensure("stale group join rejected",!transport->joinGroup(stale,groupId,error));
                    ensure("group member joins",transport->joinGroup(owner.snapshot().tag,groupId,error));
                    ensure("joining cannot send early",!transport->sendGroup(owner.snapshot().tag,groupId,"not sent",error));
                    groupStarted=true;
                }
                if (!memberships.empty() && memberships.front().state==LLVKChatProtocol::Group::State::Joined && !groupSent)
                {
                    const bool muted=memberships.front().participants.at(bootstrap.agentId).textMuted;
                    if (muted && !moderationSeen)
                    {
                        ensure("pre-reply moderator update blocks sends",!transport->sendGroup(owner.snapshot().tag,groupId,"not sent",error));
                        ensure("stale moderator command rejected",!transport->moderateGroup(stale,groupId,bootstrap.agentId,false,error));
                        ensure("moderator requests explicit unmute",transport->moderateGroup(owner.snapshot().tag,groupId,bootstrap.agentId,false,error));
                        ensure("moderation is not applied optimistically",transport->groups(owner.snapshot().tag).front().participants.at(bootstrap.agentId).textMuted);
                        ensure("concurrent moderation bounded",!transport->moderateGroup(owner.snapshot().tag,groupId,bootstrap.agentId,true,error));
                        moderationSeen=true;
                    }
                    if (!muted && moderationSeen && !memberships.front().moderationPending)
                    {
                        if (!denialRequested)
                        {
                            ensure("empty successful moderation response is accepted",memberships.front().error.empty());
                            ensure("submit server-denied moderation",transport->moderateGroup(owner.snapshot().tag,groupId,bootstrap.agentId,true,error));
                            denialRequested=true;
                        }
                        else
                        {
                            ensure("denied moderation reports permission without closing session",memberships.front().error=="Group moderation permission was denied");
                            ensure("joined group sends after denial",transport->sendGroup(owner.snapshot().tag,groupId,"Group fixture",error));
                            ensure("submit moderation before leave",transport->moderateGroup(owner.snapshot().tag,groupId,bootstrap.agentId,true,error));
                            ensure("group chat leave submits",transport->leaveGroup(owner.snapshot().tag,groupId,error));
                            ensure("left group cannot send",!transport->sendGroup(owner.snapshot().tag,groupId,"not sent",error));
                            groupSent=true;
                        }
                    }
                }
                for (const auto& message : transport->takeMessages(owner.snapshot().tag))
                {
                        if (message.dialog==0)
                        {
                            ensure("event-queue IM reaches real transport consumer",message.text=="Event IM fixture" &&
                                message.recipient==bootstrap.agentId && message.sender==LLUUID("44444444-4444-4444-4444-444444444444"));
                            ++eventInstantMessages;
                        }
                        if (message.dialog==13)
                        {
                            ensure("invitation preserves text and session",message.conversation==groupId && message.text=="Invitation fixture");
                            ensure("native consumer accepts invitation",transport->joinGroup(owner.snapshot().tag,groupId,error));
                            invitationSeen=true;
                        }
                }
                if (keepalive && searchComplete && groupLeft && invitationSeen && directObserved && !memberships.empty() && memberships.front().state==LLVKChatProtocol::Group::State::Joined)
                {
                    ensure("nonmoderator cannot issue mute request",!transport->moderateGroup(owner.snapshot().tag,groupId,bootstrap.agentId,true,error));
                    ensure("old moderation cannot publish into rejoined session",memberships.front().error.empty() && !memberships.front().moderationPending);
                    const auto groups=transport->groups(owner.snapshot().tag);
                    ensure("event queue publishes native memberships",groups.size()==1 && groups.front().name=="Fixture Group" &&
                        groups.front().powers==((std::uint64_t(1)<<16)|(std::uint64_t(1)<<37)));
                    ensure("stale owner cannot read memberships",transport->groups(stale).empty());
                    owner.shutdown(); closing=true;
                }
            }
            std::array<std::uint8_t,2048> bytes{};
            boost::system::error_code problem;
            const auto count=server.receive_from(boost::asio::buffer(bytes),client,0,problem);
            if (problem) continue;
            if (count==12 && bytes[6]==1)
            { keepalive=true; send("CompletePingCheck"); }
            if (count>=10 && bytes[6]==255 && bytes[7]==255)
            {
                const auto message=boost::endian::load_big_u16(bytes.data()+8);
                if (bytes[0]&0x40) send("PacketAck",boost::endian::load_big_u32(bytes.data()+1));
                if (message==3) send("RegionHandshake");
                if (message==249)
                { send("AgentMovementComplete"); send("OnlineNotification"); send("OfflineNotification"); }
                if (message==252) send("LogoutReply");
                if (message==254)
                {
                    const auto instant=LLVKChatProtocol::decodeInstant(std::span(bytes).subspan(10,count-10),error);
                    if (instant && instant->dialog==0)
                    {
                        ensure("offline friend flag matches reference delivery",instant->recipient==buddy["buddy_id"].asUUID() &&
                            instant->offline==1 && instant->text=="Offline delivery fixture");
                        directObserved=true;
                        continue;
                    }
                    ensure("group operation has native IM framing",instant && instant->conversation==groupId && instant->recipient==groupId);
                    if (instant->dialog==15) tls.groupStart=true;
                    if (instant->dialog==17) ensure("group wire text preserved",instant->text=="Group fixture");
                    if (instant->dialog==18 && !groupLeft) { groupLeft=true; tls.groupInvite=true; }
                }
            }
        }
        ensure("TLS authorization and capabilities maintain real UDP connection",connected && keepalive);
        ensure("resident lookup uses actual TLS GET",searchComplete && tls.searched);
        ensure("group join send leave completes through UDP and event queue",groupStarted && groupSent && groupLeft);
        ensure("group text moderation enforced",moderationSeen);
        ensure("group moderation uses exact HTTPS request",tls.groupModerated.load());
        ensure("server denial exercised",denialRequested && tls.moderationDenied.load());
        ensure("group invitation accepted through HTTPS",invitationSeen && tls.groupAccepted);
        ensure_equals("event-queue IM published exactly once",eventInstantMessages,1u);
        for (const auto stage : {"im-event-received","im-published-to-ui"})
            ensure_equals("message diagnostics are one-time",std::count(diagnosticStages.begin(),diagnosticStages.end(),stage),std::ptrdiff_t(1));
        ensure("event queue closes before successful shutdown",closing && tls.finished && owner.snapshot().state==LLVKSessionOwner::State::Stopped);
        ensure("disconnected chat context revoked",!transport->chatContext(owner.snapshot().tag));
        ensure("friend data revoked on shutdown",transport->friends(owner.snapshot().tag).empty());
        ensure("friend-aware direct delivery exercised",directSubmitted && directObserved);
        for (const auto stage : {"authorized","connection-started","seed-ready","region-connected"})
            ensure("sanitized transport stage emitted",std::find(diagnosticStages.begin(),diagnosticStages.end(),stage)!=diagnosticStages.end());
        ensure("native authentication and circuit never load desktop OpenGL",GetModuleHandleW(L"opengl32.dll")==nullptr);
    }

    template<> template<> void loginprotocol_object::test<2>()
    {
        boost::asio::io_context context;
        boost::asio::ip::tcp::acceptor acceptor(context,{boost::asio::ip::address_v4::loopback(),0});
        acceptor.non_blocking(true);
        LLVKLoginHttp::Configuration configuration;
        configuration.allowLoopbackHttpForTests=true;
        LLVKLoginHttp http(configuration);
        const auto endpoint="http://127.0.0.1:"+std::to_string(acceptor.local_endpoint().port())+"/login";
        for (const auto method : {LLVKLoginHttp::Method::Post,LLVKLoginHttp::Method::Get})
        for (const bool redirect : {false,true})
        {
            std::string error;
            const bool get=method==LLVKLoginHttp::Method::Get;
            const std::string body=get ? "" : "<fixture>synthetic-secret</fixture>";
            ensure("queue loopback request",http.start(endpoint,body,"text/xml",error,method));
            boost::asio::ip::tcp::socket socket(context);
            bool accepted=false,replied=false;
            std::string request;
            auto status=LLVKLoginHttp::Status::Pending;
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
            while (status==LLVKLoginHttp::Status::Pending && std::chrono::steady_clock::now()<deadline)
            {
                status=http.pump(error);
                boost::system::error_code problem;
                if (!accepted)
                {
                    acceptor.accept(socket,problem);
                    if (!problem) { accepted=true; socket.non_blocking(true); }
                    else ensure("nonblocking accept",problem==boost::asio::error::would_block);
                }
                if (accepted && !replied)
                {
                    std::array<char,4096> buffer{};
                    const auto count=socket.read_some(boost::asio::buffer(buffer),problem);
                    if (!problem) request.append(buffer.data(),count);
                    else ensure("nonblocking request read",problem==boost::asio::error::would_block);
                    if (get ? request.ends_with("\r\n\r\n") : request.ends_with(body))
                    {
                        ensure("HTTP method matches request",request.starts_with(get ? "GET /login " : "POST /login "));
                        ensure("HTTP content type",request.find("Content-Type: text/xml")!=request.npos);
                        const std::string response=redirect ?
                            "HTTP/1.1 302 Found\r\nLocation: http://127.0.0.1:1/leak\r\nContent-Length: 0\r\nConnection: close\r\n\r\n" :
                            "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok";
                        socket.non_blocking(false);
                        boost::asio::write(socket,boost::asio::buffer(response),problem);
                        ensure("loopback reply written",!problem);
                        socket.close();
                        replied=true;
                    }
                }
            }
            ensure("loopback completion bounded",replied && status!=LLVKLoginHttp::Status::Pending);
            if (redirect)
            {
                ensure("redirect not followed",status==LLVKLoginHttp::Status::Failed && http.responseCode()==302);
                ensure("failure redacts credentials",error.find("synthetic-secret")==error.npos);
            }
            else ensure("actual response received",status==LLVKLoginHttp::Status::Complete && http.takeResponse()=="ok");
        }
    }

    template<> template<> void loginprotocol_object::test<1>()
    {
        std::string error;
        const auto credentials=LLVKLoginProtocol::credentials("First.Last","password","last",error);
        ensure("native Second Life credential transformation",credentials && (*credentials)["first"].asString()=="First" &&
            (*credentials)["last"].asString()=="Last" && (*credentials)["passwd"].asString()=="$1$5f4dcc3b5aa765d61d8327deb882cf99");
        const auto resident=LLVKLoginProtocol::credentials("singleaccount","password","home",error);
        ensure("single account uses Resident",resident && (*resident)["last"].asString()=="Resident");
        ensure("unsupported location is not silently changed",!LLVKLoginProtocol::credentials("First.Last","password","other",error));
        LLSD data;
        data["login"]="true";
        data["agent_id"]="11111111-1111-1111-1111-111111111111";
        data["session_id"]="22222222-2222-2222-2222-222222222222";
        data["secure_session_id"]="33333333-3333-3333-3333-333333333333";
        data["circuit_code"]="4294967295";
        data["sim_ip"]="127.0.0.1";
        data["sim_port"]=13000;
        data["region_x"]=256000;
        data["region_y"]=256256;
        data["seed_capability"]="https://example.test/cap/private";
        const auto encoded=LLVKLoginProtocol::encode(data,error);
        ensure(error,encoded.has_value());
        const auto response="<methodResponse><params><param>"+data.asXMLRPCValue()+"</param></params></methodResponse>";
        const auto decoded=LLVKLoginProtocol::decode(response,error);
        ensure(error,decoded.has_value());
        const auto bootstrap=LLVKLoginProtocol::bootstrap(*decoded,error);
        ensure(error,bootstrap.has_value());
        ensure_equals("unsigned circuit retained",bootstrap->circuitCode,UINT32_MAX);
        data["circuit_code"]=-1;
        const auto signedCircuit=LLVKLoginProtocol::bootstrap(data,error);
        ensure("signed XML-RPC circuit preserves unsigned bits",signedCircuit && signedCircuit->circuitCode==UINT32_MAX);
        ensure("numeric XML-RPC errors do not throw",!LLVKLoginProtocol::decode(
            "<methodResponse><params><param><value><struct><member><name>value</name><value><int>99999999999999999999</int></value></member></struct></value></param></params></methodResponse>",error));
        ensure_equals("region handle retained",bootstrap->regionHandle,(std::uint64_t(256000)<<32)|256256);
        data["sim_port"]=65536;
        ensure("reject overflowing simulator port",!LLVKLoginProtocol::bootstrap(data,error));
        ensure("diagnostic does not expose capability",error.find("private")==error.npos);
        data["sim_port"]=13000;
        data["session_id"]="not-a-uuid";
        ensure("reject invalid session identity",!LLVKLoginProtocol::bootstrap(data,error));
        ensure("reject DTD",!LLVKLoginProtocol::decode("<!DOCTYPE methodResponse><methodResponse/>",error));
        std::string nested;
        for (unsigned depth=0; depth<65; ++depth) nested+="<value>";
        for (unsigned depth=0; depth<65; ++depth) nested+="</value>";
        ensure("reject excessive XML nesting before tree decoding",!LLVKLoginProtocol::boundedXml(nested));
        ensure("reject fault without printing service text",!LLVKLoginProtocol::decode(
            "<methodResponse><fault><value><string>private</string></value></fault></methodResponse>",error) && error.find("private")==error.npos);
        data=LLSD::emptyMap(); data["first"]="A&B<C";
        const auto escaped=LLVKLoginProtocol::encode(data,error);
        ensure("XML-RPC values are escaped",escaped && escaped->find("A&amp;B&lt;C")!=escaped->npos);
        LLVKLoginHttp http({});
        ensure("plaintext login rejected",!http.start("http://example.test/login","secret","text/xml",error));
        ensure("userinfo rejected",!http.start("https://user:secret@example.test/login","secret","text/xml",error));
        ensure("header injection rejected",!http.start("https://example.test/login","secret","text/xml\r\nX: value",error));
        ensure("request failure never exposes secret",error.find("secret")==error.npos);
        ensure("HTTPS request can be queued without blocking",http.start("https://example.invalid/login","secret","text/xml",error));
        ensure("concurrent request rejected",!http.start("https://example.invalid/login","secret","text/xml",error));
        http.cancel();
        ensure("cancelled request is idle",http.pump(error)==LLVKLoginHttp::Status::Idle);
    }
}