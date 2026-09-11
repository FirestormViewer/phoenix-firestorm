#include "llvkkeybindings.h"
#include "llstring.h"
#include <expat/expat.h>
#include <fstream>
#include <optional>
#include <sstream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#ifdef _WIN32
#include <windows.h>
#endif

namespace
{
    const std::map<std::string,KEY>& keyNames()
    {
        static const auto names=[]
        {
            std::map<std::string,KEY> result{{"SPACE",' '},{"ENTER",KEY_RETURN},{"LEFT",KEY_LEFT},{"RIGHT",KEY_RIGHT},
                {"UP",KEY_UP},{"DOWN",KEY_DOWN},{"ESC",KEY_ESCAPE},{"HOME",KEY_HOME},{"END",KEY_END},{"PGUP",KEY_PAGE_UP},
                {"PGDN",KEY_PAGE_DOWN},{"TAB",KEY_TAB},{"ADD",KEY_ADD},{"SUBTRACT",KEY_SUBTRACT},{"MULTIPLY",KEY_MULTIPLY},
                {"DIVIDE",KEY_DIVIDE},{"PAD_DIVIDE",KEY_PAD_DIVIDE},{"PAD_LEFT",KEY_PAD_LEFT},{"PAD_RIGHT",KEY_PAD_RIGHT},
                {"PAD_DOWN",KEY_PAD_DOWN},{"PAD_UP",KEY_PAD_UP},{"PAD_HOME",KEY_PAD_HOME},{"PAD_END",KEY_PAD_END},
                {"PAD_PGUP",KEY_PAD_PGUP},{"PAD_PGDN",KEY_PAD_PGDN},{"PAD_CENTER",KEY_PAD_CENTER},{"PAD_INS",KEY_PAD_INS},
                {"PAD_DEL",KEY_PAD_DEL},{"PAD_ENTER",KEY_PAD_RETURN},{"BACKSP",KEY_BACKSPACE},{"DEL",KEY_DELETE},
                {"SHIFT",KEY_SHIFT},{"CTRL",KEY_CONTROL},{"ALT",KEY_ALT},{"INS",KEY_INSERT},{"CAPSLOCK",KEY_CAPSLOCK}};
            for (int index=0; index<12; ++index) result["F"+std::to_string(index+1)]=static_cast<KEY>(KEY_F1+index);
            for (int index=0; index<16; ++index) result["PAD_BUTTON"+std::to_string(index)]=static_cast<KEY>(KEY_BUTTON0+index);
            return result;
        }();
        return names;
    }
    const std::map<std::string,MASK> maskNames{{"NONE",0},{"CTL",MASK_CONTROL},{"SHIFT",MASK_SHIFT},{"ALT",MASK_ALT},
        {"CTL_SHIFT",MASK_CONTROL|MASK_SHIFT},{"ALT_SHIFT",MASK_ALT|MASK_SHIFT},{"CTL_ALT",MASK_CONTROL|MASK_ALT},
        {"CTL_ALT_SHIFT",MASK_CONTROL|MASK_ALT|MASK_SHIFT}};
    const std::map<std::string,EMouseClickType> mouseNames{{"LMB",CLICK_LEFT},{"Double LMB",CLICK_DOUBLELEFT},{"MMB",CLICK_MIDDLE},
        {"MB4",CLICK_BUTTON4},{"MB5",CLICK_BUTTON5}};
    const std::map<std::string,LLVKKeyBindings::Mode> modeNames{{"first_person",LLVKKeyBindings::Mode::FirstPerson},
        {"third_person",LLVKKeyBindings::Mode::ThirdPerson},{"edit_avatar",LLVKKeyBindings::Mode::EditAvatar},{"sitting",LLVKKeyBindings::Mode::Sitting}};
}

