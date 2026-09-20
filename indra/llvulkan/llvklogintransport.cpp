#include "linden_common.h"
#include "llvklogintransport.h"
#include "llsdserialize.h"
#include "llstring.h"
#include <boost/url.hpp>
#include <sstream>

namespace
{
    std::string xml(const LLSD& value)
    {
        std::ostringstream stream;
        LLSDSerialize::toXML(value,stream);
        return stream.str();
    }
    std::optional<LLSD> decodeLlsd(const std::string& value)
    {
        if (!LLVKLoginProtocol::boundedXml(value)) return {};
        std::istringstream stream(value);
        LLSD result;
        if (LLSDSerialize::fromXML(result,stream)<=0 || !result.isMap()) return {};
        return result;
    }
    bool https(const std::string& value)
    {
        const auto uri=boost::urls::parse_uri(value);
        return value.size()<=16384 && uri && uri->scheme()=="https" && !uri->host().empty() && !uri->has_userinfo() && !uri->has_fragment();
    }
}

LLVKLoginTransport::LLVKLoginTransport(Configuration configuration)
    : mConfiguration(std::move(configuration)),mHttp(mConfiguration.http),mEvents(mConfiguration.http),mResidentSearch(mConfiguration.http),mGroupHttp(mConfiguration.http),mModerationHttp(mConfiguration.http) {}

bool LLVKLoginTransport::prepare(LLSD parameters,std::string& error)
{
    error.clear();
    if (mPhase!=Phase::Idle || !parameters.isMap() || parameters["first"].asString().empty() ||
        parameters["last"].asString().empty() || parameters["passwd"].asString().size()!=35 ||
        !parameters["passwd"].asString().starts_with("$1$"))
    { error="Native Second Life login parameters are invalid or busy"; return false; }
    mParameters=std::move(parameters);
    mParameters["agree_to_tos"]=false;
    mParameters["read_critical"]=false;
    mEndpoint=mConfiguration.endpoint;
    mRedirects=0; mEventFailures=0;
    return true;
}

bool LLVKLoginTransport::startAuthentication(std::string& error)
{
    const auto body=LLVKLoginProtocol::encode(mParameters,error);
    return body && mHttp.start(mEndpoint,*body,"text/xml",error);
}

void LLVKLoginTransport::report(const char* stage,long status) const noexcept
{
    try { if (mConfiguration.diagnostic) mConfiguration.diagnostic(stage,status); }
    catch (...) {}
}

LLVKSessionOwner::Code LLVKLoginTransport::connectionFailure(const char* stage,long status) const
{
    report(stage,status);
    return Owner::Code::ConnectionFailed;
}

LLVKSessionOwner::Code LLVKLoginTransport::begin(const Owner::Request& request,const std::shared_ptr<Owner::Inbox>& replies)
{
    std::string error;
    if (!replies || !request.tag.generation) return Owner::Code::InvalidReply;
    if (request.operation==Owner::Operation::Authenticate)
    {
        if (mPhase!=Phase::Idle && mPhase!=Phase::Authenticate) return Owner::Code::WrongState;
        if (!request.challengeToken.empty())
        {
            if (!mChallenge) return Owner::Code::InvalidReply;
            mParameters["token"]=request.challengeToken;
            mChallenge.reset();
        }
        if (request.acceptedAgreement)
        {
            if (!mAgreement || *mAgreement!=*request.acceptedAgreement || mAgreementField.empty()) return Owner::Code::InvalidReply;
            mParameters[mAgreementField]=true;
            mAgreement.reset(); mAgreementField.clear();
        }
        mTag=request.tag; mInbox=replies;
        if (!startAuthentication(error)) return Owner::Code::AuthenticationFailed;
        mPhase=Phase::Authenticate;
        return Owner::Code::Ok;
    }
    if (request.operation!=Owner::Operation::Connect || !mBootstrap) return Owner::Code::WrongState;
    mTag=request.tag; mInbox=replies;
    if (!mCircuit.start(*mBootstrap,error)) return connectionFailure("circuit-start-failed");
    mMessageDiagnostics={}; mInstantPublished=false;
    mInstantEventReceived=false; mInstantEventRejected=false;
    LLSD names=LLSD::emptyArray();
    for (const auto name : {"EventQueueGet","SimulatorFeatures","GetDisplayNames","AvatarPickerSearch","ChatSessionRequest","FetchInventoryDescendents2",
        "FetchLibDescendents2","FetchInventory2","FetchLib2","ViewerAsset","GetTexture","GetMesh","GetMesh2","EnvironmentSettings"})
        names.append(name);
    if (!mHttp.start(mBootstrap->seedCapability,xml(names),"application/llsd+xml",error)) return connectionFailure("seed-start-failed");
    mSeedReady=false; mConnectedPosted=false;
    mPhase=Phase::Seed;
    report("connection-started");
    return Owner::Code::Ok;
}

