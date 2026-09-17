#include "llvksessionowner.h"
#include <iostream>
#include <stdexcept>

namespace
{
    using Owner=LLVKSessionOwner;
    void check(bool value,const char* message)
    { if (!value) throw std::runtime_error(message); }
    struct Transport final : Owner::Transport
    {
        Owner::Request request;
        std::shared_ptr<Owner::Inbox> inbox;
        unsigned requests=0,drains=0,pumps=0;
        Owner::Code begin(const Owner::Request& value,const std::shared_ptr<Owner::Inbox>& replies) override
        { request=value; inbox=replies; ++requests; return Owner::Code::Ok; }
        Owner::Code pump() override { ++pumps; return Owner::Code::Ok; }
        Owner::Code quiesce(std::uint64_t) override { ++drains; return Owner::Code::Ok; }
    };
    struct Service final : Owner::Service
    {
        std::vector<unsigned>& events;
        unsigned id;
        bool fail=false;
        Service(std::vector<unsigned>& output,unsigned value) : events(output),id(value) {}
        Owner::Code acquire(const Owner::Context&) override { events.push_back(id); return Owner::Code::Ok; }
        Owner::Code retire() override
        {
            events.push_back(id+10);
            if (fail) { fail=false; return Owner::Code::CleanupFailed; }
            return Owner::Code::Ok;
        }
    };
}