bool LLVKKeyBindings::load(std::string_view xml,std::string& error)
{
    error.clear();
    if (xml.size()>4*1024*1024) { error="Native keybinding document exceeds limit"; return false; }
    struct Parser
    {
        XML_Parser parser=XML_ParserCreate(nullptr);
        LLVKKeyBindings result;
        Controls controls;
        std::optional<Mode> mode;
        std::array<bool,4> seen{};
        int depth=0;
        std::size_t count=0;
        std::string failure;
        ~Parser() { if (parser) XML_ParserFree(parser); }
        void fail(const char* message) { failure=message; XML_StopParser(parser,XML_FALSE); }
        static void XMLCALL start(void* pointer,const char* tag,const char** attributes)
        {
            auto& state=*static_cast<Parser*>(pointer);
            try
            {
                ++state.depth;
                std::map<std::string,std::string> fields;
                for (std::size_t index=0; attributes[index]; index+=2) fields.emplace(attributes[index],attributes[index+1]);
                if (state.depth==1)
                {
                    if (std::string_view(tag)!="keys" || (fields.contains("xml_version") && fields["xml_version"]!="0" && fields["xml_version"]!="1"))
                        state.fail("Invalid native keybinding root or version");
                    return;
                }
                if (state.depth==2)
                {
                    const auto mode=modeNames.find(tag);
                    if (mode==modeNames.end() || state.seen[static_cast<std::size_t>(mode->second)])
                    { state.fail("Unknown or duplicate native binding mode"); return; }
                    state.mode=mode->second; state.seen[static_cast<std::size_t>(*state.mode)]=true; state.controls.clear();
                    return;
                }
                if (state.depth!=3 || std::string_view(tag)!="binding" || !state.mode || ++state.count>10000 || fields["command"].empty())
                { state.fail("Invalid native keybinding entry"); return; }
                LLKeyData data;
                data.mIgnoreMasks=true;
                auto key=fields["key"]; LLStringUtil::toUpper(key);
                if (!key.empty() && key!="NONE")
                {
                    if (key.size()==1 && key.front()>='!' && key.front()<='~') data.mKey=static_cast<KEY>(key.front());
                    else if (const auto found=keyNames().find(key); found!=keyNames().end()) data.mKey=found->second;
                    else { state.fail("Unknown native binding key name"); return; }
                }
                const auto mask=maskNames.find(fields["mask"]);
                if (mask==maskNames.end()) { state.fail("Unknown native binding modifier name"); return; }
                data.mMask=mask->second;
                if (fields.contains("mouse"))
                {
                    const auto mouse=mouseNames.find(fields["mouse"]);
                    if (mouse==mouseNames.end()) { state.fail("Unknown native binding mouse name"); return; }
                    data.mMouse=mouse->second;
                }
                for (const auto& [name,value] : fields)
                    if (name!="key" && name!="mask" && name!="mouse" && name!="command")
                    { state.fail("Unknown native binding attribute"); return; }
                state.controls[fields["command"]].binding.addKeyData(data);
            }
            catch (...) { state.fail("Native keybinding parse allocation failed"); }
        }
        static void XMLCALL end(void* pointer,const char*)
        {
            auto& state=*static_cast<Parser*>(pointer);
            try
            {
                if (state.depth==2 && state.mode) { state.result.setControls(*state.mode,std::move(state.controls)); state.mode.reset(); }
                --state.depth;
            }
            catch (...) { state.fail("Native keybinding mode allocation failed"); }
        }
        static void XMLCALL doctype(void* pointer,const char*,const char*,const char*,int)
        { static_cast<Parser*>(pointer)->fail("Native keybinding DTD is not allowed"); }
    } state;
    if (!state.parser) { error="Native keybinding parser allocation failed"; return false; }
    XML_SetUserData(state.parser,&state); XML_SetElementHandler(state.parser,Parser::start,Parser::end);
    XML_SetStartDoctypeDeclHandler(state.parser,Parser::doctype);
    if (XML_Parse(state.parser,xml.data(),static_cast<int>(xml.size()),XML_TRUE)!=XML_STATUS_OK || !state.failure.empty())
    { error=state.failure.empty() ? "Invalid native keybinding XML" : state.failure; return false; }
    *this=std::move(state.result);
    return true;
}

bool LLVKKeyBindings::loadFile(const std::filesystem::path& path,std::string& error)
{
    error.clear();
    std::ifstream input(path,std::ios::binary|std::ios::ate);
    if (!input || input.tellg()<0 || input.tellg()>4*1024*1024) { error="Cannot read native keybinding file"; return false; }
    std::string xml(static_cast<std::size_t>(input.tellg()),'\0'); input.seekg(0);
    if (!input.read(xml.data(),xml.size())) { error="Native keybinding read failed"; return false; }
    return load(xml,error);
}