bool LLVKLoginTransport::post(Owner::Response response)
{
    response.tag=mTag;
    if (!mInbox || mInbox->post(response)!=Owner::Code::Ok) return false;
    if (response.kind==Owner::Response::Kind::Authorized) report("authorized");
    else if (response.kind==Owner::Response::Kind::RegionConnected) report("region-connected");
    if (response.kind==Owner::Response::Kind::Authorized)
        LL_INFOS("NativeLogin") << "NATIVE_LOGIN_AUTHORIZED generation=" << mTag.generation << LL_ENDL;
    else if (response.kind==Owner::Response::Kind::RegionConnected)
        LL_INFOS("NativeLogin") << "NATIVE_REGION_CONNECTED generation=" << mTag.generation << " epoch=" << mTag.regionEpoch << LL_ENDL;
    return true;
}

bool LLVKLoginTransport::startEvents(bool done,std::string& error)
{
    mEventStarted=std::chrono::steady_clock::now();
    LLSD request;
    request["ack"]=mEventAck;
    request["done"]=done;
    return mEvents.start(mEventUrl,xml(request),"application/llsd+xml",error);
}

LLVKSessionOwner::Code LLVKLoginTransport::pump()
{
    std::string error;
    if (mPhase==Phase::Authenticate)
    {
        const auto status=mHttp.pump(error);
        if (status==LLVKLoginHttp::Status::Failed) return Owner::Code::AuthenticationFailed;
        if (status!=LLVKLoginHttp::Status::Complete) return Owner::Code::Pending;
        const auto data=LLVKLoginProtocol::decode(mHttp.takeResponse(),error);
        mHttp.cancel();
        if (!data) return Owner::Code::AuthenticationFailed;
        const auto outcome=(*data)["login"].asString();
        if (data->has("mfa_hash"))
        {
            const auto hash=(*data)["mfa_hash"].asString();
            if (hash.size()>4096 || hash.find('\0')!=hash.npos) return Owner::Code::AuthenticationFailed;
            mParameters["mfa_hash"]=hash;
            mParameters["token"]="";
        }
        if (outcome=="indeterminate")
        {
            const auto endpoint=(*data)["next_url"].asString();
            const auto uri=boost::urls::parse_uri(endpoint);
            if (++mRedirects>5 || !https(endpoint) || !uri ||
                !(uri->host()=="lindenlab.com" || uri->host().ends_with(".lindenlab.com")) ||
                (*data)["next_method"].asString()!="login_to_simulator") return Owner::Code::AuthenticationFailed;
            mEndpoint=endpoint;
            return startAuthentication(error) ? Owner::Code::Pending : Owner::Code::AuthenticationFailed;
        }
        if (outcome!="true")
        {
            const auto reason=(*data)["reason"].asString();
            if (reason=="mfa_challenge")
            {
                auto key=(*data)["message_id"].asString();
                if (key.size()>128 || !key.starts_with("LoginFailedAuthenticationMFA")) key="LoginFailedAuthenticationMFARequired";
                mChallenge=Owner::Challenge{mTag.generation,mTag.request,key};
                Owner::Response response; response.kind=Owner::Response::Kind::ChallengeRequired; response.challenge=*mChallenge;
                return post(response) ? Owner::Code::Ok : Owner::Code::InvalidReply;
            }
            if (reason!="critical") return Owner::Code::AuthenticationFailed;
            const auto text=(*data)["message"].asString();
            if (text.empty() || text.size()>Owner::Agreement::maximumTextBytes || text.find('\0')!=text.npos ||
                text.starts_with("http://") || text.starts_with("https://")) return Owner::Code::AuthenticationFailed;
            mAgreement=Owner::Agreement{mTag.generation,mTag.request,text};
            mAgreementField="read_critical";
            Owner::Response response; response.kind=Owner::Response::Kind::AgreementRequired; response.agreement=*mAgreement;
            return post(response) ? Owner::Code::Ok : Owner::Code::InvalidReply;
        }
        mBootstrap=LLVKLoginProtocol::bootstrap(*data,error);
        if (!mBootstrap) return Owner::Code::AuthenticationFailed;
        mLoginData=*data;
        const auto friends=LLVKChatProtocol::decodeFriends((*data)["buddy-list"],error);
        if (!friends) return Owner::Code::AuthenticationFailed;
        mFriends.clear();
        for (const auto& buddy : *friends) mFriends.emplace(buddy.id,buddy);
        mParameters=LLSD();
        Owner::Response response; response.kind=Owner::Response::Kind::Authorized;
        response.identity={mTag.generation,mTag.generation};
        return post(response) ? Owner::Code::Ok : Owner::Code::InvalidReply;
    }
    if (mPhase!=Phase::Seed && mPhase!=Phase::Online) return Owner::Code::Pending;
    const auto circuit=mCircuit.pump(error);
    const auto messages=mCircuit.messageDiagnostics();
    if (messages.instantReceived && !mMessageDiagnostics.instantReceived) report("im-packet-received");
    if (messages.instantDecoded && !mMessageDiagnostics.instantDecoded) report("im-packet-decoded");
    if (messages.instantRejected && !mMessageDiagnostics.instantRejected) report("im-packet-rejected");
    mMessageDiagnostics=messages;
    for (const auto& update : mCircuit.takePresence())
    {
        const auto found=mFriends.find(update.id);
        if (found!=mFriends.end())
        { found->second.online=update.online; found->second.presenceReceived=true; }
    }
    if (circuit==LLVKRegionCircuit::Status::Failed) return connectionFailure("circuit-progress-failed");
    pumpResidentSearch();
    pumpGroupAcceptance();
    pumpGroupModeration();
    for (auto iterator=mGroupDeadlines.begin(); iterator!=mGroupDeadlines.end();)
    {
        if (std::chrono::steady_clock::now()<iterator->second) { ++iterator; continue; }
        auto& group=mGroups.at(iterator->first);
        group.state=LLVKChatProtocol::Group::State::Failed; group.error="Group chat join timed out";
        clearGroupUpdates(group.id);
        iterator=mGroupDeadlines.erase(iterator);
    }
    if (!mSeedReady)
    {
        const auto status=mHttp.pump(error);
        if (status==LLVKLoginHttp::Status::Failed) return connectionFailure("seed-http-failed",mHttp.responseCode());
        if (status==LLVKLoginHttp::Status::Complete)
        {
            const auto capabilities=decodeLlsd(mHttp.takeResponse());
            mHttp.cancel();
            if (!capabilities) return connectionFailure("seed-response-invalid");
            if (!(*capabilities)["EventQueueGet"].isString() || !https((*capabilities)["EventQueueGet"].asString()))
                return connectionFailure("event-queue-url-invalid");
            mCapabilities=*capabilities;
            mEventUrl=mCapabilities["EventQueueGet"].asString();
            mEventAck=LLSD();
            if (!startEvents(false,error)) return connectionFailure("event-queue-start-failed");
            mSeedReady=true;
            report("seed-ready");
        }
    }
    if (mSeedReady)
    {
        if (std::chrono::steady_clock::now()<mEventRetryAt) return Owner::Code::Pending;
        if (mEvents.pump(error)==LLVKLoginHttp::Status::Idle && !startEvents(false,error)) return connectionFailure("event-queue-retry-start-failed");
        const auto status=mEvents.pump(error);
        if (status==LLVKLoginHttp::Status::Failed)
        {
            const auto code=mEvents.responseCode();
            const bool expected=mEvents.timedOut() || code==500 || code==502 || code==503 || code==504;
            const bool held=std::chrono::steady_clock::now()-mEventStarted>=std::chrono::seconds(10);
            if (expected && held) mEventFailures=0;
            else if (++mEventFailures>3) return connectionFailure("event-queue-http-failed",code);
            mEvents.cancel();
            mEventRetryAt=std::chrono::steady_clock::now()+std::chrono::seconds(mEventFailures);
        }
        else if (status==LLVKLoginHttp::Status::Complete)
        {
            const auto response=mEvents.takeResponse();
            const auto events=decodeLlsd(response);
            mEvents.cancel();
            if (!events || !(*events)["events"].isArray() || !events->has("id")) return connectionFailure("event-queue-response-invalid");
            const auto& entries=(*events)["events"];
            if (entries.size()>256) return connectionFailure("event-queue-batch-full");
            for (auto iterator=entries.beginArray(); iterator!=entries.endArray(); ++iterator)
            {
                if ((*iterator)["message"].asString()=="AgentGroupDataUpdate")
                {
                    const auto groups=LLVKChatProtocol::decodeGroups((*iterator)["body"],mBootstrap->agentId,error);
                    if (!groups) return connectionFailure("group-membership-invalid");
                    for (const auto& group : *groups)
                    {
                        if (!mGroups.contains(group.id) && mGroups.size()>=256) return connectionFailure("group-membership-full");
                        auto& current=mGroups[group.id];
                        current.id=group.id; current.name=group.name; current.powers=group.powers;
                        current.acceptNotices=group.acceptNotices;
                    }
                    continue;
                }
                const auto event=(*iterator)["message"].asString();
                const auto& body=(*iterator)["body"];
                if (event=="ImprovedInstantMessage" && !body.has("binary-template-data"))
                {
                    if (!mInstantEventReceived) { report("im-event-received"); mInstantEventReceived=true; }
                    const auto message=LLVKChatProtocol::decodeInstantEvent(body,error);
                    if (!message)
                    {
                        if (!mInstantEventRejected) { report("im-event-rejected"); mInstantEventRejected=true; }
                        continue;
                    }
                    const auto bytes=message->name.size()+message->text.size()+message->bucket.size();
                    if (mEventMessages.size()>=1024 || mEventMessageBytes+bytes>4*1024*1024)
                        return connectionFailure("im-event-queue-full");
                    mEventMessages.push_back(*message); mEventMessageBytes+=bytes;
                    continue;
                }
                if (event=="ChatterBoxInvitation" && body.has("instantmessage"))
                {
                    const auto message=LLVKChatProtocol::decodeInvitation(body,error);
                    if (!message) return connectionFailure("group-invitation-invalid");
                    const auto found=mGroups.find(message->conversation);
                    if (found!=mGroups.end() && message->sender!=mBootstrap->agentId)
                    {
                        const auto bytes=message->name.size()+message->text.size();
                        if (mEventMessages.size()>=1024 || mEventMessageBytes+bytes>4*1024*1024)
                            return connectionFailure("group-message-queue-full");
                        mGroupInvitations.insert(found->first);
                        mEventMessages.push_back(*message); mEventMessageBytes+=bytes;
                    }
                    continue;
                }
                if (event=="ChatterBoxSessionStartReply")
                {
                    const auto found=mGroups.find(body["temp_session_id"].asUUID());
                    if (found!=mGroups.end() && found->second.state==LLVKChatProtocol::Group::State::Joining)
                    {
                        auto& group=found->second;
                        const bool success=body["success"].isBoolean() && body["success"].asBoolean() && body["session_id"].asUUID()==group.id;
                        group.state=success ? LLVKChatProtocol::Group::State::Joined : LLVKChatProtocol::Group::State::Failed;
                        group.error=success ? "" : "Group chat join was rejected";
                        if (success)
                        {
                            if (!LLVKChatProtocol::updateParticipants(group,body,true,error)) return connectionFailure("group-participants-invalid");
                            const auto pending=mGroupUpdates.find(group.id);
                            if (pending!=mGroupUpdates.end()) for (const auto& update : pending->second)
                                if (!LLVKChatProtocol::updateParticipants(group,update,false,error)) return connectionFailure("group-participants-invalid");
                        }
                        clearGroupUpdates(group.id);
                        mGroupDeadlines.erase(group.id);
                    }
                    continue;
                }
                if (event=="ChatterBoxSessionAgentListUpdates")
                {
                    const auto found=mGroups.find(body["session_id"].asUUID());
                    if (found!=mGroups.end())
                    {
                        if (found->second.state==LLVKChatProtocol::Group::State::Joining)
                        {
                            const auto bytes=xml(body).size();
                            auto& pending=mGroupUpdates[found->first];
                            if (pending.size()>=64 || mGroupUpdateBytes+bytes>1024*1024) return connectionFailure("group-updates-full");
                            pending.push_back(body); mGroupUpdateBytes+=bytes;
                        }
                        else if (found->second.state==LLVKChatProtocol::Group::State::Joined &&
                            !LLVKChatProtocol::updateParticipants(found->second,body,false,error)) return connectionFailure("group-participants-invalid");
                    }
                    continue;
                }
                if (event=="ForceCloseChatterBoxSession" || event=="ChatterBoxSessionEventReply")
                {
                    const auto found=mGroups.find(body["session_id"].asUUID());
                    if (found!=mGroups.end() && (event=="ForceCloseChatterBoxSession" || !body["success"].asBoolean()))
                    {
                        found->second.state=LLVKChatProtocol::Group::State::Failed;
                        found->second.error=event=="ForceCloseChatterBoxSession" ? "Group chat was closed by the service" : "Group chat operation was rejected";
                        mGroupDeadlines.erase(found->first);
                        clearGroupUpdates(found->first);
                    }
                    continue;
                }
                if (event=="AgentDropGroup")
                {
                    const auto& data=body.has("body") ? body["body"]["AgentData"][0] : body["AgentData"][0];
                    if (data["AgentID"].asUUID()==mBootstrap->agentId)
                    { const auto id=data["GroupID"].asUUID(); mGroups.erase(id); mGroupDeadlines.erase(id); clearGroupUpdates(id); }
                    continue;
                }
                const auto bytes=xml(*iterator).size();
                if (mPendingEvents.size()>=256 || mPendingEventBytes+bytes>16*1024*1024)
                    return connectionFailure("event-queue-retention-full");
                mPendingEventBytes+=bytes;
                mPendingEvents.push_back(*iterator);
            }
            mEventAck=(*events)["id"];
            mEventFailures=0;
            if (!startEvents(false,error)) return connectionFailure("event-queue-next-start-failed");
        }
    }
    if (mSeedReady && circuit==LLVKRegionCircuit::Status::Connected && !mConnectedPosted)
    {
        Owner::Response response; response.kind=Owner::Response::Kind::RegionConnected;
        if (!post(response)) return Owner::Code::InvalidReply;
        mConnectedPosted=true; mPhase=Phase::Online;
    }
    return Owner::Code::Ok;
}

