#include "llvksessionowner.h"

#include <exception>
#include <limits>
#include <utility>

LLVKSessionOwner::Code LLVKSessionOwner::Inbox::post(const Response& response)
{
    const std::lock_guard lock(mMutex);
    if (mStopped) return Code::Stopped;
    if (!mAccepting || response.tag.generation != mGeneration) return Code::StaleReply;
    if (response.agreement.text.size() > Agreement::maximumTextBytes) return Code::InvalidReply;
    if (response.challenge.messageKey.size()>128) return Code::InvalidReply;
    if (mResponses.size() == capacity) return Code::QueueFull;
    mResponses.push_back(response);
    return Code::Ok;
}

void LLVKSessionOwner::Inbox::reset(std::uint64_t generation, bool accepting, bool stopped)
{
    const std::lock_guard lock(mMutex);
    mResponses.clear();
    mGeneration = generation;
    mAccepting = accepting;
    mStopped = stopped;
}

std::optional<LLVKSessionOwner::Response> LLVKSessionOwner::Inbox::pop()
{
    const std::lock_guard lock(mMutex);
    if (mResponses.empty()) return {};
    auto response = mResponses.front();
    mResponses.pop_front();
    return response;
}

LLVKSessionOwner::LLVKSessionOwner(std::shared_ptr<Transport> transport)
    : mOwnerThread(std::this_thread::get_id()), mTransport(std::move(transport)), mInbox(std::make_shared<Inbox>())
{
}

LLVKSessionOwner::~LLVKSessionOwner()
{
    mInbox->reset(mTag.generation, false, true);
    if (mTransportActive) std::terminate();
    for (const auto& services : mServices) if (!services.empty()) std::terminate();
}

LLVKSessionOwner::Status LLVKSessionOwner::result(Code code, Operation operation, Action action,
                                               std::uint64_t service) const
{
    return {code, action, operation, mTag.generation, service};
}

LLVKSessionOwner::Status LLVKSessionOwner::check() const
{
    if (std::this_thread::get_id() != mOwnerThread) return {Code::WrongThread};
    if (mInCallback) return result(Code::Reentrant);
    if (mState == State::Stopped) return result(Code::Stopped);
    return result(Code::Ok);
}

LLVKSessionOwner::Snapshot LLVKSessionOwner::snapshot() const
{
    if (std::this_thread::get_id() != mOwnerThread)
    {
        Snapshot rejected;
        rejected.status.code = Code::WrongThread;
        return rejected;
    }
    Snapshot snapshot;
    snapshot.state = mState;
    snapshot.tag = mTag;
    snapshot.agreement = mAgreement;
    snapshot.challenge = mChallenge;
    snapshot.identity = mIdentity;
    snapshot.status = mStatus;
    snapshot.cleanup = mCleanup;
    for (std::size_t index = 0; index < mServices.size(); ++index)
        snapshot.owned[index] = mServices[index].size();
    return snapshot;
}

LLVKSessionOwner::Status LLVKSessionOwner::beginLogin()
{
    if (auto status = check(); !status.ok()) return status;
    if (mState != State::PreLogin) return result(Code::WrongState);
    if (!mTransport)
    {
        mStatus = result(Code::TransportUnavailable, Operation::Authenticate, Action::Close);
        return mStatus;
    }
    if (mTag.generation == std::numeric_limits<std::uint64_t>::max())
        return result(Code::CounterExhausted, Operation::Authenticate, Action::Close);
    ++mTag.generation;
    mTag.request = 0;
    mStatus = result(Code::Ok);
    mCleanup = result(Code::Ok);
    mState = State::Authenticating;
    mInbox->reset(mTag.generation, true);
    return dispatch(Operation::Authenticate);
}

LLVKSessionOwner::Status LLVKSessionOwner::dispatch(Operation operation, std::optional<Agreement> accepted,std::string token)
{
    if (mTag.request == std::numeric_limits<std::uint64_t>::max())
        return disconnect(result(Code::CounterExhausted, operation, Action::Close), true);
    ++mTag.request;
    mTransportActive = true;
    const Request request{operation, mTag, accepted,std::move(token)};
    const auto code = invoke([&] { return mTransport->begin(request, mInbox); });
    if (code != Code::Ok)
    {
        const bool transportFailure = code == Code::AuthenticationFailed || code == Code::ConnectionFailed ||
            code == Code::Timeout || code == Code::TransportUnavailable || code == Code::CallbackFailed;
        return disconnect(result(transportFailure ? code : Code::ServiceFailed, operation, Action::RetryLogin), false);
    }
    mStatus = result(Code::Ok, operation, Action::Wait);
    return mStatus;
}

