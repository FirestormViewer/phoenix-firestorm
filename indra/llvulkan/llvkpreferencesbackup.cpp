#include "llvkpreferencesbackup.h"
#include "llsdserialize.h"
#include "lluuid.h"
#include <fstream>
#include <sstream>
#if LL_WINDOWS
#include <windows.h>
#endif

namespace
{
    bool unlinkedPath(const std::filesystem::path& path,std::string& error)
    {
        if (!path.is_absolute()) { error="Backup paths must be absolute"; return false; }
        for (auto component=path; !component.empty();)
        {
#if LL_WINDOWS
            const auto attributes=GetFileAttributesW(component.c_str());
            if (attributes!=INVALID_FILE_ATTRIBUTES && (attributes&FILE_ATTRIBUTE_REPARSE_POINT))
            { error="Linked backup paths are not allowed"; return false; }
            if (attributes==INVALID_FILE_ATTRIBUTES)
            {
                const auto failure=GetLastError();
                if (failure!=ERROR_FILE_NOT_FOUND && failure!=ERROR_PATH_NOT_FOUND)
                { error="Cannot inspect backup path"; return false; }
            }
#else
            std::error_code status;
            const auto type=std::filesystem::symlink_status(component,status);
            if (status && status!=std::errc::no_such_file_or_directory) { error="Cannot inspect backup path"; return false; }
            if (std::filesystem::is_symlink(type)) { error="Linked backup paths are not allowed"; return false; }
#endif
            const auto parent=component.parent_path(); if (parent==component) break; component=parent;
        }
        return true;
    }

    bool relativeEntry(const std::string& value)
    {
        if (value.empty() || value.find_first_of(":\\")!=std::string::npos || value.find('\0')!=std::string::npos) return false;
        const auto path=std::filesystem::path(std::u8string(value.begin(),value.end()));
        if (path.is_absolute() || path.has_root_path()) return false;
        for (const auto& component : path) if (component==".." || component=="." || component.empty()) return false;
        return true;
    }
}

LLSD LLVKPreferencesBackup::settings(LLControlGroup& group,bool backupOnly)
{
    struct Export final : LLControlGroup::ApplyFunctor
    {
        LLSD document=LLSD::emptyMap();
        bool backupOnly=true;
        void apply(const std::string& name,LLControlVariable* control) override
        {
            if (control->isDefault() || !control->isPersisted() || (backupOnly && !control->isBackupable())) return;
            document[name]["Type"]=LLControlGroup::typeEnumToString(control->type());
            document[name]["Comment"]=control->getComment();
            document[name]["Persist"]=1;
            document[name]["Value"]=control->getValue();
        }
    } exported;
    exported.backupOnly=backupOnly;
    group.applyToAll(&exported);
    return exported.document;
}

std::optional<LLSD> LLVKPreferencesBackup::restoredSettings(const std::filesystem::path& defaults,const std::filesystem::path& backup,
    const std::map<std::string,LLSD>& recommended,std::string& error)
{
    error.clear();
    if (recommended.empty()) { error="Global settings restore requires native hardware recommendations"; return {}; }
    if (!unlinkedPath(backup,error)) return {};
    LLVKSettingsMgr restored;
    if (!restored.loadFile(defaults,true,true,true,error)) return {};
    for (const auto& [name,value] : recommended) if (!restored.set(name,value,true,error)) return {};
    if (!restored.loadFile(backup,true,false,true,error) || !restored.set("FSFirstRunAfterSettingsRestore",LLSD(true),true,error)) return {};
    return settings(restored.group(),false);
}