LLVKSessionOwner::Code LLVKLoginTransport::quiesce(std::uint64_t generation)
{
    if (generation!=mTag.generation) return Owner::Code::StaleReply;
    std::string error;
    if (mPhase!=Phase::Closing)
    {
        mHttp.cancel(); mEvents.cancel(); mInbox.reset(); mParameters=LLSD(); mPendingEvents.clear(); mPendingEventBytes=0;
        mResidentSearch.cancel(); mSearchResult.reset(); mSearchQuery=0;
        mGroups.clear(); mGroupDeadlines.clear();
        mFriends.clear();
        mGroupUpdates.clear(); mGroupUpdateBytes=0;
        mGroupHttp.cancel(); mGroupInvitations.clear(); mGroupVersions.clear(); mGroupAccepts.clear(); mAcceptingGroup.reset();
        mModerationHttp.cancel(); mModeratingGroup.reset();
        mEventMessages.clear(); mEventMessageBytes=0;
        mPhase=Phase::Closing;
        if (!mEventUrl.empty()) mEventsClosing=startEvents(true,error);
    }
    const auto circuit=mCircuit.close(error);
    if (circuit==LLVKRegionCircuit::Status::Closing || circuit==LLVKRegionCircuit::Status::Connecting) return Owner::Code::Pending;
    if (circuit==LLVKRegionCircuit::Status::Failed) { mCircuit.cancel(); return Owner::Code::CleanupFailed; }
    if (mEventsClosing)
    {
        const auto status=mEvents.pump(error);
        if (status==LLVKLoginHttp::Status::Pending) return Owner::Code::Pending;
        mEventsClosing=false;
        mEvents.cancel();
        if (status==LLVKLoginHttp::Status::Failed) return Owner::Code::CleanupFailed;
    }
    mCircuit.cancel();
    mBootstrap.reset(); mAgreement.reset(); mChallenge.reset(); mAgreementField.clear(); mLoginData=LLSD(); mCapabilities=LLSD();
    mEventAck=LLSD(); mEventUrl.clear(); mPhase=Phase::Idle;
    return Owner::Code::Ok;
}

