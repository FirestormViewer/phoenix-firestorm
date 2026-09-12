#ifndef LLVKTEXTURECACHE_H
#define LLVKTEXTURECACHE_H

#include "lluuid.h"
#include "llsd.h"
#include <filesystem>
#include <future>
#include <memory>
#include <map>
#include <optional>
#include <string>
#include <vector>

class LLVKTextureCache final
{
public:
    struct Configuration
    {
        std::filesystem::path directory, localAssets, executionMarker;
        std::uint64_t bytes=1024ull*1024*1024;
        std::uint32_t validationIndex=0;
        bool purge=false, versionMismatch=false, readOnly=false;
    };
    struct Result
    {
        bool success=false;
        std::vector<std::uint8_t> bytes;
        int imageSize=0, codec=0;
        bool local=false;
    };
    struct StartupPlan
    {
        Configuration configuration;
        std::map<std::string,LLSD> metadata;
    };
    static std::optional<StartupPlan> planStartup(const std::map<std::string,LLSD>& settings,
        const std::filesystem::path& defaultDirectory,const std::filesystem::path& localAssets,
        const std::filesystem::path& executionMarker,bool readOnly,std::string& error);
    LLVKTextureCache();
    ~LLVKTextureCache();
    LLVKTextureCache(const LLVKTextureCache&)=delete;
    LLVKTextureCache& operator=(const LLVKTextureCache&)=delete;
    bool start(const Configuration& configuration,std::string& error);
    bool update(std::string& error);
    bool stop(std::string& error);
    std::future<Result> read(const LLUUID& id,int offset,int size,std::string& error);
    std::future<Result> write(const LLUUID& id,std::vector<std::uint8_t> bytes,int imageSize,std::string& error);
    std::uint32_t validationIndex() const;
    bool readOnly() const;
    std::uint64_t revision() const;
    static std::string encoderVersion();
private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};

#endif