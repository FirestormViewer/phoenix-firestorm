#ifndef LLVKPREFERENCESBACKUP_H
#define LLVKPREFERENCESBACKUP_H

#include "llvksettingsmgr.h"
#include <vector>

class LLVKPreferencesBackup final
{
public:
    static LLSD settings(LLControlGroup& group,bool backupOnly=true);
    static std::optional<LLSD> restoredSettings(const std::filesystem::path& defaults,const std::filesystem::path& backup,
        const std::map<std::string,LLSD>& recommended,std::string& error);
    static bool copy(const std::filesystem::path& source,const std::filesystem::path& destination,
        const std::vector<std::string>& files,const std::vector<std::string>& folders,
        const std::map<std::string,LLSD>& generated,std::string& error);
};

#endif