LLVKSessionOwner::Status LLVKSessionOwner::decideAgreement(Tag tag, Agreement agreement, bool accepted)
{
    if (auto status = check(); !status.ok()) return status;
    if (tag != mTag) return result(Code::StaleReply, Operation::Agreement);
    if (mState != State::AwaitingAgreement) return result(Code::WrongState, Operation::Agreement);
    if (!mAgreement || agreement != *mAgreement) return result(Code::StaleReply, Operation::Agreement);
    mAgreement.reset();
    if (!accepted)
        return disconnect(result(Code::AgreementRejected, Operation::Agreement, Action::RetryLogin), false);
    mState = State::Authenticating;
    return dispatch(Operation::Authenticate, agreement);
}

LLVKSessionOwner::Status LLVKSessionOwner::pumpOne()
{
    if (auto status = check(); !status.ok()) return status;
    if (mTransportActive && mState!=State::Disconnecting)
    {
        const auto code=invoke([&] { return mTransport->pump(); });
        if (code!=Code::Ok && code!=Code::Pending)
            return disconnect(result(code, mState==State::Authenticating ? Operation::Authenticate : Operation::Connect,
                Action::RetryLogin),false);
    }
    const auto response = mInbox->pop();
    return response ? receive(*response) : result(Code::Pending, Operation::None, Action::Wait);
}

LLVKSessionOwner::Status LLVKSessionOwner::submitChallenge(Tag tag,Challenge challenge,std::string token)
{
    if (auto status=check(); !status.ok()) return status;
    if (tag!=mTag || !mChallenge || challenge!=*mChallenge) return result(Code::StaleReply);
    if (mState!=State::AwaitingChallenge) return result(Code::WrongState);
    if (token.empty() || token.size()>256 || token.find('\0')!=token.npos) return result(Code::InvalidReply);
    mChallenge.reset();
    mState=State::Authenticating;
    return dispatch(Operation::Authenticate,{},std::move(token));
}

LLVKSessionOwner::Status LLVKSessionOwner::receive(const Response& response)
{
    if (response.tag != mTag) return result(Code::StaleReply);
    switch (response.kind)
    {
    case Response::Kind::ChallengeRequired:
        if (mState!=State::Authenticating) return result(Code::WrongState);
        if (!response.challenge.id || !response.challenge.revision || response.challenge.messageKey.empty()) return result(Code::InvalidReply);
        mChallenge=response.challenge;
        mState=State::AwaitingChallenge;
        mStatus=result(Code::Ok,Operation::Authenticate,Action::Wait);
        return mStatus;
    case Response::Kind::AgreementRequired:
        if (mState != State::Authenticating) return result(Code::WrongState);
        if (!response.agreement.id || !response.agreement.revision || response.agreement.text.empty() ||
            response.agreement.text.find('\0') != std::string::npos) return result(Code::InvalidReply);
        mAgreement = response.agreement;
        mState = State::AwaitingAgreement;
        mStatus = result(Code::Ok, Operation::Agreement, Action::Wait);
        return mStatus;
    case Response::Kind::Authorized:
        if (mState != State::Authenticating) return result(Code::WrongState);
        if (!response.identity.accountHandle || !response.identity.sessionHandle) return result(Code::InvalidReply);
        if (mTag.regionEpoch == std::numeric_limits<std::uint64_t>::max())
            return disconnect(result(Code::CounterExhausted, Operation::Connect, Action::Close), true);
        mIdentity = response.identity;
        ++mTag.regionEpoch;
        mState = State::Connecting;
        return dispatch(Operation::Connect);
    case Response::Kind::RegionConnected:
        if (mState != State::Connecting) return result(Code::WrongState);
        mState = State::Connected;
        mStatus = result(Code::Ok, Operation::Connect);
        return mStatus;
    case Response::Kind::Failed:
        if (mState != State::Authenticating && mState != State::Connecting && mState != State::Connected)
            return result(Code::WrongState);
        if (response.failure != Code::AuthenticationFailed && response.failure != Code::ConnectionFailed &&
            response.failure != Code::Timeout && response.failure != Code::TransportUnavailable)
            return result(Code::InvalidReply);
        return disconnect(result(response.failure,
            mState == State::Authenticating ? Operation::Authenticate : Operation::Connect, Action::RetryLogin), false);
    }
    return result(Code::InvalidReply);
}

LLVKSessionOwner::Status LLVKSessionOwner::disconnect(Status reason, bool stopping)
{
    mStopping = stopping;
    mState = State::Disconnecting;
    mStatus = reason;
    mAgreement.reset();
    mChallenge.reset();
    mIdentity.reset();
    mInbox->reset(mTag.generation, false, stopping);
    const auto cleanup = drain();
    return cleanup.ok() ? reason : cleanup;
}

