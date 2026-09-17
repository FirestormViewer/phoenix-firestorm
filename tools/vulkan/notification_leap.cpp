#include "linden_common.h"
#include "llsdserialize.h"
#include "llcontrol.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <fcntl.h>
#include <io.h>
#include <cstdlib>
#include <windows.h>
#include <tlhelp32.h>

namespace
{
    struct ViewerPointer
    {
        HWND window=nullptr;
        POINT saved{};
        bool restore=false;
        ~ViewerPointer()
        {
            if (window) PostMessageW(window,WM_LBUTTONUP,0,MAKELPARAM(0,0));
            if (restore) SetCursorPos(saved.x,saved.y);
        }
        void click(int horizontal,int bottomOrigin)
        {
            const auto snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
            if (snapshot==INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot identify reference parent");
            PROCESSENTRY32W process{}; process.dwSize=sizeof(process);
            DWORD parent=0;
            for (BOOL found=Process32FirstW(snapshot,&process); found; found=Process32NextW(snapshot,&process))
                if (process.th32ProcessID==GetCurrentProcessId()) { parent=process.th32ParentProcessID; break; }
            CloseHandle(snapshot);
            struct Search { DWORD process; HWND window; } search{parent,nullptr};
            EnumWindows([](HWND candidate,LPARAM data)->BOOL
            {
                auto& search=*reinterpret_cast<Search*>(data);
                DWORD owner=0; GetWindowThreadProcessId(candidate,&owner);
                if (owner==search.process && IsWindowVisible(candidate) && !GetWindow(candidate,GW_OWNER))
                { search.window=candidate; return FALSE; }
                return TRUE;
            },reinterpret_cast<LPARAM>(&search));
            window=search.window;
            RECT client{};
            if (!window || !GetClientRect(window,&client) || client.right!=2560 || client.bottom!=1369)
                throw std::runtime_error("Checkbox fixture requires the qualified maximized reference window");
            const int vertical=client.bottom-1-bottomOrigin;
            if (horizontal<0 || horizontal>=client.right || vertical<0 || vertical>=client.bottom)
                throw std::runtime_error("Checkbox target outside reference client");
            restore=GetCursorPos(&saved)!=FALSE;
            POINT point{horizontal,vertical};
            SetForegroundWindow(window);
            if (!restore || !ClientToScreen(window,&point) || !SetCursorPos(point.x,point.y))
                throw std::runtime_error("Cannot place reference checkbox pointer");
            const auto position=MAKELPARAM(horizontal,vertical);
            SendMessageW(window,WM_MOUSEMOVE,0,position);
            SendMessageW(window,WM_LBUTTONDOWN,MK_LBUTTON,position);
            SendMessageW(window,WM_LBUTTONUP,0,position);
        }
    };

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
        const auto headerEnd=bytes.find('\n');
        if (headerEnd==std::string::npos || bytes.substr(0,headerEnd)!="<? LLSD/Binary ?>")
            throw std::runtime_error("Unexpected reference LEAP encoding");
        std::istringstream stream(bytes.substr(headerEnd+1));
        LLSD packet;
        const auto parsed=LLSDSerialize::fromBinary(packet,stream,size-headerEnd-1);
        if (parsed<=0 || !packet.isMap()) throw std::runtime_error("Invalid binary LLSD packet: bytes="+
            std::to_string(size)+" parsed="+std::to_string(parsed)+" position="+std::to_string(static_cast<std::streamoff>(stream.tellg())));
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
            source["data"]=LLSD::emptyMap();
            source["data"]["reqid"]="capture-username-entered";
            std::ostringstream acknowledgement;
            LLSDSerialize::serialize(source,acknowledgement,LLSDSerialize::LLSD_BINARY,LLSDFormatter::OPTIONS_NONE);
            std::istringstream ackFrame(std::to_string(acknowledgement.str().size())+":"+acknowledgement.str());
            if (receive(ackFrame)["data"]["reqid"].asString()!="capture-username-entered") throw std::runtime_error("Minimal acknowledgement failed");
            source["data"]["visible_chain"]=true;
            source["data"]["enabled"]=true;
            source["data"]["rect"]["left"]=0;
            source["data"]["rect"]["top"]=1369;
            std::ostringstream controlInfo;
            LLSDSerialize::serialize(source,controlInfo,LLSDSerialize::LLSD_BINARY,LLSDFormatter::OPTIONS_NONE);
            std::istringstream controlFrame(std::to_string(controlInfo.str().size())+":"+controlInfo.str());
            if (!receive(controlFrame)["data"]["visible_chain"].asBoolean()) throw std::runtime_error("Control-info acknowledgement failed");
            source["data"]["str"]="STATE_LOGIN_WAIT";
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
        LLSD captureRequest;
        captureRequest["name"]="MediaPluginFailed";
        captureRequest["substitutions"]["PLUGIN"]="media_plugin_cef";
        const auto requestPath=directory/"capture-request.xml";
        if (std::filesystem::exists(requestPath))
        {
            if (std::filesystem::file_size(requestPath)>65536) throw std::runtime_error("Capture request exceeds fixture budget");
            std::ifstream requestFile(requestPath);
            if (LLSDSerialize::fromXML(captureRequest,requestFile)<=0 || !captureRequest["name"].isString() ||
                captureRequest["name"].asString().empty() || !captureRequest["substitutions"].isMap())
                throw std::runtime_error("Invalid capture notification request");
        }
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
        const bool inspectDialogs=captureRequest["sequence"].asString()=="Dialogs";
        if (inspectDialogs)
        {
            listen["source"]="mainloop";
            listen["listener"]="notification-dialog-geometry";
            send(command,listen);
        }
        LLSD refresh;
        refresh["op"]="postStartupState";
        send("LLStartUp",refresh);
        bool submitted=false;
        bool loginReady=false;
        bool warningDismissed=false;
        const auto anisotropy=std::getenv("LLVK_CAPTURE_ANISOTROPY");
        if (anisotropy && std::string_view(anisotropy)!="0" && std::string_view(anisotropy)!="1")
            throw std::runtime_error("Invalid capture anisotropy policy");
        bool samplingReady=!anisotropy;
        unsigned geometryRequests=0;
        unsigned geometryReplies=0;
        LLSD geometry;
        bool dialogGeometryRequested=false;
        unsigned dialogGeometryRequests=0,dialogGeometryReplies=0;
        LLSD dialogGeometry;
        const bool buttonStates=std::getenv("LLVK_CAPTURE_LOGIN_BUTTON_STATES")!=nullptr;
        std::string loginButtonPath;
        std::string usernamePath,passwordPath,browserPath;
        bool credentialsEntered=false;
        const bool formAction=captureRequest.has("input") || captureRequest["checkIgnore"].asBoolean();
        std::string formPath;
        ViewerPointer formPointer;
        unsigned checkboxChecks=0;
        unsigned warningChecks=0;
        const auto inspectLoginButton=[&]
        {
            if (loginButtonPath.empty()) throw std::runtime_error("Login button path unavailable");
            LLSD request;
            request["op"]="getInfo"; request["path"]=loginButtonPath;
            request["reply"]=reply; request["reqid"]="capture-login-ready";
            send("LLWindow",request);
        };
        const auto warningCheckComplete=[&]
        {
            if (submitted) inspectLoginButton();
            else
            {
                warningDismissed=true;
                send("LLStartUp",refresh);
            }
        };
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
            if (inspectDialogs && !dialogGeometryRequested &&
                std::filesystem::exists(directory/"gl-browser-preferences-settled-0.rgba"))
            {
                dialogGeometryRequested=true;
                LLSD paths;
                paths["op"]="getPaths"; paths["reply"]=reply; paths["reqid"]="capture-dialog-paths";
                paths["under"]="/main_view/menu_stack/world_panel/Floater View";
                send("LLWindow",paths);
            }
            if (data["reqid"].asString()=="capture-dialog-paths")
            {
                for (auto path=data["paths"].beginArray(); path!=data["paths"].endArray(); ++path)
                {
                    const auto text=path->asString();
                    if (text.find("Name_Tag_Preference")==text.npos && text.find("RenderNameShowTime")==text.npos &&
                        text.find("copy_search_slurl_btn")==text.npos) continue;
                    if (++dialogGeometryRequests>64) throw std::runtime_error("Dialog geometry exceeds view budget");
                    LLSD request;
                    request["op"]="getInfo"; request["path"]=text; request["reply"]=reply;
                    request["reqid"]="capture-dialog:"+text;
                    send("LLWindow",request);
                }
            }
            if (data["reqid"].asString().starts_with("capture-dialog:"))
            {
                dialogGeometry.append(data);
                if (++dialogGeometryReplies==dialogGeometryRequests)
                {
                    save(directory,"gl-dialog-geometry.xml",dialogGeometry);
                    return 0;
                }
            }
            if (packet["pump"].asString()=="StartupState" && data["str"].asString()=="STATE_LOGIN_WAIT")
            {
                if (!loginReady && anisotropy)
                {
                    LLSD setting;
                    setting["op"]="set"; setting["group"]="Global"; setting["key"]="RenderAnisotropic";
                    setting["value"]=std::string_view(anisotropy)=="1";
                    setting["reply"]=reply; setting["reqid"]="capture-anisotropy";
                    send("LLViewerControl",setting);
                }
                loginReady=true;
                inspectVisible();
            }
            if (data["reqid"].asString()=="capture-anisotropy")
            {
                if (data.has("error") || !data.has("value") ||
                    data["value"].asBoolean()!=(std::string_view(anisotropy)=="1"))
                    throw std::runtime_error("Reference anisotropy setting was not applied");
                save(directory,"gl-anisotropy.xml",data);
                samplingReady=true;
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
                    LLSD paths;
                    paths["op"]="getPaths"; paths["reply"]=reply; paths["reqid"]="capture-warning-paths";
                    paths["under"]="/main_view/menu_stack/world_panel/Floater View";
                    send("LLWindow",paths);
                }
            }
            if (loginReady && warningDismissed && samplingReady && !submitted)
            {
                LLSD ignore;
                ignore["op"]="ignore"; ignore["name"]=captureRequest["name"]; ignore["ignore"]=false;
                send("LLNotifications",ignore);
                LLSD request;
                request["op"]="requestAdd"; request["name"]=captureRequest["name"];
                request["substitutions"]=captureRequest["substitutions"];
                request["reply"]=reply; request["reqid"]="capture-media-launch";
                send("LLNotifications",request);
                if (!formAction) save(directory,"gl-notification-submitted.xml",request);
                submitted=true;
                LLSD paths;
                paths["op"]="getPaths"; paths["reply"]=reply; paths["reqid"]="capture-layout-paths";
                paths["under"]="/main_view/menu_stack/world_panel/Floater View";
                send("LLWindow",paths);
            }
            if (data["reqid"].asString()=="capture-layout-paths")
            {
                for (const auto* path : {"/main_view/menu_stack/status_bar_container",
                    "/main_view/menu_stack/status_bar_container/menu_bar_holder",
                    "/main_view/menu_stack/status_bar_container/menu_bar_holder/Login Menu",
                    "/main_view/menu_stack/world_panel/login_panel_holder",
                    "/main_view/menu_stack/world_panel/login_panel_holder/panel_login",
                    "/main_view/menu_stack/world_panel/login_panel_holder/panel_login/login_html"})
                {
                    ++geometryRequests;
                    LLSD request;
                    request["op"]="getInfo"; request["path"]=path; request["reply"]=reply;
                    request["reqid"]=std::string("capture-layout:")+path;
                    send("LLWindow",request);
                }
                for (auto path=data["paths"].beginArray(); path!=data["paths"].endArray(); ++path)
                {
                    const auto text=path->asString();
                    if (formAction && (captureRequest.has("input") ? (text.ends_with("/listname") || text.ends_with("/url")) : text.ends_with("/check")))
                    {
                        if (!formPath.empty()) throw std::runtime_error("Ambiguous notification form target");
                        formPath=text;
                        LLSD action;
                        action["path"]=text; action["reply"]=reply; action["reqid"]="capture-form-action";
                        if (captureRequest.has("input"))
                        { action["op"]="pasteText"; action["text"]=captureRequest["input"]; }
                        else
                        {
                            action["op"]="getInfo"; action["reqid"]="capture-check-position";
                        }
                        send("LLWindow",action);
                    }
                    if (!text.ends_with("/toast") && !text.ends_with("/wrapper_panel") &&
                        !text.ends_with("/chiclet_container") && !text.ends_with("/chiclet_container_bottom") &&
                        !text.ends_with("/floater_snap_region") && !text.ends_with("/world_view_rect")) continue;
                    if (++geometryRequests>64) throw std::runtime_error("Layout probe exceeds view budget");
                    LLSD request;
                    request["op"]="getInfo"; request["path"]=text; request["reply"]=reply;
                    request["reqid"]="capture-layout:"+text;
                    send("LLWindow",request);
                }
                if (formAction && formPath.empty()) throw std::runtime_error("Notification form action target missing");
            }
            if (data["reqid"].asString()=="capture-check-position")
            {
                if (data.has("error") || !data["visible_chain"].asBoolean()) throw std::runtime_error("Checkbox position unavailable");
                formPointer.click(data["rect"]["left"].asInteger()+5,data["rect"]["bottom"].asInteger()+7);
                save(directory,"gl-checkbox-win32-input.xml",data);
                LLSD action; action["op"]="getInfo"; action["path"]=formPath;
                action["reply"]=reply; action["reqid"]="capture-form-ready";
                send("LLWindow",action);
            }
            if (data["reqid"].asString()=="capture-form-action")
            {
                if (data.has("error")) throw std::runtime_error("Notification form input failed");
                LLSD action; action["reply"]=reply;
                if (captureRequest["select"].asBoolean())
                {
                    action["op"]="keyDown"; action["keysym"]="HOME"; action["mask"].append("SHIFT");
                    action["reqid"]="capture-form-selection";
                }
                else { action["op"]="getInfo"; action["path"]=formPath; action["reqid"]="capture-form-ready"; }
                send("LLWindow",action);
            }
            if (data["reqid"].asString()=="capture-form-selection")
            {
                if (data.has("error")) throw std::runtime_error("Notification form selection failed");
                LLSD action; action["op"]="keyUp"; action["keysym"]="HOME"; action["mask"].append("SHIFT");
                send("LLWindow",action);
                action=LLSD::emptyMap(); action["op"]="getInfo"; action["path"]=formPath;
                action["reply"]=reply; action["reqid"]="capture-form-ready";
                send("LLWindow",action);
            }
            if (data["reqid"].asString()=="capture-form-ready")
            {
                if (captureRequest["checkIgnore"].asBoolean() && !data.has("error") && !data["value"].asBoolean() && ++checkboxChecks<120)
                {
                    LLSD action; action["op"]="getInfo"; action["path"]=formPath;
                    action["reply"]=reply; action["reqid"]="capture-form-ready";
                    send("LLWindow",action);
                    continue;
                }
                save(directory,"gl-form-state.xml",data);
                if (data.has("error") || !data["visible_chain"].asBoolean() ||
                    (captureRequest.has("input") ? data["value"].asString()!=captureRequest["input"].asString() : !data["value"].asBoolean()))
                    throw std::runtime_error("Notification form state was not applied");
                save(directory,"gl-notification-submitted.xml",captureRequest);
            }
            if (data["reqid"].asString().starts_with("capture-layout:"))
            {
                geometry[data["reqid"].asString()]=data;
                if (++geometryReplies==geometryRequests) save(directory,"gl-layout.xml",geometry);
            }
            if (submitted && data["reqid"].asString()=="capture-media-launch" && data.has("response"))
            {
                save(directory,"gl-notification-response.xml",data);
                if (inspectDialogs) continue;
                if (!buttonStates) return 0;
                LLSD paths;
                paths["op"]="getPaths"; paths["reply"]=reply; paths["reqid"]="capture-login-paths";
                send("LLWindow",paths);
            }
            if (data["reqid"].asString()=="capture-login-paths" || data["reqid"].asString()=="capture-warning-paths")
            {
                for (auto path=data["paths"].beginArray(); path!=data["paths"].endArray(); ++path)
                {
                    const auto text=path->asString();
                    if (text.ends_with("/connect_btn")) loginButtonPath=text;
                    if (text.ends_with("/username_combo/Combo Text Entry")) usernamePath=text;
                    if (text.ends_with("/password_edit")) passwordPath=text;
                    if (text.ends_with("/login_html")) browserPath=text;
                    if (text.ends_with("/Cancel_okcancelbuttons"))
                    {
                        ++warningChecks;
                        LLSD check;
                        check["op"]="getInfo"; check["path"]=text; check["reply"]=reply; check["reqid"]="capture-url-warning-button";
                        send("LLWindow",check);
                    }
                }
                if (!warningChecks) warningCheckComplete();
            }
            if (data["reqid"].asString()=="capture-url-warning-button")
            {
                if (data.has("error")) throw std::runtime_error("Cannot inspect URL-warning control");
                if (data["visible_chain"].asBoolean())
                {
                    LLSD click;
                    click["op"]="mouseDown"; click["button"]="LEFT"; click["path"]=data["path"];
                    const auto scale=captureRequest.has("display") ? captureRequest["display"]["UIScaleFactor"].asReal() : 1.0;
                    const auto& rectangle=data["rect"];
                    click["x"]=static_cast<int>(std::floor((rectangle["left"].asInteger()+rectangle["right"].asInteger())*0.5*scale+0.5));
                    click["y"]=static_cast<int>(std::floor((rectangle["bottom"].asInteger()+rectangle["top"].asInteger())*0.5*scale+0.5));
                    send("LLWindow",click);
                    click["op"]="mouseUp"; click["reply"]=reply; click["reqid"]="capture-url-warning-closed";
                    send("LLWindow",click);
                }
                else if (!--warningChecks) warningCheckComplete();
            }
            if (data["reqid"].asString()=="capture-url-warning-closed")
            {
                if (data.has("error")) throw std::runtime_error("URL-warning dismissal failed");
                LLSD check;
                check["op"]="getInfo"; check["path"]=data["path"]; check["reply"]=reply;
                check["reqid"]="capture-url-warning-verified";
                send("LLWindow",check);
            }
            if (data["reqid"].asString()=="capture-url-warning-verified")
            {
                if (!data.has("error") && data["visible_chain"].asBoolean())
                    throw std::runtime_error("URL-warning control remained visible after dismissal");
                if (!--warningChecks) warningCheckComplete();
            }
            if (data["reqid"].asString()=="capture-login-ready")
            {
                if (data.has("error") || !data["visible_chain"].asBoolean()) throw std::runtime_error("Login button is not visible");
                if (!credentialsEntered)
                {
                    if (usernamePath.empty() || passwordPath.empty() || browserPath.empty()) throw std::runtime_error("Login input paths missing");
                    LLSD paste;
                    paste["op"]="pasteText"; paste["path"]=usernamePath; paste["text"]="fixture-user";
                    paste["reply"]=reply; paste["reqid"]="capture-username-entered";
                    send("LLWindow",paste);
                    continue;
                }
                if (!data["enabled"].asBoolean()) throw std::runtime_error("Synthetic credentials did not enable Log In");
                save(directory,"gl-login-ready.xml",data);
                return 0;
            }
            if (data["reqid"].asString()=="capture-username-entered")
            {
                if (data.has("error")) throw std::runtime_error("Synthetic username entry failed");
                LLSD paste;
                paste["op"]="pasteText"; paste["path"]=passwordPath; paste["text"]="fixture-only";
                paste["reply"]=reply; paste["reqid"]="capture-password-entered";
                send("LLWindow",paste);
            }
            if (data["reqid"].asString()=="capture-password-entered")
            {
                if (data.has("error")) throw std::runtime_error("Synthetic password entry failed");
                LLSD click;
                click["op"]="mouseDown"; click["button"]="LEFT"; click["path"]=browserPath;
                send("LLWindow",click);
                click["op"]="mouseUp"; click["reply"]=reply; click["reqid"]="capture-fields-unfocused";
                send("LLWindow",click);
            }
            if (data["reqid"].asString()=="capture-fields-unfocused")
            {
                if (data.has("error")) throw std::runtime_error("Synthetic fields could not release focus");
                credentialsEntered=true;
                inspectLoginButton();
            }
        }
        return 1;
    }
    catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}