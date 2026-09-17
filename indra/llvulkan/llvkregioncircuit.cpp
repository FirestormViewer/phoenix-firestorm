#include "linden_common.h"
#include "llvkregioncircuit.h"
#include <boost/asio.hpp>
#include <boost/endian/conversion.hpp>
#include <chrono>
#include <deque>
#include <map>
#include <set>
#include <span>

namespace
{
    using Bytes=std::vector<std::uint8_t>;
    constexpr std::uint32_t low(std::uint16_t value) { return 0xffff0000u|value; }
    constexpr auto useCircuit=low(3),regionHandshake=low(148),handshakeReply=low(149);
    constexpr auto completeMovement=low(249),movementComplete=low(250),logoutRequest=low(252),logoutReply=low(253);
    constexpr std::uint32_t packetAck=0xfffffffbu;
    void add32(Bytes& bytes,std::uint32_t value)
    {
        const auto offset=bytes.size(); bytes.resize(offset+4);
        boost::endian::store_little_u32(bytes.data()+offset,value);
    }
    void addUuid(Bytes& bytes,const LLUUID& value) { bytes.insert(bytes.end(),value.mData,value.mData+16); }
    struct Packet
    {
        std::uint8_t flags=0;
        std::uint32_t sequence=0,id=0;
        Bytes payload;
        std::vector<std::uint32_t> acknowledgements;
    };
    std::optional<Packet> decode(std::span<const std::uint8_t> input)
    {
        if (input.size()<7 || input.size()>65535) return {};
        Packet packet;
        packet.flags=input[0]; packet.sequence=boost::endian::load_big_u32(input.data()+1);
        std::size_t end=input.size();
        if (packet.flags&0x10)
        {
            const auto count=input.back();
            if (end<7+std::size_t(count)*4) return {};
            end-=1+std::size_t(count)*4;
            for (std::size_t index=end; index+4<input.size(); index+=4)
                packet.acknowledgements.push_back(boost::endian::load_big_u32(input.data()+index));
        }
        Bytes data;
        for (std::size_t index=6; index<end; ++index)
        {
            const auto byte=input[index];
            if ((packet.flags&0x80) && !byte)
            {
                if (++index>=end || !input[index] || data.size()+input[index]>65535) return {};
                data.insert(data.end(),input[index],0);
            }
            else data.push_back(byte);
            if (data.size()>65535) return {};
        }
        if (data.empty()) return {};
        std::size_t offset=1;
        packet.id=data[0];
        if (packet.id==255)
        {
            if (data.size()<2) return {};
            packet.id=0xff00u|data[1]; offset=2;
            if (data[1]==255)
            {
                if (data.size()<4) return {};
                packet.id=low(boost::endian::load_big_u16(data.data()+2)); offset=4;
            }
        }
        offset+=input[5];
        if (offset>data.size()) return {};
        packet.payload.assign(data.begin()+offset,data.end());
        return packet;
    }
    bool identityMatches(const Bytes& payload,const LLVKLoginProtocol::Bootstrap& bootstrap)
    {
        return payload.size()>=32 && std::equal(bootstrap.agentId.mData,bootstrap.agentId.mData+16,payload.begin()) &&
            std::equal(bootstrap.sessionId.mData,bootstrap.sessionId.mData+16,payload.begin()+16);
    }
    bool handshakeValid(const Bytes& payload)
    {
        if (payload.size()<6) return false;
        std::size_t offset=6+payload[5]+16+1+8+16+128+32+16+8;
        for (unsigned field=0; field<3; ++field)
        {
            if (offset>=payload.size()) return false;
            offset+=1+payload[offset];
        }
        if (offset>=payload.size()) return false;
        return offset+1+std::size_t(payload[offset])*16==payload.size();
    }
}

