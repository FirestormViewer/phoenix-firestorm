#include "llvkgraphicpresets.h"
#include "llsdserialize.h"
#include "lluri.h"
#include "llstring.h"
#include "lluuid.h"
#include <fstream>
#include <sstream>
#if LL_WINDOWS
#include <windows.h>
#endif

namespace
{
    std::optional<LLSD> readPresetDocument(const std::filesystem::path& path,std::string& error)
    {
        std::ifstream input(path,std::ios::binary|std::ios::ate);
        if (!input || input.tellg()<0 || input.tellg()>4*1024*1024)
        { error="Cannot read graphics preset document or document exceeds limit"; return {}; }
        const auto size=static_cast<std::size_t>(input.tellg());
        std::string xml(size,'\0'); input.seekg(0);
        if (!input.read(xml.data(),size)) { error="Cannot read graphics preset document"; return {}; }
        std::istringstream stream(xml); LLSD document;
        if (LLSDSerialize::fromXML(document,stream)<=0) { error="Invalid graphics preset XML"; return {}; }
        return document;
    }
}

bool LLVKGraphicPresets::initialize(const std::filesystem::path& appSettings,const std::filesystem::path& directory,std::string& error)
{
    error.clear();
    if (!directory.is_absolute()) { error="Graphics preset directory must be absolute"; return false; }
    const auto names=readPresetDocument(appSettings/"graphic_preset_controls.xml",error);
    if (!names || !names->isArray() || names->size()>4096)
    { if (error.empty()) error="Invalid graphics preset control list"; return false; }
    if (!mSchema.loadFile(appSettings/"settings.xml",true,true,true,error)) return false;
    std::set<std::string> controls;
    for (auto item=names->beginArray(); item!=names->endArray(); ++item)
    {
        if (!item->isString()) { error="Invalid graphics preset control name"; return false; }
        if (mSchema.find(item->asString())) controls.insert(item->asString());
    }
    mControls=std::move(controls); mDirectory=directory;
    return true;
}

std::optional<std::filesystem::path> LLVKGraphicPresets::path(const std::string& name,bool writable,std::string& error) const
{
    error.clear();
    auto upper=name; LLStringUtil::toUpper(upper);
    if (mDirectory.empty() || name.empty() || name.size()>128 || name.find_first_of("/\\:")!=std::string::npos ||
        name.find('\0')!=std::string::npos || (writable && upper=="DEFAULT"))
    { error="Invalid or protected graphics preset name"; return {}; }
    const auto encoded=LLURI::escape(name)+".xml";
    const auto result=mDirectory/std::filesystem::path(std::u8string(encoded.begin(),encoded.end()));
    for (auto component=result; !component.empty();)
    {
        std::error_code status;
#if LL_WINDOWS
        const auto attributes=GetFileAttributesW(component.c_str());
        if (attributes!=INVALID_FILE_ATTRIBUTES && (attributes&FILE_ATTRIBUTE_REPARSE_POINT))
        { error="Linked graphics preset paths are not allowed"; return {}; }
        if (attributes==INVALID_FILE_ATTRIBUTES)
        {
            const auto failure=GetLastError();
            if (failure!=ERROR_FILE_NOT_FOUND && failure!=ERROR_PATH_NOT_FOUND)
            { error="Cannot inspect graphics preset path"; return {}; }
        }
#else
        const auto type=std::filesystem::symlink_status(component,status);
        if (status && status!=std::errc::no_such_file_or_directory)
        { error="Cannot inspect graphics preset path"; return {}; }
        if (std::filesystem::is_symlink(type)) { error="Linked graphics preset paths are not allowed"; return {}; }
#endif
        const auto parent=component.parent_path(); if (parent==component) break; component=parent;
    }
    return result;
}

std::optional<std::vector<std::string>> LLVKGraphicPresets::names(bool includeDefault,std::string& error) const
{
    error.clear(); std::vector<std::string> result;
    if (!path("catalog-check",false,error)) return {};
    try
    {
        if (!std::filesystem::exists(mDirectory)) return result;
        for (const auto& entry : std::filesystem::directory_iterator(mDirectory))
        {
            if (!entry.is_regular_file() || entry.path().extension()!=".xml") continue;
            const auto bytes=entry.path().stem().u8string();
            const auto name=LLURI::unescape(std::string(bytes.begin(),bytes.end()));
            if (!includeDefault && name=="Default") continue;
            const auto checked=path(name,false,error);
            if (!checked) return {};
            if (checked->filename()!=entry.path().filename()) continue;
            result.push_back(name);
            if (result.size()>4096) { error="Graphics preset catalog exceeds limit"; return {}; }
        }
        std::sort(result.begin(),result.end());
        if (includeDefault)
            if (const auto found=std::find(result.begin(),result.end(),"Default"); found!=result.end()) std::rotate(result.begin(),found,found+1);
        return result;
    }
    catch (const std::filesystem::filesystem_error&) { error="Cannot enumerate graphics presets"; return {}; }
}