bool LLVKLoginTransport::moderateGroup(Owner::Tag tag,const LLUUID& id,const LLUUID& participant,bool muted,std::string& error)
{
    error.clear();
    const auto context=chatContext(tag);
    const auto found=mGroups.find(id);
    if (!context || participant.isNull() || found==mGroups.end() || found->second.state!=LLVKChatProtocol::Group::State::Joined)
    { error="Group moderation requires a connected session"; return false; }
    auto& group=found->second;
    const auto self=group.participants.find(context->agent);
    if (self==group.participants.end() || !self->second.moderator)
    { error="Group moderation requires moderator permission"; return false; }
    if (mModeratingGroup) { error="A group moderation request is already pending"; return false; }
    const auto capability=mCapabilities["ChatSessionRequest"].asString();
    if (!https(capability)) { error="Group moderation service is unavailable"; return false; }
    LLSD request;
    request["method"]="mute update"; request["session-id"]=id;
    request["params"]["agent_id"]=participant; request["params"]["mute_info"]["text"]=muted;
    if (!mModerationHttp.start(capability,xml(request),"application/llsd+xml",error)) return false;
    mModeratingGroup=std::pair{id,mGroupVersions[id]};
    group.moderationPending=true; group.error.clear();
    return true;
}

void LLVKLoginTransport::pumpGroupModeration()
{
    if (!mModeratingGroup) return;
    std::string error;
    const auto status=mModerationHttp.pump(error);
    if (status==LLVKLoginHttp::Status::Pending) return;
    const auto [id,version]=*mModeratingGroup;
    const auto found=mGroups.find(id);
    if (found!=mGroups.end() && mGroupVersions[id]==version && found->second.state==LLVKChatProtocol::Group::State::Joined)
    {
        found->second.moderationPending=false;
        if (status!=LLVKLoginHttp::Status::Complete)
            found->second.error=mModerationHttp.responseCode()==403 ? "Group moderation permission was denied" : "Group moderation request failed";
    }
    mModerationHttp.cancel(); mModeratingGroup.reset();
}