int main()
{
    try
    {
        Owner unavailable;
        check(unavailable.beginLogin().code==Owner::Code::TransportUnavailable,"missing transport is explicit");
        check(unavailable.snapshot().state==Owner::State::PreLogin && !unavailable.snapshot().identity,"no fake authentication");
        check(unavailable.shutdown().ok(),"empty owner shutdown");

        {
            auto agreementTransport=std::make_shared<Transport>();
            Owner agreementOwner(agreementTransport);
            struct Cleanup { Owner& owner; ~Cleanup() { owner.shutdown(); } } cleanup{agreementOwner};
            check(agreementOwner.beginLogin().ok(),"agreement attempt started");
            Owner::Response agreementReply;
            agreementReply.kind=Owner::Response::Kind::AgreementRequired;
            agreementReply.tag=agreementOwner.snapshot().tag;
            agreementReply.agreement={7,2,{}};
            check(agreementTransport->inbox->post(agreementReply)==Owner::Code::Ok &&
                agreementOwner.pumpOne().code==Owner::Code::InvalidReply,"empty agreement cannot be accepted");
            agreementReply.agreement.text.assign(Owner::Agreement::maximumTextBytes+1,'x');
            check(agreementTransport->inbox->post(agreementReply)==Owner::Code::InvalidReply,"agreement ingress is bounded");
            agreementReply.agreement.text="The exact agreement supplied by the service.\nSecond paragraph.";
            check(agreementTransport->inbox->post(agreementReply)==Owner::Code::Ok &&
                agreementOwner.pumpOne().ok(),"agreement content accepted");
            const auto agreementSnapshot=agreementOwner.snapshot();
            check(agreementSnapshot.agreement==agreementReply.agreement,"snapshot preserves exact agreement content");
            auto changed=*agreementSnapshot.agreement;
            changed.text+=" changed";
            check(agreementOwner.decideAgreement(agreementSnapshot.tag,changed,true).code==Owner::Code::StaleReply,
                "different content cannot authorize agreement");
            check(agreementOwner.decideAgreement(agreementSnapshot.tag,*agreementSnapshot.agreement,true).ok(),"explicit acceptance");
            check(agreementTransport->request.acceptedAgreement==agreementSnapshot.agreement,"transport receives exact acceptance");
            check(agreementOwner.decideAgreement(agreementSnapshot.tag,*agreementSnapshot.agreement,true).code==Owner::Code::StaleReply,
                "old acceptance cannot apply to next request");
            agreementReply.tag=agreementOwner.snapshot().tag;
            ++agreementReply.agreement.revision;
            check(agreementTransport->inbox->post(agreementReply)==Owner::Code::Ok && agreementOwner.pumpOne().ok(),"revised agreement");
            check(agreementOwner.decideAgreement(agreementReply.tag,agreementReply.agreement,false).code==Owner::Code::AgreementRejected,
                "explicit rejection ends attempt");
            check(agreementOwner.snapshot().state==Owner::State::PreLogin && !agreementOwner.snapshot().agreement,
                "agreement discarded after rejection");
            check(agreementOwner.beginLogin().ok(),"challenge attempt started");
            Owner::Response challenge;
            challenge.kind=Owner::Response::Kind::ChallengeRequired;
            challenge.tag=agreementOwner.snapshot().tag;
            challenge.challenge={1,1,"LoginFailedAuthenticationMFARequired"};
            check(agreementTransport->inbox->post(challenge)==Owner::Code::Ok && agreementOwner.pumpOne().ok(),"challenge delivered");
            check(agreementOwner.snapshot().state==Owner::State::AwaitingChallenge,"challenge not authorization");
            check(agreementOwner.submitChallenge(challenge.tag,challenge.challenge,"").code==Owner::Code::InvalidReply,"empty token rejected");
            check(agreementOwner.submitChallenge(challenge.tag,challenge.challenge,"123456").ok(),"explicit token accepted");
            check(agreementTransport->request.challengeToken=="123456","token forwarded only to transport");
            check(agreementOwner.submitChallenge(challenge.tag,challenge.challenge,"123456").code==Owner::Code::StaleReply,"stale token rejected");
        }

        auto transport=std::make_shared<Transport>();
        Owner owner(transport);
        std::vector<unsigned> events;
        auto concrete=std::make_unique<Service>(events,1);
        auto* application=concrete.get();
        std::unique_ptr<Owner::Service> service=std::move(concrete);
        check(owner.install(Owner::Lifetime::Application,1,service).ok() && !service,"application ownership transferred");
        check(owner.beginLogin().ok(),"begin controlled login");
        owner.pumpOne();
        check(transport->pumps==1,"active transport progresses on owner thread");
        const auto cancelled=owner.snapshot().tag;
        auto oldInbox=transport->inbox;
        check(owner.cancel(cancelled).code==Owner::Code::Cancelled,"tagged cancel");
        owner.pumpOne();
        check(transport->pumps==1,"cancelled transport is not pumped");
        check(owner.snapshot().owned[0]==1 && events==std::vector<unsigned>{1},"application survives cancellation");
        check(owner.beginLogin().ok(),"new attempt");
        const auto current=owner.snapshot().tag;
        check(owner.cancel(cancelled).code==Owner::Code::StaleReply && owner.snapshot().tag==current,"old cancel rejected");
        Owner::Response response;
        response.kind=Owner::Response::Kind::Authorized;
        response.tag=cancelled;
        response.identity={1,2};
        check(oldInbox->post(response)==Owner::Code::StaleReply,"old response rejected");
        response.tag=current;
        check(transport->inbox->post(response)==Owner::Code::Ok && owner.pumpOne().ok(),"controlled authorization");
        check(owner.snapshot().state==Owner::State::Connecting,"authorization is not connection");
        response.kind=Owner::Response::Kind::RegionConnected;
        response.tag=owner.snapshot().tag;
        check(transport->inbox->post(response)==Owner::Code::Ok && owner.pumpOne().ok(),"controlled connection");
        check(owner.snapshot().state==Owner::State::Connected,"adapter-confirmed connection");
        service=std::make_unique<Service>(events,2);
        check(owner.install(Owner::Lifetime::Session,2,service).ok(),"session service adopted");
        service=std::make_unique<Service>(events,3);
        check(owner.install(Owner::Lifetime::Region,3,service).ok(),"region service adopted");
        application->fail=true;
        check(owner.shutdown().code==Owner::Code::CleanupFailed,"failed retirement retained");
        auto snapshot=owner.snapshot();
        check(snapshot.state==Owner::State::Disconnecting && !snapshot.identity,"identity revoked before retirement");
        check(snapshot.owned==std::array<std::size_t,3>{1,0,0},"failed application retained after children");
        check(events==std::vector<unsigned>{1,2,3,13,12,11},"reverse lifetime order");
        check(owner.retryCleanup(cancelled).code==Owner::Code::StaleReply,"old cleanup action rejected");
        check(events.size()==6,"stale cleanup did not retire anything");
        check(owner.retryCleanup(snapshot.tag).ok(),"explicit matching cleanup retry");
        check(owner.snapshot().state==Owner::State::Stopped,"shutdown completed");
        check(owner.shutdown().ok() && events.size()==7,"shutdown idempotent");
        check(transport->drains==2,"each transport attempt drained once");
        std::cout<<"Native session integration: all checks passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}