struct LLVKRegionCircuit::Impl
{
    using Clock=std::chrono::steady_clock;
    boost::asio::io_context context;
    boost::asio::ip::udp::socket socket{context};
    LLVKLoginProtocol::Bootstrap bootstrap;
    struct Reliable { Bytes bytes; Clock::time_point sent; unsigned retries=0; std::uint32_t id=0; };
    std::map<std::uint32_t,Reliable> outstanding;
    std::set<std::uint32_t> received;
    std::deque<std::uint32_t> receivedOrder;
    std::uint32_t sequence=0;
    std::uint8_t ping=0;
    bool circuitAcknowledged=false,handshake=false,movement=false,movementSent=false;
    Status status=Status::Idle;
    Clock::time_point started,lastReceive,lastPing,closeStarted;
    bool send(std::uint32_t id,const Bytes& payload,bool reliable,std::string& error)
    {
        if (sequence==UINT32_MAX || payload.size()>1400 || (reliable && outstanding.size()>=32))
        { error="Native region send budget exceeded"; return false; }
        Bytes bytes(6,0);
        bytes[0]=reliable ? 0x40 : 0;
        boost::endian::store_big_u32(bytes.data()+1,++sequence);
        if (id<255) bytes.push_back(static_cast<std::uint8_t>(id));
        else
        {
            bytes.push_back(255); bytes.push_back(255);
            bytes.push_back(static_cast<std::uint8_t>(id>>8)); bytes.push_back(static_cast<std::uint8_t>(id));
        }
        bytes.insert(bytes.end(),payload.begin(),payload.end());
        boost::system::error_code problem;
        socket.send(boost::asio::buffer(bytes),0,problem);
        if (problem) { error="Native simulator UDP send failed"; return false; }
        if (reliable) outstanding.emplace(sequence,Reliable{std::move(bytes),Clock::now(),0,id});
        return true;
    }
    Bytes identity() const
    {
        Bytes bytes; addUuid(bytes,bootstrap.agentId); addUuid(bytes,bootstrap.sessionId); return bytes;
    }
    bool acknowledge(std::uint32_t acknowledged,std::string& error)
    {
        const auto found=outstanding.find(acknowledged);
        if (found==outstanding.end()) return true;
        if (found->second.id==useCircuit) circuitAcknowledged=true;
        outstanding.erase(found);
        if (circuitAcknowledged && !movementSent && status==Status::Connecting)
        {
            auto bytes=identity(); add32(bytes,bootstrap.circuitCode);
            if (!send(completeMovement,bytes,true,error)) return false;
            movementSent=true;
        }
        return true;
    }
    bool accept(const Packet& packet,std::string& error)
    {
        for (const auto acknowledged : packet.acknowledgements)
            if (!acknowledge(acknowledged,error)) return false;
        if (packet.flags&0x40)
        {
            Bytes ack{1}; add32(ack,packet.sequence);
            if (!send(packetAck,ack,false,error)) return false;
            if (received.contains(packet.sequence)) return true;
            received.insert(packet.sequence); receivedOrder.push_back(packet.sequence);
            if (receivedOrder.size()>4096) { received.erase(receivedOrder.front()); receivedOrder.pop_front(); }
        }
        const auto& payload=packet.payload;
        if (packet.id==packetAck)
        {
            if (payload.empty() || payload.size()!=1+std::size_t(payload[0])*4) return true;
            for (std::size_t offset=1; offset<payload.size(); offset+=4)
                if (!acknowledge(boost::endian::load_little_u32(payload.data()+offset),error)) return false;
        }
        else if (packet.id==1 && payload.size()==5)
        {
            if (!send(2,Bytes{payload[0]},false,error)) return false;
        }
        else if (packet.id==regionHandshake && status==Status::Connecting)
        {
            if (!handshakeValid(payload)) { error="Native simulator handshake is malformed"; return false; }
            auto reply=identity(); add32(reply,0);
            if (!send(handshakeReply,reply,true,error)) return false;
            handshake=true;
        }
        else if (packet.id==movementComplete && status==Status::Connecting)
        {
            if (!identityMatches(payload,bootstrap) || payload.size()<70 ||
                boost::endian::load_little_u64(payload.data()+56)!=bootstrap.regionHandle ||
                payload.size()!=70+boost::endian::load_little_u16(payload.data()+68))
            { error="Native simulator movement identity or payload is invalid"; return false; }
            movement=true;
        }
        else if (packet.id==logoutReply && status==Status::Closing)
        {
            if (!identityMatches(payload,bootstrap) || payload.size()<33 || payload.size()!=33+std::size_t(payload[32])*16)
            { error="Native simulator logout reply is invalid"; return false; }
            status=Status::Closed;
        }
        else if (packet.id==low(163) || packet.id==0xfffffffdu)
        { error="Native simulator closed the circuit"; return false; }
        if (status==Status::Connecting && circuitAcknowledged && handshake && movement) status=Status::Connected;
        return true;
    }
};

LLVKRegionCircuit::LLVKRegionCircuit() : mImpl(std::make_unique<Impl>()) {}
LLVKRegionCircuit::~LLVKRegionCircuit()=default;

