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
    std::optional<LLVKChatProtocol::Context> chatContext(LLVKSessionOwner::Tag tag) const;
    std::vector<LLVKChatProtocol::Message> takeMessages(LLVKSessionOwner::Tag tag);
    bool searchResidents(LLVKSessionOwner::Tag tag,std::uint64_t query,std::string name,std::string& error);
    std::optional<LLVKChatProtocol::SearchResult> takeResidentSearch(LLVKSessionOwner::Tag tag);
    std::vector<LLVKChatProtocol::Group> groups(LLVKSessionOwner::Tag tag) const;
    std::vector<LLVKChatProtocol::Friend> friends(LLVKSessionOwner::Tag tag) const;
    bool joinGroup(LLVKSessionOwner::Tag tag,const LLUUID& group,std::string& error);
    bool leaveGroup(LLVKSessionOwner::Tag tag,const LLUUID& group,std::string& error);
    bool sendGroup(LLVKSessionOwner::Tag tag,const LLUUID& group,const std::string& text,std::string& error);
    bool moderateGroup(LLVKSessionOwner::Tag tag,const LLUUID& group,const LLUUID& participant,bool muted,std::string& error);
    bool sendLocal(LLVKSessionOwner::Tag tag,const std::string& text,std::uint8_t type,std::string& error);
    bool sendDirect(LLVKSessionOwner::Tag tag,const LLUUID& recipient,const std::string& text,bool typing,
        bool typingStopped,std::string& error);
private:
    using Owner=LLVKSessionOwner;
    enum class Phase { Idle, Authenticate, Seed, Online, Closing };
    bool startAuthentication(std::string& error);
    bool startEvents(bool done,std::string& error);
    void pumpResidentSearch();
    void pumpGroupAcceptance();
    void pumpGroupModeration();
    void clearGroupUpdates(const LLUUID& group);
    bool groupMessage(LLVKSessionOwner::Tag tag,const LLUUID& group,std::uint8_t dialog,const std::string& text,std::string& error);
    bool post(Owner::Response response);
    void report(const char* stage,long status=0) const noexcept;
    Owner::Code connectionFailure(const char* stage,long status=0) const;
    Configuration mConfiguration;
    LLVKLoginHttp mHttp,mEvents,mResidentSearch,mGroupHttp,mModerationHttp;
    std::optional<std::pair<LLUUID,std::uint64_t>> mModeratingGroup;
    std::optional<LLVKChatProtocol::SearchResult> mSearchResult;
    std::uint64_t mSearchQuery=0;
    std::map<LLUUID,LLVKChatProtocol::Group> mGroups;
    std::map<LLUUID,std::chrono::steady_clock::time_point> mGroupDeadlines;
    std::map<LLUUID,std::vector<LLSD>> mGroupUpdates;
    std::size_t mGroupUpdateBytes=0;
    std::set<LLUUID> mGroupInvitations;
    std::map<LLUUID,std::uint64_t> mGroupVersions;
    std::deque<std::pair<LLUUID,std::uint64_t>> mGroupAccepts;
    std::optional<std::pair<LLUUID,std::uint64_t>> mAcceptingGroup;
    std::vector<LLVKChatProtocol::Message> mEventMessages;
    std::size_t mEventMessageBytes=0;
    LLVKRegionCircuit mCircuit;
    std::map<LLUUID,LLVKChatProtocol::Friend> mFriends;
    LLVKRegionCircuit::MessageDiagnostics mMessageDiagnostics;
    bool mInstantPublished=false;
    bool mInstantEventReceived=false,mInstantEventRejected=false;
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