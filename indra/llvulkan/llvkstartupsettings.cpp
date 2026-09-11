#include "llvkstartupsettings.h"
#include "llsdserialize.h"
#include <fstream>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#endif

namespace
{
    LLSD comparable(const std::string& type,const LLSD& value)
    {
        if (type == "Boolean") return LLSD(value.asString() == "0" ? false : value.asBoolean());
        if (type == "S32" || type == "U32") return LLSD(value.asInteger());
        if (type == "F32") return LLSD(value.asReal());
        if (type == "String") return LLSD(value.asString());
        return value;
    }
}

bool LLVKStartupSettings::load(std::string_view xml,bool defaults,bool saved,std::string& error)
{
    error.clear();
    if (xml.size() > 16*1024*1024) { error = "Native settings document exceeds byte limit"; return false; }
    std::istringstream stream{std::string(xml)};
    LLSD document;
    if (LLSDSerialize::fromXML(document,stream,false) == LLSDParser::PARSE_FAILURE || !document.isMap())
    { error = "Invalid native LLSD settings document"; return false; }
    auto entries = mEntries;
    for (auto item = document.beginMap(); item != document.endMap(); ++item)
    {
        const auto& name = item->first;
        const auto& definition = item->second;
        if (name.empty() || !definition.isMap() || !definition.has("Value"))
        { error = "Invalid native settings entry: "+name; return false; }
        auto found = entries.find(name);
        const auto type = definition["Type"].asString();
        if (found == entries.end())
        {
            if (type.empty()) { error = "Native settings entry has no type: "+name; return false; }
            Entry entry;
            entry.type = type;
            entry.definition = definition;
            entry.defaultValue = comparable(type,definition["Value"]);
            entry.persistent = !defaults || !definition.has("Persist") || definition["Persist"].asInteger() != 0;
            entries.emplace(name,std::move(entry));
        }
        else if (defaults)
        {
            if (found->second.type != type) { error = "Native settings type mismatch: "+name; return false; }
            found->second.defaultValue = comparable(type,definition["Value"]);
            found->second.definition = definition;
            found->second.saved.reset();
            found->second.transient.reset();
            found->second.persistent = !definition.has("Persist") || definition["Persist"].asInteger() != 0;
        }
        else if (found->second.persistent)
        {
            const auto value = comparable(found->second.type,definition["Value"]);
            if (saved) { found->second.saved = value; found->second.transient.reset(); }
            else found->second.transient = value;
        }
    }
    if (entries.size() > 20000) { error = "Native settings entry limit exceeded"; return false; }
    mEntries = std::move(entries);
    return true;
}

bool LLVKStartupSettings::loadFile(const std::filesystem::path& path,bool required,bool defaults,bool saved,std::string& error)
{
    error.clear();
    std::ifstream stream(path,std::ios::binary | std::ios::ate);
    if (!stream)
    {
        if (required) error = "Required native settings file is unavailable";
        return !required;
    }
    const auto size = stream.tellg();
    if (size < 0 || size > 16*1024*1024) { error = "Native settings file exceeds byte limit"; return false; }
    std::string xml(static_cast<std::size_t>(size),'\0');
    stream.seekg(0);
    if (!stream.read(xml.data(),size)) { error = "Native settings file read failed"; return false; }
    return load(xml,defaults,saved,error);
}

bool LLVKStartupSettings::set(const std::string& name,const LLSD& value,bool saved,std::string& error)
{
    error.clear();
    const auto found = mEntries.find(name);
    if (found == mEntries.end()) { error = "Unknown native setting: "+name; return false; }
    if (saved) { found->second.saved = comparable(found->second.type,value); found->second.transient.reset(); }
    else found->second.transient = comparable(found->second.type,value);
    return true;
}

const LLVKStartupSettings::Entry* LLVKStartupSettings::find(const std::string& name) const
{
    const auto found = mEntries.find(name);
    return found == mEntries.end() ? nullptr : &found->second;
}

bool LLVKStartupSettings::saveChanges(const std::filesystem::path& path,const std::map<std::string,LLSD>& changes,std::string& error)
{
    error.clear();
    auto updated = *this;
    for (const auto& [name,value] : changes)
    {
        const auto* entry = find(name);
        if (!entry || !entry->persistent) { error = "Native preference is not persistent: "+name; return false; }
        if (!updated.set(name,value,true,error)) return false;
    }
    if (changes.empty()) return true;
    std::error_code status;
    std::filesystem::create_directories(path.parent_path(),status);
    if (status) { error = "Native settings directory cannot be created: "+status.message(); return false; }
    auto staging = path;
    staging += ".native-write";
    if (!std::filesystem::create_directory(staging,status))
    { error = "Native settings update cannot acquire its staging directory"; return false; }
    struct Cleanup
    {
        std::filesystem::path directory;
        ~Cleanup() { std::error_code ignored; std::filesystem::remove(directory/"settings.tmp",ignored); std::filesystem::remove(directory,ignored); }
    } cleanup{staging};
    LLSD document = LLSD::emptyMap();
    const bool exists = std::filesystem::exists(path,status);
    if (status) { error = "Native settings file status failed: "+status.message(); return false; }
    if (exists)
    {
        std::ifstream input(path,std::ios::binary|std::ios::ate);
        if (!input || input.tellg() < 0 || input.tellg() > 16*1024*1024)
        { error = "Native settings file cannot be read safely"; return false; }
        input.seekg(0);
        if (LLSDSerialize::fromXML(document,input,false) == LLSDParser::PARSE_FAILURE || !document.isMap())
        { error = "Existing native settings file is invalid; it was not overwritten"; return false; }
    }
    for (const auto& [name,value] : changes)
    {
        const auto* entry = updated.find(name);
        if (!document[name].isMap()) document[name] = entry->definition;
        document[name]["Type"] = entry->type;
        document[name]["Value"] = entry->saveValue();
    }
    const auto temporary = staging/"settings.tmp";
    std::ofstream output(temporary,std::ios::binary|std::ios::trunc);
    if (!output) { error = "Native settings staging file cannot be opened"; return false; }
    LLSDSerialize::toPrettyXML(document,output);
    output.close();
    if (!output) { error = "Native settings write failed"; return false; }
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
    { error = "Native settings replacement failed: "+std::to_string(GetLastError()); return false; }
#else
    std::filesystem::rename(temporary,path,status);
    if (status) { error = "Native settings replacement failed: "+status.message(); return false; }
#endif
    mEntries = std::move(updated.mEntries);
    return true;
}

std::map<std::string,LLSD> LLVKStartupSettings::values() const
{
    std::map<std::string,LLSD> values;
    for (const auto& [name,entry] : mEntries) values.emplace(name,entry.value());
    return values;
}