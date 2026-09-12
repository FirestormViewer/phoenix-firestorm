#ifndef LLVKGRAPHICPRESETS_H
#define LLVKGRAPHICPRESETS_H

#include "llvksettingsmgr.h"
#include <set>

class LLVKGraphicPresets final
{
public:
    bool initialize(const std::filesystem::path& appSettings,const std::filesystem::path& directory,std::string& error);
    const std::set<std::string>& controls() const { return mControls; }
    std::optional<std::vector<std::string>> names(bool includeDefault,std::string& error) const;
    bool save(const std::string& name,const std::map<std::string,LLSD>& values,std::string& error) const;
    bool createDefault(const std::map<std::string,LLSD>& values,std::string& error) const;
    std::optional<std::map<std::string,LLSD>> load(const std::string& name,std::string& error) const;
    bool remove(const std::string& name,std::string& error) const;
private:
    bool write(const std::string& name,const std::map<std::string,LLSD>& values,bool createDefault,std::string& error) const;
    std::optional<std::filesystem::path> path(const std::string& name,bool writable,std::string& error) const;
    LLVKSettingsMgr mSchema;
    std::set<std::string> mControls;
    std::filesystem::path mDirectory;
};

#endif