void LLVKLoginTransport::pumpGroupAcceptance()
{
    std::string error;
    if (mAcceptingGroup)
    {
        const auto status=mGroupHttp.pump(error);
        if (status==LLVKLoginHttp::Status::Pending) return;
        const auto [id,version]=*mAcceptingGroup;
        const auto found=mGroups.find(id);
        if (found!=mGroups.end() && mGroupVersions[id]==version && found->second.state==LLVKChatProtocol::Group::State::Joining)
        {
            auto& group=found->second;
            const auto data=status==LLVKLoginHttp::Status::Complete ? decodeLlsd(mGroupHttp.takeResponse()) : std::nullopt;
            bool success=data && LLVKChatProtocol::updateParticipants(group,*data,true,error);
            const auto pending=mGroupUpdates.find(id);
            if (success && pending!=mGroupUpdates.end()) for (const auto& update : pending->second)
                if (!LLVKChatProtocol::updateParticipants(group,update,false,error)) { success=false; break; }
            group.state=success ? LLVKChatProtocol::Group::State::Joined : LLVKChatProtocol::Group::State::Failed;
            group.error=success ? "" : "Group chat invitation could not be accepted";
            mGroupDeadlines.erase(id); clearGroupUpdates(id);
        }
        mGroupHttp.cancel(); mAcceptingGroup.reset();
    }
    while (!mGroupAccepts.empty())
    {
        const auto request=mGroupAccepts.front(); mGroupAccepts.pop_front();
        const auto found=mGroups.find(request.first);
        if (found==mGroups.end() || mGroupVersions[request.first]!=request.second || found->second.state!=LLVKChatProtocol::Group::State::Joining) continue;
        LLSD data; data["method"]="accept invitation"; data["session-id"]=request.first;
        const auto capability=mCapabilities["ChatSessionRequest"].asString();
        if (!https(capability) || !mGroupHttp.start(capability,xml(data),"application/llsd+xml",error))
        { found->second.state=LLVKChatProtocol::Group::State::Failed; found->second.error="Group chat invitation service is unavailable"; continue; }
        mAcceptingGroup=request;
        break;
    }
}

