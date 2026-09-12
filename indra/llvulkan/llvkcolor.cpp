#include "llvkcolor.h"
#include "llsd.h"
#include "lluuid.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <fstream>
#include <iomanip>
#include <limits>
#if LL_WINDOWS
#include <windows.h>
#endif

#include <algorithm>
#include <cmath>
#include <charconv>
#include <exception>
#include <set>

#if __has_include(<expat.h>)
#include <expat.h>
#else
#include <expat/expat.h>
#endif

namespace
{
    struct ColorDeclaration
    {
        std::string name;
        std::optional<LLVKColor::Value> value;
        std::optional<std::string> reference;
    };

    struct ColorParser
    {
        XML_Parser parser = nullptr;
        unsigned depth = 0;
        bool root = false;
        std::vector<ColorDeclaration> declarations;
        std::vector<std::string> warnings;
        std::string error;
        std::exception_ptr exception;
        ~ColorParser() { if (parser) XML_ParserFree(parser); }
        void reject(const std::string& reason) { error = reason; XML_StopParser(parser,XML_FALSE); }
        template<class Operation> void guarded(Operation operation) noexcept
        {
            try { operation(); }
            catch (...) { exception = std::current_exception(); XML_StopParser(parser,XML_FALSE); }
        }
        static void XMLCALL start(void* pointer, const XML_Char* name, const XML_Char** attributes)
        {
            auto& state = *static_cast<ColorParser*>(pointer);
            state.guarded([&]
            {
                const std::string_view tag(name);
                if (!state.depth)
                {
                    if (tag != "colors" || state.root || attributes[0])
                    { state.reject("Invalid native colors root"); return; }
                    state.root = true;
                }
                else if (state.depth == 1 && tag == "color")
                {
                    if (state.declarations.size() >= 10000)
                    { state.reject("Native colors exceed declaration budget"); return; }
                    ColorDeclaration declaration;
                    bool invalidValue = false;
                    for (std::size_t index = 0; attributes[index]; index += 2)
                    {
                        const std::string_view attribute(attributes[index]);
                        std::string_view text(attributes[index+1]);
                        if (attribute == "name") declaration.name = text;
                        else if (attribute == "reference") declaration.reference = text;
                        else if (attribute == "value")
                        {
                            LLVKColor::Value value{0,0,0,1};
                            unsigned components = 0;
                            for (auto& channel : value)
                            {
                                const auto start = text.find_first_not_of(" \t\r\n");
                                if (start == std::string_view::npos) break;
                                text.remove_prefix(start);
                                float parsed = channel;
                                const auto result = std::from_chars(text.data(),text.data()+text.size(),parsed);
                                if (result.ec != std::errc()) break;
                                if (!std::isfinite(parsed)) { invalidValue = true; break; }
                                channel = parsed;
                                ++components;
                                text.remove_prefix(result.ptr-text.data());
                            }
                            if (components >= 3 && !invalidValue) declaration.value = value;
                            else invalidValue = true;
                        }
                        else { state.reject("Unsupported native color attribute"); return; }
                    }
                    if (invalidValue && !declaration.reference)
                    {
                        state.warnings.push_back("Skipped invalid native color value: " + declaration.name);
                        ++state.depth;
                        return;
                    }
                    if (declaration.name.empty() || declaration.value.has_value() == declaration.reference.has_value())
                    { state.reject("Native color requires name and exactly one value or reference"); return; }
                    state.declarations.push_back(std::move(declaration));
                }
                else { state.reject("Unexpected native color element"); return; }
                ++state.depth;
            });
        }
        static void XMLCALL end(void* pointer, const XML_Char*) { --static_cast<ColorParser*>(pointer)->depth; }
        static void XMLCALL text(void* pointer, const XML_Char* text, int length)
        {
            auto& state = *static_cast<ColorParser*>(pointer);
            state.guarded([&]
            {
                if (std::string_view(text,length).find_first_not_of(" \t\r\n") != std::string_view::npos)
                    state.reject("Native colors do not accept text content");
            });
        }
        static void XMLCALL doctype(void* pointer,const XML_Char*,const XML_Char*,const XML_Char*,int)
        {
            auto& state = *static_cast<ColorParser*>(pointer);
            state.guarded([&] { state.reject("Native colors do not allow DTDs or external entities"); });
        }
    };
}

