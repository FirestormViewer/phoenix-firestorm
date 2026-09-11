#include "llvkspellcheck.h"
#include "llsdserialize.h"
#include "llstring.h"
#include <hunspell.hxx>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <expat/expat.h>
#ifdef _WIN32
#include <windows.h>
#endif

namespace
{
    std::string filename(const std::filesystem::path& path)
    {
        const auto utf8=path.u8string();
        return std::string(utf8.begin(),utf8.end());
    }

    LLSD readCatalog(const std::filesystem::path& path)
    {
        std::ifstream input(path,std::ios::binary|std::ios::ate);
        if (!input || input.tellg()<0 || input.tellg()>4*1024*1024) return LLSD::emptyArray();
        input.seekg(0);
        LLSD document;
        if (LLSDSerialize::fromXML(document,input,false)==LLSDParser::PARSE_FAILURE || !document.isArray()) return LLSD::emptyArray();
        return document;
    }

    bool validBaseName(const std::string& name)
    {
        return !name.empty() && name!="." && name!=".." && name.find_first_of("/\\:")==std::string::npos;
    }

    bool writeCatalog(const std::filesystem::path& path,const LLSD& catalog,std::string& error)
    {
        auto staging=path; staging+=".native-write";
        std::error_code status;
        if (!std::filesystem::create_directory(staging,status))
        { error="Cannot acquire native dictionary catalog staging directory"; return false; }
        struct Cleanup
        {
            std::filesystem::path directory;
            ~Cleanup() { std::error_code ignored; std::filesystem::remove(directory/"catalog.tmp",ignored); std::filesystem::remove(directory,ignored); }
        } cleanup{staging};
        const auto temporary=staging/"catalog.tmp";
        std::ofstream output(temporary,std::ios::binary|std::ios::trunc);
        if (!output) { error="Cannot open native dictionary catalog staging file"; return false; }
        LLSDSerialize::toPrettyXML(catalog,output); output.close();
        if (!output) { error="Native dictionary catalog write failed"; return false; }
#ifdef _WIN32
        if (!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        { error="Native dictionary catalog replacement failed: "+std::to_string(GetLastError()); return false; }
#else
        std::filesystem::rename(temporary,path,status);
        if (status) { error="Native dictionary catalog replacement failed: "+status.message(); return false; }
#endif
        return true;
    }
}

LLVKSpellCheck::LLVKSpellCheck(std::filesystem::path appDirectory,std::filesystem::path userDirectory)
    : mAppDirectory(std::move(appDirectory)),mUserDirectory(std::move(userDirectory))
{
}

LLVKSpellCheck::~LLVKSpellCheck() = default;

const LLSD* LLVKSpellCheck::dictionary(const std::string& language) const
{
    for (auto entry=mDictionaries.beginArray(); entry!=mDictionaries.endArray(); ++entry)
        if ((*entry)["language"].asString()==language) return &*entry;
    return nullptr;
}

bool LLVKSpellCheck::refresh(std::string& error)
{
    error.clear();
    auto catalog=readCatalog(mUserDirectory/"dictionaries.xml");
    if (!catalog.size()) catalog=readCatalog(mAppDirectory/"dictionaries.xml");
    auto user=readCatalog(mUserDirectory/"user_dictionaries.xml");
    for (auto entry=user.beginArray(); entry!=user.endArray(); ++entry)
    {
        if (!entry->isMap() || (*entry)["language"].asString().empty()) continue;
        (*entry)["user_installed"]=true;
        const auto found=std::find_if(catalog.beginArray(),catalog.endArray(),[&](const auto& existing)
        { return existing["language"].asString()==(*entry)["language"].asString(); });
        if (found==catalog.endArray()) catalog.append(*entry); else *found=*entry;
    }
    for (auto entry=catalog.beginArray(); entry!=catalog.endArray(); ++entry)
    {
        const auto name=(*entry)["name"].asString();
        if (!entry->isMap() || !validBaseName(name))
        { error="Invalid native spelling dictionary filename"; return false; }
        const auto file=std::filesystem::path(std::u8string(name.begin(),name.end())+u8".dic");
        (*entry)["installed"]=std::filesystem::is_regular_file(mUserDirectory/file) || std::filesystem::is_regular_file(mAppDirectory/file);
    }
    mDictionaries=std::move(catalog);
    return true;
}

bool LLVKSpellCheck::activate(const std::string& primary,const std::vector<std::string>& secondary,std::string& error)
{
    error.clear();
    const auto* entry=dictionary(primary);
    if (primary.empty() || !entry || !(*entry)["installed"].asBoolean() || !(*entry)["is_primary"].asBoolean())
    { mHunspell.reset(); mPrimary.clear(); mIgnored.clear(); mSecondary=secondary; return true; }
    const auto name=(*entry)["name"].asString();
    const auto base=std::filesystem::path(std::u8string(name.begin(),name.end()));
    auto aff=base; aff+=".aff"; auto dic=base; dic+=".dic";
    const auto directory=std::filesystem::is_regular_file(mUserDirectory/aff) && std::filesystem::is_regular_file(mUserDirectory/dic) ? mUserDirectory : mAppDirectory;
    if (!std::filesystem::is_regular_file(directory/aff) || !std::filesystem::is_regular_file(directory/dic))
    { mHunspell.reset(); mPrimary.clear(); mIgnored.clear(); mSecondary=secondary; return true; }
    auto engine=std::make_unique<Hunspell>(filename(directory/aff).c_str(),filename(directory/dic).c_str());
    const auto custom=mUserDirectory/"user_custom.dic";
    if (std::filesystem::is_regular_file(custom) && engine->add_dic(filename(custom).c_str())!=0)
    { error="Cannot load native custom spelling dictionary"; return false; }
    std::vector<std::string> ignored;
    std::ifstream ignoreFile(mUserDirectory/"user_ignore.dic");
    std::string word;
    if (ignoreFile && std::getline(ignoreFile,word))
        while (std::getline(ignoreFile,word))
        {
            if (!word.empty() && word.back()=='\r') word.pop_back();
            LLStringUtil::toLower(word); ignored.push_back(word);
            if (ignored.size()>100000) { error="Native spelling ignore list exceeds budget"; return false; }
        }
    for (const auto& language : secondary)
    {
        const auto* additional=dictionary(language);
        if (!additional || !(*additional)["installed"].asBoolean()) continue;
        const auto secondaryName=(*additional)["name"].asString()+".dic";
        const auto relative=std::filesystem::path(std::u8string(secondaryName.begin(),secondaryName.end()));
        const auto path=std::filesystem::is_regular_file(mUserDirectory/relative) ? mUserDirectory/relative : mAppDirectory/relative;
        if (engine->add_dic(filename(path).c_str())!=0) { error="Cannot load native secondary spelling dictionary"; return false; }
    }
    mHunspell=std::move(engine); mPrimary=primary; mSecondary=secondary; mIgnored=std::move(ignored);
    return true;
}

bool LLVKSpellCheck::check(const std::string& word) const
{
    if (!mHunspell || word.size()<3 || mHunspell->spell(word)) return true;
    auto lower=word; LLStringUtil::toLower(lower);
    return std::find(mIgnored.begin(),mIgnored.end(),lower)!=mIgnored.end();
}

std::vector<std::string> LLVKSpellCheck::suggestions(const std::string& word) const
{
    return !mHunspell || word.size()<3 ? std::vector<std::string>() : mHunspell->suggest(word);
}

bool LLVKSpellCheck::canRemove(const std::string& language) const
{
    const auto* entry=dictionary(language);
    return entry && (*entry)["user_installed"].asBoolean() && (!active() ||
        (mPrimary!=language && std::find(mSecondary.begin(),mSecondary.end(),language)==mSecondary.end()));
}

bool LLVKSpellCheck::remove(const std::string& language,std::string& error)
{
    error.clear();
    if (!canRemove(language)) { error="Native dictionary is not removable"; return false; }
    auto catalog=readCatalog(mUserDirectory/"user_dictionaries.xml");
    for (int index=0; index<static_cast<int>(catalog.size()); ++index)
    {
        if (catalog[index]["language"].asString()!=language) continue;
        const auto name=catalog[index]["name"].asString();
        if (!validBaseName(name)) { error="Invalid removable dictionary filename"; return false; }
        for (const auto extension : {".dic",".aff"})
        {
            const auto base=std::filesystem::path(std::u8string(name.begin(),name.end()));
            auto path=mUserDirectory/base; path+=extension;
            std::error_code status; std::filesystem::remove(path,status);
            if (status) { error="Native dictionary removal failed: "+status.message(); return false; }
        }
        catalog.erase(index);
        if (!writeCatalog(mUserDirectory/"user_dictionaries.xml",catalog,error)) return false;
        return refresh(error);
    }
    error="Native user dictionary catalog entry is missing";
    return false;
}

std::optional<std::filesystem::path> LLVKSpellCheck::resolveImportPath(const std::filesystem::path& path,std::string& error)
{
    error.clear();
    auto extension=filename(path.extension()); LLStringUtil::toLower(extension);
    if (extension!=".xcu") return path;
    std::ifstream input(path,std::ios::binary|std::ios::ate);
    if (!input || input.tellg()<0 || input.tellg()>4*1024*1024)
    { error="Native dictionary XCU file cannot be read safely"; return std::nullopt; }
    std::string xml(static_cast<std::size_t>(input.tellg()),'\0'); input.seekg(0);
    if (!input.read(xml.data(),xml.size())) { error="Native dictionary XCU read failed"; return std::nullopt; }
    struct Parser
    {
        XML_Parser parser=XML_ParserCreate(nullptr);
        std::vector<std::string> stack;
        std::string property,format,locations;
        std::optional<std::string> result;
        bool collecting=false,failed=false;
        ~Parser() { if (parser) XML_ParserFree(parser); }
        static void XMLCALL start(void* pointer,const char* tag,const char** attributes)
        {
            auto& state=*static_cast<Parser*>(pointer);
            try
            {
                if (state.stack.size()>=64) throw std::runtime_error("XCU depth limit");
                std::string name;
                for (std::size_t index=0; attributes[index]; index+=2)
                    if (std::string_view(attributes[index])=="oor:name") name=attributes[index+1];
                state.stack.push_back(name);
                if (state.stack.size()==4 && state.stack[1]=="ServiceManager" && state.stack[2]=="Dictionaries")
                { state.format.clear(); state.locations.clear(); }
                if (state.stack.size()==5) state.property=name;
                if (state.stack.size()==6 && std::string_view(tag)=="value" && state.stack[1]=="ServiceManager" && state.stack[2]=="Dictionaries")
                    state.collecting=state.property=="Format" || state.property=="Locations";
            }
            catch (...) { state.failed=true; XML_StopParser(state.parser,XML_FALSE); }
        }
        static void XMLCALL text(void* pointer,const char* value,int length)
        {
            auto& state=*static_cast<Parser*>(pointer);
            try { if (state.collecting) (state.property=="Format" ? state.format : state.locations).append(value,length); }
            catch (...) { state.failed=true; XML_StopParser(state.parser,XML_FALSE); }
        }
        static void XMLCALL end(void* pointer,const char*)
        {
            auto& state=*static_cast<Parser*>(pointer);
            try
            {
                if (state.stack.size()==6) state.collecting=false;
                if (state.stack.size()==4 && !state.result)
                {
                    LLStringUtil::trim(state.format);
                    if (state.format=="DICT_SPELL")
                    {
                        std::istringstream locations(state.locations);
                        std::string location;
                        while (locations>>location)
                        {
                            std::replace(location.begin(),location.end(),'\\','/');
                            auto extension=filename(std::filesystem::path(std::u8string(location.begin(),location.end())).extension());
                            LLStringUtil::toLower(extension);
                            if (extension==".dic") { state.result=location; break; }
                        }
                    }
                }
                state.stack.pop_back();
            }
            catch (...) { state.failed=true; XML_StopParser(state.parser,XML_FALSE); }
        }
        static void XMLCALL doctype(void* pointer,const char*,const char*,const char*,int)
        { auto& state=*static_cast<Parser*>(pointer); state.failed=true; XML_StopParser(state.parser,XML_FALSE); }
    } state;
    if (!state.parser) { error="Native dictionary XCU parser allocation failed"; return std::nullopt; }
    XML_SetUserData(state.parser,&state); XML_SetElementHandler(state.parser,Parser::start,Parser::end);
    XML_SetCharacterDataHandler(state.parser,Parser::text); XML_SetStartDoctypeDeclHandler(state.parser,Parser::doctype);
    if (XML_Parse(state.parser,xml.data(),static_cast<int>(xml.size()),XML_TRUE)!=XML_STATUS_OK || state.failed || !state.result)
    { error="Native dictionary XCU has no valid spelling dictionary location"; return std::nullopt; }
    LLStringUtil::replaceString(*state.result,"%origin%",filename(path.parent_path()));
    return std::filesystem::path(std::u8string(state.result->begin(),state.result->end()));
}

bool LLVKSpellCheck::importDictionary(const std::filesystem::path& path,std::string language,std::string& error)
{
    error.clear(); LLStringUtil::trim(language);
    const auto name=filename(path.stem());
    if (!validBaseName(name) || language.empty()) { error="A dictionary file and language are required"; return false; }
    auto dic=path; dic.replace_extension(".dic");
    auto aff=path; aff.replace_extension(".aff");
    if (!std::filesystem::is_regular_file(dic)) { error="Dictionary word file is missing"; return false; }
    std::error_code status;
    std::filesystem::create_directories(mUserDirectory,status);
    if (status) { error="Cannot create native user dictionary directory: "+status.message(); return false; }
    const bool primary=std::filesystem::is_regular_file(aff);
    for (const auto& source : {dic,aff})
    {
        if (source==aff && !primary) continue;
        const auto target=mUserDirectory/source.filename();
        if (std::filesystem::exists(target) && std::filesystem::equivalent(source,target)) continue;
        std::filesystem::copy_file(source,target,std::filesystem::copy_options::overwrite_existing,status);
        if (status) { error="Native dictionary import failed: "+status.message(); return false; }
    }
    LLSD entry; entry["name"]=name; entry["language"]=language; entry["is_primary"]=primary;
    auto catalog=readCatalog(mUserDirectory/"user_dictionaries.xml");
    const auto found=std::find_if(catalog.beginArray(),catalog.endArray(),[&](const auto& item) { return item["name"].asString()==name; });
    if (found==catalog.endArray()) catalog.append(entry); else *found=entry;
    return writeCatalog(mUserDirectory/"user_dictionaries.xml",catalog,error) && refresh(error);
}