void LLVKLoginTransport::clearGroupUpdates(const LLUUID& group)
{
    const auto found=mGroupUpdates.find(group);
    if (found==mGroupUpdates.end()) return;
    for (const auto& update : found->second) mGroupUpdateBytes-=xml(update).size();
    mGroupUpdates.erase(found);
}

bool LLVKLoginTransport::groupMessage(Owner::Tag tag,const LLUUID& group,std::uint8_t dialog,const std::string& text,std::string& error)
{
    const auto context=chatContext(tag);
    if (!context) { error="Group chat is disconnected"; return false; }
    LLVKChatProtocol::Message message;
    message.kind=LLVKChatProtocol::Message::Kind::Instant;
    message.recipient=group; message.conversation=group; message.dialog=dialog;
    message.name=context->name; message.text=text;
    return mCircuit.sendInstant(message,error);
}

bool LLVKLoginTransport::joinGroup(Owner::Tag tag,const LLUUID& id,std::string& error)
{
    error.clear();
    const auto found=mGroups.find(id);
    if (!chatContext(tag) || found==mGroups.end() || !(found->second.powers&(std::uint64_t(1)<<16)))
    { error="Group chat membership or permission is unavailable"; return false; }
    auto& group=found->second;
    if (group.state==LLVKChatProtocol::Group::State::Joining || group.state==LLVKChatProtocol::Group::State::Joined) return true;
    if (mGroupInvitations.contains(id))
    {
        if (!https(mCapabilities["ChatSessionRequest"].asString()) || mGroupAccepts.size()>=256)
        { error="Group chat invitation service is unavailable or busy"; return false; }
        mGroupAccepts.emplace_back(id,++mGroupVersions[id]);
        mGroupInvitations.erase(id);
    }
    else
    {
        if (!groupMessage(tag,id,15,"",error)) return false;
        ++mGroupVersions[id];
    }
    group.state=LLVKChatProtocol::Group::State::Joining; group.error.clear();
    group.moderationPending=false;
    group.participants.clear(); clearGroupUpdates(id);
    mGroupDeadlines[id]=std::chrono::steady_clock::now()+std::chrono::seconds(30);
    return true;
}

