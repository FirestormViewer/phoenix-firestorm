#ifndef LLVKGRAPHICSPOLICY_H
#define LLVKGRAPHICSPOLICY_H

#include "llvksettingsmgr.h"

class LLVKGraphicsPolicy final
{
public:
    struct Device
    {
        std::uint32_t vendor=0;
        std::uint64_t videoBytes=0,systemBytes=0;
        float bandwidth=-1.f,cpuBias=1.f,classOneBandwidth=16.f;
        bool skipBenchmark=false;
    };
    bool initialize(const std::filesystem::path& table,const std::filesystem::path& settings,std::string& error);
    static int recommendedLevel(const Device& device);
    std::optional<std::map<std::string,LLSD>> settings(int level,const Device& device,bool recommended,std::string& error) const;
private:
    struct Feature { bool available; float value; };
    std::map<std::string,std::map<std::string,Feature>> mMasks;
    LLVKSettingsMgr mSchema;
};

#endif