LLVKSessionOwner::Status LLVKSessionOwner::drain()
{
    const auto cleanupResult = [&](Code code, std::uint64_t service = 0)
    {
        mCleanup = result(code == Code::Pending || code == Code::CallbackFailed ? code : Code::CleanupFailed,
            Operation::Retire, code == Code::Pending ? Action::Wait : Action::RetryCleanup, service);
        return mCleanup;
    };
    if (mTransportActive)
    {
        const auto code = invoke([&] { return mTransport->quiesce(mTag.generation); });
        if (code != Code::Ok) return cleanupResult(code);
        mTransportActive = false;
    }
    const std::size_t first = mStopping ? 0 : 1;
    for (std::size_t index = mServices.size(); index-- > first;)
    {
        auto& services = mServices[index];
        while (!services.empty())
        {
            auto& entry = services.back();
            const auto code = invoke([&] { return entry.service->retire(); });
            if (code != Code::Ok) return cleanupResult(code, entry.id);
            mInCallback = true;
            services.pop_back();
            mInCallback = false;
        }
    }
    mCleanup = result(Code::Ok, Operation::Retire);
    mState = mStopping ? State::Stopped : State::PreLogin;
    if (mStopping)
    {
        mInCallback = true;
        mTransport.reset();
        mInCallback = false;
    }
    return mCleanup;
}

LLVKSessionOwner::Status LLVKSessionOwner::cancel(Tag tag)
{
    if (auto status = check(); !status.ok()) return status;
    if (tag != mTag) return result(Code::StaleReply, Operation::Cancel);
    return cancel();
}

LLVKSessionOwner::Status LLVKSessionOwner::cancel()
{
    if (auto status = check(); !status.ok()) return status;
    if (mState == State::Disconnecting) return result(Code::WrongState, Operation::Cancel);
    if (mState == State::PreLogin) return result(Code::Ok, Operation::Cancel);
    return disconnect(result(Code::Cancelled, Operation::Cancel, Action::RetryLogin), false);
}

LLVKSessionOwner::Status LLVKSessionOwner::shutdown()
{
    const auto status = check();
    if (status.code == Code::Stopped) return result(Code::Ok, Operation::Shutdown);
    if (!status.ok()) return status;
    if (mState == State::Disconnecting)
    {
        mStopping = true;
        mInbox->reset(mTag.generation, false, true);
        return drain();
    }
    return disconnect(result(Code::Ok, Operation::Shutdown), true);
}

LLVKSessionOwner::Status LLVKSessionOwner::retryCleanup(Tag tag)
{
    if (auto status = check(); !status.ok()) return status;
    if (tag != mTag) return result(Code::StaleReply, Operation::Retire);
    return retryCleanup();
}

LLVKSessionOwner::Status LLVKSessionOwner::retryCleanup()
{
    if (auto status = check(); !status.ok()) return status;
    if (mState != State::Disconnecting) return result(Code::WrongState, Operation::Retire);
    return drain();
}

LLVKSessionOwner::Status LLVKSessionOwner::install(Lifetime lifetime, std::uint64_t serviceId,
                                                std::unique_ptr<Service>& service)
{
    if (auto status = check(); !status.ok()) return status;
    const auto index = static_cast<std::size_t>(lifetime);
    if (index >= mServices.size() || !service || !serviceId) return result(Code::InvalidReply, Operation::Acquire);
    const bool allowed = lifetime == Lifetime::Application ? mState == State::PreLogin :
        mState == State::Connecting || mState == State::Connected;
    if (!allowed) return result(Code::WrongState, Operation::Acquire);
    if (lifetime == Lifetime::Session && !mServices[2].empty()) return result(Code::WrongState, Operation::Acquire);
    for (const auto& services : mServices)
        for (const auto& entry : services)
            if (entry.id == serviceId) return result(Code::WrongState, Operation::Acquire);
    auto& services = mServices[index];
    services.reserve(services.size() + 1);
    services.push_back({serviceId, std::move(service)});
    const Context context{mTag, mIdentity};
    const auto code = invoke([&] { return services.back().service->acquire(context); });
    if (code != Code::Ok)
        return disconnect(result(code == Code::CallbackFailed ? code : Code::ServiceFailed,
            Operation::Acquire, lifetime == Lifetime::Application ? Action::Close : Action::RetryLogin, serviceId),
            lifetime == Lifetime::Application);
    return result(Code::Ok, Operation::Acquire, Action::None, serviceId);
}