bool LLVKLoginTransport::leaveGroup(Owner::Tag tag,const LLUUID& id,std::string& error)
{
    error.clear();
    const auto found=mGroups.find(id);
    if (!chatContext(tag) || found==mGroups.end()) { error="Group chat membership is unavailable"; return false; }
    if (found->second.state==LLVKChatProtocol::Group::State::Closed) return true;
    if (!groupMessage(tag,id,18,"",error)) return false;
    ++mGroupVersions[id]; mGroupInvitations.erase(id);
    found->second.state=LLVKChatProtocol::Group::State::Closed; found->second.error.clear();
    found->second.moderationPending=false;
    found->second.participants.clear(); clearGroupUpdates(id);
    mGroupDeadlines.erase(id);
    return true;
}

bool LLVKLoginTransport::sendGroup(Owner::Tag tag,const LLUUID& id,const std::string& text,std::string& error)
{
    error.clear();
    const auto found=mGroups.find(id);
    if (!chatContext(tag) || found==mGroups.end() || found->second.state!=LLVKChatProtocol::Group::State::Joined ||
        !(found->second.powers&(std::uint64_t(1)<<16)))
    { error="Group chat is not ready to send"; return false; }
    const auto self=found->second.participants.find(mBootstrap->agentId);
    if (self!=found->second.participants.end() && self->second.textMuted)
    { error="Group chat text is muted by a moderator"; return false; }
    return groupMessage(tag,id,17,text,error);
}

std::vector<LLVKChatProtocol::Friend> LLVKLoginTransport::friends(Owner::Tag tag) const
{
    std::vector<LLVKChatProtocol::Friend> result;
    if (chatContext(tag)) for (const auto& [id,buddy] : mFriends) result.push_back(buddy);
    return result;
}

std::vector<LLVKChatProtocol::Group> LLVKLoginTransport::groups(Owner::Tag tag) const
{
    std::vector<LLVKChatProtocol::Group> result;
    if (chatContext(tag)) for (const auto& [id,group] : mGroups) result.push_back(group);
    return result;
}

bool LLVKLoginTransport::searchResidents(Owner::Tag tag,std::uint64_t query,std::string name,std::string& error)
{
    error.clear();
    if (!chatContext(tag) || !query || name.empty() || name.size()>255 || name.find('\0')!=name.npos ||
        wstring_to_utf8str(utf8str_to_wstring(name))!=name)
    { error="Invalid or disconnected resident search"; return false; }
    const auto capability=mCapabilities["AvatarPickerSearch"].asString();
    if (!https(capability)) { error="Resident search is unavailable in this region"; return false; }
    std::replace(name.begin(),name.end(),'.',' ');
    boost::urls::url url(capability);
    auto path=std::string(url.path());
    if (path.empty() || path.back()!='/') path+='/';
    url.set_path(path);
    url.params().set("page_size","100");
    url.params().set("names",name);
    mResidentSearch.cancel(); mSearchResult.reset(); mSearchQuery=0;
    if (!mResidentSearch.start(std::string(url.buffer()),"","application/llsd+xml",error,LLVKLoginHttp::Method::Get)) return false;
    mSearchQuery=query;
    return true;
}