bool LLVKColorTable::load(std::string_view xml, Layer layer, std::vector<std::string>& warnings, std::string& error)
{
    error.clear();
    warnings.clear();
    if (xml.size() > 4 * 1024 * 1024 || (layer != Layer::Loaded && layer != Layer::User))
    { error = "Invalid native color declaration size or layer"; return false; }
    ColorParser state;
    state.parser = XML_ParserCreate(nullptr);
    if (!state.parser) { error = "Native color parser allocation failed"; return false; }
    XML_SetUserData(state.parser,&state);
    XML_SetElementHandler(state.parser,ColorParser::start,ColorParser::end);
    XML_SetCharacterDataHandler(state.parser,ColorParser::text);
    XML_SetStartDoctypeDeclHandler(state.parser,ColorParser::doctype);
    XML_SetParamEntityParsing(state.parser,XML_PARAM_ENTITY_PARSING_NEVER);
    const auto status = XML_Parse(state.parser,xml.data(),static_cast<int>(xml.size()),XML_TRUE);
    if (state.exception) std::rethrow_exception(state.exception);
    if (status != XML_STATUS_OK || !state.root)
    {
        error = state.error.empty() ? "Invalid native colors XML" : state.error;
        error += " at line " + std::to_string(XML_GetCurrentLineNumber(state.parser));
        return false;
    }
    std::map<std::string,LLVKColor::Value> loaded, user;
    warnings = std::move(state.warnings);
    for (const auto& [name,slot] : mLoaded) loaded.emplace(name,slot->value);
    for (const auto& [name,slot] : mUser) user.emplace(name,slot->value);
    auto& values = layer == Layer::Loaded ? loaded : user;
    std::map<std::string,std::string> references;
    for (const auto& declaration : state.declarations)
    {
        if (declaration.value) values.insert_or_assign(declaration.name,*declaration.value);
        else references.emplace(declaration.name,*declaration.reference);
    }
    while (!references.empty())
    {
        std::set<std::string> visited;
        std::string current = references.begin()->first;
        std::optional<LLVKColor::Value> resolved;
        for (;;)
        {
            const auto next = references.find(current);
            if (next == references.end())
            {
                const auto literal = loaded.find(current);
                if (literal != loaded.end()) resolved = literal->second;
                else warnings.push_back("Native color reference missing: " + current);
                break;
            }
            if (!visited.insert(current).second)
            { warnings.push_back("Native color reference cycle: " + current); break; }
            current = next->second;
        }
        for (const auto& name : visited)
        {
            if (resolved) loaded.insert_or_assign(name,*resolved);
            references.erase(name);
        }
    }
    if (loaded.size()+user.size() > 20000)
    { error = "Native color table exceeds entry budget"; return false; }
    auto nextLoaded = mLoaded;
    auto nextUser = mUser;
    std::vector<std::pair<std::shared_ptr<LLVKColor::Slot>,LLVKColor::Value>> updates;
    const auto prepare = [&](auto& destination, const auto& source)
    {
        for (const auto& [name,color] : source)
        {
            const auto found = destination.find(name);
            if (found == destination.end()) destination.emplace(name,std::make_shared<LLVKColor::Slot>(LLVKColor::Slot{color}));
            else updates.emplace_back(found->second,color);
        }
    };
    prepare(nextLoaded,loaded);
    prepare(nextUser,user);
    mLoaded.swap(nextLoaded);
    mUser.swap(nextUser);
    for (const auto& [slot,color] : updates) slot->value = color;
    return true;
}

std::optional<std::string> LLVKColorTable::serializeUser(std::string& error) const
{
    error.clear();
    try
    {
        boost::property_tree::ptree document,colors;
        for (const auto& [name,slot] : mUser)
        {
            const auto original=mLoaded.find(name);
            if (original!=mLoaded.end() && original->second->value==slot->value) continue;
            boost::property_tree::ptree entry;
            entry.put("<xmlattr>.name",name);
            std::ostringstream channels; channels.imbue(std::locale::classic());
            channels<<std::setprecision(std::numeric_limits<float>::max_digits10);
            for (std::size_t index=0; index<slot->value.size(); ++index)
            { if (index) channels<<' '; channels<<slot->value[index]; }
            entry.put("<xmlattr>.value",channels.str());
            colors.add_child("color",entry);
        }
        document.add_child("colors",colors);
        std::ostringstream output;
        boost::property_tree::write_xml(output,document);
        const auto xml=output.str();
        if (xml.size()>4*1024*1024) { error="Native user colors exceed document limit"; return {}; }
        return xml;
    }
    catch (const std::exception&) { error="Cannot serialize native user colors"; return {}; }
}