bool LLVKRegionCircuit::start(const LLVKLoginProtocol::Bootstrap& bootstrap,std::string& error)
{
    error.clear();
    cancel();
    mImpl=std::make_unique<Impl>();
    mImpl->bootstrap=bootstrap;
    boost::system::error_code problem;
    const auto address=boost::asio::ip::make_address(bootstrap.simulatorAddress,problem);
    if (problem || !address.is_v4() || !bootstrap.simulatorPort || !bootstrap.circuitCode ||
        bootstrap.agentId.isNull() || bootstrap.sessionId.isNull())
    { error="Native simulator circuit parameters are invalid"; return false; }
    mImpl->socket.open(boost::asio::ip::udp::v4(),problem);
    if (!problem) mImpl->socket.connect({address,bootstrap.simulatorPort},problem);
    if (!problem) mImpl->socket.non_blocking(true,problem);
    if (problem) { error="Native simulator UDP socket failed"; cancel(); return false; }
    mImpl->started=mImpl->lastReceive=mImpl->lastPing=Impl::Clock::now();
    mImpl->status=Status::Connecting;
    Bytes payload; add32(payload,bootstrap.circuitCode); addUuid(payload,bootstrap.sessionId); addUuid(payload,bootstrap.agentId);
    if (!mImpl->send(useCircuit,payload,true,error)) { cancel(); return false; }
    return true;
}

LLVKRegionCircuit::Status LLVKRegionCircuit::pump(std::string& error)
{
    error.clear();
    if (mImpl->status==Status::Idle || mImpl->status==Status::Closed || mImpl->status==Status::Failed) return mImpl->status;
    const auto fail=[&](const char* message)
    { if (error.empty()) error=message; mImpl->status=Status::Failed; return mImpl->status; };
    for (unsigned count=0; count<128; ++count)
    {
        std::array<std::uint8_t,65536> buffer{};
        boost::system::error_code problem;
        const auto size=mImpl->socket.receive(boost::asio::buffer(buffer),0,problem);
        if (problem==boost::asio::error::would_block || problem==boost::asio::error::try_again) break;
        if (problem) return fail("Native simulator UDP receive failed");
        const auto packet=decode({buffer.data(),size});
        if (!packet) continue;
        mImpl->lastReceive=Impl::Clock::now();
        if (!mImpl->accept(*packet,error)) return fail("Native simulator packet failed");
        if (mImpl->status==Status::Closed) return mImpl->status;
    }
    const auto now=Impl::Clock::now();
    if ((mImpl->status==Status::Connecting && now-mImpl->started>std::chrono::seconds(60)) ||
        now-mImpl->lastReceive>std::chrono::seconds(60)) return fail("Native simulator connection timed out");
    if (mImpl->status==Status::Closing && now-mImpl->closeStarted>std::chrono::seconds(10))
        return fail("Native simulator logout timed out");
    for (auto& [sequence,packet] : mImpl->outstanding)
    {
        if (now-packet.sent<std::chrono::seconds(2)) continue;
        if (++packet.retries>5) return fail("Native simulator reliable packet was not acknowledged");
        packet.bytes[0]|=0x20;
        boost::system::error_code problem;
        mImpl->socket.send(boost::asio::buffer(packet.bytes),0,problem);
        if (problem) return fail("Native simulator reliable resend failed");
        packet.sent=now;
    }
    if (mImpl->status==Status::Connected && now-mImpl->lastPing>=std::chrono::seconds(5))
    {
        Bytes ping{++mImpl->ping}; add32(ping,mImpl->outstanding.empty() ? 0 : mImpl->outstanding.begin()->first);
        if (!mImpl->send(1,ping,false,error)) return fail("Native simulator keepalive failed");
        mImpl->lastPing=now;
    }
    return mImpl->status;
}

LLVKRegionCircuit::Status LLVKRegionCircuit::close(std::string& error)
{
    error.clear();
    if (mImpl->status==Status::Idle || mImpl->status==Status::Closed) return Status::Closed;
    if (mImpl->status==Status::Failed) return Status::Failed;
    if (mImpl->status!=Status::Closing)
    {
        mImpl->outstanding.clear();
        if (!mImpl->send(logoutRequest,mImpl->identity(),true,error)) { mImpl->status=Status::Failed; return Status::Failed; }
        mImpl->status=Status::Closing;
        mImpl->closeStarted=Impl::Clock::now();
    }
    return pump(error);
}

void LLVKRegionCircuit::cancel()
{
    boost::system::error_code ignored;
    mImpl->socket.close(ignored);
    mImpl->outstanding.clear(); mImpl->received.clear(); mImpl->receivedOrder.clear();
    mImpl->bootstrap={}; mImpl->status=Status::Closed;
}