std::optional<std::string> LLVKKeyBindings::serialize(std::string& error) const
{
    error.clear();
    boost::property_tree::ptree document,root;
    root.put("<xmlattr>.xml_version",1);
    for (const auto& [modeName,mode] : modeNames)
    {
        boost::property_tree::ptree contents;
        for (const auto& [command,control] : controls(mode))
        {
            if (!control.assignable || control.binding.empty()) continue;
            auto binding=control.binding; binding.trimEmpty();
            for (std::uint32_t index=0; index<binding.getDataCount(); ++index)
            {
                const auto data=binding.getKeyData(index);
                boost::property_tree::ptree entry;
                std::string key;
                if (data.mKey!=KEY_NONE)
                {
                    const auto named=std::find_if(keyNames().begin(),keyNames().end(),[&](const auto& item) { return item.second==data.mKey; });
                    if (named!=keyNames().end()) key=named->first;
                    else if (data.mKey>='!' && data.mKey<='~') key=std::string(1,static_cast<char>(data.mKey));
                    else { error="Native binding key cannot be serialized"; return std::nullopt; }
                }
                entry.put("<xmlattr>.key",key);
                const auto mask=std::find_if(maskNames.begin(),maskNames.end(),[&](const auto& item) { return item.second==data.mMask; });
                if (mask==maskNames.end()) { error="Native binding modifier cannot be serialized"; return std::nullopt; }
                entry.put("<xmlattr>.mask",mask->first);
                if (data.mMouse!=CLICK_NONE)
                {
                    const auto mouse=std::find_if(mouseNames.begin(),mouseNames.end(),[&](const auto& item) { return item.second==data.mMouse; });
                    if (mouse==mouseNames.end()) { error="Native binding mouse cannot be serialized"; return std::nullopt; }
                    entry.put("<xmlattr>.mouse",mouse->first);
                }
                entry.put("<xmlattr>.command",command);
                contents.add_child("binding",entry);
            }
        }
        root.add_child(modeName,contents);
    }
    document.add_child("keys",root);
    std::ostringstream output;
    boost::property_tree::write_xml(output,document,boost::property_tree::xml_writer_make_settings<std::string>(' ',2));
    if (!output || output.str().size()>4*1024*1024) { error="Native binding output exceeds limit"; return std::nullopt; }
    return output.str();
}

bool LLVKKeyBindings::saveFile(const std::filesystem::path& path,std::string& error) const
{
    const auto xml=serialize(error);
    if (!xml) return false;
    std::error_code status;
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path(),status);
    if (status) { error="Cannot create native binding directory"; return false; }
    auto staging=path; staging+=".native-write";
    if (!std::filesystem::create_directory(staging,status)) { error="Cannot acquire native binding staging directory"; return false; }
    struct Cleanup
    {
        std::filesystem::path directory;
        ~Cleanup() { std::error_code ignored; std::filesystem::remove(directory/"bindings.tmp",ignored); std::filesystem::remove(directory,ignored); }
    } cleanup{staging};
    const auto temporary=staging/"bindings.tmp";
    std::ofstream output(temporary,std::ios::binary|std::ios::trunc);
    if (!output) { error="Cannot open native binding staging file"; return false; }
    output << *xml; output.close();
    if (!output) { error="Native binding write failed"; return false; }
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
    { error="Native binding replacement failed: "+std::to_string(GetLastError()); return false; }
#else
    std::filesystem::rename(temporary,path,status);
    if (status) { error="Native binding replacement failed: "+status.message(); return false; }
#endif
    return true;
}

std::string LLVKKeyBindings::label(const LLKeyData& data,const std::function<std::string(const std::string&)>& translate)
{
    std::string result;
    if (data.mMask&MASK_CONTROL) result+=translate("accel-win-control");
    if (data.mMask&MASK_ALT) result+=translate("accel-win-alt");
    if (data.mMask&MASK_SHIFT) result+=translate("accel-win-shift");
    if (data.mKey!=KEY_NONE)
    {
        std::string key;
        const auto found=std::find_if(keyNames().begin(),keyNames().end(),[&](const auto& entry) { return entry.second==data.mKey; });
        if (found!=keyNames().end())
        {
            key=found->first;
            static const std::map<std::string,std::string> display{{"SPACE","Space"},{"ENTER","Enter"},{"LEFT","Left"},
                {"RIGHT","Right"},{"UP","Up"},{"DOWN","Down"},{"ESC","Esc"},{"HOME","Home"},{"END","End"},
                {"PGUP","PgUp"},{"PGDN","PgDn"},{"TAB","Tab"},{"ADD","Add"},{"SUBTRACT","Subtract"},
                {"MULTIPLY","Multiply"},{"DIVIDE","Divide"},{"BACKSP","Backsp"},{"DEL","Del"},
                {"SHIFT","Shift"},{"CTRL","Ctrl"},{"ALT","Alt"},{"INS","Ins"},{"CAPSLOCK","CapsLock"},{"PAD_ENTER","PAD_Enter"}};
            if (const auto named=display.find(key); named!=display.end()) key=named->second;
        }
        else key.assign(1,static_cast<char>(data.mKey));
        if (data.mMask && (key=="-" || key=="=" || key=="+")) result+=' ';
        result+=translate(key);
    }
    for (const auto& [name,mouse] : mouseNames) if (mouse==data.mMouse) result+=translate(name);
    if (data.mMouse==CLICK_RIGHT) result+=translate("RMB");
    return result;
}

