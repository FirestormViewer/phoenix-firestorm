#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

class LLVKSessionOwner final
{
public:
    enum class State { PreLogin, Authenticating, AwaitingAgreement, Connecting, Connected, Disconnecting, Stopped, AwaitingChallenge };
    enum class Lifetime { Application, Session, Region };
    enum class Code
    {
        Ok, Pending, WrongThread, Reentrant, WrongState, Stopped, StaleReply,
        InvalidReply, QueueFull, TransportUnavailable, AuthenticationFailed,
        ConnectionFailed, Timeout, AgreementRejected, Cancelled, ServiceFailed,
        CleanupFailed, CallbackFailed, CounterExhausted
    };
    enum class Action { None, Wait, RetryLogin, RetryCleanup, Close };
    enum class Operation { None, Authenticate, Agreement, Connect, Acquire, Retire, Cancel, Shutdown };
    struct Tag
    {
        std::uint64_t generation = 0;
        std::uint64_t request = 0;
        std::uint64_t regionEpoch = 0;
        bool operator==(const Tag&) const = default;
    };
    struct Agreement
    {
        static constexpr std::size_t maximumTextBytes = 65536;
        std::uint64_t id = 0;
        std::uint64_t revision = 0;
        std::string text;
        bool operator==(const Agreement&) const = default;
    };
    struct Identity
    {
        std::uint64_t accountHandle = 0;
        std::uint64_t sessionHandle = 0;
        bool operator==(const Identity&) const = default;
    };
    struct Challenge
    {
        std::uint64_t id=0,revision=0;
        std::string messageKey;
        bool operator==(const Challenge&) const = default;
    };
    struct Status
    {
        Code code = Code::Ok;
        Action action = Action::None;
        Operation operation = Operation::None;
        std::uint64_t generation = 0;
        std::uint64_t service = 0;
        bool ok() const { return code == Code::Ok; }
    };
    struct Response
    {
        enum class Kind { AgreementRequired, Authorized, RegionConnected, Failed, ChallengeRequired };
        Kind kind = Kind::Failed;
        Tag tag;
        Agreement agreement;
        Identity identity;
        Code failure = Code::ConnectionFailed;
        Challenge challenge;
    };
    class Inbox final
    {
    public:
        static constexpr std::size_t capacity = 64;
        Code post(const Response& response);
    private:
        friend class LLVKSessionOwner;
        void reset(std::uint64_t generation, bool accepting, bool stopped = false);
        std::optional<Response> pop();
        std::mutex mMutex;
        std::deque<Response> mResponses;
        std::uint64_t mGeneration = 0;
        bool mAccepting = false;
        bool mStopped = false;
    };
    struct Request
    {
        Operation operation = Operation::None;
        Tag tag;
        std::optional<Agreement> acceptedAgreement;
        std::string challengeToken;
    };
    class Transport
    {
    public:
        virtual ~Transport() = default;
        virtual Code begin(const Request& request, const std::shared_ptr<Inbox>& replies) = 0;
        virtual Code pump() { return Code::Ok; }
        virtual Code quiesce(std::uint64_t generation) = 0;
    };
    struct Context
    {
        Tag tag;
        std::optional<Identity> identity;
    };
    class Service
    {
    public:
        virtual ~Service() = default;
        virtual Code acquire(const Context& context) = 0;
        virtual Code retire() = 0;
    };
    struct Snapshot
    {
        State state = State::PreLogin;
        Tag tag;
        std::optional<Agreement> agreement;
        std::optional<Identity> identity;
        Status status;
        Status cleanup;
        std::array<std::size_t, 3> owned{};
        std::optional<Challenge> challenge;
    };

    explicit LLVKSessionOwner(std::shared_ptr<Transport> transport = {});
    ~LLVKSessionOwner();
    LLVKSessionOwner(const LLVKSessionOwner&) = delete;
    LLVKSessionOwner& operator=(const LLVKSessionOwner&) = delete;
    LLVKSessionOwner(LLVKSessionOwner&&) = delete;
    LLVKSessionOwner& operator=(LLVKSessionOwner&&) = delete;

    Status beginLogin();
    Status decideAgreement(Tag tag, Agreement agreement, bool accepted);
    Status submitChallenge(Tag tag,Challenge challenge,std::string token);
    Status pumpOne();
    Status cancel(Tag tag);
    Status cancel();
    Status shutdown();
    Status retryCleanup(Tag tag);
    Status retryCleanup();
    Status install(Lifetime lifetime, std::uint64_t serviceId, std::unique_ptr<Service>& service);
    Snapshot snapshot() const;

private:
    struct Entry { std::uint64_t id; std::unique_ptr<Service> service; };
    Status check() const;
    Status result(Code code, Operation operation = Operation::None, Action action = Action::None,
                  std::uint64_t service = 0) const;
    Status dispatch(Operation operation, std::optional<Agreement> accepted = {},std::string token = {});
    Status receive(const Response& response);
    Status disconnect(Status reason, bool stopping);
    Status drain();
    template<class Callback> Code invoke(Callback&& callback)
    {
        mInCallback = true;
        Code code;
        try { code = callback(); }
        catch (...) { code = Code::CallbackFailed; }
        mInCallback = false;
        return code;
    }
    const std::thread::id mOwnerThread;
    std::shared_ptr<Transport> mTransport;
    std::shared_ptr<Inbox> mInbox;
    std::array<std::vector<Entry>, 3> mServices;
    State mState = State::PreLogin;
    Tag mTag;
    std::optional<Agreement> mAgreement;
    std::optional<Challenge> mChallenge;
    std::optional<Identity> mIdentity;
    Status mStatus;
    Status mCleanup;
    bool mInCallback = false;
    bool mTransportActive = false;
    bool mStopping = false;
};