#include "llvksettingsmgr.h"
#include "llsdserialize.h"
#include "llstring.h"
#include "llsdutil.h"
#include "lluuid.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#endif

std::optional<std::filesystem::path> LLVKSettingsMgr::isolatedProfile(const std::filesystem::path& base,
    std::string_view name,std::string& error)
{
    error.clear();
    if (!base.is_absolute() || name.empty() || name.size()>48 ||
        !std::all_of(name.begin(),name.end(),[](char character)
        { return (character>='a' && character<='z') || (character>='0' && character<='9') || character=='-' || character=='_'; }))
    { error="Invalid isolated native profile name"; return {}; }
    const auto path=base/"native_profiles"/("profile-"+std::string(name));
    for (auto component=path; !component.empty();)
    {
        std::error_code status;
        const auto type=std::filesystem::symlink_status(component,status);
        if ((status && status!=std::errc::no_such_file_or_directory) || std::filesystem::is_symlink(type))
        { error="Cannot use linked or inaccessible native profile path"; return {}; }
#ifdef _WIN32
        const auto attributes=GetFileAttributesW(component.c_str());
        if (attributes!=INVALID_FILE_ATTRIBUTES && (attributes&FILE_ATTRIBUTE_REPARSE_POINT))
        { error="Cannot use linked native profile path"; return {}; }
#endif
        const auto parent=component.parent_path();
        if (parent==component) break;
        component=parent;
    }
    return path;
}

namespace
{
    bool browserCachePathUnlinked(const std::filesystem::path& path,std::string& error)
    {
        for (auto component=path; !component.empty();)
        {
#ifdef _WIN32
            const auto attributes=GetFileAttributesW(component.c_str());
            if (attributes!=INVALID_FILE_ATTRIBUTES && (attributes&FILE_ATTRIBUTE_REPARSE_POINT))
            { error="Native browser cache contains a linked path"; return false; }
            if (attributes==INVALID_FILE_ATTRIBUTES)
            {
                const auto failure=GetLastError();
                if (failure!=ERROR_FILE_NOT_FOUND && failure!=ERROR_PATH_NOT_FOUND)
                { error="Cannot inspect native browser cache path"; return false; }
            }
#else
            std::error_code status;
            const auto type=std::filesystem::symlink_status(component,status);
            if (status && status!=std::errc::no_such_file_or_directory)
            { error="Cannot inspect native browser cache path"; return false; }
            if (std::filesystem::is_symlink(type)) { error="Native browser cache contains a linked path"; return false; }
#endif
            const auto parent=component.parent_path();
            if (parent==component) break;
            component=parent;
        }
        return true;
    }

    std::optional<LLSD> readAutoReplaceDocument(const std::filesystem::path& path,std::string& error)
    {
        error.clear();
        std::ifstream input(path,std::ios::binary|std::ios::ate);
        if (!input || input.tellg()<0 || input.tellg()>16*1024*1024)
        { error="AutoReplace file cannot be read safely"; return std::nullopt; }
        input.seekg(0);
        LLSD document;
        if (LLSDSerialize::fromXML(document,input,false)==LLSDParser::PARSE_FAILURE)
        { error="Invalid AutoReplace LLSD document"; return std::nullopt; }
        return document;
    }

    bool writeAutoReplaceDocument(const std::filesystem::path& path,const LLSD& document,std::string& error)
    {
        error.clear();
        std::error_code status;
        if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path(),status);
        if (status) { error="Cannot create AutoReplace directory: "+status.message(); return false; }
        auto staging=path; staging+=".native-write";
        if (!std::filesystem::create_directory(staging,status))
        { error="Cannot acquire AutoReplace staging directory"; return false; }
        struct Cleanup
        {
            std::filesystem::path directory;
            ~Cleanup() { std::error_code ignored; std::filesystem::remove(directory/"settings.tmp",ignored); std::filesystem::remove(directory,ignored); }
        } cleanup{staging};
        const auto temporary=staging/"settings.tmp";
        std::ofstream output(temporary,std::ios::binary|std::ios::trunc);
        if (!output) { error="Cannot open AutoReplace staging file"; return false; }
        LLSDSerialize::toPrettyXML(document,output);
        output.close();
        if (!output) { error="AutoReplace file write failed"; return false; }
#ifdef _WIN32
        if (!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        { error="AutoReplace replacement failed: "+std::to_string(GetLastError()); return false; }
#else
        std::filesystem::rename(temporary,path,status);
        if (status) { error="AutoReplace replacement failed: "+status.message(); return false; }
#endif
        return true;
    }

}

