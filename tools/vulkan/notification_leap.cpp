#include "linden_common.h"
#include "llsdserialize.h"
#include "llcontrol.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <fcntl.h>
#include <io.h>

namespace
{
    LLSD receive(std::istream& input)
    {
        std::string prefix;
        char character=0;
        while (input.get(character) && character!=':')
        {
            if (character<'0' || character>'9' || prefix.size()>=8) throw std::runtime_error("Invalid LEAP frame prefix");
            prefix+=character;
        }
        if (!input || prefix.empty()) throw std::runtime_error("LEAP stream closed");
        const auto size=std::stoul(prefix);
        if (!size || size>1024*1024) throw std::runtime_error("LEAP frame exceeds fixture budget");
        std::string bytes(size,'\0');
        if (!input.read(bytes.data(),size)) throw std::runtime_error("Truncated LEAP frame");
        std::istringstream stream(bytes);
        LLSD packet;
        if (!LLSDSerialize::deserialize(packet,stream,size) || !packet.isMap()) throw std::runtime_error("Invalid LLSD packet");
        return packet;
    }

    void send(const std::string& pump,const LLSD& data)
    {
        LLSD packet;
        packet["pump"]=pump;
        packet["data"]=data;
        std::ostringstream bytes;
        LLSDSerialize::toNotation(packet,bytes);
        const auto body=bytes.str();
        std::cout<<body.size()<<':'<<body<<std::flush;
        if (!std::cout) throw std::runtime_error("LEAP output closed");
    }

    void save(const std::filesystem::path& directory,const char* name,const LLSD& data)
    {
        const auto path=directory/name;
        if (std::filesystem::exists(path)) throw std::runtime_error("Refusing to overwrite driver evidence");
        std::ofstream file(path);
        LLSDSerialize::toPrettyXML(data,file);
        file.close();
        if (!file) throw std::runtime_error("Cannot save driver evidence");
    }
}