bool LLVKPreferencesBackup::copy(const std::filesystem::path& source,const std::filesystem::path& destination,
    const std::vector<std::string>& files,const std::vector<std::string>& folders,
    const std::map<std::string,LLSD>& generated,std::string& error)
{
    error.clear();
    if (!unlinkedPath(source,error) || !unlinkedPath(destination,error)) return false;
    try
    {
        const auto sourceRoot=std::filesystem::weakly_canonical(source);
        const auto destinationRoot=std::filesystem::weakly_canonical(destination);
        const auto contains=[](const auto& parent,const auto& child)
        {
            const auto relative=child.lexically_relative(parent);
            return !relative.empty() && (relative=="." || *relative.begin()!="..");
        };
        if (contains(sourceRoot,destinationRoot) || contains(destinationRoot,sourceRoot))
        { error="Backup source and destination must not overlap"; return false; }
        std::map<std::filesystem::path,std::string> outputs;
        std::uintmax_t total=0;
        const auto add=[&](const std::string& relative) -> bool
        {
            if (!relativeEntry(relative)) { error="Invalid backup entry path"; return false; }
            const auto item=std::filesystem::path(std::u8string(relative.begin(),relative.end()));
            const auto input=source/item,output=destination/item;
            if (!unlinkedPath(input,error) || !unlinkedPath(output,error)) return false;
            if (!std::filesystem::exists(input)) return true;
            if (!std::filesystem::is_regular_file(input)) { error="Backup file entry is not a regular file"; return false; }
            const auto size=std::filesystem::file_size(input);
            if (size>64*1024*1024 || total+size>256*1024*1024 || outputs.size()>=10000)
            { error="Backup selection exceeds file or byte limit"; return false; }
            std::ifstream stream(input,std::ios::binary); std::string bytes(static_cast<std::size_t>(size),'\0');
            if (!stream.read(bytes.data(),bytes.size())) { error="Cannot read backup file"; return false; }
            total+=size; outputs[item]=std::move(bytes); return true;
        };
        for (const auto& file : files) if (!add(file)) return false;
        std::vector<std::string> selectedFolders;
        for (const auto& folder : folders)
        {
            if (!relativeEntry(folder)) { error="Invalid backup folder path"; return false; }
            if (folder=="presets") { selectedFolders.push_back("presets/graphic"); selectedFolders.push_back("presets/camera"); }
            else selectedFolders.push_back(folder);
        }
        for (const auto& folder : selectedFolders)
        {
            const auto path=source/std::filesystem::path(std::u8string(folder.begin(),folder.end()));
            if (!unlinkedPath(path,error)) return false;
            if (!std::filesystem::exists(path)) continue;
            if (!std::filesystem::is_directory(path)) { error="Backup folder entry is not a directory"; return false; }
            for (const auto& entry : std::filesystem::directory_iterator(path))
            {
                if (!unlinkedPath(entry.path(),error)) return false;
                if (!entry.is_regular_file()) continue;
                const auto name=entry.path().filename().u8string();
                if (!add(folder+"/"+std::string(name.begin(),name.end()))) return false;
            }
        }
        for (const auto& [name,document] : generated)
        {
            if (!relativeEntry(name)) { error="Invalid generated backup filename"; return false; }
            const auto relative=std::filesystem::path(std::u8string(name.begin(),name.end()));
            if (!unlinkedPath(destination/relative,error)) return false;
            std::ostringstream output; LLSDSerialize::toPrettyXML(document,output);
            auto bytes=output.str(); total+=bytes.size();
            if (total>256*1024*1024) { error="Backup settings exceed byte limit"; return false; }
            outputs[relative]=std::move(bytes);
        }
        for (const auto& [relative,bytes] : outputs)
        {
            const auto target=destination/relative;
            if (std::filesystem::exists(target) && !std::filesystem::is_regular_file(target))
            { error="Backup destination file is not a regular file"; return false; }
        }
        std::filesystem::create_directories(destination);
        const auto staging=destination/(".native-backup-"+LLUUID::generateNewID().asString());
        if (!std::filesystem::create_directory(staging)) { error="Cannot stage backup"; return false; }
        struct Cleanup
        {
            std::filesystem::path directory;
            std::vector<std::filesystem::path> files;
            ~Cleanup() { std::error_code ignored; for (const auto& file : files) std::filesystem::remove(file,ignored); std::filesystem::remove(directory,ignored); }
        } cleanup{staging,{}};
        for (const auto& [relative,bytes] : outputs)
        {
            const auto temporary=staging/std::to_string(cleanup.files.size()); cleanup.files.push_back(temporary);
            std::ofstream output(temporary,std::ios::binary); output.write(bytes.data(),bytes.size()); output.close();
            if (!output) { error="Cannot stage backup file"; return false; }
        }
        std::size_t index=0;
        for (const auto& [relative,bytes] : outputs)
        {
            const auto target=destination/relative;
            if (!unlinkedPath(target,error)) return false;
            std::filesystem::create_directories(target.parent_path());
#if LL_WINDOWS
            if (!MoveFileExW(cleanup.files[index].c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
            { error="Cannot publish backup file; earlier files may have been copied"; return false; }
#else
            std::filesystem::rename(cleanup.files[index],target);
#endif
            ++index;
        }
        return true;
    }
    catch (const std::filesystem::filesystem_error&) { error="Backup filesystem operation failed"; return false; }
}