bool LLVKAutoReplaceSettings::loadFile(const std::filesystem::path& path,std::string& error)
{
    const auto document=readAutoReplaceDocument(path,error);
    if (!document) return false;
    if (!set(*document)) { error="Invalid AutoReplace settings list array"; return false; }
    return true;
}

bool LLVKAutoReplaceSettings::saveFile(const std::filesystem::path& path,std::string& error) const
{
    return writeAutoReplaceDocument(path,mLists,error);
}

std::optional<LLSD> LLVKAutoReplaceSettings::readListFile(const std::filesystem::path& path,std::string& error)
{
    auto document=readAutoReplaceDocument(path,error);
    if (!document || !validList(*document))
    { if (error.empty()) error="Invalid AutoReplace list"; return std::nullopt; }
    return document;
}

bool LLVKAutoReplaceSettings::writeListFile(const std::filesystem::path& path,const LLSD& list,std::string& error)
{
    if (!validList(list)) { error="Invalid AutoReplace list"; return false; }
    return writeAutoReplaceDocument(path,list,error);
}

bool LLVKAutoReplaceSettings::validList(const LLSD& list)
{
    if (!list.isMap() || !list["name"].isString() || list["name"].asString().empty() || !list["replacements"].isMap()) return false;
    const auto& entries=list["replacements"];
    return std::all_of(entries.beginMap(),entries.endMap(),[](const auto& entry) { return entry.second.isString(); });
}

bool LLVKAutoReplaceSettings::set(const LLSD& lists)
{
    if (!lists.isArray() || !std::all_of(lists.beginArray(),lists.endArray(),[](const auto& list)
        { return !list.isDefined() || validList(list); })) return false;
    mLists=lists;
    return true;
}

const LLSD* LLVKAutoReplaceSettings::find(const std::string& name) const
{
    for (auto item=mLists.beginArray(); item!=mLists.endArray(); ++item)
    {
        const auto& list=*item;
        if (list.isMap() && list["name"].asString()==name) return &list;
    }
    return nullptr;
}

LLVKAutoReplaceSettings::AddResult LLVKAutoReplaceSettings::add(const LLSD& list,bool replace)
{
    if (!validList(list)) return AddResult::InvalidList;
    const auto name=list["name"].asString();
    for (int index=0; index<mLists.size(); ++index)
    {
        if (!mLists[index].isMap() || mLists[index]["name"].asString()!=name) continue;
        if (!replace) return AddResult::DuplicateName;
        mLists[index]=list;
        return AddResult::Added;
    }
    if (replace) return AddResult::InvalidList;
    mLists.append(list);
    return AddResult::Added;
}

bool LLVKAutoReplaceSettings::remove(const std::string& name)
{
    for (int index=0; index<mLists.size(); ++index)
        if (mLists[index].isMap() && mLists[index]["name"].asString()==name)
        { mLists.erase(index); return true; }
    return false;
}

bool LLVKAutoReplaceSettings::move(const std::string& name,bool up)
{
    int previous=-1;
    for (int index=0; index<mLists.size(); ++index)
    {
        if (!mLists[index].isMap()) continue;
        if (mLists[index]["name"].asString()!=name) { previous=index; continue; }
        if (up)
        {
            if (previous>=0)
            { const auto list=mLists[index]; mLists.erase(index); mLists.insert(previous,list); }
        }
        else
        {
            for (int next=index+1; next<mLists.size(); ++next)
                if (mLists[next].isMap())
                { const auto list=mLists[next]; mLists.erase(next); mLists.insert(index,list); break; }
        }
        return true;
    }
    return false;
}