void LLVKKeyBindings::setControls(Mode mode,Controls controls)
{
    const auto reserve=[&](const char* name) { controls[name].assignable=false; controls[name].conflicts=0; };
    if (mode==Mode::FirstPerson)
        for (const auto name : {"look_up","look_down","move_forward","move_backward","move_forward_fast","move_backward_fast",
            "spin_over","spin_under","pan_up","pan_down","pan_left","pan_right","pan_in","pan_out","spin_around_ccw","spin_around_cw",
            "roll_left","roll_right","edit_avatar_spin_ccw","edit_avatar_spin_cw","edit_avatar_spin_over","edit_avatar_spin_under",
            "edit_avatar_move_forward","edit_avatar_move_backward","walk_to","teleport_to"}) reserve(name);
    if (mode==Mode::EditAvatar) { reserve("walk_to"); reserve("teleport_to"); }
    if (mode==Mode::Sitting) reserve("walk_to");
    else
        for (const auto name : {"move_forward_sitting","move_backward_sitting","spin_over_sitting","spin_under_sitting",
            "spin_around_ccw_sitting","spin_around_cw_sitting"}) reserve(name);
    controls["script_trigger_lbutton"].conflicts=UINT32_MAX & ~(1u<<1);
    mControls.at(static_cast<std::size_t>(mode))=std::move(controls);
}

bool LLVKKeyBindings::assign(Mode mode,const std::string& command,std::uint32_t index,const LLKeyData& data,const Reservation& reserved)
{
    if (command.empty() || index>=10000) return false;
    auto& controls=mControls.at(static_cast<std::size_t>(mode));
    const auto current=controls.find(command);
    if (current!=controls.end() && !current->second.assignable) return false;
    if (current!=controls.end() && current->second.binding.getKeyData(index)==data) return true;
    if (reserved && reserved(data)) return false;
    auto conflictMask=current==controls.end() ? UINT32_MAX : current->second.conflicts;
    if (data.mMouse==CLICK_LEFT && data.mKey==KEY_NONE && data.mMask==MASK_NONE) conflictMask&=(1u<<1);
    else conflictMask&=~(1u<<1);
    std::map<std::string,int> displaced;
    for (const auto& [name,control] : controls)
    {
        if (!(control.conflicts&conflictMask)) continue;
        const auto conflict=control.binding.findKeyData(data);
        if (conflict<0) continue;
        if (!control.assignable) return false;
        displaced[name]=conflict;
    }
    auto updated=controls;
    for (const auto& [name,slot] : displaced) updated.at(name).binding.resetKeyData(slot);
    updated[command].binding.replaceKeyData(data,index);
    controls=std::move(updated);
    return true;
}

bool LLVKKeyBindings::clear(Mode mode,const std::string& command,std::uint32_t index)
{
    if (command.empty() || index>=10000) return false;
    auto& controls=mControls.at(static_cast<std::size_t>(mode));
    const auto found=controls.find(command);
    if (found!=controls.end() && !found->second.assignable) return false;
    controls[command].binding.resetKeyData(static_cast<int>(index));
    return true;
}

bool LLVKKeyBindings::handles(Mode mode,const std::string& command,EMouseClickType mouse,KEY key,MASK mask) const
{
    const auto& controls=this->controls(mode);
    const auto found=controls.find(command);
    return found!=controls.end() && found->second.binding.canHandle(mouse,key,mask);
}

bool LLVKKeyBindings::setClickAction(const std::string& command,EMouseClickType mouse,bool enabled)
{
    const auto mode=Mode::ThirdPerson;
    const bool recorded=handles(mode,command,mouse,KEY_NONE,MASK_NONE);
    if (enabled==recorded) return true;
    auto& binding=mControls.at(static_cast<std::size_t>(mode))[command].binding;
    if (enabled)
    {
        std::uint32_t slot=0;
        for (std::uint32_t index=0; index<3; ++index)
            if (binding.getKeyData(index).isEmpty()) { slot=index; break; }
        return assign(mode,command,slot,LLKeyData(mouse,KEY_NONE,MASK_NONE,true));
    }
    for (std::uint32_t index=0; index<3; ++index)
    {
        const auto data=binding.getKeyData(index);
        if (data.mMouse==mouse && data.mKey==KEY_NONE && data.mMask==MASK_NONE && !clear(mode,command,index)) return false;
    }
    return true;
}