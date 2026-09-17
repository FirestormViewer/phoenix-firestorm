#ifndef LLVKSTARTUPSTATUS_H
#define LLVKSTARTUPSTATUS_H

#include "llvkskinfiles.h"
#include "llvkerror.h"
#include <memory>

class LLVKStartupStatus final
{
public:
    LLVKStartupStatus();
    ~LLVKStartupStatus();
    LLVKStartupStatus(const LLVKStartupStatus&)=delete;
    LLVKStartupStatus& operator=(const LLVKStartupStatus&)=delete;
    bool load(const LLVKSkinFiles::Configuration& skin,const std::string& title,std::string& error);
    bool show(const std::string& key,std::string& error);
    void hide();
    LLVKError::Resolver errorResolver() const;
private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};

#endif