bool LLVKAutoReplaceSettings::setEntry(const std::string& name,const std::string& keyword,const std::string& replacement)
{
    const auto wide=utf8str_to_wstring(keyword);
    if (wide.empty() || replacement.empty() || !std::all_of(wide.begin(),wide.end(),[](auto character)
        { return LLWStringUtil::isPartOfWord(character) || character=='(' || character==')' || character=='.' || character==',' || character=='-' || character=='_'; })) return false;
    for (auto item=mLists.beginArray(); item!=mLists.endArray(); ++item)
    {
        auto& list=*item;
        if (list.isMap() && list["name"].asString()==name)
        { list["replacements"][keyword]=replacement; return true; }
    }
    return false;
}

bool LLVKAutoReplaceSettings::removeEntry(const std::string& name,const std::string& keyword)
{
    for (auto item=mLists.beginArray(); item!=mLists.endArray(); ++item)
    {
        auto& list=*item;
        if (list.isMap() && list["name"].asString()==name)
        { list["replacements"].erase(keyword); return true; }
    }
    return false;
}

std::string LLVKAutoReplaceSettings::replaceWord(const std::string& word,bool enabled) const
{
    if (enabled)
        for (auto item=mLists.beginArray(); item!=mLists.endArray(); ++item)
        {
            const auto& list=*item;
            if (list["replacements"].has(word)) return list["replacements"][word].asString();
        }
    return word;
}

LLVKSettingsMgr::LLVKSettingsMgr()
    : mGroup(std::make_shared<LLControlGroup>("NativeSettings-"+LLUUID::generateNewID().asString()))
{
}

LLVKSettingsMgr::LLVKSettingsMgr(LLControlGroup& group)
    : mGroup(&group,[](LLControlGroup*) {})
{
}

bool LLVKSettingsMgr::load(std::string_view xml,bool defaults,bool saved,std::string& error)
{
    error.clear();
    if (xml.size() > 16*1024*1024) { error = "Native settings document exceeds byte limit"; return false; }
    std::istringstream stream{std::string(xml)};
    LLSD document;
    if (LLSDSerialize::fromXML(document,stream,false) == LLSDParser::PARSE_FAILURE || !document.isMap())
    { error = "Invalid native LLSD settings document"; return false; }
    if (document.size()>20000) { error="Native settings entry limit exceeded"; return false; }
    for (auto item = document.beginMap(); item != document.endMap(); ++item)
    {
        const auto& name = item->first;
        auto& definition = document[name];
        if (name.empty() || !definition.isMap() || !definition.has("Value"))
        { error = "Invalid native settings entry: "+name; return false; }
        if (definition["Type"].asString()=="Integer") definition["Type"]="S32";
        const auto type=LLControlGroup::typeStringToEnum(definition["Type"].asString());
        if (type<0 || type>=TYPE_COUNT) { error="Invalid native settings type: "+name; return false; }
        auto existing=mGroup->getControl(name);
        if (defaults && existing && !existing->isType(type)) { error="Native settings type mismatch: "+name; return false; }
        if (definition["Comment"].asString().empty()) definition["Comment"]="Imported setting: "+name;
    }
    return mGroup->loadFromLLSD(document,"native settings document",defaults,saved)==document.size();
}

bool LLVKSettingsMgr::loadFile(const std::filesystem::path& path,bool required,bool defaults,bool saved,std::string& error)
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

bool LLVKSettingsMgr::consumeBrowserCacheClear(const std::filesystem::path& profile,const std::filesystem::path& settingsFile,std::string& error)
{
    error.clear();
    const auto requested=find("FSStartupClearBrowserCache");
    if (!requested || !requested->getValue().asBoolean()) return true;
    if (!clearBrowserCache(profile,error)) return false;
    return saveChanges(settingsFile,{{"FSStartupClearBrowserCache",LLSD(false)}},error);
}