int main(int count,char** arguments)
{
    try
    {
        _setmode(_fileno(stdin),_O_BINARY);
        _setmode(_fileno(stdout),_O_BINARY);
        if (count==4 && std::string_view(arguments[1])=="--check-settings")
        {
            LLControlGroup settings("notification-fixture-preflight");
            if (!settings.loadFromFile(arguments[2],true,true)) throw std::runtime_error("Reference defaults did not load");
            if (!settings.loadFromFile(arguments[3],true,true)) throw std::runtime_error("Fixture overrides did not load");
            const auto command=settings.getLLSD("LeapCommand");
            if (!command.isArray() || command.size()!=1 || !command[0].isString() || command[0].asString().empty())
                throw std::runtime_error("Fixture defaults lack the LEAP command");
            const auto tokens=LLStringUtil::getTokens(command[0].asString()," \t\r\n","","\"'","\\");
            if (tokens.size()!=2 || !std::filesystem::is_regular_file(tokens[0]) || !std::filesystem::is_directory(tokens[1]))
                throw std::runtime_error("LEAP command does not resolve to an executable and evidence directory");
            if (settings.getString("RenderBackend")!="OpenGL" || settings.getBOOL("AutoLogin"))
                throw std::runtime_error("Fixture backend/login policy invalid");
            std::cerr<<"Fixture default controls and LEAP command loaded successfully\n";
            return 0;
        }
        if (count==3 && std::string_view(arguments[1])=="--check-command")
        {
            std::istringstream input(arguments[2]);
            LLSD command;
            if (LLSDSerialize::fromNotation(command,input,std::string_view(arguments[2]).size())<=0 ||
                !command.isString() || command.asString().empty()) throw std::runtime_error("Invalid LEAP command notation");
            std::cerr<<"LEAP command notation valid: "<<command.asString()<<'\n';
            return 0;
        }
        if (count==2 && std::string_view(arguments[1])=="--self-test")
        {
            LLSD source;
            source["pump"]="fixture";
            source["data"]["str"]="STATE_LOGIN_WAIT";
            std::ostringstream body;
            LLSDSerialize::serialize(source,body,LLSDSerialize::LLSD_BINARY,LLSDFormatter::OPTIONS_NONE);
            std::istringstream frame(std::to_string(body.str().size())+":"+body.str());
            const auto decoded=receive(frame);
            if (decoded["data"]["str"].asString()!="STATE_LOGIN_WAIT") throw std::runtime_error("LEAP roundtrip failed");
            std::ostringstream outgoing;
            LLSDSerialize::toNotation(source,outgoing);
            std::istringstream notation(outgoing.str());
            LLSD parsed;
            if (LLSDSerialize::fromNotation(parsed,notation,outgoing.str().size())<=0 ||
                parsed["pump"].asString()!="fixture" || parsed["data"]["str"].asString()!="STATE_LOGIN_WAIT")
                throw std::runtime_error("Pinned LEAP notation reply failed");
            std::istringstream invalid("1048577:");
            bool rejected=false;
            try { receive(invalid); } catch (const std::runtime_error&) { rejected=true; }
            if (!rejected) throw std::runtime_error("Unbounded packet accepted");
            std::cerr<<"Notification LEAP framing self-test passed\n";
            return 0;
        }
        if (count!=2) throw std::runtime_error("Usage: notification_leap evidence-directory | --self-test");
        const std::string pathArgument=arguments[1];
        const auto directory=std::filesystem::path(std::u8string(pathArgument.begin(),pathArgument.end()));
        if (!directory.is_absolute() || !std::filesystem::is_directory(directory)) throw std::runtime_error("Existing absolute evidence directory required");
        const auto hello=receive(std::cin);
        const auto reply=hello["pump"].asString();
        const auto command=hello["data"]["command"].asString();
        if (reply.empty() || command.empty()) throw std::runtime_error("Invalid LEAP handshake");
        LLSD quitPolicy;
        quitPolicy["op"]="ignore"; quitPolicy["name"]="ConfirmQuit"; quitPolicy["ignore"]=true;
        send("LLNotifications",quitPolicy);
        LLSD observe;
        observe["op"]="forward"; observe["channel"]="Visible";
        observe["pump"]=reply; observe["respond"]=false;
        send("LLNotifications",observe);
        LLSD listen;
        listen["op"]="listen"; listen["source"]="StartupState";
        listen["listener"]="notification-capture"; listen["reply"]=reply;
        send(command,listen);
        LLSD refresh;
        refresh["op"]="postStartupState";
        send("LLStartUp",refresh);
        bool submitted=false;
        bool loginReady=false;
        bool warningDismissed=false;
        unsigned geometryRequests=0;
        unsigned geometryReplies=0;
        LLSD geometry;
        const auto inspectVisible=[&]
        {
            LLSD request;
            request["op"]="listChannelNotifications"; request["channel"]="Visible";
            request["reply"]=reply; request["reqid"]="capture-visible-notifications";
            send("LLNotifications",request);
        };
        const auto dismissWarning=[&](const LLSD& warning)
        {
            if (warning["id"].asUUID().isNull()) throw std::runtime_error("URL warning has no identity");
            LLSD dismiss;
            dismiss["op"]="respond"; dismiss["uuid"]=warning["id"];
            dismiss["response"]["Cancel_okcancelbuttons"]=true;
            send("LLNotifications",dismiss);
        };
        while (std::cin)
        {
            const auto packet=receive(std::cin);
            const auto& data=packet["data"];
            if (packet["pump"].asString()=="StartupState" && data["str"].asString()=="STATE_LOGIN_WAIT")
            {
                loginReady=true;
                inspectVisible();
            }
            if (data["name"].asString()=="WarnForceLoginURL")
            {
                dismissWarning(data);
                if (loginReady) inspectVisible();
            }
            if (data["reqid"].asString()=="capture-visible-notifications" && !submitted)
            {
                if (!data["notifications"].isArray()) throw std::runtime_error("Visible notification list missing");
                bool foundWarning=false;
                for (auto notification=data["notifications"].beginArray(); notification!=data["notifications"].endArray(); ++notification)
                    if ((*notification)["name"].asString()=="WarnForceLoginURL")
                    { dismissWarning(*notification); foundWarning=true; }
                if (foundWarning) inspectVisible();
                else
                {
                    save(directory,"gl-preflight-notifications.xml",data);
                    warningDismissed=true;
                }
            }
            if (loginReady && warningDismissed && !submitted)
            {
                LLSD ignore;
                ignore["op"]="ignore"; ignore["name"]="MediaPluginFailed"; ignore["ignore"]=false;
                send("LLNotifications",ignore);
                LLSD request;
                request["op"]="requestAdd"; request["name"]="MediaPluginFailed";
                request["substitutions"]["PLUGIN"]="media_plugin_cef";
                request["reply"]=reply; request["reqid"]="capture-media-launch";
                send("LLNotifications",request);
                save(directory,"gl-notification-submitted.xml",request);
                submitted=true;
                LLSD paths;
                paths["op"]="getPaths"; paths["reply"]=reply; paths["reqid"]="capture-layout-paths";
                send("LLWindow",paths);
            }
            if (data["reqid"].asString()=="capture-layout-paths")
            {
                for (auto path=data["paths"].beginArray(); path!=data["paths"].endArray(); ++path)
                {
                    const auto text=path->asString();
                    if (text.find("toast")==std::string::npos && text.find("chiclet_container")==std::string::npos &&
                        text.find("floater_snap_region")==std::string::npos && text.find("screen_channel")==std::string::npos &&
                        text.find("world_view_rect")==std::string::npos) continue;
                    if (++geometryRequests>64) throw std::runtime_error("Layout probe exceeds view budget");
                    LLSD request;
                    request["op"]="getInfo"; request["path"]=text; request["reply"]=reply;
                    request["reqid"]="capture-layout:"+text;
                    send("LLWindow",request);
                }
            }
            if (data["reqid"].asString().starts_with("capture-layout:"))
            {
                geometry[data["reqid"].asString()]=data;
                if (++geometryReplies==geometryRequests) save(directory,"gl-layout.xml",geometry);
            }
            if (submitted && data["reqid"].asString()=="capture-media-launch" && data.has("response"))
            {
                save(directory,"gl-notification-response.xml",data);
                return 0;
            }
        }
        return 1;
    }
    catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}