#include "llvkstartupsettings.h"
#include "llsdserialize.h"
#include <fstream>
#include <sstream>

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
            entry.defaultValue = comparable(type,definition["Value"]);
            entry.persistent = !defaults || !definition.has("Persist") || definition["Persist"].asInteger() != 0;
            entries.emplace(name,std::move(entry));
        }
        else if (defaults)
        {
            if (found->second.type != type) { error = "Native settings type mismatch: "+name; return false; }
            found->second.defaultValue = comparable(type,definition["Value"]);
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

std::map<std::string,LLSD> LLVKStartupSettings::values() const
{
    std::map<std::string,LLSD> values;
    for (const auto& [name,entry] : mEntries) values.emplace(name,entry.value());
    return values;
}