bool LLVKSettingsMgr::clearBrowserCache(const std::filesystem::path& profile,std::string& error)
{
    error.clear();
    if (!profile.is_absolute() || profile==profile.root_path() || profile.lexically_normal()!=profile)
    { error="Native browser cache requires an absolute profile directory"; return false; }
    const auto cache=profile/"native_browser";
    if (!browserCachePathUnlinked(cache,error)) return false;
    try
    {
        if (!std::filesystem::exists(cache)) return true;
        if (!std::filesystem::is_directory(cache)) { error="Native browser cache is not a directory"; return false; }
        std::vector<std::filesystem::path> paths{cache};
        for (std::size_t index=0; index<paths.size(); ++index)
        {
            if (!std::filesystem::is_directory(paths[index])) continue;
            for (const auto& entry : std::filesystem::directory_iterator(paths[index]))
            {
                if (paths.size()>=100000) { error="Native browser cache exceeds entry limit"; return false; }
                if (!browserCachePathUnlinked(entry.path(),error)) return false;
                const auto relative=entry.path().lexically_relative(cache);
                if (std::distance(relative.begin(),relative.end())>64) { error="Native browser cache exceeds depth limit"; return false; }
                if (!entry.is_regular_file() && !entry.is_directory()) { error="Unexpected native browser cache entry"; return false; }
                paths.push_back(entry.path());
            }
        }
        for (auto entry=paths.rbegin(); entry!=paths.rend(); ++entry)
        {
            if (!browserCachePathUnlinked(*entry,error)) return false;
            std::error_code status;
            std::filesystem::remove(*entry,status);
            if (status) { error="Cannot remove native browser cache entry; request retained for retry"; return false; }
        }
        return true;
    }
    catch (const std::filesystem::filesystem_error&) { error="Cannot clear native browser cache; request retained for retry"; return false; }
}

bool LLVKSettingsMgr::scheduleReset(const std::filesystem::path& profile,std::string& error)
{
    error.clear();
    if (profile.empty() || !profile.is_absolute()) { error="Native settings reset requires an absolute profile path"; return false; }
    std::error_code status;
    std::filesystem::create_directories(profile/"logs",status);
    if (status) { error="Cannot create native reset marker directory: "+status.message(); return false; }
    std::ofstream marker(profile/"logs"/"CLEAR",std::ios::binary|std::ios::app);
    marker.close();
    if (!marker) { error="Cannot create native settings reset marker"; return false; }
    return true;
}

std::optional<bool> LLVKSettingsMgr::consumeReset(const std::filesystem::path& profile,
    const std::filesystem::path& selectedSettings,std::string& error)
{
    error.clear();
    if (profile.empty() || !profile.is_absolute()) { error="Native settings reset requires an absolute profile path"; return std::nullopt; }
    try
    {
        const auto root=std::filesystem::weakly_canonical(profile);
        const auto marker=root/"logs"/"CLEAR",settings=root/"user_settings";
        if (!std::filesystem::exists(marker)) return false;
        const auto selected=selectedSettings.lexically_normal();
        if (selected.empty() || selected.is_absolute() || selected.has_parent_path())
        { error="Native settings reset requires a profile-local settings filename"; return std::nullopt; }
        std::vector<std::filesystem::path> files{settings/selected},directories;
        const auto verify=[&](const std::filesystem::path& path)
        {
            if (std::filesystem::weakly_canonical(path)!=path.lexically_normal())
                throw std::runtime_error("Native settings reset refuses linked paths");
        };
        verify(marker); verify(settings);
        for (const auto name : {"account_settings_phoenix.xml","agents.xml","bin_conf.dat","client_list_v2.xml","colors.xml",
            "ignorable_dialogs.xml","grids.remote.xml","grids.user.xml","password.dat","quick_preferences.xml","releases.xml","settings_crash_behavior.xml"})
            files.push_back(settings/name);
        for (const auto name : {"beams","beamsColors","windlight/water","windlight/days","windlight/skies","windlight"})
            directories.push_back(settings/name);
        if (std::filesystem::exists(settings))
            for (const auto& entry : std::filesystem::directory_iterator(settings))
            {
                const auto name=entry.path().filename().string();
                if (((name.starts_with("feature") || name.starts_with("gpu")) && name.ends_with(".txt")) ||
                    (name.starts_with("settings_") && name.ends_with(".xml"))) files.push_back(entry.path());
                if (files.size()>20000) throw std::runtime_error("Native settings reset file limit exceeded");
            }
        directories.push_back(root/"browser_profile"); directories.push_back(root/"data");
        for (const auto& entry : std::filesystem::directory_iterator(root))
        {
            if (!entry.is_directory()) continue;
            verify(entry.path());
            for (const auto name : {"filters.xml","medialist.xml","plugin_cookies.xml","search_history.xml","settings_friends_groups.xml",
                "settings_per_account.xml","teleport_history.xml","texture_list_last.xml","toolbars.xml","typed_locations.xml","url_history.xml","volume_settings.xml"})
                files.push_back(entry.path()/name);
            directories.push_back(entry.path()/"browser_profile");
            if (files.size()>20000) throw std::runtime_error("Native settings reset file limit exceeded");
        }
        for (const auto& path : files) verify(path);
        for (const auto& path : directories) verify(path);
        for (const auto& path : files)
        {
            if (std::filesystem::is_directory(path)) throw std::runtime_error("Native settings reset file is a directory");
            std::filesystem::remove(path);
        }
        for (const auto& path : directories)
            if (std::filesystem::is_directory(path) && std::filesystem::is_empty(path)) std::filesystem::remove(path);
        std::filesystem::remove(marker);
        return true;
    }
    catch (const std::exception& failure) { error=std::string("Native settings reset failed: ")+failure.what(); return std::nullopt; }
}