void LLVKLoginTransport::pumpResidentSearch()
{
    if (!mSearchQuery) return;
    std::string error;
    const auto status=mResidentSearch.pump(error);
    if (status==LLVKLoginHttp::Status::Pending) return;
    LLVKChatProtocol::SearchResult result; result.query=mSearchQuery;
    const auto response=status==LLVKLoginHttp::Status::Complete ? decodeLlsd(mResidentSearch.takeResponse()) : std::nullopt;
    if (!response || !(*response)["agents"].isArray() || (*response)["agents"].size()>100 || response->has("failure_reason"))
        result.error="Resident search failed";
    else
    {
        const auto& agents=(*response)["agents"];
        for (auto iterator=agents.beginArray(); iterator!=agents.endArray(); ++iterator)
        {
            const auto& row=*iterator;
            const auto id=row["id"].asUUID();
            const auto display=row["display_name"].asString(),username=row["username"].asString();
            const auto valid=[](const std::string& text)
            { return !text.empty() && text.size()<=255 && text.find('\0')==text.npos && wstring_to_utf8str(utf8str_to_wstring(text))==text; };
            if (!row.isMap() || id.isNull() || !valid(display) || !valid(username))
            { result.residents.clear(); result.error="Resident search returned invalid identities"; break; }
            if (std::none_of(result.residents.begin(),result.residents.end(),[&](const auto& resident) { return resident.id==id; }))
                result.residents.push_back({id,display,username});
        }
    }
    mResidentSearch.cancel(); mSearchQuery=0; mSearchResult=std::move(result);
}

std::optional<LLVKChatProtocol::SearchResult> LLVKLoginTransport::takeResidentSearch(Owner::Tag tag)
{
    if (!chatContext(tag)) return {};
    return std::exchange(mSearchResult,std::nullopt);
}

std::optional<LLVKChatProtocol::Context> LLVKLoginTransport::chatContext(Owner::Tag tag) const
{
    if (mPhase!=Phase::Online || !mBootstrap || tag!=mTag) return {};
    auto name=mLoginData["first_name"].asString();
    const auto last=mLoginData["last_name"].asString();
    if (!last.empty()) name+=(name.empty() ? "" : " ")+last;
    return LLVKChatProtocol::Context{mTag,mBootstrap->agentId,name,mCircuit.regionName()};
}

std::vector<LLVKChatProtocol::Message> LLVKLoginTransport::takeMessages(Owner::Tag tag)
{
    if (!chatContext(tag)) return {};
    auto messages=mCircuit.takeMessages();
    messages.insert(messages.end(),std::make_move_iterator(mEventMessages.begin()),std::make_move_iterator(mEventMessages.end()));
    mEventMessages.clear(); mEventMessageBytes=0;
    if (!mInstantPublished && std::any_of(messages.begin(),messages.end(),[](const auto& message)
        { return message.kind==LLVKChatProtocol::Message::Kind::Instant; }))
    { report("im-published-to-ui"); mInstantPublished=true; }
    return messages;
}

bool LLVKLoginTransport::sendLocal(Owner::Tag tag,const std::string& text,std::uint8_t type,std::string& error)
{
    error.clear();
    if (!chatContext(tag)) { error="Local chat session is no longer connected"; return false; }
    return mCircuit.sendLocal(text,type,0,error);
}

bool LLVKLoginTransport::sendDirect(Owner::Tag tag,const LLUUID& recipient,const std::string& text,
    bool typing,bool typingStopped,std::string& error)
{
    error.clear();
    const auto context=chatContext(tag);
    if (!context) { error="IM session is no longer connected"; return false; }
    LLVKChatProtocol::Message message;
    message.kind=LLVKChatProtocol::Message::Kind::Instant;
    message.recipient=recipient; message.name=context->name; message.text=typing || typingStopped ? "typing" : text;
    message.dialog=typing ? 41 : typingStopped ? 42 : 0;
    const auto buddy=mFriends.find(recipient);
    if (!typing && !typingStopped && buddy!=mFriends.end() && !buddy->second.online) message.offline=1;
    message.conversation=LLVKChatProtocol::directSession(context->agent,recipient);
    return mCircuit.sendInstant(message,error);
}