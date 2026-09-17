#include "linden_common.h"
#include "llvklogintransport.h"
#include "llsdserialize.h"
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
    : mConfiguration(std::move(configuration)),mHttp(mConfiguration.http),mEvents(mConfiguration.http) {}

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
    LLSD names=LLSD::emptyArray();
    for (const auto name : {"EventQueueGet","SimulatorFeatures","GetDisplayNames","FetchInventoryDescendents2",
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
        mParameters=LLSD();
        Owner::Response response; response.kind=Owner::Response::Kind::Authorized;
        response.identity={mTag.generation,mTag.generation};
        return post(response) ? Owner::Code::Ok : Owner::Code::InvalidReply;
    }
    if (mPhase!=Phase::Seed && mPhase!=Phase::Online) return Owner::Code::Pending;
    const auto circuit=mCircuit.pump(error);
    if (circuit==LLVKRegionCircuit::Status::Failed) return connectionFailure("circuit-progress-failed");
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
            if (mPendingEvents.size()+(*events)["events"].size()>256 || mPendingEventBytes+response.size()>16*1024*1024)
                return connectionFailure("event-queue-retention-full");
            const auto& entries=(*events)["events"];
            if (entries.size()) mPendingEventBytes+=response.size();
            for (auto iterator=entries.beginArray(); iterator!=entries.endArray(); ++iterator) mPendingEvents.push_back(*iterator);
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