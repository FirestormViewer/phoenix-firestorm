#ifndef LLVKSTARTUP_H
#define LLVKSTARTUP_H

#include <optional>
#include <string>
#include "llvkproxy.h"
#include "llvktexturecache.h"
#include "llvksessionowner.h"
#include "llvkerror.h"

class LLVKFatalReporting final
{
public:
    using Presenter=std::function<void(const LLVKError&)>;
    LLVKFatalReporting(const std::filesystem::path& record,Presenter presenter);
    ~LLVKFatalReporting();
    void setErrorResolver(LLVKError::Resolver resolver);
    std::optional<LLVKError::Code> failure() const noexcept;
    LLVKFatalReporting(const LLVKFatalReporting&)=delete;
    LLVKFatalReporting& operator=(const LLVKFatalReporting&)=delete;
private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};

class LLVKStartupStatus;
class LLVKApplicationCache final : public LLVKSessionOwner::Service
{
public:
    explicit LLVKApplicationCache(LLVKTextureCache::Configuration configuration,
        std::shared_ptr<LLVKStartupStatus> status = {})
        : mConfiguration(std::move(configuration)),mStatus(std::move(status)) {}
    LLVKSessionOwner::Code acquire(const LLVKSessionOwner::Context&) override;
    LLVKSessionOwner::Code retire() override;
    LLVKTextureCache& cache() noexcept { return mCache; }
private:
    LLVKTextureCache::Configuration mConfiguration;
    LLVKTextureCache mCache;
    std::shared_ptr<LLVKStartupStatus> mStatus;
};

class LLControlGroup;
std::optional<int> llvkStartup(const std::wstring& commandLine, const std::string& profileName,
    const std::string& shortVersion, LLControlGroup& globalSettings,
    LLControlGroup& accountSettings, LLControlGroup& crashSettings, LLControlGroup& warningSettings,
    const LLVKProxy::CredentialFactory& proxyCredentials = {}, const std::function<void()>& clearSpamQueues = {},
    const std::string& executionMarkerName = {});

#endif