bool LLVKColorTable::saveUserFile(const std::filesystem::path& path,std::string& error) const
{
    const auto xml=serializeUser(error);
    if (!xml) return false;
    try
    {
        if (!path.is_absolute() || path.filename().empty())
        { error="Native user colors require an absolute unlinked path"; return false; }
        for (auto component=path.lexically_normal(); !component.empty();)
        {
#if LL_WINDOWS
            const auto attributes=GetFileAttributesW(component.c_str());
            if (attributes==INVALID_FILE_ATTRIBUTES)
            {
                const auto failure=GetLastError();
                if (failure!=ERROR_FILE_NOT_FOUND && failure!=ERROR_PATH_NOT_FOUND)
                { error="Cannot inspect native user color destination"; return false; }
            }
            else if (attributes&FILE_ATTRIBUTE_REPARSE_POINT)
            { error="Native user colors require an absolute unlinked path"; return false; }
#else
            std::error_code status;
            const auto type=std::filesystem::symlink_status(component,status);
            if (status && status!=std::errc::no_such_file_or_directory)
            { error="Cannot inspect native user color destination"; return false; }
            if (std::filesystem::is_symlink(type))
            { error="Native user colors require an absolute unlinked path"; return false; }
#endif
            const auto parent=component.parent_path();
            if (parent==component) break;
            component=parent;
        }
        std::filesystem::create_directories(path.parent_path());
        const auto temporary=path.parent_path()/(".native-colors-"+LLUUID::generateNewID().asString());
        if (!std::filesystem::create_directory(temporary)) { error="Cannot stage native user colors"; return false; }
        struct Cleanup
        {
            std::filesystem::path path;
            ~Cleanup() { std::error_code ignored; std::filesystem::remove(path/"colors.xml",ignored); std::filesystem::remove(path,ignored); }
        } cleanup{temporary};
        const auto pending=temporary/"colors.xml";
        std::ofstream output(pending,std::ios::binary|std::ios::trunc);
        output.write(xml->data(),xml->size()); output.close();
        if (!output) { error="Cannot write native user colors"; return false; }
#if LL_WINDOWS
        if (!MoveFileExW(pending.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        { error="Cannot replace native user colors"; return false; }
#else
        std::filesystem::rename(pending,path);
#endif
        return true;
    }
    catch (const std::filesystem::filesystem_error&) { error="Cannot persist native user colors"; return false; }
}

bool LLVKColorTable::loadUserFile(const std::filesystem::path& path,std::string& error)
{
    error.clear();
    std::error_code status;
    if (!std::filesystem::exists(path,status))
    { if (status) error="Cannot inspect native user colors"; return !status; }
    std::ifstream input(path,std::ios::binary|std::ios::ate);
    if (!input || input.tellg()<0 || input.tellg()>4*1024*1024)
    { error="Cannot read native user colors or document exceeds limit"; return false; }
    std::string xml(static_cast<std::size_t>(input.tellg()),'\0'); input.seekg(0);
    if (!input.read(xml.data(),xml.size())) { error="Cannot read native user colors"; return false; }
    std::vector<std::string> warnings;
    return load(xml,Layer::User,warnings,error);
}

bool LLVKColorTable::setRuntime(const std::string& name,LLVKColor::Value color)
{
    if (name.empty() || !std::all_of(color.begin(),color.end(),[](float channel) { return std::isfinite(channel); })) return false;
    auto& slot=mRuntime[name];
    if (!slot) slot=std::make_shared<LLVKColor::Slot>(LLVKColor::Slot{color});
    else slot->value=color;
    return true;
}

std::vector<std::string> LLVKColorTable::names() const
{
    std::set<std::string> names;
    for (const auto& [name,color] : mLoaded) names.insert(name);
    for (const auto& [name,color] : mUser) names.insert(name);
    return {names.begin(),names.end()};
}

bool LLVKColorTable::define(const std::string& name, LLVKColor::Value color)
{
    if (name.empty() || !std::all_of(color.begin(),color.end(),[](float channel) { return std::isfinite(channel); })) return false;
    return mLoaded.emplace(name,std::make_shared<LLVKColor::Slot>(LLVKColor::Slot{color})).second;
}

bool LLVKColorTable::set(const std::string& name, LLVKColor::Value color)
{
    if (name.empty() || !std::all_of(color.begin(),color.end(),[](float channel) { return std::isfinite(channel); })) return false;
    const auto user = mUser.find(name);
    if (user != mUser.end()) user->second->value = color;
    else
    {
        const auto loaded = mLoaded.find(name);
        if (loaded == mLoaded.end()) mUser.emplace(name,std::make_shared<LLVKColor::Slot>(LLVKColor::Slot{color}));
        else
        {
            auto original = std::make_shared<LLVKColor::Slot>(*loaded->second);
            auto slot = loaded->second;
            mUser.emplace(name,slot);
            loaded->second = std::move(original);
            slot->value = color;
        }
    }
    return true;
}

std::optional<LLVKColor> LLVKColorTable::find(const std::string& name) const
{
    if (const auto found=mRuntime.find(name); found!=mRuntime.end())
    {
        LLVKColor color;
        color.mReference=found->second;
        return color;
    }
    const auto user = mUser.find(name);
    const auto loaded = mLoaded.find(name);
    if (user == mUser.end() && loaded == mLoaded.end()) return std::nullopt;
    LLVKColor color;
    color.mReference = user != mUser.end() ? user->second : loaded->second;
    return color;
}

bool LLVKColorTable::isDefault(const std::string& name) const
{
    const auto loaded = mLoaded.find(name);
    const auto user = mUser.find(name);
    if (loaded == mLoaded.end()) return user != mUser.end();
    return user == mUser.end() || user->second->value == loaded->second->value;
}

bool LLVKColorTable::resetToDefault(const std::string& name)
{
    const auto loaded = mLoaded.find(name);
    const auto user = mUser.find(name);
    if (loaded == mLoaded.end() || user == mUser.end()) return false;
    user->second->value = loaded->second->value;
    return true;
}

void LLVKColorTable::clear()
{
    for (auto& [name,slot] : mLoaded) slot->value = {1,0,1,1};
    for (auto& [name,slot] : mUser) slot->value = {1,0,1,1};
    for (auto& [name,slot] : mRuntime) slot->value = {1,0,1,1};
}