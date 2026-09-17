#pragma once

#include "llvksessionowner.h"
#include "llvkloginhttp.h"
#include "llvkregioncircuit.h"
#include <deque>
#include <chrono>
#include <functional>

class LLVKLoginTransport final : public LLVKSessionOwner::Transport
{
public:
    struct Configuration
    {
        LLVKLoginHttp::Configuration http;
        std::string endpoint="https://login.agni.lindenlab.com/cgi-bin/login.cgi";
        std::function<void(const char*,long)> diagnostic;
    };
    explicit LLVKLoginTransport(Configuration configuration);
    bool prepare(LLSD parameters,std::string& error);
    LLVKSessionOwner::Code begin(const LLVKSessionOwner::Request& request,
        const std::shared_ptr<LLVKSessionOwner::Inbox>& replies) override;
    LLVKSessionOwner::Code pump() override;
    LLVKSessionOwner::Code quiesce(std::uint64_t generation) override;
private:
    using Owner=LLVKSessionOwner;
    enum class Phase { Idle, Authenticate, Seed, Online, Closing };
    bool startAuthentication(std::string& error);
    bool startEvents(bool done,std::string& error);
    bool post(Owner::Response response);
    void report(const char* stage,long status=0) const noexcept;
    Owner::Code connectionFailure(const char* stage,long status=0) const;
    Configuration mConfiguration;
    LLVKLoginHttp mHttp,mEvents;
    LLVKRegionCircuit mCircuit;
    LLSD mParameters,mLoginData,mCapabilities,mEventAck;
    std::deque<LLSD> mPendingEvents;
    std::optional<LLVKLoginProtocol::Bootstrap> mBootstrap;
    std::optional<Owner::Agreement> mAgreement;
    std::optional<Owner::Challenge> mChallenge;
    std::string mAgreementField,mEndpoint,mEventUrl;
    std::shared_ptr<Owner::Inbox> mInbox;
    Owner::Tag mTag;
    Phase mPhase=Phase::Idle;
    unsigned mRedirects=0,mEventFailures=0;
    std::size_t mPendingEventBytes=0;
    std::chrono::steady_clock::time_point mEventStarted,mEventRetryAt;
    bool mSeedReady=false,mConnectedPosted=false,mEventsClosing=false;
};