bool LLVKSettingsMgr::set(const std::string& name,const LLSD& value,bool saved,std::string& error)
{
    error.clear();
    auto control=mGroup->getControl(name);
    if (!control) { error="Unknown native setting: "+name; return false; }
    control->setValue(value,saved);
    return true;
}

std::map<std::string,LLSD> LLVKSettingsMgr::defaults() const
{
    struct Collect final : LLControlGroup::ApplyFunctor
    {
        std::map<std::string,LLSD> result;
        void apply(const std::string& name,LLControlVariable* control) override
        { result.emplace(name,control->isType(TYPE_BOOLEAN) ? LLSD(control->getDefault().asBoolean()) : control->getDefault()); }
    } collect;
    mGroup->applyToAll(&collect);
    return collect.result;
}

LLControlVariable* LLVKSettingsMgr::find(const std::string& name) const
{
    return mGroup->getControl(name).get();
}

bool LLVKSettingsMgr::saveChanges(const std::filesystem::path& path,const std::map<std::string,LLSD>& changes,std::string& error)
{
    error.clear();
    LLControlGroup updated("NativeSettingsWrite-"+LLUUID::generateNewID().asString());
    for (const auto& [name,value] : changes)
    {
        auto control=mGroup->getControl(name);
        if (!control || !control->isPersisted()) { error="Native preference is not persistent: "+name; return false; }
        if (!(*control->getValidateSignal())(control.get(),value)) { error="Preference validation rejected: "+name; return false; }
        const auto staged=updated.declareControl(name,control->type(),control->getDefault(),control->getComment(),
            SANITY_TYPE_NONE,{},"",LLControlVariable::PERSIST_ALWAYS,control->isBackupable(),control->isHiddenFromSettingsEditor());
        staged->setValue(value,true);
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
        auto control=updated.getControl(name);
        document[name]["Type"] = LLControlGroup::typeEnumToString(control->type());
        document[name]["Comment"] = control->getComment();
        document[name]["Backup"] = control->isBackupable();
        document[name]["Value"] = control->getSaveValue();
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
    for (const auto& [name,value] : changes) mGroup->getControl(name)->setValue(updated.getControl(name)->getSaveValue(),true);
    return true;
}

std::map<std::string,LLSD> LLVKSettingsMgr::values() const
{
    struct Collect final : LLControlGroup::ApplyFunctor
    {
        std::map<std::string,LLSD> result;
        void apply(const std::string& name,LLControlVariable* control) override
        { result.emplace(name,control->isType(TYPE_BOOLEAN) ? LLSD(control->getValue().asBoolean()) : control->getValue()); }
    } collect;
    mGroup->applyToAll(&collect);
    return collect.result;
}