bool LLVKGraphicPresets::save(const std::string& name,const std::map<std::string,LLSD>& values,std::string& error) const
{
    return write(name,values,false,error);
}

bool LLVKGraphicPresets::createDefault(const std::map<std::string,LLSD>& values,std::string& error) const
{
    return write("Default",values,true,error);
}

bool LLVKGraphicPresets::write(const std::string& name,const std::map<std::string,LLSD>& values,bool createDefault,std::string& error) const
{
    const auto target=path(name,!createDefault,error); if (!target) return false;
    std::error_code status;
    if (createDefault && std::filesystem::exists(*target,status))
    {
        if (!std::filesystem::is_regular_file(*target,status) || status) { error="Default preset is not a regular file"; return false; }
        return true;
    }
    if (status) { error="Cannot inspect Default preset"; return false; }
    LLSD document=LLSD::emptyMap();
    auto controls=mControls;
    if (createDefault) for (const auto& [name,value] : values) if (mSchema.find(name)) controls.insert(name);
    for (const auto& controlName : controls)
    {
        const auto found=values.find(controlName); if (found==values.end()) continue;
        auto control=mSchema.find(controlName);
        document[controlName]["Comment"]=control->getComment();
        document[controlName]["Type"]=LLControlGroup::typeEnumToString(control->type());
        document[controlName]["Persist"]=1;
        document[controlName]["Value"]=found->second;
    }
    if (document.size()<2) { error="No graphics settings available to save"; return false; }
    try
    {
        std::filesystem::create_directories(mDirectory);
        const auto staging=mDirectory/(".native-preset-"+LLUUID::generateNewID().asString());
        if (!std::filesystem::create_directory(staging)) { error="Cannot stage graphics preset"; return false; }
        struct Cleanup { std::filesystem::path directory; ~Cleanup() { std::error_code ignored; std::filesystem::remove(directory/"preset.xml",ignored); std::filesystem::remove(directory,ignored); } } cleanup{staging};
        const auto temporary=staging/"preset.xml";
        std::ofstream output(temporary,std::ios::binary); LLSDSerialize::toPrettyXML(document,output); output.close();
        if (!output) { error="Cannot write graphics preset"; return false; }
#if LL_WINDOWS
        if (!MoveFileExW(temporary.c_str(),target->c_str(),(createDefault ? 0 : MOVEFILE_REPLACE_EXISTING)|MOVEFILE_WRITE_THROUGH))
        { error="Cannot publish graphics preset"; return false; }
#else
        std::filesystem::rename(temporary,*target);
#endif
        return true;
    }
    catch (const std::filesystem::filesystem_error&) { error="Cannot save graphics preset"; return false; }
}

std::optional<std::map<std::string,LLSD>> LLVKGraphicPresets::load(const std::string& name,std::string& error) const
{
    const auto target=path(name,false,error); if (!target) return {};
    const auto document=readPresetDocument(*target,error);
    if (!document || !document->isMap() || document->size()>4096)
    { if (error.empty()) error="Invalid graphics preset settings"; return {}; }
    LLSD selected=LLSD::emptyMap();
    for (auto entry=document->beginMap(); entry!=document->endMap(); ++entry)
    {
        if (!mControls.contains(entry->first)) continue;
        auto control=mSchema.find(entry->first);
        if (!entry->second.isMap() || !entry->second.has("Value") ||
            entry->second["Type"].asString()!=LLControlGroup::typeEnumToString(control->type()))
        { error="Graphics preset setting type mismatch: "+entry->first; return {}; }
        selected[entry->first]=entry->second;
    }
    if (selected.size()==0) { error="Graphics preset contains no supported settings"; return {}; }
    std::ostringstream output; LLSDSerialize::toXML(selected,output);
    LLVKSettingsMgr staged;
    if (!staged.load(output.str(),true,true,error)) return {};
    auto values=staged.values();
    for (const auto setting : {"RenderSkyAutoAdjustLegacy","RenderSkyAmbientScale"})
        if (const auto control=mSchema.find(setting)) values[setting]=control->getDefault();
    return values;
}

bool LLVKGraphicPresets::remove(const std::string& name,std::string& error) const
{
    const auto target=path(name,true,error); if (!target) return false;
    std::error_code status;
    if (!std::filesystem::is_regular_file(*target,status) || !std::filesystem::remove(*target,status) || status)
    { error="Cannot delete graphics preset"; return false; }
    return true;
}